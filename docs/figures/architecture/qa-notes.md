# Architecture diagram: QA notes

Automated audit: **pass**, with no failures or warnings. Manifest hashes, expected formats, live SVG text, PDF text bounds, and font sizes passed the strict checks. The PDF has one page, a minimum font size of 12 points, and no detected text-box collisions. The original audit result is preserved in `qa-auto.json`.

Visual review covers the final PNG and a rasterization of the actual PDF. Labels, arrowheads, boundary membership, and reading order are legible. No clipping, missing glyphs, visible text collisions, or arrows crossing labels were found. The diagrams use DejaVu Sans and English explanatory labels while retaining C++ identifiers.

Semantic review checked the specification's evidence references against the current implementation. Source file hashes match `../inputs/source-snapshot.json`, and the bundled input JSON matches the current generated specification. The architecture shows declarations, planning, dispatch, and writes. Read-only state/value inputs are described in the callback box; reverse read arrows are omitted for clarity. Boundaries represent logical stages and instance ownership, not process deployment.

The canvas now has only its title above the graph. The former status, provenance, and legend block is removed; provenance remains in the JSON and manifest. Node and edge semantics are unchanged.

Renderer and environment details are recorded in `manifest.json`. SVG and DOT remain editable; PDF and PNG are exported views. The figures contain no performance measurements or statistical conclusions. The audit checks artifact structure and supported geometry; source fidelity and visual appearance were reviewed separately.
