# L1 result: PASS within the declared bounds

The sequential four-node example satisfied all eight invariants across both finite configurations. TLC explored 234,373 distinct states for windows 1/2 with up to five records, and 1,891,422 distinct states for windows 2/3 with up to six records. Both searches completed, with every instrumented transition family exercised.

The checked contracts are:

- Valid ring cells, count, insertion position and running sum match an independent chronological history at each completed operation, including repeated equal-price trades, warm-up, eviction and reset.
- Trade handlers read the midpoint derived from the current book. The oracle uses the book independently, so stale midpoint data would yield a semantic mismatch.
- Matching history handlers execute once per event; reset does not evaluate midpoint; shared planned actions are not duplicated.
- Query evaluation leaves all historical state unchanged and returns mathematically correct averages in requested output order.
- A separate engine replaying the full retained record sequence yields the same query-output prefix and ultimately the same final state and value slots.

This is evidence for the source-derived example plans and supported small runtime window settings. It is not a universal proof of every graph, arbitrary user kernels, all stream lengths, C++ code generation or IEEE-754 arithmetic. Outputs are modeled as exact rational pairs, and callbacks execute atomically under the documented single-thread/non-reentrant usage contract. Allocation and generated/interpreted C++ equivalence are verified by executable tests separately.

## Fidelity & coverage
- Fidelity: 11 transition families modeled, all cited file:line; 0 UNCLEAR/TODO; abstractions listed in 01_reasoning.md. Three families are parameterized over two node instances, giving 14 Next disjuncts. The complete variable/action map is at the top of the model.
- Code coverage: modeled 11 of 11 selected execution/caller transition families in `include/flexmodel/engine.hpp`, `include/flexmodel/replay.hpp`, `examples/market_kernels.hpp` and the successful-record caller path. This is design-surface coverage, not binary line coverage; CSV parsing, compiler binding, allocation and floating-point edge cases are excluded.
- Model coverage: TLC --coverage — all actions fired in both configurations. Parameterized trade/reset/eval actions are aggregated by TLC; exact-once checks cover their required node instances. Coverage counts and raw logs are recorded in 03_run_log.md.

## Export

For the two checked configurations and supplied plans, synchronous event execution preserves history independently of query cadence, and ordered replay reproduces query observations and final instance state. L0 supplies bounded planner-contract evidence; the composition is an explicit interface assumption, not a mechanically established unbounded refinement relation.
