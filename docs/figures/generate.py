#!/usr/bin/env python3
"""Generate SVG, PNG, and PDF architecture and dependency figures for FlexModelPlanner.

Requires Python 3.10+ and `rsvg-convert` (from librsvg) when exporting PNG/PDF.
Verifies that `examples/composite_feature.cpp` declares the expected 8-node,
13-input graph so documentation diagrams stay synchronized with the code.
"""
from __future__ import annotations

import argparse
from html import escape
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = Path(__file__).resolve().parent

FONT_SANS = "'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', 'DejaVu Sans', sans-serif"
FONT_MONO = "'JetBrains Mono', 'SFMono-Regular', Consolas, 'DejaVu Sans Mono', monospace"


def verify_composite_source(source_path: Path) -> None:
    """Check that examples/composite_feature.cpp matches the documented 8-node DAG."""
    code = source_path.read_text(encoding="utf-8")
    graph_block = code.split("using Graph =", 1)[1].split("using Outputs =", 1)[0]
    node_ids = re.findall(r"fm::node<(\w+),", graph_block)
    expected_nodes = [
        "BookId", "MidId", "SpreadId", "ImbalanceId",
        "FairValueId", "BookBiasId", "TradePressureId", "ScoreId",
    ]
    if node_ids != expected_nodes:
        raise ValueError(f"Unexpected node list in {source_path}: {node_ids}")
    ports = re.findall(r"fm::(state_of|value_of)<(\w+)>", graph_block)
    if len(ports) != 13:
        raise ValueError(f"Expected 13 input ports in {source_path}, found {len(ports)}")


def svg_header(width: int, height: int, title: str, desc: str) -> list[str]:
    return [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" '
        f'width="{width}" height="{height}" role="img" aria-label="{escape(title)}">',
        f"  <title>{escape(title)}</title>",
        f"  <desc>{escape(desc)}</desc>",
        "  <defs>",
        '    <marker id="arrow-slate" viewBox="0 0 10 10" refX="9" refY="5" '
        'markerWidth="7" markerHeight="7" orient="auto-start-reverse">',
        '      <path d="M 1 1.5 L 9 5 L 1 8.5 z" fill="#FFFFFF" stroke="#475569" stroke-width="1.4"/>',
        "    </marker>",
        '    <marker id="arrow-navy" viewBox="0 0 10 10" refX="8.8" refY="5" '
        'markerWidth="6.5" markerHeight="6.5" orient="auto-start-reverse">',
        '      <path d="M 0.5 1.5 L 9 5 L 0.5 8.5 z" fill="#1E3A5F"/>',
        "    </marker>",
        '    <marker id="arrow-amber" viewBox="0 0 10 10" refX="8.8" refY="5" '
        'markerWidth="6.5" markerHeight="6.5" orient="auto-start-reverse">',
        '      <path d="M 0.5 1.5 L 9 5 L 0.5 8.5 z" fill="#B45309"/>',
        "    </marker>",
        '    <marker id="arrow-indigo" viewBox="0 0 10 10" refX="8.8" refY="5" '
        'markerWidth="6.5" markerHeight="6.5" orient="auto-start-reverse">',
        '      <path d="M 0.5 1.5 L 9 5 L 0.5 8.5 z" fill="#4338CA"/>',
        "    </marker>",
        '    <marker id="arrow-teal" viewBox="0 0 10 10" refX="8.8" refY="5" '
        'markerWidth="6.5" markerHeight="6.5" orient="auto-start-reverse">',
        '      <path d="M 0.5 1.5 L 9 5 L 0.5 8.5 z" fill="#0F766E"/>',
        "    </marker>",
        '    <marker id="arrow-muted" viewBox="0 0 10 10" refX="8.8" refY="5" '
        'markerWidth="5.5" markerHeight="5.5" orient="auto-start-reverse">',
        '      <path d="M 0.5 1.5 L 9 5 L 0.5 8.5 z" fill="#64748B"/>',
        "    </marker>",
        "  </defs>",
        f'  <rect x="1" y="1" width="{width - 2}" height="{height - 2}" rx="10" '
        'fill="#FFFFFF" stroke="#E2E8F0" stroke-width="1.5"/>',
    ]


