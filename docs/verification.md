# Verification scope and results

Local acceptance covers GCC, Clang, ASan/UBSan, independent graph algorithm checks, and five TLC model-checking configurations. All recorded checks passed. The TLA+ results apply to the finite state spaces listed below; C++ type rules, generated code, and user kernels have separate tests and contracts.

## Reproducing the checks

Run from the repository root with GNU Make, a C++20 GCC/Clang toolchain, a POSIX shell, and Python 3:

```sh
make -j4 test CXX=g++ BUILD_DIR=build-make-gcc
make -j4 test CXX=clang++ BUILD_DIR=build-make-clang
make sanitize SANITIZE_CXX=clang++
make model-check
```

The Make test target also installs the headers into a temporary prefix and compiles an independent consumer against that installation. Keep separate `BUILD_DIR` directories for different compilers or flags. Sanitizer output defaults to `build-make-sanitize`.

The CI configuration retains Linux GCC/Clang, macOS Clang, Linux sanitizers, and model checks. Its Windows job uses the [official MSYS2 setup action](https://github.com/msys2/setup-msys2) with UCRT64, MinGW GCC, GNU Make, and UCRT Python, following the [MSYS2 CI instructions](https://www.msys2.org/docs/ci/). Windows commands run inside the UCRT64 shell. The table below records local checks; hosted results are available in [GitHub Actions](https://github.com/Obscurepawn/FlexModelPlanner/actions/workflows/ci.yml). Native MSVC invocation is outside the provided Make interface.

`make model-check` invokes `tla/check.sh` and requires Java (17+ recommended), Bash, curl, and either `sha256sum` or `shasum`. If TLC is missing locally, the script downloads the pinned 1.7.4 release and verifies its SHA-256 checksum. `TLA_TOOLS_JAR` can point to a local copy of that release. The script checks fingerprints of the modeled source files, runs every configuration in [models.tsv](../tla/models.tsv), and checks action coverage. An unregistered configuration, changed source file, failed check, or model action that never fires causes the script to fail. Logs are written to `build-tla/`.

When a source fingerprint changes, review the correspondence between model and implementation before updating fingerprints and line anchors. Rerunning an old model alone does not establish that it still represents the new code. [PROFILE](../tla/PROFILE.md) records concurrency and fault assumptions; [MEMORY](../tla/MEMORY.md) tracks model status.

## Project coverage matrix

| Area | Method and local result | Scope |
|---|---|---|
| Node IDs, subscriptions, inputs, queries, callback signatures | 19 expected compile-failure cases check specific diagnostics; all pass with GCC 13.3 and Clang 18.1.3 | Covers the committed valid and invalid cases, rather than proving the C++ compiler correct |
| Event/query execution, dependency deduplication, state isolation, roots, replay | 453 functional assertions, 0 failures; direct execution, interpretation, and an independent arithmetic reference are cross-checked | Both executors share a plan, so their agreement alone does not establish planner correctness |
| Actual DFS implementation | `test_planner_properties` enumerates all directed graphs on 4 actions (65,536 graphs) × 16 root sets: 1,048,576 cases, with 98,033 acyclic reachable closures and 950,543 cases containing a reachable cycle; 0 failures | Uses independent reachability and cycle detection as the reference; covers the action helper, not template instantiations of arbitrary size |
| Planning with typed dependencies | Three L0 TLC configurations check root closure, event-specific dependencies, DFS, completeness, minimality, ordering, deduplication, and termination | 2 nodes, up to 2 event kinds, and up to 2 declared inputs in total; verifies planning subroutines |
| Event history, windows, queries, offline replay | Two L1 TLC configurations execute the example's callback steps and compare them with an independent chronological sample list | Fixed example graph, small integer inputs, windows 1/2 or 2/3, and at most 5/6 records |
| Composite feature in the README | GCC/Clang compile and run the complete example; score changes from 0.0625 to -0.25 after reset, fair value stays 101, and actual action plans match the documented sequences | This eight-node EMA example is separate from the rolling-window example modeled in L1 |
| Dynamic allocation | A dedicated test counts calls to the C++ `new` family; the runtime count is 0 | Covers the framework and tested paths; arbitrary `malloc` calls and custom allocators are not counted, and user callbacks may allocate |
| Memory safety and undefined behavior | `make sanitize` with Clang ASan + UBSan passes 27/27 checks, with no reports | Covers paths exercised by the tests |
| Examples, CSV, installation, public headers | GCC 13.3 and Clang 18.1.3 each pass 27/27 `make test` checks, including examples, valid/invalid CSV inputs, and an independent consumer using only headers installed by `make install`; all six public headers also passed standalone compilation | See hosted CI for macOS and Windows UCRT64/MinGW results |
| Plan printing and build packaging | Examples print actual plans; an independent consumer builds and runs against the installation prefix | These formatting and integration behaviors are checked through C++ builds, not modeled in TLA+ |

## TLC results

Every configuration uses TLC 1.7.4 with one worker, explores its complete configured state space, and enables action coverage. Every named action in each model has positive generated coverage. A terminal self-loop produces no new distinct state but still counts as an exercised action.

| Configuration | Bounds | Generated | Distinct | Maximum depth | Result |
|---|---|---:|---:|---:|---|
| L0 primary | 2 nodes, 1 event, ≤1 input | 17,288 | 16,238 | 29 | PASS |
| L0 two_inputs | 2 nodes, 1 event, ≤2 inputs | 78,710 | 73,894 | 29 | PASS |
| L0 two_events | 2 nodes, 2 events, ≤2 inputs | 895,016 | 839,836 | 29 | PASS |
| L1 primary | Windows 1/2, prices `{0,1}`, ≤5 records | 251,059 | 234,373 | 49 | PASS |
| L1 scale | Windows 2/3, prices `{0,1}`, ≤6 records | 2,008,707 | 1,891,422 | 59 | PASS |

The [L0 model](../tla/engine_models/contracts/L0_planning/02_models/L0_Planning.tla) transcribes the source loop order and DFS call stack into individual transitions, then checks the result with independent set-reachability and cycle predicates. It verifies planning subroutines for selected roots. The actual engine additionally validates the entire declared graph eagerly: engine instantiation rejects a cycle even outside the selected roots, as covered by compile-failure tests. The two-event configuration also includes valid cases where the union of node dependencies is cyclic but each event's action graph is acyclic.

The [L1 model](../tla/engine_models/contracts/L1_execution/02_models/L1_Execution.tla) executes book, mid, trade-window, reset, and eval callbacks individually within fixed example plans. It checks repeated equal-price trades, window warm-up and eviction, the mid at trade time, queries preserving history, exactly-once execution of each plan action, and matching state and query outputs after an independent instance replays the original input order. The window reference uses a chronological sample list instead of reusing the ring-buffer algorithm under test. Query records are routed by the example layer; typed and variant dispatch in the generic `replay()` helper has separate C++ tests.

Both levels follow the single-threaded, non-reentrant contract for each engine instance. They do not introduce concurrent calls on one instance, crash recovery, or network faults. L1 uses exact integer/rational arithmetic, excluding IEEE-754 rounding, NaNs, infinities, overflow, and hidden global side effects in arbitrary user kernels. Compilation cost for large graphs, code size, and latency under real market workloads require separate performance measurements.

Per-configuration commands, elapsed times, and action counts are recorded in the [L0 run log](../tla/engine_models/contracts/L0_planning/03_run_log.md) and [L1 run log](../tla/engine_models/contracts/L1_execution/03_run_log.md). Each level's `00_input.md`, `01_reasoning.md`, `05_output.md`, and `anchors.lock` retain the source evidence, abstraction rationale, conclusions, and baseline for detecting source drift.
