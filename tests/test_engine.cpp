#include <flexmodel/flexmodel.hpp>

#include <array>
#include <cmath>
#include <cstdio>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace fm = flexmodel;

namespace {
int checks = 0;
int failures = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { ++failures; \
    std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); } } while (false)
void check_near(double actual, double expected) {
    ++checks;
    if (!std::isfinite(actual) || std::fabs(actual - expected) > 1e-10) {
        ++failures;
        std::printf("FAIL: got %.17g, expected %.17g\n", actual, expected);
    }
}

enum class step { book_handle, mid_eval, history_handle, history_reset, history_eval,
                  spread_eval, normalized_eval, producer_handle, producer_eval,
                  consumer_handle, consumer_eval, a_trade, a_reset, b_reset };
std::array<step, 128> trace{};
std::size_t trace_size = 0;
void record(step s) noexcept { if (trace_size < trace.size()) trace[trace_size++] = s; }
void clear_trace() { trace_size = 0; }
template<class... Steps> void expect_trace(Steps... expected) {
    std::array<step, sizeof...(Steps)> wanted{expected...};
    CHECK(trace_size == wanted.size());
    for (std::size_t i = 0; i < wanted.size() && i < trace_size; ++i)
        CHECK(trace[i] == wanted[i]);
    clear_trace();
}
std::size_t count_step(step s) {
    std::size_t result = 0;
    for (std::size_t i = 0; i < trace_size; ++i) result += trace[i] == s;
    return result;
}

struct Quote { double bid, ask; };
struct Trade { double price; };
struct Reset {};
struct Unknown {};
struct BookId {};
struct MidId {};
struct HistoryId {};
struct SpreadId {};
struct NormalizedId {};
struct UnusedId {};

struct Book {
    struct state_type { double bid = 0, ask = 0; };
    static void handle(state_type& s, const Quote& q) noexcept {
        s.bid = q.bid; s.ask = q.ask; record(step::book_handle);
    }
};
struct Mid {
    struct state_type {};
    using value_type = double;
    static double eval(const state_type&, const Book::state_type& book) noexcept {
        record(step::mid_eval); return (book.bid + book.ask) / 2;
    }
};
struct History {
    struct state_type {
        std::array<double, 3> buffer{};
        std::size_t position = 0, count = 0;
        double sum = 0;
    };
    using value_type = double;
    static void trade(state_type& s, const Trade& t, const double& mid) noexcept {
        const double difference = t.price - mid;
        if (s.count == s.buffer.size()) s.sum -= s.buffer[s.position];
        else ++s.count;
        s.buffer[s.position] = difference;
        s.sum += difference;
        s.position = (s.position + 1) % s.buffer.size();
        record(step::history_handle);
    }
    static void reset(state_type& s, const Reset&) noexcept {
        s = {}; record(step::history_reset);
    }
    static double eval(const state_type& s) noexcept {
        record(step::history_eval); return s.count ? s.sum / s.count : 0;
    }
};
struct Spread {
    struct state_type {};
    using value_type = double;
    static double eval(const state_type&, const Book::state_type& book) noexcept {
        record(step::spread_eval); return book.ask - book.bid;
    }
};
struct Normalized {
    struct state_type {};
    using value_type = double;
    static double eval(const state_type&, const double& history, const double& spread) noexcept {
        record(step::normalized_eval); return spread ? history / spread : 0;
    }
};
struct Unused {
    struct state_type { int trades = 0; };
    static void handle(state_type& s, const Trade&) noexcept { ++s.trades; }
};
using BookNode = fm::node<BookId, Book, fm::on<Quote>>;
using MidNode = fm::node<MidId, Mid, fm::eval<fm::inputs<fm::state_of<BookId>>>>;
using HistoryNode = fm::node<HistoryId, History,
    fm::on<Trade, fm::inputs<fm::value_of<MidId>>, &History::trade>,
    fm::on<Reset, fm::inputs<>, &History::reset>, fm::eval<>>;
using SpreadNode = fm::node<SpreadId, Spread, fm::eval<fm::inputs<fm::state_of<BookId>>>>;
using NormalizedNode = fm::node<NormalizedId, Normalized,
    fm::eval<fm::inputs<fm::value_of<HistoryId>, fm::value_of<SpreadId>>>>;
