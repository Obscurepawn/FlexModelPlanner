// EXPECT_ERROR: cyclic action dependency
#include <flexmodel/flexmodel.hpp>
namespace fm = flexmodel;
struct A {}; struct B {}; struct Live {}; struct Tick {};
struct K { using state_type = int;
    static void handle(int&, const Tick&, const int&) noexcept {} };
struct Empty { using state_type = int; };
using G = fm::graph<fm::node<A, K, fm::on<Tick, fm::inputs<fm::state_of<B>>>>,
    fm::node<B, K, fm::on<Tick, fm::inputs<fm::state_of<A>>>>, fm::node<Live, Empty>>;
// A cycle must fail at engine instantiation, even outside the live closure.
fm::engine<G, fm::roots<Live>> engine;
