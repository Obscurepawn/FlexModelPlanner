# L0 Planning — review notes

The two-event configuration includes graphs whose union of node dependencies is cyclic while each selected event action graph is acyclic. Root closure intentionally follows the union, but cycle detection follows the selected action graph. Retaining both structures in the model prevents an incorrect union-graph cycle restriction.

Independent C++ enumeration in `tests/test_planner_properties.cpp` tests the actual DFS helper on all 65,536 directed graphs with four action slots and all 16 seed masks. Its separate reachability/cycle oracle complements this typed-metadata model and provides a direct implementation check.