using UnusedNode = fm::node<UnusedId, Unused, fm::on<Trade>>;
using MarketGraph = fm::graph<HistoryNode, MidNode, BookNode, NormalizedNode, SpreadNode, UnusedNode>;
using Market = fm::engine<MarketGraph>;
using ReverseMarket = fm::engine<fm::graph<UnusedNode, SpreadNode, NormalizedNode, BookNode, MidNode, HistoryNode>>;
using MarketOutputs = fm::query<HistoryId, MidId, NormalizedId>;

void history_and_query_semantics() {
    Market m;
    clear_trace();
    m.on(Quote{100, 102});
    expect_trace(step::book_handle);
    m.on(Trade{103});
    expect_trace(step::mid_eval, step::history_handle);
    m.on(Trade{103});
    expect_trace(step::mid_eval, step::history_handle);
    CHECK(m.state<HistoryId>().count == 2);
    check_near(m.cached_value<HistoryId>(), 0); // Event updates state, not unrequested output.
    auto [first] = m.evaluate<fm::query<HistoryId>>();
    check_near(first, 2);
    expect_trace(step::history_eval);
    m.on(Quote{102, 104});
    m.on(Trade{104});
    clear_trace();
    check_near(std::get<0>(m.evaluate<fm::query<HistoryId>>()), 5.0 / 3.0);
    expect_trace(step::history_eval);
    m.on(Trade{110}); // Evict the oldest 2; retain 2, 1, 7.
    clear_trace();
    check_near(std::get<0>(m.evaluate<fm::query<HistoryId>>()), 10.0 / 3.0);
    expect_trace(step::history_eval);
    m.on(Reset{});
    expect_trace(step::history_reset); // Reset has no dependency on Mid.
    CHECK(m.state<HistoryId>().count == 0);
    check_near(std::get<0>(m.evaluate<fm::query<HistoryId>>()), 0);
    expect_trace(step::history_eval);
    m.on(Unknown{});
    expect_trace();
}

void grouped_outputs_and_borrowed_values() {
    Market m;
    m.on(Quote{100, 102}); m.on(Trade{104}); clear_trace();
    using Outputs = fm::query<NormalizedId, HistoryId, MidId, SpreadId>;
    auto values = m.evaluate<Outputs>();
    static_assert(std::is_same_v<decltype(values), std::tuple<const double&, const double&, const double&, const double&>>);
    check_near(std::get<0>(values), 1.5);
    check_near(std::get<1>(values), 3);
    check_near(std::get<2>(values), 101);
    check_near(std::get<3>(values), 2);
    CHECK(count_step(step::history_eval) == 1);
    CHECK(count_step(step::spread_eval) == 1);
    CHECK(count_step(step::mid_eval) == 1);
    CHECK(count_step(step::normalized_eval) == 1);
    CHECK(count_step(step::history_handle) == 0);
    CHECK(m.state<HistoryId>().count == 1);
    clear_trace();
    (void)m.evaluate<Outputs>();
    CHECK(count_step(step::history_eval) == 1); // No hidden cross-query cache.
    CHECK(count_step(step::spread_eval) == 1);
    clear_trace();
    std::tuple<double, double, double, double> copied;
    m.evaluate_into<Outputs>(copied);
    check_near(std::get<1>(copied), 3);
    m.on(Trade{108});
    (void)m.evaluate<fm::query<HistoryId>>();
    check_near(std::get<1>(values), 5); // Borrowed reference observes the rewritten slot.
    check_near(std::get<1>(copied), 3);
    clear_trace();
}

struct Pulse { int amount; };
struct ProducerId {};
struct ConsumerId {};
struct Producer {
    struct state_type { int total = 0; };
    using value_type = int;
    static void handle(state_type& s, const Pulse& e) noexcept {
        s.total += e.amount; record(step::producer_handle);
    }
    static int eval(const state_type& s) noexcept { record(step::producer_eval); return s.total; }
};
struct Consumer {
    struct state_type { int last = -1; int history = 0; };
    using value_type = int;
    static void handle(state_type& s, const Pulse&, const int& value) noexcept {
        s.last = value; s.history += value; record(step::consumer_handle);
    }
    static int eval(const state_type& s) noexcept { record(step::consumer_eval); return s.history; }
};
using ProducerNode = fm::node<ProducerId, Producer, fm::on<Pulse>, fm::eval<>>;
using ConsumerNode = fm::node<ConsumerId, Consumer,
    fm::on<Pulse, fm::inputs<fm::value_of<ProducerId>>>, fm::eval<>>;
