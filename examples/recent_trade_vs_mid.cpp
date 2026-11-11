#include "market_kernels.hpp"
#include <flexmodel/inspect.hpp>
#include <array>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <tuple>

int main() {
  auto engine = market::make_engine();
  constexpr std::array names{std::string_view{"book"}, std::string_view{"mid"},
      std::string_view{"recent10"}, std::string_view{"recent3"}};
  std::cout << "Book plan:\n";
  flexmodel::dump_plan(market::Engine::event_plan<market::BookEvent>(), std::cout, names);
  std::cout << "Trade plan:\n";
  flexmodel::dump_plan(market::Engine::event_plan<market::TradeEvent>(), std::cout, names);
  std::cout << "Reset plan:\n";
  flexmodel::dump_plan(market::Engine::event_plan<market::Reset>(), std::cout, names);
  std::cout << "Query plan:\n";
  flexmodel::dump_plan(market::Engine::query_plan<market::Outputs>(), std::cout, names);
  engine.on(market::BookEvent{100, 102});
  engine.on(market::TradeEvent{103});
  engine.on(market::TradeEvent{103});
  engine.on(market::BookEvent{102, 104});
  engine.on(market::TradeEvent{104});
  auto [recent10, recent3] = engine.evaluate<market::Outputs>();
  std::cout << std::setprecision(6) << "recent10=" << recent10
            << ", recent3=" << recent3 << '\n';
}
