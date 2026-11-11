# Working on FlexModelPlanner

- C++20, header-only, namespace `flexmodel`; no third-party runtime dependency.
- Public contracts are in `SPECS.md`. Keep README examples executable.
- Graph metadata and planning live in `include/flexmodel/`; state is owned by an engine instance. Plans never capture instance addresses.
- Preserve event history: every matching handler of a live node runs once per event, independently of queries. Do not add cutoffs or cross-event value caching implicitly.
- After public header changes, build and run CTest with GCC and Clang. Negative compilation tests must check the intended diagnostic and receive the public include path.
- Validate allocation-free dispatch with the dedicated allocation test, not the logging tests. Performance numbers require an explicit workload and compiler/build mode.
- Build artifacts belong in `build-*`; do not edit generated files. Run sanitizers after changes to state access, dispatch or replay.
