#!/usr/bin/env python3
"""Extract the composite example's node/port metadata for its dependency figure.

This is a deliberately narrow reader for the explicit fm::graph declaration in
examples/composite_feature.cpp, not a general C++ parser.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re


def arguments(expression: str) -> list[str]:
    begin = expression.index("<") + 1
    depth, current, result = 0, [], []
    for char in expression[begin:]:
        if char == ">" and depth == 0:
            if "".join(current).strip():
                result.append("".join(current).strip())
            return result
        if char == "," and depth == 0:
            result.append("".join(current).strip())
            current = []
        else:
            current.append(char)
            if char == "<":
                depth += 1
            elif char == ">":
                depth -= 1
    raise ValueError("Unbalanced template declaration")


def generate(repo: Path, destination: Path, source_file: Path | None = None) -> None:
    source = source_file or repo / "examples/composite_feature.cpp"
    code = source.read_text()
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    declaration = code.split("using Graph =", 1)[1].split("using Outputs =", 1)[0]
    outputs = arguments(code.split("using Outputs =", 1)[1])
    declarations = arguments(declaration)
    names = {}
    parsed = []
    for declaration in declarations:
        parts = arguments(declaration)
        node_id, kernel, *operations = parts
        if not node_id.endswith("Id"):
            raise ValueError(f"Unexpected node ID: {node_id}")
        names[node_id] = kernel
        parsed.append((node_id, kernel, operations))
    nodes, edges = [], []
    for node_id, kernel, operations in parsed:
        events = [arguments(op)[0] for op in operations if op.startswith("fm::on<")]
        has_eval = any(op.startswith("fm::eval<") for op in operations)
        if events:
            detail = " + ".join(f"on<{event}>" for event in events)
            label = kernel + "\n" + detail + ("\neval = EMA" if has_eval else "\nbook state")
        else:
            label = kernel + "\neval" + (" · queried" if node_id in outputs else "")
        node_line = next(i for i, line in enumerate(code.splitlines(), 1)
                         if f"fm::node<{node_id}," in line)
        evidence = f"examples/composite_feature.cpp:{node_line}; node {node_id}; sha256={digest}"
        nodes.append({"id": kernel, "label": label, "evidence": evidence})
        for operation in operations:
            handler = operation.startswith("fm::on<")
            event = arguments(operation)[0] if handler else None
            consumer = f"on<{event}>" if handler else "eval"
            for port, producer_id in re.findall(r"fm::(state_of|value_of)<(\w+)>", operation):
                edges.append({
                    "from": names[producer_id], "to": kernel,
                    "label": f"{port} → {consumer}", "kind": "data",
                    "evidence": evidence + f"; {consumer} declares {port}<{producer_id}>"
                })
    if len(nodes) != 8 or len(edges) != 13:
        raise ValueError(f"Expected 8 nodes and 13 inputs; found {len(nodes)} and {len(edges)}")
    if sum(edge["label"] == "state_of → eval" for edge in edges) != 3:
        raise ValueError("Expected exactly three book-state reads")
    handler_edges = [edge for edge in edges if "on<" in edge["label"]]
    if {(e["from"], e["to"], e["label"]) for e in handler_edges} != {
        ("Mid", "TradePressure", "value_of → on<Trade>"),
        ("Spread", "TradePressure", "value_of → on<Trade>")
    }:
        raise ValueError("TradePressure handler dependencies changed; review the figure encoding")
    spec = {
        "title": "Composite Feature DAG",
        "data_kind": "observed",
        "source": f"Explicit Graph declaration in examples/composite_feature.cpp; sha256={digest}. Node-level declared input dependencies, not an execution sequence.",
        "rankdir": "LR", "font_family": "DejaVu Sans", "font_size": 14,
        "nodes": nodes, "edges": edges
    }
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(spec, indent=2, ensure_ascii=False) + "\n")
    print(f"Extracted {len(nodes)} nodes and {len(edges)} typed input edges: {destination}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--source", type=Path, help="Optional C++ snapshot to read instead of the live repository file")
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parent / "inputs/composite-feature.json")
    options = parser.parse_args()
    generate(options.repo, options.output, options.source)
