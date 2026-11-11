# L0 Planning — reasoning

## Concern and mode
Faithful transcription of scheduling subroutines, without injected faults. State consists of the exact loop indices, live mask, DFS colors, explicit recursion frames and emitted plan. Recursive calls are expanded into small transitions so gray-node reentry and black-node reuse are observable.

## Concrete behavior and abstraction

| Concrete behavior | Abstraction and reason |
|---|---|
| N in-place closure passes, row/column order | Boolean arrays are sets/functions; updates preserve source iteration order. |
| Per-event subscriptions, state/value ports, own-handler-before-eval | C++ types become bounded integer IDs; only validated input relations are admitted. No type diagnostic claim follows. |
| DFS recursion, dependency iteration, gray/black colors, result append | An explicit stack preserves call/return behavior; no concurrent execution is added. |
| Event seeds and declaration-ordered query seeds | Seed IDs use masks. Duplicate input ports collapse because the planner also stores one Boolean edge; this says nothing about callback parameter binding. |
| Compile-time throw on a gray node | Terminal `error` denotes compile-time rejection, not a runtime failure. |
| Complete and minimal plan | Independent finite reachability/cycle predicates are the oracle, rather than a second copy of DFS. |

Every state graph is bounded by the committed configs: two nodes, all handler/evaluator presence masks, all root subsets, one or two event kinds, and at most one or two declared ports in total. No symmetry or state truncation is used. The bounds retain self-cycles, two-action cycles, state versus value reads, handler/eval interleaving, shared prerequisites, empty roots, absent handlers, and structural cycles distributed across different event kinds. Larger/deeper graphs remain outside the claim.

## API boundary
This verifies `live_nodes`, `dependency_graph`, `plan_builder::visit`, `build_event` and the traversal portion of `query_plan`. It does **not** equate a successful selected-plan traversal with successful engine instantiation. Actual `validate_all` rejects cycles anywhere in every declared event/query graph before public dispatch, including components outside selected roots. The model deliberately also exercises subroutine calls on those bounded metadata inputs; eager rejection is covered by C++ negative tests.

For a query, the root mask simultaneously selects the requested evaluable output set and determines structural liveness. Any valid query subset of a larger live graph has the same action closure, since query planning reads the requested mask and dependencies; it does not execute extra live handlers. Queries outside liveness, duplicate outputs and missing evaluators remain C++ validation obligations.

## Concurrency and liveness
A builder invocation is single-threaded and completes synchronously. Weak fairness of `Next` rules out artificial infinite stuttering before the deterministic algorithm finishes; it does not impose a deployment scheduler. Terminal states stutter. `Complete`, `CycleSound` and `Terminates` jointly check that every bounded invocation either returns exactly the acyclic reachable closure or reports a reachable cycle.
