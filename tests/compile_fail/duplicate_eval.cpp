// EXPECT_ERROR: duplicate eval
#include <flexmodel/flexmodel.hpp>
namespace fm = flexmodel;
struct Id {};
struct K { using state_type = int; using value_type = int; static int eval(const int&) noexcept { return 0; } };
using E = fm::engine<fm::graph<fm::node<Id, K, fm::eval<>, fm::eval<>>>>;
E engine;
