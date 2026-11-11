# Working on FlexModelPlanner

- C++20, header-only, namespace `flexmodel`; no third-party runtime dependency.
- Public contracts are in `SPECS.md`. Keep README examples executable.
- Graph metadata and planning live in `include/flexmodel/`; state is owned by an engine instance. Plans never capture instance addresses.
- Preserve event history: every matching handler of a live node runs once per event, independently of queries. Do not add cutoffs or cross-event value caching implicitly.
- After public header changes, run `make test` with GCC and Clang in separate `BUILD_DIR` directories. Negative compilation tests must check the intended diagnostic and receive the public include path.
- `make model-check` runs the bounded TLC suite. Review source/model correspondence before updating model fingerprints or anchors.
- Validate allocation-free dispatch with the dedicated allocation test, not the logging tests. Performance numbers require an explicit workload and compiler/build mode.
- Build artifacts belong in `build-*` (`build-make` by default); do not edit generated files. Run `make sanitize SANITIZE_CXX=clang++` after changes to state access, dispatch or replay.
- The build interface is GNU Make with a POSIX shell; tests require Python 3. Windows CI uses the MSYS2 UCRT64 shell and MinGW GCC. Keep compiler flags compatible with GCC/Clang-style drivers; do not add a separate MSVC build branch.
- `make test` includes a real `make install` and an independent consumer compiled only against installed headers. Preserve this check when changing packaging.
