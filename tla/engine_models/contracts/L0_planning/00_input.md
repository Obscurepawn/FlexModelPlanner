# L0 Planning — input (frozen at close)

The request to verify the whole project resolves first to the planner: it determines whether event histories see prerequisite state and values in the required order. Runtime history and replay form L1; C++ type diagnostics and allocation behavior have separate test gates.

No previous-level properties are assumed.

Verified source anchors:

- `include/flexmodel/meta.hpp:12-34`: graph/node/operation/port/roots descriptors.
- `include/flexmodel/planner.hpp:9-13`: named non-constexpr cycle rejection helper; it preserves the runtime exception text.
- `include/flexmodel/planner.hpp:15-37`: action graph and recursive gray/black DFS.
- `include/flexmodel/planner.hpp:46-78`: all-node or root-selected liveness closure.
- `include/flexmodel/planner.hpp:80-109`: event-specific handler/value prerequisites.
- `include/flexmodel/planner.hpp:112-118`: live event handler roots.
- `include/flexmodel/planner.hpp:121-135`: additional eager whole-graph validation (scope boundary).
- `include/flexmodel/planner.hpp:142-157`: query validity checks, requested mask, declaration-order traversal.

Source files were read directly. The implementation is synchronous; no concurrency or fault tolerance is claimed. Bounded enumeration is test input generation, not an assumption that production supports only these graph sizes.
