// EXPECT_ERROR: value dependency requires eval
#include <flexmodel/flexmodel.hpp>
namespace fm = flexmodel;
struct ProducerId {}; struct ConsumerId {}; struct Tick {};
struct Producer { using state_type = int; };
struct Consumer { using state_type = int;
    template<class T> static void handle(int&, const Tick&, const T&) noexcept {} };
using G = fm::graph<fm::node<ProducerId, Producer>,
    fm::node<ConsumerId, Consumer, fm::on<Tick, fm::inputs<fm::value_of<ProducerId>>>>>;
fm::engine<G> engine;