def bezier(x1: float, y1: float, x2: float, y2: float, *, color: str, marker: str,
           dashed: bool = False, width: float = 1.65, tension: float = 0.45) -> str:
    dx = (x2 - x1) * tension
    dash = ' stroke-dasharray="6,4"' if dashed else ""
    return (
        f'  <path d="M {x1:.1f} {y1:.1f} C {x1 + dx:.1f} {y1:.1f}, '
        f'{x2 - dx:.1f} {y2:.1f}, {x2:.1f} {y2:.1f}" fill="none" '
        f'stroke="{color}" stroke-width="{width}"{dash} marker-end="url(#{marker})"/>'
    )


def build_composite_dag() -> str:
    w, h = 980, 456
    lines = svg_header(
        w, h,
        "Composite Feature Dependency Graph",
        "Eight-node feature graph showing state_of reads, evaluator value_of reads, "
        "and Trade handler value_of reads in examples/composite_feature.cpp.",
    )

    # Title + Subtitle
    lines.append(
        f'  <text x="28" y="35" font-family="{FONT_SANS}" font-size="15" '
        'font-weight="700" fill="#0F172A">Composite Feature Dependency Graph</text>'
    )
    lines.append(
        f'  <text x="28" y="54" font-family="{FONT_SANS}" font-size="11.5" '
        'fill="#64748B">Declared input ports in examples/composite_feature.cpp '
        '(producer &#8594; consumer)</text>'
    )

    # Top-right legend bar (plenty of clearance from subtitle)
    lx, ly = 510, 16
    lines.append(
        f'  <rect x="{lx}" y="{ly}" width="442" height="48" rx="7" '
        'fill="#F8FAFC" stroke="#E2E8F0" stroke-width="1"/>'
    )
    # Legend item 1: state_of
    lines.append(
        f'  <line x1="{lx + 14}" y1="{ly + 16}" x2="{lx + 44}" y2="{ly + 16}" '
        'stroke="#475569" stroke-width="1.6" marker-end="url(#arrow-slate)"/>'
    )
    lines.append(
        f'  <text x="{lx + 52}" y="{ly + 20}" font-family="{FONT_MONO}" '
        'font-size="10.5" fill="#334155">state_of&lt;Id&gt;</text>'
    )
    # Legend item 2: value_of (eval)
    lines.append(
        f'  <line x1="{lx + 178}" y1="{ly + 16}" x2="{lx + 208}" y2="{ly + 16}" '
        'stroke="#1E3A5F" stroke-width="1.7" marker-end="url(#arrow-navy)"/>'
    )
    lines.append(
        f'  <text x="{lx + 216}" y="{ly + 20}" font-family="{FONT_MONO}" '
        'font-size="10.5" fill="#334155">value_of&lt;Id&gt; (eval)</text>'
    )
    # Legend item 3: value_of (on<Trade>)
    lines.append(
        f'  <line x1="{lx + 14}" y1="{ly + 35}" x2="{lx + 44}" y2="{ly + 35}" '
        'stroke="#B45309" stroke-width="1.7" stroke-dasharray="5,3.5" marker-end="url(#arrow-amber)"/>'
    )
    lines.append(
        f'  <text x="{lx + 52}" y="{ly + 39}" font-family="{FONT_MONO}" '
        'font-size="10.5" fill="#92400E">value_of&lt;Id&gt; (on&lt;Trade&gt;)</text>'
    )
    # Legend item 4: queried output
    lines.append(
        f'  <rect x="{lx + 252}" y="{ly + 27}" width="14" height="12" rx="3" '
        'fill="#F0FDFA" stroke="#0F766E" stroke-width="1.8"/>'
    )
    lines.append(
        f'  <text x="{lx + 272}" y="{ly + 37}" font-family="{FONT_SANS}" '
        'font-size="10.5" fill="#0F766E" font-weight="600">Queried in Outputs</text>'
    )

    # Node coordinates: (x, y, w, h, title, code_line, role_badge, theme)
    nodes = {
        "Book": (28, 206, 152, 78, "Book", "on<BookUpdate>", "state source", "blue"),
        "Imbalance": (236, 94, 134, 60, "Imbalance", "eval(book)", None, "slate"),
        "Mid": (236, 215, 134, 60, "Mid", "eval(book)", None, "slate"),
        "Spread": (236, 334, 134, 60, "Spread", "eval(book)", None, "slate"),
        "FairValue": (428, 98, 154, 78, "FairValue", "eval(mid, spr, imb)", "query #2", "teal"),
        "BookBias": (632, 174, 162, 62, "BookBias", "eval(fair, mid, spr)", None, "slate"),
        "TradePressure": (632, 310, 162, 78, "TradePressure", "on<Trade> · on<Reset>", "EMA state + eval", "amber"),
        "Score": (832, 232, 120, 78, "Score", "eval(bias, ema)", "query #1", "teal"),
    }

    themes = {
        "blue": ("#EFF6FF", "#3B82F6", "#1E3A8A", "#DBEAFE", "#1D4ED8"),
        "slate": ("#F8FAFC", "#94A3B8", "#0F172A", "#E2E8F0", "#475569"),
        "teal": ("#F0FDFA", "#0F766E", "#134E4A", "#CCFBF1", "#0F766E"),
        "amber": ("#FFFBEB", "#D97706", "#78350F", "#FEF3C7", "#B45309"),
    }

    def right(name: str, dy: float = 0.0) -> tuple[float, float]:
        x, y, bw, bh, *_ = nodes[name]
        return x + bw, y + bh / 2.0 + dy

    def left(name: str, dy: float = 0.0) -> tuple[float, float]:
        x, y, bw, bh, *_ = nodes[name]
        return x, y + bh / 2.0 + dy

    # Edges (drawn beneath nodes)
    # 1. Book state_of -> Imbalance, Mid, Spread
    lines.append(bezier(*right("Book", -16), *left("Imbalance", 4), color="#475569", marker="arrow-slate"))
    lines.append(bezier(*right("Book", 0), *left("Mid", 0), color="#475569", marker="arrow-slate"))
    lines.append(bezier(*right("Book", 16), *left("Spread", -4), color="#475569", marker="arrow-slate"))

    # 2. Imbalance, Mid, Spread value_of -> FairValue
    lines.append(bezier(*right("Imbalance", 0), *left("FairValue", -16), color="#1E3A5F", marker="arrow-navy"))
    lines.append(bezier(*right("Mid", -12), *left("FairValue", 2), color="#1E3A5F", marker="arrow-navy"))
    lines.append(bezier(*right("Spread", -14), *left("FairValue", 20), color="#1E3A5F", marker="arrow-navy", tension=0.42))

    # 3. FairValue, Mid, Spread value_of -> BookBias
    lines.append(bezier(*right("FairValue", 8), *left("BookBias", -14), color="#1E3A5F", marker="arrow-navy"))
    lines.append(bezier(*right("Mid", 0), *left("BookBias", 2), color="#1E3A5F", marker="arrow-navy"))
    lines.append(bezier(*right("Spread", -4), *left("BookBias", 16), color="#1E3A5F", marker="arrow-navy", tension=0.5))

    # 4. Mid, Spread value_of -> TradePressure (on<Trade> handler inputs)
    lines.append(bezier(*right("Mid", 14), *left("TradePressure", -12), color="#B45309", marker="arrow-amber", dashed=True, tension=0.48))
    lines.append(bezier(*right("Spread", 10), *left("TradePressure", 12), color="#B45309", marker="arrow-amber", dashed=True))

    # 5. BookBias, TradePressure value_of -> Score
    lines.append(bezier(*right("BookBias", 4), *left("Score", -14), color="#1E3A5F", marker="arrow-navy"))
    lines.append(bezier(*right("TradePressure", -6), *left("Score", 14), color="#1E3A5F", marker="arrow-navy"))

    # Draw node cards
    for _, (x, y, bw, bh, title, code_line, badge, theme) in nodes.items():
        fill, stroke, title_col, badge_bg, badge_fg = themes[theme]
        sw = "2.0" if theme == "teal" else "1.4"
        lines.append(
            f'  <rect x="{x}" y="{y}" width="{bw}" height="{bh}" rx="8" '
            f'fill="{fill}" stroke="{stroke}" stroke-width="{sw}"/>'
        )
        if badge:
            ty = y + 22
            cy = y + 40
            pill_y = y + 51
            lines.append(
                f'  <text x="{x + bw / 2:.1f}" y="{ty}" text-anchor="middle" '
                f'font-family="{FONT_SANS}" font-size="13" font-weight="700" fill="{title_col}">{escape(title)}</text>'
            )
            lines.append(
                f'  <text x="{x + bw / 2:.1f}" y="{cy}" text-anchor="middle" '
                f'font-family="{FONT_MONO}" font-size="10" fill="#475569">{escape(code_line)}</text>'
            )
            pill_w = max(68, len(badge) * 6.5 + 16)
            px = x + (bw - pill_w) / 2
            lines.append(
                f'  <rect x="{px:.1f}" y="{pill_y}" width="{pill_w:.1f}" height="17" rx="8.5" '
                f'fill="{badge_bg}"/>'
            )
            lines.append(
                f'  <text x="{x + bw / 2:.1f}" y="{pill_y + 12}" text-anchor="middle" '
                f'font-family="{FONT_SANS}" font-size="9.5" font-weight="600" fill="{badge_fg}">{escape(badge)}</text>'
            )
        else:
            lines.append(
                f'  <text x="{x + bw / 2:.1f}" y="{y + 26}" text-anchor="middle" '
                f'font-family="{FONT_SANS}" font-size="13" font-weight="700" fill="{title_col}">{escape(title)}</text>'
            )
            lines.append(
                f'  <text x="{x + bw / 2:.1f}" y="{y + 45}" text-anchor="middle" '
                f'font-family="{FONT_MONO}" font-size="10" fill="#475569">{escape(code_line)}</text>'
            )

    # Bottom subtle note
    lines.append(
        f'  <text x="28" y="436" font-family="{FONT_SANS}" font-size="10.5" fill="#64748B">'
        'Note: TradePressure::eval() reads only its own EMA state; Mid and Spread are evaluated '
        'for TradePressure solely during on(Trade).</text>'
    )
    lines.append("</svg>\n")
    return "\n".join(lines)


