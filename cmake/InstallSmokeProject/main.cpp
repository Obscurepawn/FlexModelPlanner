#include <flexmodel/flexmodel.hpp>
#include <tuple>
struct Tick {};
struct Id {};
struct Counter {
  struct state_type { int count = 0; };
  using value_type = int;
  static void handle(state_type& state, const Tick&) noexcept { ++state.count; }
  static int eval(const state_type& state) noexcept { return state.count; }
};
using Graph = flexmodel::graph<flexmodel::node<Id, Counter, flexmodel::on<Tick>, flexmodel::eval<>>>;
int main() {
  flexmodel::engine<Graph> engine;
  engine.on(Tick{});
  return std::get<0>(engine.evaluate<flexmodel::query<Id>>()) == 1 ? 0 : 1;
}
