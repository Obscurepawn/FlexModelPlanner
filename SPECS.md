# FlexModelPlanner v1 behavior contract

This project implements a C++20 static event-driven feature engine inspired by Jane Street's Flex Model. It is an independent implementation. The graph topology is a compile-time type; parameters and state are instance data initialized at startup.

## Public API

Namespace `flexmodel`, umbrella header `<flexmodel/flexmodel.hpp>`.

- `inputs<Ports...>`, `state_of<Id>`, `value_of<Id>` describe read-only inputs.
- `on<Event, Inputs = inputs<>, auto Function = nullptr>` registers one handler per event per node. The default callback is static `Kernel::handle(State&, const Event&, const Input&...)`; an explicit function pointer overrides it.
- `eval<Inputs = inputs<>, auto Function = nullptr>` registers one evaluator per node. The default callback is static `Kernel::eval(const State&, const Input&...)`. It returns `Kernel::value_type` by value; the engine owns the output slot.
- `node<Id, Kernel, Operations...>` and `graph<Nodes...>` describe topology. A kernel supplies `state_type`, and `value_type` when it has an evaluator. IDs are unique within a graph; kernels can be reused.
- `query<Ids...>` names an ordered output group. `roots<Ids...>` optionally selects live features; `all_nodes` retains all nodes.
- `engine<Graph, Roots = all_nodes>` stores independent state and value slots. Default construction value-initializes state. An explicit `state_tuple` constructor initializes configured states; states are movable and default construction is required only when used.
- `on(const Event&)` runs the static event plan. Unknown event types have an empty plan.
- `evaluate<Query>()` runs a shared dependency closure once and returns `std::tuple<const Value&...>`. References are borrowed from the engine and can change on subsequent evaluation; they must not outlive/move across the owning engine. `evaluate_into<Query>(tuple&)` copies outputs into caller storage.
- `state<Id>() const` exposes read-only state. `cached_value<Id>() const` exposes an explicitly potentially stale value; use evaluate for freshness.
- `on_interpreted` / `evaluate_interpreted` execute the same plan through a diagnostic interpreter; they are comparison tools, not an independent planner oracle.

## Scheduling and validation

- Metadata validation rejects duplicate IDs, duplicate handlers for an event, duplicate evaluators, unknown input IDs and value inputs without an evaluator, including unused declarations.
- Each declared event's action graph and each evaluable node's query graph are validated at engine instantiation; cycles in the union across different events are allowed when individual action graphs are acyclic.
- A state input requires the producer's handler for the current event, if present. A value input additionally requires its eval. The consumer runs afterwards. Evaluation of a node follows its own handler when applicable.
- Handler and eval actions may interleave. Each action runs at most once per plan. No coalescing, cutoffs, cross-event memoization or runtime graph changes.
- Root closure follows all handler and evaluator inputs. Matching handlers of live stateful nodes run on every event even if no output is queried. Queries outside this live closure are rejected.
- Callbacks receive only their own state, the event and declared read-only inputs. Callback signatures, return types and `noexcept` are checked. C++ cannot enforce absence of hidden global effects or allocations inside callbacks.
- Query output IDs are evaluable and unique. Output order follows query order. Plan roots and prerequisites are visited in graph declaration order by DFS; this is deterministic, not a global minimum-ready-node ordering. Repeated root IDs in `roots` are idempotent.
- Evaluator `value_type` must be default-constructible and nothrow assignable from its returned value. `evaluate_into` uses the caller's assignment semantics and can allocate or throw for dynamic output types.

## Runtime, replay and observability

- Plan construction allocates no runtime graph. The generated dispatcher uses direct template calls. State may allocate at startup. Hot-path allocation freedom also requires allocation-free kernel operations and output handling; dedicated scalar/fixed-buffer tests enforce the shipped examples' behavior.
- Each engine instance is single-threaded and non-reentrant. Independent instances may run in parallel. The framework provides no implicit synchronization.
- Replay preserves input order and reuses the same dispatch/evaluation methods. Generic typed/variant range replay is provided; a CSV example includes quote, trade, reset and query records. No implicit timestamp sorting, checkpoint serialization, network ingestion or JIT is promised.
- Plans can be inspected as arrays and printed with node names and action kinds outside the hot path.

## Acceptance

Tests cover repeated equal-price events, warm-up/eviction, trade-time mid, event-specific reset dependencies, producer-handle-before-consumer-read, same-kernel/different-ID isolation, multiple-output deduplication, query/history separation, root liveness, unknown events, reference/static equivalence, independent arithmetic oracle and replay parity. Negative compilation cases cover invalid metadata, cycles and invalid callbacks. GCC and Clang builds, installation-consumer smoke tests, and ASan/UBSan runs complete the local acceptance gate.