def build_architecture() -> str:
    w, h = 980, 434
    lines = svg_header(
        w, h,
        "FlexModelPlanner Architecture",
        "Separation between compile-time type-level metadata/planning and runtime engine instance state.",
    )

    # Header
    lines.append(
        f'  <text x="28" y="35" font-family="{FONT_SANS}" font-size="15" '
        'font-weight="700" fill="#0F172A">Compile-Time Planning vs. Runtime Execution</text>'
    )
    lines.append(
        f'  <text x="28" y="54" font-family="{FONT_SANS}" font-size="11.5" '
        'fill="#64748B">Topology and plans are pure C++20 types and constexpr arrays; '
        'each engine&lt;Graph, Roots&gt; instance owns isolated state and value slots.</text>'
    )

    # Left container: Compile-Time (consteval) -> x=26..400 (w=374)
    lines.append(
        '  <rect x="26" y="74" width="374" height="334" rx="10" '
        'fill="#F8FAFC" stroke="#CBD5E1" stroke-width="1.4" stroke-dasharray="6,4"/>'
    )
    lines.append(
        '  <rect x="42" y="86" width="194" height="22" rx="5" fill="#EEF2FF"/>'
    )
    lines.append(
        f'  <text x="52" y="101" font-family="{FONT_SANS}" font-size="11" '
        'font-weight="700" fill="#3730A3">COMPILE TIME (consteval)</text>'
    )

    # Right container: Runtime Engine Instance -> x=494..954 (w=460)
    lines.append(
        '  <rect x="494" y="74" width="460" height="334" rx="10" '
        'fill="#F8FAFC" stroke="#94A3B8" stroke-width="1.4"/>'
    )
    lines.append(
        '  <rect x="510" y="86" width="308" height="22" rx="5" fill="#ECFEFF"/>'
    )
    lines.append(
        f'  <text x="520" y="101" font-family="{FONT_SANS}" font-size="11" '
        'font-weight="700" fill="#0F766E">RUNTIME INSTANCE (engine&lt;Graph, Roots&gt;)</text>'
    )

    def card(x: int, y: int, cw: int, ch: int, title: str, l1: str, l2: str,
             border: str = "#94A3B8", bg: str = "#FFFFFF", title_color: str = "#0F172A") -> None:
        lines.append(
            f'  <rect x="{x}" y="{y}" width="{cw}" height="{ch}" rx="7" '
            f'fill="{bg}" stroke="{border}" stroke-width="1.4"/>'
        )
        lines.append(
            f'  <text x="{x + 14}" y="{y + 22}" font-family="{FONT_SANS}" '
            f'font-size="12.5" font-weight="700" fill="{title_color}">{escape(title)}</text>'
        )
        lines.append(
            f'  <text x="{x + 14}" y="{y + 41}" font-family="{FONT_MONO}" '
            f'font-size="10" fill="#334155">{escape(l1)}</text>'
        )
        lines.append(
            f'  <text x="{x + 14}" y="{y + 58}" font-family="{FONT_SANS}" '
            f'font-size="10.2" fill="#64748B">{escape(l2)}</text>'
        )

    # Compile-time cards (x=42, width=342)
    card(42, 120, 342, 70,
         "1. Declarative Graph Schema",
         "graph<node<Id, Kernel, on<...>, eval<...>>>",
         "Type-level IDs, ports (state_of / value_of), and roots<...>",
         border="#818CF8", bg="#FFFFFF", title_color="#312E81")

    card(42, 216, 342, 70,
         "2. Static Validator & Action Planner",
         "validate_metadata() · validate_callbacks() · DFS",
         "Checks signatures, noexcept, and cycles; finds live closure",
         border="#818CF8", bg="#FFFFFF", title_color="#312E81")

    card(42, 312, 342, 76,
         "3. Immutable Execution Plans",
         "constexpr plan<N> { actions[2N], size }",
         "Ordered {node_index, handle | eval}; no instance pointers",
         border="#6366F1", bg="#EEF2FF", title_color="#312E81")

    # Compile-time vertical arrows
    lines.append(
        '  <line x1="213" y1="190" x2="213" y2="214" stroke="#4338CA" '
        'stroke-width="1.6" marker-end="url(#arrow-indigo)"/>'
    )
    lines.append(
        '  <line x1="213" y1="286" x2="213" y2="310" stroke="#4338CA" '
        'stroke-width="1.6" marker-end="url(#arrow-indigo)"/>'
    )

    # Runtime cards (x=512, width=424)
    card(512, 120, 424, 68,
         "Unrolled Static Dispatcher",
         "on(const Event&) · evaluate<Query>() · replay(engine, range)",
         "Fold expression expands plan.actions[I] into direct static calls",
         border="#0F766E", bg="#FFFFFF", title_color="#134E4A")

    card(512, 216, 424, 68,
         "Isolated Kernel Callbacks (noexcept)",
         "Kernel::handle(state&, event, in...)  |  Kernel::eval(state, in...)",
         "Receives only own state and declared read-only input references",
         border="#64748B", bg="#FFFFFF", title_color="#0F172A")

    # Storage cards side-by-side at bottom of runtime (204 + 16 + 204 = 424)
    card(512, 314, 204, 74,
         "Owned states_ Tuple",
         "std::tuple<State...>",
         "Mutated only by own handle()",
         border="#3B82F6", bg="#EFF6FF", title_color="#1E3A8A")

    card(732, 314, 204, 74,
         "Owned values_ Slots",
         "std::tuple<Value...>",
         "Updated by eval(); lent on query",
         border="#0F766E", bg="#F0FDFA", title_color="#134E4A")

    # Bridge arrow across clean 94px center gutter (x=384..512)
    lines.append(
        '  <path d="M 384 350 C 452 350, 442 156, 510 156" fill="none" '
        'stroke="#4338CA" stroke-width="1.8" stroke-dasharray="6,4" marker-end="url(#arrow-indigo)"/>'
    )
    lines.append(
        '  <rect x="405" y="234" width="84" height="34" rx="6" '
        'fill="#EEF2FF" stroke="#C7D2FE" stroke-width="1.1"/>'
    )
    lines.append(
        f'  <text x="447" y="248" text-anchor="middle" font-family="{FONT_MONO}" '
        'font-size="9.5" font-weight="600" fill="#3730A3">plan.actions</text>'
    )
    lines.append(
        f'  <text x="447" y="261" text-anchor="middle" font-family="{FONT_SANS}" '
        'font-size="9.2" fill="#4338CA">inlined at CT</text>'
    )

    # Runtime internal arrows
    lines.append(
        '  <line x1="724" y1="188" x2="724" y2="214" stroke="#0F766E" '
        'stroke-width="1.7" marker-end="url(#arrow-teal)"/>'
    )
    lines.append(
        f'  <text x="734" y="205" font-family="{FONT_SANS}" font-size="10" fill="#0F766E">'
        'direct static call sequence</text>'
    )

    # Callbacks <-> states_ and values_
    lines.append(
        '  <line x1="614" y1="284" x2="614" y2="312" stroke="#1E3A5F" '
        'stroke-width="1.6" marker-end="url(#arrow-navy)"/>'
    )
    lines.append(
        f'  <text x="604" y="302" text-anchor="end" font-family="{FONT_MONO}" '
        'font-size="9.5" fill="#1E3A8A">handle / state_of</text>'
    )

    lines.append(
        '  <line x1="834" y1="284" x2="834" y2="312" stroke="#0F766E" '
        'stroke-width="1.6" marker-end="url(#arrow-teal)"/>'
    )
    lines.append(
        f'  <text x="843" y="302" font-family="{FONT_MONO}" '
        'font-size="9.5" fill="#0F766E">eval / value_of</text>'
    )

    lines.append("</svg>\n")
    return "\n".join(lines)


