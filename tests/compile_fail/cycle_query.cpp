// EXPECT_ERROR: cyclic action dependency
#include <flexmodel/flexmodel.hpp>
namespace fm = flexmodel;
struct A {}; struct B {}; struct Live {};
struct K { using state_type = int; using value_type = int;
    static int eval(const int&, const int& input) noexcept { return input; } };
struct Empty { using state_type = int; };
using G = fm::graph<fm::node<A, K, fm::eval<fm::inputs<fm::value_of<B>>>>,
    fm::node<B, K, fm::eval<fm::inputs<fm::value_of<A>>>>, fm::node<Live, Empty>>;
fm::engine<G, fm::roots<Live>> engine;
