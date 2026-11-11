// EXPECT_ERROR: invalid operation descriptor
#include <flexmodel/flexmodel.hpp>
namespace fm = flexmodel;
struct Id {};
struct K { using state_type = int; };
fm::engine<fm::graph<fm::node<Id, K, int>>> engine;
