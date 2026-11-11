#include <flexmodel/flexmodel.hpp>
#include <iomanip>
#include <iostream>
#include <tuple>

namespace fm = flexmodel;

struct BookUpdate { double bid{}, ask{}, bid_size{}, ask_size{}; };
struct Trade { double price; };
struct Reset {};

struct BookId {};
struct MidId {};
struct SpreadId {};
struct ImbalanceId {};
struct FairValueId {};
struct BookBiasId {};
struct TradePressureId {};
struct ScoreId {};

struct Book {
    using state_type = BookUpdate;
    static void handle(state_type& state, const BookUpdate& event) noexcept {
        state = event;
    }
};

// Stateless scalar evaluators share these two type declarations.
struct Scalar {
    struct state_type {};
    using value_type = double;
};

struct Mid : Scalar {
    static double eval(const state_type&, const BookUpdate& book) noexcept {
        return book.bid / 2.0 + book.ask / 2.0;
    }
};

struct Spread : Scalar {
    static double eval(const state_type&, const BookUpdate& book) noexcept {
        return book.ask - book.bid;
    }
};

struct Imbalance : Scalar {
    static double eval(const state_type&, const BookUpdate& book) noexcept {
        const double size = book.bid_size + book.ask_size;
        return size > 0.0 ? (book.bid_size - book.ask_size) / size : 0.0;
    }
};

struct FairValue : Scalar {
    static double eval(const state_type&, const double& mid,
                       const double& spread, const double& imbalance) noexcept {
        return mid + 0.5 * spread * imbalance;
    }
};

struct BookBias : Scalar {
    static double eval(const state_type&, const double& fair,
                       const double& mid, const double& spread) noexcept {
        return spread > 0.0 ? (fair - mid) / spread : 0.0;
    }
};

struct TradePressure {
    struct state_type { double ema = 0.0; bool initialized = false; };
    using value_type = double;

    static void handle(state_type& state, const Trade& event,
                       const double& mid, const double& spread) noexcept {
        const double sample = spread > 0.0 ? (event.price - mid) / spread : 0.0;
        state.ema = state.initialized ? 0.5 * state.ema + 0.5 * sample : sample;
        state.initialized = true;
    }
    static void handle(state_type& state, const Reset&) noexcept { state = {}; }
    static double eval(const state_type& state) noexcept { return state.ema; }
};

struct Score : Scalar {
    static double eval(const state_type&, const double& bias,
                       const double& pressure) noexcept {
        return bias + pressure;
    }
};

using Graph = fm::graph<
    fm::node<BookId, Book, fm::on<BookUpdate>>,
    fm::node<MidId, Mid, fm::eval<fm::inputs<fm::state_of<BookId>>>>,
    fm::node<SpreadId, Spread, fm::eval<fm::inputs<fm::state_of<BookId>>>>,
    fm::node<ImbalanceId, Imbalance, fm::eval<fm::inputs<fm::state_of<BookId>>>>,
    fm::node<FairValueId, FairValue,
        fm::eval<fm::inputs<fm::value_of<MidId>, fm::value_of<SpreadId>,
                            fm::value_of<ImbalanceId>>>>,
    fm::node<BookBiasId, BookBias,
        fm::eval<fm::inputs<fm::value_of<FairValueId>, fm::value_of<MidId>,
                            fm::value_of<SpreadId>>>>,
    fm::node<TradePressureId, TradePressure,
        fm::on<Trade, fm::inputs<fm::value_of<MidId>, fm::value_of<SpreadId>>>,
        fm::on<Reset>, fm::eval<>>,
    fm::node<ScoreId, Score,
        fm::eval<fm::inputs<fm::value_of<BookBiasId>, fm::value_of<TradePressureId>>>>
>;
using Outputs = fm::query<ScoreId, FairValueId>;

int main() {
    fm::engine<Graph, fm::roots<ScoreId>> model;
    model.on(BookUpdate{99.0, 101.0, 30.0, 10.0});
    model.on(Trade{101.0});
    model.on(BookUpdate{100.0, 104.0, 10.0, 30.0});
    model.on(Trade{103.0});
    model.on(Trade{103.0});  // The same price still changes the EMA.

    std::cout << std::fixed << std::setprecision(4);
    const auto print = [&](const char* label) {
        const auto [score, fair] = model.evaluate<Outputs>();
        std::cout << label << ": score=" << score << ", fair=" << fair << '\n';
    };
    print("before reset");
    model.on(Reset{});
    print("after reset");
}
