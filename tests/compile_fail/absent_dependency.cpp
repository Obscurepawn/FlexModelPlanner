// EXPECT_ERROR: dependency absent
#include <flexmodel/flexmodel.hpp>
namespace fm = flexmodel;
struct LiveId {}; struct DeadId {}; struct MissingId {};
struct K { using state_type = int; using value_type = int;
    template<class... T> static int eval(const int&, const T&...) noexcept { return 0; } };
using G = fm::graph<fm::node<LiveId, K, fm::eval<>>,
    fm::node<DeadId, K, fm::eval<fm::inputs<fm::state_of<MissingId>>>>>;
// Unused declarations still require validation.
fm::engine<G, fm::roots<LiveId>> engine;
