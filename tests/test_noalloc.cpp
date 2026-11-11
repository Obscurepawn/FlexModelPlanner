#include <flexmodel/flexmodel.hpp>
#include "../examples/market_kernels.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <tuple>
#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace {
bool tracking = false;
std::size_t allocations = 0;
void note_allocation() noexcept { if (tracking) ++allocations; }
void* allocate(std::size_t size) {
    note_allocation();
    if (void* result = std::malloc(size ? size : 1)) return result;
    throw std::bad_alloc();
}
void* allocate_aligned(std::size_t size, std::size_t alignment) {
    note_allocation();
#if defined(_MSC_VER)
    if (void* result = _aligned_malloc(size ? size : 1, alignment)) return result;
#else
    void* result = nullptr;
    if (posix_memalign(&result, alignment, size ? size : 1) == 0) return result;
#endif
    throw std::bad_alloc();
}
void free_aligned(void* p) noexcept {
#if defined(_MSC_VER)
    _aligned_free(p);
#else
    std::free(p);
#endif
}
} // namespace

void* operator new(std::size_t size) { return allocate(size); }
void* operator new[](std::size_t size) { return allocate(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    try { return allocate(size); } catch (...) { return nullptr; }
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    try { return allocate(size); } catch (...) { return nullptr; }
}
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }
void* operator new(std::size_t size, std::align_val_t a) {
    return allocate_aligned(size, static_cast<std::size_t>(a));
}
void* operator new[](std::size_t size, std::align_val_t a) {
    return allocate_aligned(size, static_cast<std::size_t>(a));
}
void operator delete(void* p, std::align_val_t) noexcept { free_aligned(p); }
void operator delete[](void* p, std::align_val_t) noexcept { free_aligned(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { free_aligned(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { free_aligned(p); }
void* operator new(std::size_t size, std::align_val_t a, const std::nothrow_t&) noexcept {
    try { return allocate_aligned(size, static_cast<std::size_t>(a)); } catch (...) { return nullptr; }
}
void* operator new[](std::size_t size, std::align_val_t a, const std::nothrow_t&) noexcept {
    try { return allocate_aligned(size, static_cast<std::size_t>(a)); } catch (...) { return nullptr; }
}
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept { free_aligned(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { free_aligned(p); }

namespace fm = flexmodel;
namespace {
struct Quote { double bid, ask; };
struct Trade { double price; };
struct BookId {};
struct MidId {};
struct HistoryId {};
struct Book {
    struct state_type { double bid = 0, ask = 0; };
    static void handle(state_type& s, const Quote& q) noexcept { s = {q.bid, q.ask}; }
};
struct Mid {
    struct state_type {};
    using value_type = double;
    static double eval(const state_type&, const Book::state_type& b) noexcept { return (b.bid + b.ask) / 2; }
};
struct History {
    struct state_type {
        std::array<double, 8> ring{};
        std::size_t pos = 0, count = 0;
        double sum = 0;
    };
    using value_type = double;
    static void handle(state_type& s, const Trade& t, const double& mid) noexcept {
        const double x = t.price - mid;
        s.sum += x - s.ring[s.pos]; s.ring[s.pos] = x;
        s.pos = (s.pos + 1) % s.ring.size();
        if (s.count < s.ring.size()) ++s.count;
    }
    static double eval(const state_type& s) noexcept { return s.count ? s.sum / s.count : 0; }
};
using Engine = fm::engine<fm::graph<
    fm::node<HistoryId, History, fm::on<Trade, fm::inputs<fm::value_of<MidId>>>, fm::eval<>>,
    fm::node<MidId, Mid, fm::eval<fm::inputs<fm::state_of<BookId>>>>,
    fm::node<BookId, Book, fm::on<Quote>>>>;
using Outputs = fm::query<HistoryId, MidId>;
}

int main() {
    // A control checks the allocation hook before measuring any engine work.
    tracking = true;
    void* control = ::operator new(13);
    ::operator delete(control);
    tracking = false;
    if (allocations != 1) return 2;
    allocations = 0;
    Engine generated;
    Engine interpreted;
    // Exercise the shipped kernels as well: vector-backed windows allocate at
    // startup, but retain their fixed size throughout the measured event loop.
    auto shipped = market::make_engine(10, 3);
    std::tuple<double, double> copied;
    double checksum = 0;
    tracking = true;
    for (int i = 0; i < 10000; ++i) {
        const Quote quote{100.0 + i % 17, 102.0 + i % 17};
        const Trade trade{103.0 + i % 19};
        generated.on(quote); generated.on(trade);
        const auto values = generated.evaluate<Outputs>();
        checksum += std::get<0>(values) + std::get<1>(values);
        generated.evaluate_into<Outputs>(copied);
        interpreted.on_interpreted(quote); interpreted.on_interpreted(trade);
        const auto reference = interpreted.evaluate_interpreted<Outputs>();
        checksum += std::get<0>(reference);
        shipped.on(market::BookEvent{quote.bid, quote.ask});
        if (i % 31 == 0) shipped.on(market::Reset{});
        shipped.on(market::TradeEvent{trade.price});
        const auto example_values = shipped.evaluate<market::Outputs>();
        checksum += std::get<0>(example_values) + std::get<1>(example_values);
        shipped.evaluate_into<market::Outputs>(copied);
    }
    tracking = false;
    std::printf("hot-path allocations: %zu; checksum: %.9g\n", allocations, checksum);
    return allocations == 0 && checksum > 0 ? 0 : 1;
}