def build_execution_plans() -> str:
    w, h = 1040, 420
    lines = svg_header(
        w, h,
        "Per-Request Static Execution Plans",
        "Four static action sequences compiled from the eight-node composite feature graph.",
    )

    # Header
    lines.append(
        f'  <text x="28" y="35" font-family="{FONT_SANS}" font-size="15" '
        'font-weight="700" fill="#0F172A">One Graph, Four Specialized Execution Plans</text>'
    )
    lines.append(
        f'  <text x="28" y="54" font-family="{FONT_SANS}" font-size="11.5" '
        'fill="#64748B">Each event type or query compiles to its minimal topologically ordered action sequence.</text>'
    )

    # Legend pills at top-right (ample width so nothing clips)
    lx, ly = 640, 18
    lines.append(
        f'  <rect x="{lx}" y="{ly}" width="372" height="38" rx="7" '
        'fill="#F8FAFC" stroke="#E2E8F0" stroke-width="1"/>'
    )
    lines.append(
        f'  <rect x="{lx + 14}" y="{ly + 11}" width="16" height="16" rx="4" '
        'fill="#FFFBEB" stroke="#D97706" stroke-width="1.4"/>'
    )
    lines.append(
        f'  <text x="{lx + 36}" y="{ly + 23}" font-family="{FONT_MONO}" '
        'font-size="10.5" fill="#92400E">.handle (mutates state)</text>'
    )
    lines.append(
        f'  <rect x="{lx + 202}" y="{ly + 11}" width="16" height="16" rx="4" '
        'fill="#EFF6FF" stroke="#3B82F6" stroke-width="1.4"/>'
    )
    lines.append(
        f'  <text x="{lx + 224}" y="{ly + 23}" font-family="{FONT_MONO}" '
        'font-size="10.5" fill="#1E3A8A">.eval (updates value)</text>'
    )

    lanes = [
        (
            "on(BookUpdate)",
            "1 action · pure state write",
            [("Book.handle", "handle")],
            "7 downstream nodes skipped; evaluators run lazily when needed",
            76,
        ),
        (
            "on(Trade)",
            "3 actions · eval + handle",
            [("Mid.eval", "eval"), ("Spread.eval", "eval"), ("TradePressure.handle", "handle")],
            "Computes only Mid & Spread for TradePressure; skips Imbalance, FairValue, BookBias, Score",
            158,
        ),
        (
            "on(Reset)",
            "1 action · input-free handler",
            [("TradePressure.handle", "handle")],
            "on<Reset> declares inputs<>, so even Mid and Spread are skipped",
            240,
        ),
        (
            "evaluate<Outputs>()",
            "7 actions · shared closure",
            [
                ("Mid.eval", "eval"),
                ("Spread.eval", "eval"),
                ("Imbalance.eval", "eval"),
                ("FairValue.eval", "eval"),
                ("BookBias.eval", "eval"),
                ("TradePressure.eval", "eval"),
                ("Score.eval", "eval"),
            ],
            "Zero handlers run (history preserved); shared Mid, Spread & FairValue each run once -> (score, fair)",
            322,
        ),
    ]

    for req, meta, steps, note, y in lanes:
        lines.append(
            f'  <rect x="26" y="{y}" width="988" height="72" rx="8" '
            'fill="#F8FAFC" stroke="#E2E8F0" stroke-width="1.2"/>'
        )
        lines.append(
            f'  <text x="40" y="{y + 28}" font-family="{FONT_MONO}" font-size="12" '
            f'font-weight="700" fill="#0F172A">{escape(req)}</text>'
        )
        lines.append(
            f'  <text x="40" y="{y + 46}" font-family="{FONT_SANS}" font-size="10.2" '
            f'fill="#64748B">{escape(meta)}</text>'
        )
        lines.append(
            f'  <line x1="204" y1="{y + 10}" x2="204" y2="{y + 62}" '
            'stroke="#E2E8F0" stroke-width="1.2"/>'
        )

        cx = 218
        compact = len(steps) > 4
        for idx, (label, kind) in enumerate(steps):
            full_text = f"{idx}: {label}"
            if compact:
                cw = int(len(full_text) * 5.45 + 14)
                ch = 28
                cy = y + 11
                font_sz = "9.2"
            else:
                cw = max(112, int(len(full_text) * 6.5 + 22))
                ch = 30
                cy = y + 10
                font_sz = "10.5"

            if kind == "handle":
                bg, border, fg = "#FFFBEB", "#D97706", "#92400E"
            else:
                bg, border, fg = "#EFF6FF", "#3B82F6", "#1E3A8A"

            lines.append(
                f'  <rect x="{cx}" y="{cy}" width="{cw}" height="{ch}" rx="6" '
                f'fill="{bg}" stroke="{border}" stroke-width="1.35"/>'
            )
            lines.append(
                f'  <text x="{cx + cw / 2:.1f}" y="{cy + 18.5}" text-anchor="middle" '
                f'font-family="{FONT_MONO}" font-size="{font_sz}" font-weight="600" '
                f'fill="{fg}">{escape(full_text)}</text>'
            )
            cx += cw
            if idx + 1 < len(steps):
                gap = 12 if compact else 22
                lines.append(
                    f'  <line x1="{cx + 2}" y1="{cy + ch / 2:.1f}" '
                    f'x2="{cx + gap - 2}" y2="{cy + ch / 2:.1f}" '
                    'stroke="#64748B" stroke-width="1.35" marker-end="url(#arrow-muted)"/>'
                )
                cx += gap

        lines.append(
            f'  <text x="218" y="{y + 58}" font-family="{FONT_SANS}" font-size="10.2" '
            f'fill="#475569">{escape(note)}</text>'
        )

    lines.append("</svg>\n")
    return "\n".join(lines)


def export_formats(svg_path: Path, rsvg: str | None) -> None:
    if not rsvg:
        return
    png_path = svg_path.with_suffix(".png")
    pdf_path = svg_path.with_suffix(".pdf")
    subprocess.run([rsvg, "-f", "png", "-z", "2", str(svg_path), "-o", str(png_path)], check=True)
    subprocess.run([rsvg, "-f", "pdf", str(svg_path), "-o", str(pdf_path)], check=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=OUT_DIR)
    args = parser.parse_args()

    verify_composite_source(ROOT / "examples/composite_feature.cpp")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    rsvg = shutil.which("rsvg-convert")

    figures = {
        "composite-feature.svg": build_composite_dag(),
        "architecture.svg": build_architecture(),
        "execution-plans.svg": build_execution_plans(),
    }
    for filename, content in figures.items():
        target = args.output_dir / filename
        target.write_text(content, encoding="utf-8")
        export_formats(target, rsvg)
        print(f"Generated {target.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
