// EXPECT_ERROR: query outside live graph
#include <flexmodel/flexmodel.hpp>
namespace fm = flexmodel;
struct A {}; struct B {};
struct K { using state_type = int; using value_type = int;
    static int eval(const int& s) noexcept { return s; } };
using G = fm::graph<fm::node<A, K, fm::eval<>>, fm::node<B, K, fm::eval<>>>;
int main() { fm::engine<G, fm::roots<A>> engine; (void)engine.evaluate<fm::query<B>>(); }