using Interleaved = fm::engine<fm::graph<ConsumerNode, ProducerNode>>;

void interleaving_and_plan_inspection() {
    constexpr auto plan = Interleaved::event_plan<Pulse>();
    static_assert(plan.size == 3);
    static_assert(plan.actions[0].node == 1 && plan.actions[0].kind == fm::action_kind::handle);
    static_assert(plan.actions[1].node == 1 && plan.actions[1].kind == fm::action_kind::eval);
    static_assert(plan.actions[2].node == 0 && plan.actions[2].kind == fm::action_kind::handle);
    constexpr auto query = Interleaved::query_plan<fm::query<ConsumerId, ProducerId>>();
    static_assert(query.size == 2);
    static_assert(Interleaved::event_plan<Unknown>().size == 0);
    Interleaved m;
    clear_trace(); m.on(Pulse{2});
    expect_trace(step::producer_handle, step::producer_eval, step::consumer_handle);
    m.on(Pulse{2}); clear_trace();
    CHECK(m.state<ConsumerId>().last == 4);
    auto [consumer, producer] = m.evaluate<fm::query<ConsumerId, ProducerId>>();
    CHECK(consumer == 6); CHECK(producer == 4);
    CHECK(m.state<ProducerId>().total == 4);
    clear_trace();
}

struct AId {};
struct BId {};
struct AState { int value = 7; };
struct BState { int value = 3; };
struct A {
    using state_type = AState;
    using value_type = int;
    static void trade(state_type& s, const Trade&, const BState& b) noexcept {
        s.value += b.value; record(step::a_trade);
    }
    static void reset(state_type& s, const Reset&) noexcept { s.value = 0; record(step::a_reset); }
    static int eval(const state_type& s) noexcept { return s.value; }
};
struct B {
    using state_type = BState;
    using value_type = int;
    static void reset(state_type& s, const Reset&, const AState& a) noexcept {
        s.value = a.value + 1; record(step::b_reset);
    }
    static int eval(const state_type& s) noexcept { return s.value; }
};
using CrossEventGraph = fm::graph<
    fm::node<BId, B, fm::on<Reset, fm::inputs<fm::state_of<AId>>, &B::reset>, fm::eval<>>,
    fm::node<AId, A, fm::on<Trade, fm::inputs<fm::state_of<BId>>, &A::trade>,
             fm::on<Reset, fm::inputs<>, &A::reset>, fm::eval<>>>;
using CrossEvent = fm::engine<CrossEventGraph>;

void event_specific_dependencies() {
    CrossEvent m;
    clear_trace(); m.on(Trade{0}); expect_trace(step::a_trade);
    CHECK(m.state<AId>().value == 10);
    m.on(Reset{}); expect_trace(step::a_reset, step::b_reset);
    CHECK(m.state<AId>().value == 0);
    CHECK(m.state<BId>().value == 1);
    m.on(Trade{0}); expect_trace(step::a_trade);
    CHECK(m.state<AId>().value == 1);
    // Liveness is a closure, not a topological sort of the cross-event union.
    fm::engine<CrossEventGraph, fm::roots<AId>> rooted;
    rooted.on(Reset{}); expect_trace(step::a_reset, step::b_reset);
    CHECK(rooted.state<BId>().value == 1);
}

void root_liveness_keeps_handler_only_dependencies() {
    using Live = fm::engine<MarketGraph, fm::roots<NormalizedId>>;
    Live m;
    m.on(Quote{100, 102});
    for (int i = 0; i < 7; ++i) m.on(Trade{101.0 + i});
    CHECK(m.state<HistoryId>().count == 3);
    CHECK(m.state<UnusedId>().trades == 0);
    check_near(std::get<0>(m.evaluate<fm::query<NormalizedId>>()), 2.5);
    // Mid is retained because History's handler reads it, even though History.eval does not.
    check_near(std::get<0>(m.evaluate<fm::query<MidId>>()), 101);
    using NoRoots = fm::engine<MarketGraph, fm::roots<>>;
    static_assert(NoRoots::event_plan<Trade>().size == 0);
    static_assert(NoRoots::event_plan<Quote>().size == 0);
    using RepeatedRoots = fm::engine<MarketGraph, fm::roots<NormalizedId, NormalizedId>>;
    static_assert(RepeatedRoots::event_plan<Trade>().actions == Live::event_plan<Trade>().actions);
    RepeatedRoots repeated;
    repeated.on(Quote{100, 102}); repeated.on(Trade{104});
    CHECK(repeated.state<HistoryId>().count == 1);
    clear_trace();
}

