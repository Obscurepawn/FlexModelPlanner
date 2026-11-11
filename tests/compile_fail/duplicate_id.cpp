// EXPECT_ERROR: duplicate node id
#include <flexmodel/flexmodel.hpp>
namespace fm = flexmodel;
struct Id {}; struct K { using state_type = int; };
using E = fm::engine<fm::graph<fm::node<Id, K>, fm::node<Id, K>>>;
E engine;
