// EXPECT_ERROR: invalid input port
#include <flexmodel/flexmodel.hpp>
namespace fm = flexmodel;
struct Id {};
struct K {
    using state_type = int;
    using value_type = int;
    template<class... T> static int eval(const int&, const T&...) noexcept { return 0; }
};
fm::engine<fm::graph<fm::node<Id, K, fm::eval<fm::inputs<int>>>>> engine;