struct LeftId {};
struct RightId {};
struct Counter {
    struct state_type { int value = 0; };
    using value_type = int;
    static void handle(state_type& s, const Pulse& p) noexcept { s.value += p.amount; }
    static int eval(const state_type& s) noexcept { return s.value; }
};
using CounterEngine = fm::engine<fm::graph<
    fm::node<LeftId, Counter, fm::on<Pulse>, fm::eval<>>,
    fm::node<RightId, Counter, fm::on<Pulse>, fm::eval<>>>>;
struct Configured {
    struct state_type {
        int value;
        state_type() = delete;
        explicit state_type(int v) : value(v) {}
        state_type(const state_type&) = delete;
        state_type& operator=(const state_type&) = delete;
        state_type(state_type&&) = default;
        state_type& operator=(state_type&&) = default;
    };
    using value_type = int;
    static int eval(const state_type& s) noexcept { return s.value; }
};
void identities_configuration_and_instances() {
    constexpr auto reordered_query = CounterEngine::query_plan<fm::query<RightId, LeftId>>();
    static_assert(reordered_query.size == 2);
    static_assert(reordered_query.actions[0].node == 0,
                  "Independent query actions follow graph declaration order");
    static_assert(reordered_query.actions[1].node == 1);
    CounterEngine a(CounterEngine::state_tuple{Counter::state_type{2}, Counter::state_type{20}});
    CounterEngine b;
    a.on(Pulse{3}); b.on(Pulse{1});
    auto [left, right] = a.evaluate<fm::query<LeftId, RightId>>();
    CHECK(left == 5); CHECK(right == 23);
    auto [right_first, left_second] = a.evaluate<fm::query<RightId, LeftId>>();
    CHECK(right_first == 23); CHECK(left_second == 5); // Output order remains the requested order.
    CHECK(b.state<LeftId>().value == 1);
    static_assert(std::is_same_v<decltype(a.state<LeftId>()), const Counter::state_type&>);
    using ConfiguredEngine = fm::engine<fm::graph<fm::node<LeftId, Configured, fm::eval<>>>>;
    ConfiguredEngine configured(ConfiguredEngine::state_tuple{Configured::state_type{42}});
    CHECK(std::get<0>(configured.evaluate<fm::query<LeftId>>()) == 42);
}

void empty_graph_and_empty_query() {
    using Empty = fm::engine<fm::graph<>>;
    static_assert(Empty::event_plan<Unknown>().size == 0);
    static_assert(Empty::query_plan<fm::query<>>().size == 0);
    Empty empty;
    empty.on(Unknown{});
    empty.on_interpreted(Unknown{});
    auto values = empty.evaluate<fm::query<>>();
    static_assert(std::is_same_v<decltype(values), std::tuple<>>);
    (void)empty.evaluate_interpreted<fm::query<>>();
    Market market;
    clear_trace();
    (void)market.evaluate<fm::query<>>();
    expect_trace();
}

struct SummaryId {};
struct FlagId {};
struct Summary {
    using state_type = int;
    using value_type = std::array<int, 2>;
    static void handle(int& s, const Pulse& p) noexcept { s += p.amount; }
    static value_type summarize(const int& s) noexcept { return {s, 2 * s}; }
};
struct Flag {
    struct state_type {};
    using value_type = bool;
    static bool eval(const state_type&, const Summary::value_type& summary) noexcept {
        return summary[1] > 10;
    }
};
void structured_outputs_and_explicit_evaluator() {
    using Engine = fm::engine<fm::graph<
        fm::node<SummaryId, Summary, fm::on<Pulse>, fm::eval<fm::inputs<>, &Summary::summarize>>,
        fm::node<FlagId, Flag, fm::eval<fm::inputs<fm::value_of<SummaryId>>>>>>;
    Engine m;
    m.on(Pulse{6});
    auto values = m.evaluate<fm::query<FlagId, SummaryId>>();
    static_assert(std::is_same_v<decltype(values), std::tuple<const bool&, const std::array<int, 2>&>>);
    CHECK(std::get<0>(values));
    CHECK(std::get<1>(values)[0] == 6);
    CHECK(std::get<1>(values)[1] == 12);
}

