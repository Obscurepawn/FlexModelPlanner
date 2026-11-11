# Contract ladder

Frozen charter: verify ordering and event-history semantics of the implementation under its synchronous, single-owner instance contract. Both levels use faithful transcription without fault injection. Metadata validation, compiler type rules and the generated machine code require separate C++ evidence.

1. **L0_planning** models bounded typed inputs, in-place root closure, per-event/query action prerequisites and the actual DFS control flow. Check minimal complete ordered plans, no duplicates, cycle soundness and termination. This is verification of planner subroutines; eager validation of every declared graph is an additional C++ gate.
2. **L1_execution** adds stateful kernels and ordered replay. It instantiates legal schedules for the example graph and checks history against an independent chronological oracle. It uses L0's scheduling contract as an interface, not as an unbounded refinement proof.

No same-instance concurrent invocation, runtime topology mutation, external I/O, crash recovery, arbitrary hidden kernel effects or floating-point rounding is modeled. Additional levels require new explicit scope and source anchors.
