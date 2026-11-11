#!/usr/bin/env python3
"""Create diagram specifications from implementation evidence and real plan dumps."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--example", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "docs/figures/inputs")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    files = ["include/flexmodel/meta.hpp", "include/flexmodel/planner.hpp",
             "include/flexmodel/engine.hpp", "include/flexmodel/replay.hpp",
             "include/flexmodel/inspect.hpp", "examples/market_kernels.hpp",
             "examples/recent_trade_vs_mid.cpp"]
    hashes = {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in files}
    def evidence(path, symbol):
        return f"{path}, {symbol}, sha256={hashes[path]}"
    meta = evidence(files[0], "node, on, eval, inputs, schema")
    planner = evidence(files[1], "planner::live_nodes, validate_all, build_event, query_plan")
    engine = evidence(files[2], "engine::dispatch, compute, handle, eval, states_, values_, outputs")
    replay = evidence(files[3], "replay_range")
    source = "Implemented C++20 source snapshot, identified by the file SHA-256 values accompanying each evidence reference. No runtime latency is encoded."
    architecture = {
        "title": "FlexModelPlanner Architecture",
        "data_kind": "observed", "source": source, "rankdir": "TB", "font_size": 12,
        "font_family": "DejaVu Sans",
        "boundaries": [{"id": "compile", "label": "Compile-time metadata and validation"},
                       {"id": "runtime", "label": "Engine instance"}],
        "nodes": [
            {"id": "schema", "label": "Node IDs + kernels\nPer-operation input ports + roots", "boundary": "compile", "evidence": meta},
            {"id": "planner", "label": "Validate declarations and callbacks\nFind live closure; order actions", "boundary": "compile", "evidence": planner + "; " + engine},
            {"id": "plans", "label": "Per-event plans + grouped query plans\nActions = node index + handle/eval", "boundary": "compile", "evidence": planner},
            {"id": "records", "label": "Application events / query requests\nReplay preserves input order", "evidence": replay + "; " + engine},
            {"id": "dispatch", "label": "Direct template dispatch\nOptional interpreter uses the same plan", "boundary": "runtime", "evidence": engine},
            {"id": "kernels", "label": "Kernel callbacks\nOwn state + declared read-only inputs", "boundary": "runtime", "evidence": engine},
            {"id": "state", "label": "Owned state tuple\nStartup parameters and history", "boundary": "runtime", "evidence": engine},
            {"id": "values", "label": "Owned output slots\nUpdated on evaluation", "boundary": "runtime", "evidence": engine},
            {"id": "outputs", "label": "Grouped output references\nCaller may copy a snapshot", "evidence": engine}
        ],
        "edges": [
            {"from": "schema", "to": "planner", "label": "declarations", "kind": "data", "evidence": planner},
            {"from": "planner", "to": "plans", "label": "build plans", "kind": "data", "evidence": planner},
            {"from": "plans", "to": "dispatch", "label": "specialize calls", "kind": "control", "evidence": engine},
            {"from": "records", "to": "dispatch", "label": "on / evaluate", "kind": "sync", "evidence": replay + "; " + engine},
            {"from": "dispatch", "to": "kernels", "label": "ordered calls", "kind": "sync", "evidence": engine},
            {"from": "kernels", "to": "state", "label": "own state writes", "kind": "data", "evidence": engine},
            {"from": "kernels", "to": "values", "label": "eval result", "kind": "data", "evidence": engine},
            {"from": "values", "to": "outputs", "label": "query output order", "kind": "data", "evidence": engine}
        ]
    }
    output = subprocess.check_output([str(args.example.resolve())], text=True)
    groups = {}
    current = None
    for line in output.splitlines():
        match = re.fullmatch(r"(Book|Trade|Reset|Query) plan:", line)
        if match:
            current = match.group(1)
            groups[current] = []
        elif re.fullmatch(r"[0-9]+: .+", line):
            if current is None:
                raise ValueError("plan action without group")
            groups[current].append(line.split(": ", 1)[1])
    if set(groups) != {"Book", "Trade", "Reset", "Query"}:
        raise ValueError("example did not print all four plans")
    plan_evidence = evidence(files[5], "Graph") + "; " + evidence(files[6], "main: actual dump_plan output")
    nodes, edges, boundaries = [], [], []
    # dot packs disconnected LR clusters bottom-to-top; reverse the input so
    # the page reads Book, Trade, Reset, Query from top to bottom.
    for kind, actions in reversed(list(groups.items())):
        group = kind.lower()
        labels = {"Book": "Book", "Trade": "Trade", "Reset": "Reset", "Query": "Query"}
        boundaries.append({"id": group, "label": f"{labels[kind]}: actual ordered plan"})
        start = group + "_request"
        nodes.append({"id": start, "label": "evaluate<Outputs>()" if kind == "Query" else kind + "Event" if kind != "Reset" else "Reset",
                      "boundary": group, "evidence": plan_evidence})
        previous = start
        for i, action in enumerate(actions):
            node = f"{group}_{i}"
            nodes.append({"id": node, "label": f"{i + 1}. {action}", "boundary": group, "evidence": plan_evidence})
            edges.append({"from": previous, "to": node, "label": "next", "kind": "control", "evidence": plan_evidence})
            previous = node
    plans = {"title": "Event and Query Plans",
             "data_kind": "observed", "source": "Actual output of recent_trade_vs_mid. Arrows show execution sequence, not dependency edges. " + source,
             "rankdir": "LR", "font_size": 12, "font_family": "DejaVu Sans", "boundaries": boundaries,
             "nodes": nodes, "edges": edges}
    for name, data in [("architecture.json", architecture), ("event-plans.json", plans),
                       ("source-snapshot.json", {"status": "source snapshot identified by file SHA-256", "files": hashes, "plans": groups})]:
        (args.output / name).write_text(json.dumps(data, indent=2) + "\n")
    (args.output / "plans.txt").write_text(output)

if __name__ == "__main__":
    main()
