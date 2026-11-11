# L0 Planning — review notes

The two-event configuration includes graphs whose union of node dependencies is cyclic while each selected event action graph is acyclic. Root closure intentionally follows the union, but cycle detection follows the selected action graph. Retaining both structures in the model prevents an incorrect union-graph cycle restriction.

Independent C++ enumeration in `tests/test_planner_properties.cpp` tests the actual DFS helper on all 65,536 directed graphs with four action slots and all 16 seed masks. Its separate reachability/cycle oracle complements this typed-metadata model and provides a direct implementation check.

## Diagnostic portability sync — 2026-11-11

The cycle branch at `include/flexmodel/planner.hpp:31` now calls the named non-constexpr `cyclic_action_dependency()` helper (`include/flexmodel/planner.hpp:9-13`). This makes cycle diagnostics identifiable on compilers that omit thrown string literals. Constant evaluation rejects the helper call; a runtime call still throws the original `const char*` message.

Source comparison found no change to DFS guards, graph construction, liveness, action ordering or state updates. The model's `Cycle` transition still denotes the same terminal rejection. Only source mappings and the description of that rejection changed; the executable TLA+ definitions, invariants and all configuration bounds remain unchanged. No material model drift was found. Source-line anchors and whole-file fingerprints were refreshed after this review.
