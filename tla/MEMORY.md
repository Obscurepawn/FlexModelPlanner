# Verification memory

## engine / contracts / L0_planning — PASS (2026-11-11)
- Mode: faithful transcription, no fault injection.
- Property: live reachability, unique/minimal/complete topological plans, reachable-cycle rejection, termination.
- Code anchors: `include/flexmodel/meta.hpp:12-34`; `include/flexmodel/planner.hpp:9-31`, `40-72`, `74-103`, `106-112`, `136-151`.
- TLC: primary 16,238 distinct / depth 29; two-inputs 73,894 / 29; two-events 839,836 / 29; every configured check PASS, all transition actions generated states.
- Finding: none.
- Scope: planning subroutines, two-node bounded typed metadata; eager whole-graph validation and C++ type rules checked independently.
- Reviewed: parent agent reviewed model/source correspondence and scope boundaries.
- Last synced: 2026-11-11; per-line anchors and whole-file source fingerprints recorded.
- Record: [L0 output](engine_models/contracts/L0_planning/05_output.md).

## engine / contracts / L1_execution — PASS (2026-11-11)
- Mode: faithful sequential transcription, no fault injection.
- Property: rolling history matches independent chronological oracle; trade-time mid; query/history separation; exact-once plan actions; ordered replay parity.
- Code anchors: `include/flexmodel/engine.hpp:130-151`, `160-179`, `194-196`, `209-240`; `include/flexmodel/replay.hpp:22-36`; `examples/market_kernels.hpp:19-57`, `61-77`; `examples/csv_replay.cpp:42-88`.
- TLC: primary 234,373 distinct / depth 49; scale 1,891,422 / 59; both PASS, all transition families generated states.
- Finding: none.
- Scope: fixed example graph, finite prices/records/windows, exact rational arithmetic; arbitrary callbacks, C++ memory model and IEEE-754 behavior excluded.
- Reviewed: parent agent independently reviewed current model/source correspondence.
- Last synced: 2026-11-11; per-line anchors and whole-file source fingerprints recorded.
- Record: [L1 output](engine_models/contracts/L1_execution/05_output.md).

Portable rerun: `bash tla/check.sh`. Whole-project matrix: [verification](../docs/verification.md). No staged work or open candidate remains.
