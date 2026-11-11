// EXPECT_ERROR: duplicate query output
#include <flexmodel/flexmodel.hpp>
namespace fm = flexmodel;
struct Id {};
struct K { using state_type = int; using value_type = int;
    static int eval(const int& s) noexcept { return s; } };
int main() { fm::engine<fm::graph<fm::node<Id, K, fm::eval<>>>> engine;
    (void)engine.evaluate<fm::query<Id, Id>>(); }
