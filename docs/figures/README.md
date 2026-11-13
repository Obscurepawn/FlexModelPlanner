# Documentation Figures

This directory contains the vector diagrams embedded in [README.md](../../README.md) together with their deterministic generator script:

- [composite-feature.svg](composite-feature.svg) ([PNG](composite-feature.png) · [PDF](composite-feature.pdf)) — Eight-node feature DAG and typed input ports (`state_of<Id>`, evaluator `value_of<Id>`, and `on<Trade>` `value_of<Id>`) declared in [examples/composite_feature.cpp](../../examples/composite_feature.cpp).
- [architecture.svg](architecture.svg) ([PNG](architecture.png) · [PDF](architecture.pdf)) — Separation between `consteval` graph validation/planning and per-instance runtime storage and dispatch.
- [execution-plans.svg](execution-plans.svg) ([PNG](execution-plans.png) · [PDF](execution-plans.pdf)) — The four specialized action sequences (`BookUpdate`, `Trade`, `Reset`, and `evaluate<Outputs>()`) compiled from `composite_feature.cpp`.

## Regenerating

`generate.py` verifies that `examples/composite_feature.cpp` still matches the expected 8-node, 13-port topology before writing SVG files and exporting PNG/PDF siblings via `rsvg-convert` (if installed):

```sh
python3 docs/figures/generate.py
```
