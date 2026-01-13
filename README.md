# Inverted Index (SPIMI)

Small C++ project that builds an inverted index over the files in `corpus/` using a SPIMI-like pipeline, writes partial indexes under `indexes/`, merges them, and saves the final index to `final_index/`.

## Prerequisites
- C++17 compiler (e.g., MinGW g++ on Windows)
- Standard library only; no external dependencies

## Build
```bash
g++ -std=c++17 -O2 source.cpp -o inverted-index
```

## Run
```bash
./inverted-index
```
- Prompts for a query; enter terms to search the indexed corpus.
- Outputs are written to `indexes/` (partial indexes) and `final_index/` (merged index + documents map).

## Project Layout
- `source.cpp` — entry point, orchestrates SPIMI build + search.
- `headers/spimi.h` — SPIMI implementation (indexing, merging, search).
- `corpus/` — input text files to index.
- `indexes/` — intermediate partial indexes.
- `final_index/` — merged index (`final_index.json`) and document map (`documents_map.csv`).

## Notes
- Adjust `threshold` or data directory in `source.cpp` if your corpus size changes.
- Keep `source.exe` and other build artifacts out of version control (see `.gitignore`).