// This oracle keeps chronological samples in a vector and recomputes their sum.
// It does not use the planner, the kernel's ring-buffer logic or its running sum.
struct Oracle {
    double bid = 0, ask = 0;
    std::vector<double> differences;
    void on(const Quote& q) { bid = q.bid; ask = q.ask; }
    void on(const Trade& t) {
        differences.push_back(t.price - (bid + ask) / 2);
        if (differences.size() > 3) differences.erase(differences.begin());
    }
    void on(const Reset&) { differences.clear(); }
    double history() const {
        double sum = 0; for (double x : differences) sum += x;
        return differences.empty() ? 0 : sum / differences.size();
    }
    double mid() const { return (bid + ask) / 2; }
    double normalized() const { return ask == bid ? 0 : history() / (ask - bid); }
};
using MarketEvent = std::variant<Quote, Trade, Reset>;
std::vector<MarketEvent> event_stream() {
    std::vector<MarketEvent> events{Quote{100, 102}};
    for (int i = 0; i < 97; ++i) {
        if (i % 17 == 0) events.emplace_back(Reset{});
        if (i % 5 == 0) events.emplace_back(Quote{100.0 + i, 102.0 + i});
        events.emplace_back(Trade{103.0 + (i / 2) % 19}); // Repeated prices are intentional.
    }
    return events;
}
void independent_oracle_and_interpreter() {
    Market generated;
    Market interpreted;
    ReverseMarket reverse;
    Oracle oracle;
    std::size_t index = 0;
    for (const auto& event : event_stream()) {
        std::visit([&](const auto& e) {
            generated.on(e); interpreted.on_interpreted(e); reverse.on(e); oracle.on(e);
        }, event);
        // Some events deliberately have no query; handlers must still preserve history.
        if (++index % 3 != 0) { clear_trace(); continue; }
        const auto actual = generated.evaluate<MarketOutputs>();
        const auto reference = interpreted.evaluate_interpreted<MarketOutputs>();
        const auto reordered = reverse.evaluate<MarketOutputs>();
        const std::array<double, 3> expected{oracle.history(), oracle.mid(), oracle.normalized()};
        std::apply([&](const auto&... x) { std::size_t i = 0; (check_near(x, expected[i++]), ...); }, actual);
        std::apply([&](const auto&... x) { std::size_t i = 0; (check_near(x, expected[i++]), ...); }, reference);
        std::apply([&](const auto&... x) { std::size_t i = 0; (check_near(x, expected[i++]), ...); }, reordered);
        clear_trace();
    }
}

void replay_preserves_input_order() {
    const auto events = event_stream();
    Market direct;
    Market replayed;
    for (const auto& event : events)
        std::visit([&](const auto& e) { direct.on(e); }, event);
    fm::replay(replayed, events);
    auto expected = direct.evaluate<MarketOutputs>();
    auto actual = replayed.evaluate<MarketOutputs>();
    check_near(std::get<0>(actual), std::get<0>(expected));
    check_near(std::get<1>(actual), std::get<1>(expected));
    check_near(std::get<2>(actual), std::get<2>(expected));
    std::array<Pulse, 3> pulses{{{2}, {3}, {-1}}};
    CounterEngine counters;
    fm::replay(counters, pulses);
    CHECK(counters.state<LeftId>().value == 4);
    CHECK(counters.state<RightId>().value == 4);
    clear_trace();
}
} // namespace

int main() {
    history_and_query_semantics();
    grouped_outputs_and_borrowed_values();
    interleaving_and_plan_inspection();
    event_specific_dependencies();
    root_liveness_keeps_handler_only_dependencies();
    identities_configuration_and_instances();
    empty_graph_and_empty_query();
    structured_outputs_and_explicit_evaluator();
    independent_oracle_and_interpreter();
    replay_preserves_input_order();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
