#!/usr/bin/env bash
# heesch-forge literature fetcher.
#
# Downloads open-access PDFs of the references listed in ../../LITERATURE.md
# into this directory. PDFs themselves are gitignored (see .gitignore in
# this directory) -- the canonical metadata is bibtex.bib + LITERATURE.md;
# each contributor populates their own copy with this script.
#
# Sources covered: arXiv preprints (free), and the open journal CDM. Items
# that are paywalled (Mann 2004 in AMM, Mann & Thomas 2016 in Exp. Math.,
# Bašić's Mathematical Intelligencer paper) are flagged at the bottom and
# need institutional access -- file them by hand and they will be tracked
# by their bibtex key.
#
# Requires: curl. Idempotent: skips files that already exist locally.

set -euo pipefail
cd "$(dirname "$0")"

dl() {
    local key="$1" url="$2" outname="${key}.pdf"
    if [[ -f "$outname" ]]; then
        printf '  skip %-32s already at %s\n' "$key" "$outname"
        return 0
    fi
    printf '  fetch %-32s %s\n' "$key" "$url"
    curl --fail --silent --show-error --location --max-time 90 \
         --user-agent "heesch-forge-litfetch/0.1 (https://github.com/Darian-Frey/heesch-forge)" \
         "$url" -o "$outname.part"
    mv "$outname.part" "$outname"
}

echo "# heesch-forge literature fetcher"
echo "# target: $(pwd)"
echo

# -- A. Heesch problem (open access only) --------------------------------
dl KaplanA8           https://arxiv.org/pdf/2105.09438.pdf
dl KaplanA9           https://arxiv.org/pdf/2406.16407.pdf

# -- B. Aperiodic monotile -----------------------------------------------
dl SmithMyersKaplanGoodmanStraussB1 https://arxiv.org/pdf/2303.10798.pdf
# B2 (chiral monotile): arXiv ID still to be confirmed; see [VERIFY] in
# bibtex.bib. Add a line here once it's pinned.

# -- C. Combinatorial-search techniques ----------------------------------
dl KnuthC1            https://arxiv.org/pdf/cs/0011047.pdf
# C2--C6 are tools, not papers; tool docs are linked in bibtex.bib.
# Pin a specific cite for each tool when M1.x decides which version
# heesch-forge depends on.

# -- D. Machine learning -------------------------------------------------
dl WagnerD1           https://arxiv.org/pdf/2104.14516.pdf
# D2 Truter: arXiv ID still to be confirmed.
# D3 Liu et al. TilinGNN: SIGGRAPH paper, often available from authors;
#    fetch by hand if needed.
dl SelsamBjornerD5    https://arxiv.org/pdf/1903.04671.pdf

# -- E. Theoretical / topological ----------------------------------------
dl AkhmedovE1         https://arxiv.org/pdf/1412.0358.pdf
# E2 hyperbolic monotile: ID flagged [VERIFY] in bibtex.bib.
dl GoodmanStraussE3   https://strauss.hosted.uark.edu/papers/survey.pdf

# -- F. Software and datasets --------------------------------------------
# F.1-F.4 are tooling/datasets; the dataset itself is already mirrored at
# data/kaplan-2022/. No PDFs to fetch here.

echo
echo "# done. Paywalled / unverified items still need manual filing:"
echo "#   - MannA3 (AMM 2004)               -- institutional access"
echo "#   - MannThomasA4 (Exp. Math. 2016)  -- institutional access"
echo "#   - BasicA7 (Math. Intelligencer)   -- institutional access"
echo "#   - BasicSlivkovaA6 (DCG 2020)      -- check open-access status"
echo "#   - SmithMyersKaplanGoodmanStraussB2 -- pin arXiv ID first"
echo "#   - TruterD2                        -- pin arXiv ID first"
echo "#   - HyperbolicMonotileE2            -- pin arXiv ID first (current ID is suspicious; submission date 2026-03)"
