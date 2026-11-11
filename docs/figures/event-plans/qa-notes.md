# Event-plan diagram: QA notes

Automated audit: **pass**, with no failures or warnings. Manifest hashes, expected formats, live SVG text, PDF text bounds, and font sizes passed the strict checks. The PDF has one page, a minimum font size of 12 points, and no detected text-box collisions. The original audit result is preserved in `qa-auto.json`.

Visual review covers the final PNG and a rasterization of the actual PDF. Labels, arrowheads, boundary membership, and reading order are legible. No clipping, missing glyphs, visible text collisions, or arrows crossing labels were found. The diagrams use DejaVu Sans and English explanatory labels while retaining C++ identifiers.

Semantic review checked the specification's evidence references against the current implementation. Source file hashes match `../inputs/source-snapshot.json`, and the bundled input JSON matches the current generated specification. Action sequences come directly from the example output: Book runs book.handle; Trade runs mid.eval, recent10.handle, then recent3.handle; Reset runs only the two window handlers; Query runs only the two window evaluators. Arrows show execution order, not a data dependency between the two independent windows.

Renderer and environment details are recorded in `manifest.json`. SVG and DOT remain editable; PDF and PNG are exported views. The figures contain no performance measurements or statistical conclusions. The audit checks artifact structure and supported geometry; source fidelity and visual appearance were reviewed separately.
