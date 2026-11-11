// EXPECT_ERROR: duplicate handler
#include <flexmodel/flexmodel.hpp>
namespace fm = flexmodel;
struct Id {}; struct Tick {};
struct K { using state_type = int; static void handle(int&, const Tick&) noexcept {} };
using E = fm::engine<fm::graph<fm::node<Id, K, fm::on<Tick>, fm::on<Tick>>>>;
E engine;
