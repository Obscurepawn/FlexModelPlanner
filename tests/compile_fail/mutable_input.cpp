// EXPECT_ERROR: handler callback signature mismatch
#include <flexmodel/flexmodel.hpp>
namespace fm = flexmodel;
struct A {}; struct B {}; struct Tick {};
struct Source { using state_type = int; };
struct Consumer { using state_type = int;
    static void handle(int&, const Tick&, int&) noexcept {} };
fm::engine<fm::graph<fm::node<A, Source>,
    fm::node<B, Consumer, fm::on<Tick, fm::inputs<fm::state_of<A>>>>>> engine;
