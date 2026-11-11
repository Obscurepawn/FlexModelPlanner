#pragma once
#include "meta.hpp"

namespace flexmodel::detail {

// An evaluator query has no event handler roots or producer updates.
struct query_event {};

// A named non-constexpr failure path keeps cycle diagnostics identifiable on
// compilers that omit thrown string literals from constant-evaluation errors.
[[noreturn]] inline void cyclic_action_dependency() {
    throw "cyclic action dependency";
}

template<std::size_t N> struct action_graph {
    static constexpr std::size_t capacity = 2 * N;
    // Handles are [0,N); evaluators are [N,2N).
    std::array<bool, capacity> exists{};
    std::array<std::array<bool, capacity>, capacity> needs{};
};

// Value-level DFS: templates build metadata and emit calls; ordinary arrays and
// loops do graph traversal. A gray action denotes a real cycle in this plan.
template<std::size_t N> struct plan_builder {
    const action_graph<N>& graph;
    std::array<unsigned char, 2 * N> marks{};
    plan<N> result{};

    constexpr void visit(std::size_t a) {
        if (marks[a] == 2) return;
        if (marks[a] == 1) cyclic_action_dependency();
        marks[a] = 1;
        for (std::size_t dep = 0; dep < 2 * N; ++dep)
            if (graph.needs[a][dep]) visit(dep);
        marks[a] = 2;
        result.actions[result.size++] = action{a < N ? a : a - N,
                                              a < N ? action_kind::handle : action_kind::eval};
    }
};

template<class Graph, class Roots = all_nodes> struct planner {
    using S = schema<Graph>;
    static constexpr std::size_t node_count = S::node_count;
    static constexpr std::size_t N = node_count;

    static consteval auto all_active() {
        std::array<bool, N> a{};
        a.fill(true);
        return a;
    }

    static consteval auto live_nodes() {
        static_assert(validate_metadata<Graph>());
        if constexpr (std::is_same_v<Roots, all_nodes>) return all_active();
        else {
            std::array<bool, N> live{};
            for_each_type<typename Roots::ids>([&]<class Id>() { live[S::template index<Id>()] = true; });
            std::array<std::array<bool, N>, N> depends{};
            for_each_index<N>([&](auto index) {
                constexpr auto i = decltype(index)::value;
                using Node = typename S::template node_type<i>;
                for_each_type<typename Node::operations>([&]<class Op>() {
                    for_each_type<typename Op::inputs_type::ports>([&]<class Port>() {
                        depends[i][S::template index<typename Port::node_id>()] = true;
                    });
                });
            });
            // Structural reachability includes every handler's inputs, even if
            // that event is not the event currently being planned.
            for (std::size_t pass = 0; pass < N; ++pass)
                for (std::size_t i = 0; i < N; ++i)
                    if (live[i])
                        for (std::size_t j = 0; j < N; ++j)
                            if (depends[i][j]) live[j] = true;
            return live;
        }
    }
    static constexpr auto active = live_nodes();

    template<class Event> static consteval auto dependency_graph() {
        action_graph<N> g;
        for_each_index<N>([&](auto index) {
            constexpr auto i = decltype(index)::value;
            using Eval = typename S::template eval_type<i>;
            using Handler = typename S::template handler_type<i, Event>;
            if constexpr (!std::is_same_v<Event, query_event> && !std::is_void_v<Handler>) g.exists[i] = true;
            if constexpr (!std::is_void_v<Eval>) g.exists[N + i] = true;
        });
        for_each_index<N>([&](auto index) {
            constexpr auto i = decltype(index)::value;
            using Eval = typename S::template eval_type<i>;
            using Handler = typename S::template handler_type<i, Event>;
            // Every declared read is a read after the producer's matching
            // handler. Value reads also need its evaluator; state reads do not.
            auto add_inputs = [&]<class Op>(std::size_t consumer) {
                for_each_type<typename Op::inputs_type::ports>([&]<class Port>() {
                    constexpr auto producer = S::template index<typename Port::node_id>();
                    if (g.exists[producer]) g.needs[consumer][producer] = true;
                    if constexpr (port_traits<Port>::is_value) g.needs[consumer][N + producer] = true;
                });
            };
            if constexpr (!std::is_same_v<Event, query_event> && !std::is_void_v<Handler>)
                add_inputs.template operator()<Handler>(i);
            if constexpr (!std::is_void_v<Eval>) {
                if (g.exists[i]) g.needs[N + i][i] = true;
                add_inputs.template operator()<Eval>(N + i);
            }
        });
        return g;
    }

    template<class Event>
    static consteval auto build_event(const std::array<bool, N>& live) {
        const auto g = dependency_graph<Event>();
        plan_builder<N> builder{g};
        for (std::size_t i = 0; i < N; ++i)
            if (live[i] && g.exists[i]) builder.visit(i);
        return builder.result;
    }

    static consteval bool validate_all() {
        static_assert(validate_metadata<Graph>());
        // Validate declared event plans even when never called, and regardless
        // of a particular instance's selected roots.
        for_each_type<typename S::events>([]<class E>() {
            const auto checked = build_event<E>(all_active());
            (void)checked;
        });
        const auto g = dependency_graph<query_event>();
        plan_builder<N> builder{g};
        for (std::size_t i = 0; i < N; ++i)
            if (g.exists[N + i]) builder.visit(N + i);
        return true;
    }
    static constexpr bool validated = validate_all();

    template<class Event> static consteval auto event() {
        static_assert(validated);
        return build_event<Event>(active);
    }

    template<class Query> static consteval auto query_plan() {
        static_assert(validated);
        static_assert(unique_types<typename Query::ids>::value, "duplicate query output");
        const auto g = dependency_graph<query_event>();
        plan_builder<N> builder{g};
        std::array<bool, N> requested{};
        for_each_type<typename Query::ids>([&]<class Id>() {
            constexpr auto i = S::template index<Id>();
            static_assert(active[i], "query outside live graph");
            static_assert(!std::is_void_v<typename S::template eval_type<i>>, "query requires eval");
            requested[i] = true;
        });
        // Scheduling order is independent of the requested output tuple order.
        for (std::size_t i = 0; i < N; ++i)
            if (requested[i]) builder.visit(N + i);
        return builder.result;
    }
};

} // namespace flexmodel::detail
