#include "market_kernels.hpp"
#include <charconv>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <tuple>

namespace {
template<bool Interpreted>
double run(std::uint64_t count, double& checksum) {
  auto engine = market::make_engine(10, 3);
  engine.on(market::BookEvent{100, 102});
  auto step = [&](std::uint64_t i) {
    const market::TradeEvent event{102.0 + static_cast<double>(i % 7)};
    if constexpr (Interpreted) engine.on_interpreted(event);
    else engine.on(event);
    if constexpr (Interpreted) {
      auto values = engine.template evaluate_interpreted<market::Outputs>();
      checksum += std::get<0>(values) + std::get<1>(values);
    } else {
      auto values = engine.template evaluate<market::Outputs>();
      checksum += std::get<0>(values) + std::get<1>(values);
    }
  };
  for (std::uint64_t i = 0; i < 10000; ++i) step(i);
  checksum = 0;
  const auto start = std::chrono::steady_clock::now();
  for (std::uint64_t i = 0; i < count; ++i) step(i);
  const auto stop = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::nano>(stop - start).count() / static_cast<double>(count);
}
}

int main(int argc, char** argv) {
  std::uint64_t count = 1000000;
  if (argc > 2) { std::cerr << "usage: dispatch_benchmark [iterations]\n"; return 2; }
  if (argc == 2) {
    const std::string_view arg(argv[1]);
    auto parsed = std::from_chars(arg.data(), arg.data() + arg.size(), count);
    if (parsed.ec != std::errc{} || parsed.ptr != arg.data() + arg.size() || count == 0) {
      std::cerr << "iterations must be a positive integer\n"; return 2;
    }
  }
  double static_sum = 0, interpreted_sum = 0;
  const double static_ns = run<false>(count, static_sum);
  const double interpreted_ns = run<true>(count, interpreted_sum);
  if (static_sum != interpreted_sum) { std::cerr << "checksum mismatch\n"; return 1; }
  std::cout << "mode,iterations,ns_per_trade_and_group_query,checksum\n"
            << std::setprecision(12)
            << "static," << count << ',' << static_ns << ',' << static_sum << '\n'
            << "interpreted," << count << ',' << interpreted_ns << ',' << interpreted_sum << '\n';
}
