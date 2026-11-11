// EXPECT_ERROR: handler callback must return void
#include <flexmodel/flexmodel.hpp>
namespace fm = flexmodel;
struct Id {}; struct Tick {};
struct K { using state_type = int;
    static int handle(int&, const Tick&) noexcept { return 1; } };
fm::engine<fm::graph<fm::node<Id, K, fm::on<Tick>>>> engine;
