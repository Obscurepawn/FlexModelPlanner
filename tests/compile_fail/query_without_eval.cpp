// EXPECT_ERROR: query requires eval
#include <flexmodel/flexmodel.hpp>
namespace fm = flexmodel;
struct Id {}; struct K { using state_type = int; };
int main() { fm::engine<fm::graph<fm::node<Id, K>>> engine; (void)engine.evaluate<fm::query<Id>>(); }
