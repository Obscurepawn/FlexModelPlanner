# L1 inputs: sequential execution, history and offline replay

The requested whole-project verification is split into layers. This level follows L0's planner contract by executing the concrete accepted plans for the four-node market example. It verifies execution and history semantics; it does not claim a universal refinement proof between C++ templates and TLA+.

Mode: faithful sequential transcription, without fault injection. The shipped example calls one engine from a sequential input loop; replay is also a sequential range traversal. No concurrent callers, reentrancy, crashes or message faults are in this API's supported regime.

Verified source inputs:

- `include/flexmodel/engine.hpp:130-151`: independent instance storage, read-only input binding, evaluator output assignment.
- `include/flexmodel/engine.hpp:160-179`: one action at a time in a statically expanded ordered plan.
- `include/flexmodel/engine.hpp:194-201`: requested-order borrowed output references and state-tuple construction.
- `include/flexmodel/engine.hpp:209-245`: generated and interpreted dispatch/query paths, optional copying of outputs.
- `include/flexmodel/replay.hpp:22-36`: input-order iteration and observer invocation after each record.
- `examples/market_kernels.hpp:19-29`: book updates and current-mid evaluation.
- `examples/market_kernels.hpp:33-57`: fixed-size ring initialization, warm-up, eviction, insertion, reset and evaluation.
- `examples/market_kernels.hpp:61-77`: graph declarations and independently configured window sizes.
- `examples/csv_replay.cpp:60-88`: sequential record processing, query positions, preceding-book guard and end-of-input behavior.

The model uses book updates represented by their integer midpoint (the concrete bid and ask are midpoint minus/plus one), integer trade prices, two configurable window sizes, and a bounded log of records. It records the live stream, then replays exactly that stream into a separate initially empty engine. Query selections include one output and both outputs requested in reverse order; scheduling remains in graph declaration order.

Developer expectations: every equal-price trade is preserved; each matching handler runs once; trades use the midpoint at that trade; warm-up and eviction match chronological samples; reset clears valid history while old ring storage may remain; queries do not mutate history; queried values match independently summed samples; replay returns the same query outputs and final state.

Caller obligations are represented: a book record precedes the first trade, operations complete before the next record, two engine instances do not share state, and output snapshots are copied when retained for replay comparison.
