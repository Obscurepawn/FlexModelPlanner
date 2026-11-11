# Architecture and event-plan diagrams

The architecture diagram shows where graph declarations, plans, and instance state belong. The event-plan diagram shows the actions executed by the same market graph for book updates, trades, resets, and queries. Boundaries distinguish compile-time metadata from a runtime engine instance; they are not process boundaries. Arrows in the plan diagram show execution order. In particular, an arrow between the two window handlers does not imply a data dependency.

The diagrams describe the implemented code. `inputs/source-snapshot.json` records SHA-256 hashes of the relevant source files, and `inputs/plans.txt` contains the four plans printed by the example. `make_inputs.py` derives the event-plan diagram from that output. Architecture nodes and edges carry evidence references to code symbols and file hashes.

- [Architecture SVG](architecture/architecture.svg) · [PDF](architecture/architecture.pdf) · [PNG](architecture/architecture.png) · [DOT](architecture/architecture.dot)
- [Event plans SVG](event-plans/architecture.svg) · [PDF](event-plans/architecture.pdf) · [PNG](event-plans/architecture.png) · [DOT](event-plans/architecture.dot)

Each diagram directory includes its JSON specification, a copy of the renderer, an environment and file-hash manifest, reproduction commands, automated checks, and visual-review notes. Labels use DejaVu Sans and preserve C++ identifiers. The diagrams are intended for display at roughly 900 pixels wide; open the SVG directly to inspect it at larger sizes.

Regenerate the inputs, then render them into new, empty directories using the bundled renderer:

```sh
cmake --build build-gcc --target recent_trade_vs_mid
python3 docs/figures/make_inputs.py --example build-gcc/recent_trade_vs_mid
python3 docs/figures/architecture/code/render_architecture.py --input docs/figures/inputs/architecture.json --output build-figures/architecture
python3 docs/figures/event-plans/code/render_architecture.py --input docs/figures/inputs/event-plans.json --output build-figures/event-plans
```

Rendering requires Python 3.10+, Graphviz `dot`, and the DejaVu Sans font. These diagrams contain no measurements and do not establish performance or concurrency correctness.
