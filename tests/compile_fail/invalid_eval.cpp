// EXPECT_ERROR: eval callback must return exact value_type
#include <flexmodel/flexmodel.hpp>
namespace fm = flexmodel;
struct Id {};
struct K { using state_type = int; using value_type = int;
    static double eval(const int&) noexcept { return 1; } };
fm::engine<fm::graph<fm::node<Id, K, fm::eval<>>>> engine;
