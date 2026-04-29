# Literature directory

Canonical metadata for everything cited in `../../LITERATURE.md`.

## What's here

- `bibtex.bib` — BibTeX entries for every reference. Citation keys mirror
  the LITERATURE.md section IDs (`A1`, `A2`, …, `F4`) plus a short
  surname slug, e.g. `KaplanA8`. Use `\cite{KaplanA8}` in any paper
  written under `../`.
- `fetch_pdfs.sh` — best-effort fetcher for the open-access PDFs (arXiv
  preprints and the CDM open journal). Idempotent; skips files that
  already exist locally.
- `.gitignore` — ignores `*.pdf` in this directory. PDFs are **not**
  committed back to the repo: redistribution rights vary per publisher,
  the bibtex + fetch script make them reproducibly retrievable, and a
  research repo of binary literature would balloon quickly.

## How to populate locally

```bash
bash paper/lit/fetch_pdfs.sh
```

This downloads the open-access subset. Paywalled papers (Mann 2004 AMM,
Mann & Thomas 2016 Exp. Math., Bašić's *Mathematical Intelligencer*
piece) need institutional access — once obtained, save the PDF in this
directory under the bibtex key, e.g. `MannA3.pdf`, and it will be
recognised by any future tooling that maps key → file by name.

## Verification status

Some entries in `bibtex.bib` and `LITERATURE.md` carry a `[VERIFY]` tag.
Per CLAUDE.md ("never cite what hasn't been read"), no `[VERIFY]` entry
should appear in a heesch-forge writeup until a maintainer has read the
paper and confirmed the citation. The current pass adds the BibTeX
records as a structural skeleton; promoting them to `[EST]` is a
manual reading-pass task.

## Why bibtex keys mirror section IDs

Two indexes — Section IDs in LITERATURE.md (human-friendly,
topic-grouped) and BibTeX keys in `bibtex.bib` (LaTeX-friendly) — would
diverge over time if they didn't share a stem. Picking `<Surname><ID>`
as the BibTeX key keeps both pointing at the same underlying entry, and
makes diffing trivial when LITERATURE.md is reorganised.
