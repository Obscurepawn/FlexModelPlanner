#include "market_kernels.hpp"
#include <charconv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

namespace {
struct QueryRecord {};
using Record = std::variant<market::BookEvent, market::TradeEvent, market::Reset, QueryRecord>;

std::string_view trim(std::string_view text) {
  const auto first = text.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos) return {};
  const auto last = text.find_last_not_of(" \t\r\n");
  return text.substr(first, last - first + 1);
}

double number(std::string_view text) {
  double value = 0;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || !std::isfinite(value))
    throw std::invalid_argument("expected a finite number, got '" + std::string(text) + "'");
  return value;
}

Record parse(std::string_view line) {
  std::vector<std::string_view> fields;
  for (;;) {
    const auto comma = line.find(',');
    fields.push_back(trim(line.substr(0, comma)));
    if (comma == std::string_view::npos) break;
    line.remove_prefix(comma + 1);
  }
  if (fields[0] == "B" && fields.size() == 3) {
    const double bid = number(fields[1]);
    const double ask = number(fields[2]);
    if (bid > ask) throw std::invalid_argument("book bid exceeds ask");
    return market::BookEvent{bid, ask};
  }
  if (fields[0] == "T" && fields.size() == 2) return market::TradeEvent{number(fields[1])};
  if (fields[0] == "R" && fields.size() == 1) return market::Reset{};
  if (fields[0] == "Q" && fields.size() == 1) return QueryRecord{};
  throw std::invalid_argument("expected B,bid,ask / T,price / R / Q with exact field count");
}

int run(std::istream& input) {
  auto engine = market::make_engine();
  bool has_book = false;
  std::string line;
  std::size_t line_number = 0;
  std::cout << std::setprecision(6);
  while (std::getline(input, line)) {
    ++line_number;
    const auto text = trim(line);
    if (text.empty() || text.front() == '#') continue;
    try {
      const auto record = parse(text);
      std::visit([&](const auto& event) {
        using Event = std::decay_t<decltype(event)>;
        if constexpr (std::is_same_v<Event, QueryRecord>) {
          auto [recent10, recent3] = engine.evaluate<market::Outputs>();
          std::cout << "Q," << recent10 << ',' << recent3 << '\n';
        } else {
          if constexpr (std::is_same_v<Event, market::TradeEvent>) {
            if (!has_book) throw std::invalid_argument("trade requires a preceding book record");
          }
          engine.on(event);
          if constexpr (std::is_same_v<Event, market::BookEvent>) has_book = true;
        }
      }, record);
    } catch (const std::exception& error) {
      std::cerr << "line " << line_number << ": " << error.what() << '\n';
      return 1;
    }
  }
  if (input.bad()) {
    std::cerr << "input read failed after line " << line_number << '\n';
    return 1;
  }
  return 0;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc > 2) {
    std::cerr << "usage: csv_replay [file.csv|-]\n";
    return 2;
  }
  if (argc == 1 || std::string_view(argv[1]) == "-") return run(std::cin);
  std::ifstream input(argv[1]);
  if (!input) {
    std::cerr << "cannot open input: " << argv[1] << '\n';
    return 2;
  }
  return run(input);
}
