# Diagram brief and evidence

The intended readers are engineers using or maintaining the library. The architecture diagram explains how compile-time graph declarations produce plans and how a runtime instance owns state and output slots. The event-plan diagram compares the actual action sequences for four kinds of request in the market example.

The scope is the implemented in-process engine and example. Networking, persistence, thread pools, and market-data platforms are outside the view. The architecture diagram shows dispatch and result writes; input reads are described in the callback box rather than drawn as return arrows. Event-plan arrows represent execution sequence, not data dependencies. Every node and edge has source evidence, and event actions come from the compiled example's printed plans.

The intended display width is approximately 900 pixels. Deliverables include SVG and PDF with live text, plus PNG previews, using DejaVu Sans at 12 points. There are no numeric axes, statistics, or measured results. The source snapshot is identified by the file SHA-256 hashes in `inputs/source-snapshot.json`.

Each drawing has a single short title above the graph. Source provenance remains in the accompanying JSON and manifest; there is no status banner or top legend on the canvas. Node and edge labels retain their existing meaning.
