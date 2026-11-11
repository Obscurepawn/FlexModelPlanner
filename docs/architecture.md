# From a graph to an event's call sequence

The graph structure is a C++ type; state is data owned by an engine instance. `Id` separates node identity from the computation kernel, so the same `RecentDiffKernel` can serve both a 10-trade window and a 3-trade window. A state tuple supplies initial configuration. Plans contain only node indices and action kinds; they do not capture instance addresses.

## Input ports and actions

Each handler binds an event type to a set of inputs. Each evaluator also declares its own inputs. `state_of<A>` provides read-only access to A's state; `value_of<A>` provides A's evaluator output. Callbacks receive only their own state and their declared inputs. A handler may modify its own state. An evaluator receives read-only state and returns a value, which the engine writes to its output slot.

The planner orders actions in an action graph. If a consumer action C reads A, its prerequisites are:

| Input | Prerequisites in the current event plan |
|---|---|
| `state_of<A>` | A's handler for the current event, if present |
| `value_of<A>` | A's handler for the current event, if present, followed by A's eval |
| A as a query output | A's eval and its recursive input dependencies; no handlers run |

Handlers and evaluators can therefore interleave. Each action has a separate visitation mark to prevent duplicate execution within a plan. Revisiting an action that is still being explored identifies a cycle. A cycle in the union of edges from different events does not imply a cycle in any individual event plan. Validation checks the action graph for each declared event and the query graph for each evaluator.

In the market example, BookEvent updates only the book. TradeEvent evaluates the mid before updating the two independent windows. Reset clears both windows, and a query evaluates their averages. The diagrams are generated from these declarations; their sources and reproduction commands are documented in the [figure directory](figures/README.md).

## Lifecycle and execution

The root set determines the live closure by following every handler and evaluator input of each reachable node. This selection happens at compile time. At runtime, each on call executes all live subscribers for that event, including nodes that have never been queried but must retain history for future queries.

Static dispatch turns plan actions into calls whose types are known at compile time. Interpreted dispatch reads the same plan and selects the corresponding actions. It can check dispatch equivalence, but cannot independently detect a planning error shared by both paths. The tests supplement this comparison with hand-calculated expectations and a simple rolling-window oracle.

Value slots may remain stale after an event completes. `evaluate<Query>` computes values from the current state and returns borrowed references. Callers that need to retain results across evaluations copy them into their own tuple. Replay uses the same interfaces; the position of each query record determines which point in the event history it observes.

## Compile-time cost

Declarations such as `node::operations` use ordinary `using` type aliases. They let the planner traverse operation types, resolve input IDs, and convert type declarations into arrays. No objects of these descriptor types are constructed at runtime.

The current implementation represents action dependencies with a `2N × 2N` Boolean matrix. DFS for a single plan scans O(N²) entries in the worst case. When a root set is specified, the three nested loops that compute the live closure perform O(N³) checks in the worst case. This work happens at compile time. Large graphs remain subject to compiler limits on constant-evaluation steps and template depth, as well as generated code size. The current validation establishes no compile-time or runtime performance results for graphs with thousands of nodes.

## Validation boundaries

Metadata validation checks for duplicate IDs, duplicate handlers for an event, duplicate evaluators, missing dependencies, and value dependencies on nodes without evaluators. Plan validation checks for action cycles. Callback validation checks inputs, return types, and `noexcept`. Callbacks remain ordinary C++; their authors control any external side effects and dynamic allocation.

The library does not sort timestamps, persist checkpoints, ingest network data, or provide thread synchronization. CSV parsing happens before event dispatch and may allocate memory or report input errors. Hot-path allocation tests measure on/evaluate calls after startup has completed. The optional microbenchmark measures average iteration time for the example graph under the local build configuration.
