# Architecture & Planner Internals

FlexModelPlanner separates **compile-time graph structure** from **per-instance runtime state**. A `graph<Nodes...>` type encodes topology, port bindings, and operation descriptors without allocating runtime metadata. Each `engine<Graph, Roots>` instance owns an isolated `state_tuple` and `value_tuple`. Node identity (`Id`) is decoupled from kernel implementation (`Kernel`), allowing a single kernel such as `RecentDiffKernel` in [examples/market_kernels.hpp](../examples/market_kernels.hpp) to back both a 10-trade and a 3-trade rolling window with distinct startup parameters.

---

## Input Ports and Action Dependencies

Every handler binds an `Event` type to a list of read-only input ports (`inputs<Ports...>`), and every evaluator declares its own read-only input ports:

- `state_of<A>` passes `const StateA&` from `A`'s state slot.
- `value_of<A>` passes `const ValueA&` from `A`'s output slot (requiring `A` to declare `eval<...>`).

Handlers receive mutable access only to their own state (`State&`); evaluators receive `const State&` and return `value_type` by value, which the engine assigns into the node's output slot.

For a graph with `N` nodes, the planner constructs a `2N`-vertex action graph (`handle(0 .. N-1)` and `eval(0 .. N-1)`) specialized for each request kind:

| Consumer port / request | Prerequisites in the compiled plan |
|---|---|
| `state_of<A>` in an event plan | `handle(A)` for the current event, if `A` subscribes to that event |
| `value_of<A>` in an event plan | `handle(A)` for the current event (if present), followed by `eval(A)` |
| `eval(X)` in an event plan | `handle(X)` for the current event (if present), plus `X`'s declared evaluator inputs |
| `A` in `query<...>` | `eval(A)` and its transitive evaluator inputs; no handlers execute |

Because an event handler can read `value_of<A>`, `.eval` and `.handle` actions naturally interleave within an event plan while executing at most once per plan. Cycles are checked per individual event action graph and on the evaluator query graph; edges from distinct events may form a cycle in their union as long as each single-event plan is a DAG.

---

## Live Closure and Dispatch Paths

When `roots<Ids...>` is provided, `planner::live_nodes()` computes the compile-time transitive closure across **all** handler and evaluator inputs of reachable nodes. Every live node subscribing to `Event` runs its handler on every `on(Event)` call, preserving path-dependent state history even when no query follows.

`engine` provides two dispatch paths over the exact same `constexpr plan<N>` value:

1. **Static dispatch (`on`, `evaluate`)**: Expands `plan.actions[0 .. plan.size - 1]` via a `std::index_sequence` fold expression into a straight-line sequence of typed static calls.
2. **Interpreted dispatch (`on_interpreted`, `evaluate_interpreted`)**: Loops over `plan` at runtime and invokes per-node function pointers from a `static constexpr` dispatch table.

Because both paths consume the same `plan<N>`, agreement between them validates code generation rather than the planner itself; the test suite pairs differential dispatch checks with exhaustive small-graph verification (`tests/test_planner_properties.cpp`) and independent reference calculations.

---

## Compile-Time Complexity

- **Action dependency matrix**: `action_graph<N>` represents prerequisites as a dense `2N × 2N` Boolean adjacency matrix (`std::array<std::array<bool, 2 * N>, 2 * N>`). Post-order DFS for a single plan inspects `O(N²)` entries in the worst case.
- **Root reachability**: Computing `live_nodes()` for `roots<...>` performs a fixed-point pass in `O(N³)` constant-evaluation operations.

All traversal runs inside `consteval` functions using ordinary value-level arrays and loops rather than deep recursive class-template instantiations. Very large graphs remain bounded by the host compiler's `constexpr` step limit (`-fconstexpr-ops-limit` / `-fconstexpr-steps`) and generated code size.

---

## Scope Boundaries

FlexModelPlanner focuses strictly on in-process compile-time planning, state isolation, allocation-free dispatch, and ordered range replay. Concerns outside this layer—network transport, timestamp alignment across feeds, input validation/parsing (such as `examples/csv_replay.cpp`), checkpoint serialization, and cross-thread synchronization—belong to the surrounding application.
