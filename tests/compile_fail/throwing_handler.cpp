// EXPECT_ERROR: handler callback must be noexcept
#include <flexmodel/flexmodel.hpp>
namespace fm = flexmodel;
struct Id {}; struct Tick {};
struct K { using state_type = int; static void handle(int&, const Tick&) {} };
fm::engine<fm::graph<fm::node<Id, K, fm::on<Tick>>>> engine;
