#pragma once
#include <flexmodel/flexmodel.hpp>
#include <cstddef>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace market {
struct BookEvent { double bid; double ask; };
struct TradeEvent { double price; };
struct Reset {};
struct BookId { static constexpr std::string_view name = "book"; };
struct MidId { static constexpr std::string_view name = "mid"; };
struct Recent10Id { static constexpr std::string_view name = "recent10"; };
struct Recent3Id { static constexpr std::string_view name = "recent3"; };

struct BookKernel {
  struct state_type { double bid = 0; double ask = 0; };
  static void handle(state_type& state, const BookEvent& e) noexcept {
    state.bid = e.bid;
    state.ask = e.ask;
  }
};
struct MidKernel {
  struct state_type {};
  using value_type = double;
  static double eval(const state_type&, const BookKernel::state_type& book) noexcept {
    return book.bid / 2.0 + book.ask / 2.0;
  }
};
struct RecentDiffKernel {
  struct state_type {
    std::vector<double> ring;
    std::size_t next = 0;
    std::size_t count = 0;
    double sum = 0;
    explicit state_type(std::size_t window = 10) : ring(window) {
      if (window == 0) throw std::invalid_argument("window must be greater than zero");
    }
  };
  using value_type = double;
  static void handle(state_type& state, const TradeEvent& event, const double& mid) noexcept {
    const double difference = event.price - mid;
    if (state.count == state.ring.size()) state.sum -= state.ring[state.next];
    else ++state.count;
    state.ring[state.next] = difference;
    state.sum += difference;
    state.next = (state.next + 1) % state.ring.size();
  }
  static void handle(state_type& state, const Reset&) noexcept {
    state.next = 0;
    state.count = 0;
    state.sum = 0;
  }
  static double eval(const state_type& state) noexcept {
    return state.count == 0 ? 0.0 : state.sum / static_cast<double>(state.count);
  }
};

using Graph = flexmodel::graph<
    flexmodel::node<BookId, BookKernel, flexmodel::on<BookEvent>>,
    flexmodel::node<MidId, MidKernel,
      flexmodel::eval<flexmodel::inputs<flexmodel::state_of<BookId>>>>,
    flexmodel::node<Recent10Id, RecentDiffKernel,
      flexmodel::on<TradeEvent, flexmodel::inputs<flexmodel::value_of<MidId>>>,
      flexmodel::on<Reset>, flexmodel::eval<>>,
    flexmodel::node<Recent3Id, RecentDiffKernel,
      flexmodel::on<TradeEvent, flexmodel::inputs<flexmodel::value_of<MidId>>>,
      flexmodel::on<Reset>, flexmodel::eval<>>>;
using Engine = flexmodel::engine<Graph>;
using Outputs = flexmodel::query<Recent10Id, Recent3Id>;

inline Engine make_engine(std::size_t first_window = 10, std::size_t second_window = 3) {
  return Engine(Engine::state_tuple{
      BookKernel::state_type{}, MidKernel::state_type{},
      RecentDiffKernel::state_type{first_window}, RecentDiffKernel::state_type{second_window}});
}
}  // namespace market
