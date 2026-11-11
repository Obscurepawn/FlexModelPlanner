# Composite feature dependency figure

Question: how do eight declared nodes share inputs and combine current-book values with trade history to produce Score and FairValue?

Audience: README readers following the executable C++ example. Intended display width: approximately 900 pixels. The visible header contains only `Composite Feature DAG`, as requested; provenance and review information remain in the bundle metadata. This is a node-level dependency view, not an event execution timeline, benchmark, or proposed design.

Evidence: the explicit `Graph` and `Outputs` declarations in `examples/composite_feature.cpp`, copied into `inputs/composite_feature.cpp` and identified by SHA-256 in `inputs/source-snapshot.json`. The extractor produces exactly eight nodes and thirteen declared input edges: three state reads into evaluators, eight value reads into evaluators, and two value reads into the Trade handler. Reset declares no inputs and therefore adds no edge.

Encoding: producer-to-consumer arrows with short `state`, `value`, or `value → Trade` labels. Open slate arrows denote state reads; filled dark arrows denote value reads into evaluators; dashed amber arrows denote value reads into `on<Trade>`. The latter do not describe inputs to TradePressure.eval. Green borders mark the two requested outputs. The output tuple order is Score, FairValue; node placement does not encode output order or execution order.

Deliverables: editable DOT and JSON, live-text SVG, PDF, PNG, source snapshot, extraction/rendering code, hashes, reproduction instructions, automated and manual QA. The renderer is a small layout adaptation of the repository's bundled architecture renderer, retaining its schema validation, file export and hash manifest.
