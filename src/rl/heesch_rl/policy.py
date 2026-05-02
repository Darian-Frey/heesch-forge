"""GNN policy network per M4.1 §2.2 + §3.2.

Architecture (locked at the M4.2 baseline):
    - Per-cell features f(c) ∈ R^6  (xy after canonicalisation,
      is_boundary, boundary_degree, is_corner, n_normalised).
    - Three GraphSAGE layers (hidden dim H = 64) with ReLU.
    - Per-boundary-cell add-logit: a small MLP head over the
      boundary cells' hidden vectors.
    - Stop-logit: a small MLP head over a graph-pool readout.

The boundary cells are *empty* positions adjacent to the polyomino;
they are NOT nodes in the cell-adjacency graph (which contains only
occupied cells). To produce a per-boundary-position logit, we
compute each boundary's embedding as the mean of its 4-cardinal
*occupied* neighbours' hidden vectors. This is M4.1-compliant: the
GNN runs over the polyomino-cell graph, and the action head reads
neighbourhood-aggregated features for each candidate add-position.

This file deliberately does NOT depend on any specific torch
version's quirks beyond what `pyproject.toml` pins. M4.3 (training
loop) lives in a separate file; this module only defines the
forward pass.
"""

from __future__ import annotations

from dataclasses import dataclass

import torch
import torch.nn as nn
import torch.nn.functional as F
from torch_geometric.nn import SAGEConv

from .canonical import Cells
from .env import boundary_cells

NODE_FEATURE_DIM = 6
_DIRS_4 = ((1, 0), (-1, 0), (0, 1), (0, -1))


@dataclass(frozen=True)
class Observation:
    """A single MDP-state observation in tensor form.

    Attributes:
        node_features: [N_cells, NODE_FEATURE_DIM] cell features.
        edge_index:    [2, E] long tensor; 4-cardinal cell adjacency
                       (each undirected edge appears twice, both
                       directions).
        cell_index:    list of (x, y) tuples in the same order as
                       `node_features` rows. Used by the action head
                       to map boundary positions back to cells.
        boundaries:    list of (x, y) boundary positions (the legal
                       Add(x, y) targets), in the same order as the
                       env's `legal_actions()` output.
        stop_legal:    True if the env's legal_actions includes Stop.
    """

    node_features: torch.Tensor
    edge_index: torch.Tensor
    cell_index: tuple[tuple[int, int], ...]
    boundaries: tuple[tuple[int, int], ...]
    stop_legal: bool


def encode_observation(cells: Cells, n_max: int) -> Observation:
    """Build the GNN observation from a canonical polyomino."""
    cell_list = sorted(cells)
    pos_to_idx = {c: i for i, c in enumerate(cell_list)}

    feats: list[list[float]] = []
    for x, y in cell_list:
        empties = sum(
            1 for dx, dy in _DIRS_4 if (x + dx, y + dy) not in cells
        )
        is_boundary = 1.0 if empties > 0 else 0.0
        # is_corner: heuristic from M4.1 §2.2 -- exactly two
        # diagonally-opposite occupied 4-cardinal neighbours, the
        # other two empty.
        north = (x, y + 1) in cells
        south = (x, y - 1) in cells
        east = (x + 1, y) in cells
        west = (x - 1, y) in cells
        is_corner = 1.0 if (
            (north and south and not east and not west)
            or (east and west and not north and not south)
        ) else 0.0
        feats.append([
            float(x), float(y),
            is_boundary,
            float(empties),
            is_corner,
            len(cells) / max(n_max, 1),
        ])

    edges: list[tuple[int, int]] = []
    for i, (x, y) in enumerate(cell_list):
        for dx, dy in _DIRS_4:
            n = (x + dx, y + dy)
            j = pos_to_idx.get(n)
            if j is not None:
                edges.append((i, j))   # both directions emerge naturally

    boundaries = boundary_cells(cells)

    node_features = torch.tensor(feats, dtype=torch.float32)
    if edges:
        edge_index = torch.tensor(edges, dtype=torch.long).t().contiguous()
    else:
        edge_index = torch.zeros((2, 0), dtype=torch.long)

    return Observation(
        node_features=node_features,
        edge_index=edge_index,
        cell_index=tuple(cell_list),
        boundaries=boundaries,
        stop_legal=len(cells) >= 1,
    )


class HeeschPolicy(nn.Module):
    """GraphSAGE encoder + add-logit head + stop-logit head.

    Forward returns a tuple (add_logits, stop_logit). Add_logits has
    one entry per boundary in the input observation, in the same
    order as `obs.boundaries`. The caller composes these with the
    stop-logit (when stop_legal) into a single softmax.
    """

    def __init__(self, hidden_dim: int = 64, num_layers: int = 3) -> None:
        super().__init__()
        if num_layers < 1:
            raise ValueError("num_layers must be >= 1")
        self.hidden_dim = hidden_dim
        layers: list[nn.Module] = []
        in_dim = NODE_FEATURE_DIM
        for _ in range(num_layers):
            layers.append(SAGEConv(in_dim, hidden_dim))
            in_dim = hidden_dim
        self.convs = nn.ModuleList(layers)

        self.add_head = nn.Sequential(
            nn.Linear(hidden_dim, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, 1),
        )
        self.stop_head = nn.Sequential(
            nn.Linear(hidden_dim, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, 1),
        )

    def forward(self, obs: Observation) -> tuple[torch.Tensor, torch.Tensor]:
        h = obs.node_features
        for conv in self.convs:
            h = F.relu(conv(h, obs.edge_index))
        # Per-boundary-position embedding: mean of occupied 4-cardinal neighbours.
        cell_to_idx = {c: i for i, c in enumerate(obs.cell_index)}
        add_logits_list: list[torch.Tensor] = []
        for bx, by in obs.boundaries:
            neigh_idx: list[int] = []
            for dx, dy in _DIRS_4:
                idx = cell_to_idx.get((bx + dx, by + dy))
                if idx is not None:
                    neigh_idx.append(idx)
            if not neigh_idx:
                # Should be impossible for a real boundary cell.
                emb = torch.zeros(self.hidden_dim, dtype=h.dtype, device=h.device)
            else:
                emb = h[neigh_idx].mean(dim=0)
            add_logits_list.append(self.add_head(emb).squeeze())
        add_logits = (
            torch.stack(add_logits_list)
            if add_logits_list
            else torch.empty(0, dtype=h.dtype, device=h.device)
        )
        # Graph-level pool for stop logit.
        graph_emb = h.mean(dim=0) if h.shape[0] > 0 else torch.zeros(
            self.hidden_dim, dtype=h.dtype, device=h.device
        )
        stop_logit = self.stop_head(graph_emb).squeeze()
        return add_logits, stop_logit
