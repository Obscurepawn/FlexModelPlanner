# AGENTS.md — Working on FlexModelPlanner

## Core Invariants

- **Language & packaging**: C++20, header-only under `include/flexmodel/`, namespace `flexmodel`, with zero third-party runtime dependencies.
- **Compile-time topology vs. instance state**: Graph metadata and `plan<N>` generation are pure compile-time (`consteval`) constructs; mutable state and output slots belong exclusively to `engine` instances. Plans must never capture instance addresses.
- **Event-history completeness**: Every matching `on<Event, ...>` handler in the live closure executes once per event, independent of queries or payload equality. Never introduce implicit value cutoffs, event coalescing, or cross-event/cross-query value caching.
- **Zero hot-path allocation**: `on()` and `evaluate()` dispatch must not allocate dynamically. Verify allocation freedom with `tests/test_noalloc.cpp`, not stream/logging tests.

## Repository Layout

- `include/flexmodel/` — Public headers (`flexmodel.hpp`, `meta.hpp`, `planner.hpp`, `engine.hpp`, `inspect.hpp`, `replay.hpp`).
- `SPECS.md` — Normative API, scheduling, validation, and lifetime specification.
- `examples/` — Executable examples (`composite_feature.cpp`, `recent_trade_vs_mid.cpp`, `csv_replay.cpp`, `market_kernels.hpp`).
- `tests/` — Functional, allocation (`test_noalloc.cpp`), exhaustive planner (`test_planner_properties.cpp`), installed-consumer (`install_consumer.cpp`), and negative compilation (`tests/compile_fail/*.cpp`) suites orchestrated by `scripts/check.py`.
- `tla/` — Bounded TLA+/TLC formal specifications (`check.sh`, `models.tsv`, `source.sha256`).
- `docs/figures/` — Diagram generator (`generate.py`) and rendered `.svg`/`.png`/`.pdf` assets.
- `build-*` — Ephemeral build directories (`build-make` by default). Never hand-edit build outputs or generated artifacts.

## Toolchain & Portability Constraints

- The build interface is GNU Make with a POSIX shell; running `make test` requires Python 3.
- Windows CI runs inside the MSYS2 UCRT64 shell with MinGW GCC. Keep compiler flags compatible with GCC/Clang-style drivers; do not add a separate MSVC build branch.
- Always use distinct `BUILD_DIR` paths when switching compilers or compiler flags.

## Verification & Change Rules

Run both GCC and Clang test suites after modifying public headers, examples, or build scripts:

```sh
make -j test CXX=g++ BUILD_DIR=build-gcc
make -j test CXX=clang++ BUILD_DIR=build-clang
```

Additional required gates by change scope:

- **State access, dispatch, or replay changes**: Run AddressSanitizer + UndefinedBehaviorSanitizer:
  ```sh
  make -j sanitize SANITIZE_CXX=clang++
  ```
- **Modeled source changes (`tla/source.sha256`)**: Run `make model-check` (TLC). If a modeled file's hash or line anchors change, review source-to-model correspondence before updating `tla/source.sha256` or `anchors.lock`.
- **Public contract or example changes**: Update `SPECS.md` alongside API/semantic changes, and keep code examples in `README.md` compilable and synchronized with `examples/`. Regenerate figures via `python3 docs/figures/generate.py` if `examples/composite_feature.cpp` topology changes.
- **Negative compilation tests**: Every file in `tests/compile_fail/` must compile against `-Iinclude` and assert its exact intended diagnostic via `// EXPECT_ERROR: <substring>`, not merely a non-zero exit code.
- **Packaging**: `make test` executes a staged `make install` and compiles `tests/install_consumer.cpp` strictly against the installed header tree. Preserve this smoke test when changing headers or `Makefile`.
- **Benchmarks**: Any reported performance figure must state the exact workload, compiler version, and build flags (`make benchmark`).
