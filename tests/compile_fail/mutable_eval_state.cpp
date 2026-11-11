// EXPECT_ERROR: eval callback signature mismatch
#include <flexmodel/flexmodel.hpp>
namespace fm = flexmodel;
struct Id {};
struct K { using state_type = int; using value_type = int;
    static int eval(int& state) noexcept { return ++state; } };
fm::engine<fm::graph<fm::node<Id, K, fm::eval<>>>> engine;
