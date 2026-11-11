// EXPECT_ERROR: eval callback must be noexcept
#include <flexmodel/flexmodel.hpp>
namespace fm = flexmodel;
struct Id {};
struct K { using state_type = int; using value_type = int;
    static int eval(const int&) { return 1; } };
fm::engine<fm::graph<fm::node<Id, K, fm::eval<>>>> engine;
