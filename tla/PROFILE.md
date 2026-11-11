# FlexModelPlanner verification profile

## Mission
Check bounded scheduling and execution contracts of this C++20 feature engine against the implementation. A clean model run is not a proof of all C++ programs or arbitrary user kernels.

## Repository binding
`REPO_ROOT=/home/obscure/FlexModelPlanner`, local mode; workspace `tla/`. Source roots: `include/flexmodel/`, `examples/`, `tests/`; exclude generated `build-*`, downloaded tools, `.git`, and files outside this repository.

## Build and runtime model
Header-only C++20, GNU Make with a POSIX shell, Python 3 test driver, and GCC/Clang acceptance gates. Windows builds use MSYS2 UCRT64 with MinGW GCC. Each engine owns its state and cached values. Calls on one instance are synchronous, single-threaded and non-reentrant by contract (SPECS.md); there is no queue, worker pool, synchronization or callback reentry support. Independent engine instances may run on separate threads, with no shared kernel state assumed. The models do not manufacture concurrent calls to one engine.

## Subsystems
- `meta.hpp`: descriptors, unique IDs, operation/input validity. Compiler negative tests cover C++ type rules.
- `planner.hpp`: all-operation root closure; per-event state/value prerequisites; gray/black DFS; eager whole-graph validation.
- `engine.hpp`: direct and interpreted execution, restricted read-only inputs, independent state tuples, borrowed query values.
- `replay.hpp`: ordered range traversal and variant dispatch, observer after each event.
- `examples/market_kernels.hpp`: book state, mid value, rolling trade-minus-mid state.
- `inspect.hpp`, build/export files: formatting and installation, covered by C++/consumer checks rather than TLA+.

## Fault model
Faithful transcription with no injected failures. The engine promises neither crash recovery nor concurrent mutation nor transactional rollback of arbitrary callbacks. Kernel globals, memory safety, allocation and IEEE-754 edge cases remain external verification obligations.

## Bounds
L0 enumerates two-node metadata with one/two event kinds, zero-to-two declared ports, every handler/evaluator presence mask, root subset and query/event selection. These are exploration limits, not supported-library limits. L1 uses explicit finite event alphabets, configurable small windows and bounded input lengths documented in its configs.

## Toolchain
TLC 1.7.4, Java 21 locally; `tla/tools/tla2tools.jar`, SHA-256 `936a262061c914694dfd669a543be24573c45d5aa0ff20a8b96b23d01e050e88`. `make model-check` invokes the portable `bash tla/check.sh` entrypoint; machine-specific `config.env` is only for the host skill. Runs use one worker and action coverage. No symmetry reduction is applied.
