#pragma once

#include "planner.hpp"

#include <array>
#include <concepts>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace flexmodel {

namespace detail {
template<class Op>
inline constexpr bool uses_default_callback =
    std::is_same_v<std::remove_cv_t<decltype(Op::function)>, std::nullptr_t>;

template<class Kernel, class Op, class... Args>
decltype(auto) call_handler(Args&&... args) noexcept {
    if constexpr (uses_default_callback<Op>) {
        return Kernel::handle(std::forward<Args>(args)...);
    } else {
        return std::invoke(Op::function, std::forward<Args>(args)...);
    }
}

template<class Kernel, class Op, class... Args>
decltype(auto) call_eval(Args&&... args) noexcept {
    if constexpr (uses_default_callback<Op>) {
        return Kernel::eval(std::forward<Args>(args)...);
    } else {
        return std::invoke(Op::function, std::forward<Args>(args)...);
    }
}

template<class Graph, class Node, class Op, class... Ports>
consteval void validate_callback(std::tuple<Ports...>*) {
    using Kernel = typename Node::kernel;
    using State = typename Kernel::state_type;
    if constexpr (!uses_default_callback<Op> && std::is_pointer_v<decltype(Op::function)>) {
        static_assert(Op::function != nullptr, "callback pointer must not be null");
    }
    if constexpr (operation_traits<Op>::is_handler) {
        using Event = typename Op::event_type;
        if constexpr (uses_default_callback<Op>) {
            constexpr bool callable = requires(State& state, const Event& event,
                                               const input_type_t<Ports, Graph>&... values) {
                Kernel::handle(state, event, values...);
            };
            static_assert(callable, "handler callback signature mismatch");
            if constexpr (callable) {
                static_assert(std::is_same_v<void, decltype(Kernel::handle(
                    std::declval<State&>(), std::declval<const Event&>(),
                    std::declval<const input_type_t<Ports, Graph>&>()...))>,
                    "handler callback must return void");
                static_assert(noexcept(Kernel::handle(std::declval<State&>(),
                    std::declval<const Event&>(), std::declval<const input_type_t<Ports, Graph>&>()...)),
                    "handler callback must be noexcept");
            }
        } else {
            constexpr bool callable = std::is_invocable_v<decltype(Op::function), State&,
                const Event&, const input_type_t<Ports, Graph>&...>;
            static_assert(callable, "handler callback signature mismatch");
            if constexpr (callable) {
                static_assert(std::is_same_v<void, std::invoke_result_t<decltype(Op::function),
                    State&, const Event&, const input_type_t<Ports, Graph>&...>>,
                    "handler callback must return void");
                static_assert(std::is_nothrow_invocable_v<decltype(Op::function), State&,
                    const Event&, const input_type_t<Ports, Graph>&...>, "handler callback must be noexcept");
            }
        }
    } else if constexpr (operation_traits<Op>::is_eval) {
        using Value = typename Kernel::value_type;
        static_assert(std::is_default_constructible_v<Value>, "value_type must be default constructible");
        static_assert(std::is_nothrow_assignable_v<Value&, Value>, "value_type assignment must be noexcept");
        if constexpr (uses_default_callback<Op>) {
            constexpr bool callable = requires(const State& state,
                                               const input_type_t<Ports, Graph>&... values) {
                Kernel::eval(state, values...);
            };
            static_assert(callable, "eval callback signature mismatch");
            if constexpr (callable) {
                static_assert(std::is_same_v<Value, decltype(Kernel::eval(
                    std::declval<const State&>(), std::declval<const input_type_t<Ports, Graph>&>()...))>,
                    "eval callback must return exact value_type");
                static_assert(noexcept(Kernel::eval(std::declval<const State&>(),
                    std::declval<const input_type_t<Ports, Graph>&>()...)), "eval callback must be noexcept");
            }
        } else {
            constexpr bool callable = std::is_invocable_v<decltype(Op::function), const State&,
                const input_type_t<Ports, Graph>&...>;
            static_assert(callable, "eval callback signature mismatch");
            if constexpr (callable) {
                static_assert(std::is_same_v<Value, std::invoke_result_t<decltype(Op::function),
                    const State&, const input_type_t<Ports, Graph>&...>>,
                    "eval callback must return exact value_type");
                static_assert(std::is_nothrow_invocable_v<decltype(Op::function), const State&,
                    const input_type_t<Ports, Graph>&...>, "eval callback must be noexcept");
            }
        }
    }
}

template<class Graph>
consteval bool validate_callbacks() {
    for_each_type<typename schema<Graph>::nodes>([]<class Node>() {
        for_each_type<typename Node::operations>([]<class Op>() {
            if constexpr (operation_traits<Op>::valid) {
                validate_callback<Graph, Node, Op>(static_cast<typename Op::inputs_type::ports*>(nullptr));
            }
        });
    });
    return true;
}
} // namespace detail

template<class Graph, class Roots = all_nodes>
class engine {
    using schema_type = detail::schema<Graph>;
    using planner_type = detail::planner<Graph, Roots>;
    static_assert(planner_type::validated);
    static_assert(detail::validate_callbacks<Graph>());

public:
    using state_tuple = typename schema_type::state_tuple;
    using value_tuple = typename schema_type::value_tuple;
    static constexpr std::size_t node_count = schema_type::node_count;

private:
    state_tuple states_;
    value_tuple values_{};

    template<class Port>
    const auto& input() const noexcept {
        constexpr auto i = schema_type::template index<typename Port::node_id>();
        if constexpr (detail::port_traits<Port>::is_value) return std::get<i>(values_);
        else return std::get<i>(states_);
    }

    template<std::size_t I, class Op, class Event, class... Ports>
    void handle(const Event& event, std::tuple<Ports...>*) noexcept {
        using Kernel = typename schema_type::template node_type<I>::kernel;
        detail::call_handler<Kernel, Op>(std::get<I>(states_), event, input<Ports>()...);
    }

    template<std::size_t I, class Op, class... Ports>
    void eval(std::tuple<Ports...>*) noexcept {
        using Kernel = typename schema_type::template node_type<I>::kernel;
        const auto& state = std::get<I>(states_);
        std::get<I>(values_) = detail::call_eval<Kernel, Op>(state, input<Ports>()...);
    }

    template<std::size_t I>
    void eval_node() noexcept {
        using Op = typename schema_type::template eval_type<I>;
        if constexpr (!std::is_void_v<Op>) eval<I, Op>(static_cast<typename Op::inputs_type::ports*>(nullptr));
    }

    template<action Action, class Event>
    void perform(const Event& event) noexcept {
        if constexpr (Action.kind == action_kind::handle) {
            using Op = typename schema_type::template handler_type<Action.node, Event>;
            handle<Action.node, Op>(event, static_cast<typename Op::inputs_type::ports*>(nullptr));
        } else {
            eval_node<Action.node>();
        }
    }

    template<class Event, std::size_t... I>
    void dispatch(const Event& event, std::index_sequence<I...>) noexcept {
        [[maybe_unused]] constexpr auto plan = event_plan<Event>();
        (perform<plan.actions[I]>(event), ...);
    }

    template<class Query, std::size_t... I>
    void compute(std::index_sequence<I...>) noexcept {
        [[maybe_unused]] constexpr auto plan = query_plan<Query>();
        (eval_node<plan.actions[I].node>(), ...);
    }

    template<std::size_t I, class Event>
    static void interpreted_action(engine& instance, const Event& event, action_kind kind) noexcept {
        if (kind == action_kind::handle) {
            using Op = typename schema_type::template handler_type<I, Event>;
            if constexpr (!std::is_void_v<Op>) {
                instance.template handle<I, Op>(event, static_cast<typename Op::inputs_type::ports*>(nullptr));
            }
        } else {
            instance.template eval_node<I>();
        }
    }

    template<class... Ids>
    auto outputs(query<Ids...>*) const noexcept {
        return std::forward_as_tuple(cached_value<Ids>()...);
    }

public:
    engine() requires std::is_default_constructible_v<state_tuple> : states_{} {}
    explicit engine(state_tuple states) : states_(std::move(states)) {}

    template<class Event>
    static consteval auto event_plan() { return planner_type::template event<Event>(); }

    template<class Query>
    static consteval auto query_plan() { return planner_type::template query_plan<Query>(); }

    template<class Event>
    void on(const Event& event) noexcept {
        constexpr auto plan = event_plan<Event>();
        dispatch(event, std::make_index_sequence<plan.size>{});
    }

    template<class Event>
    void on_interpreted(const Event& event) noexcept {
        constexpr auto plan = event_plan<Event>();
        using Function = void (*)(engine&, const Event&, action_kind) noexcept;
        static constexpr auto functions = []<std::size_t... I>(std::index_sequence<I...>) {
            return std::array<Function, node_count>{&interpreted_action<I, Event>...};
        }(std::make_index_sequence<node_count>{});
        for (const auto& action : plan) functions[action.node](*this, event, action.kind);
    }

    template<class Query>
    auto evaluate() noexcept {
        constexpr auto plan = query_plan<Query>();
        compute<Query>(std::make_index_sequence<plan.size>{});
        return outputs(static_cast<Query*>(nullptr));
    }

    template<class Query>
    auto evaluate_interpreted() noexcept {
        constexpr auto plan = query_plan<Query>();
        using Function = void (*)(engine&) noexcept;
        static constexpr auto functions = []<std::size_t... I>(std::index_sequence<I...>) {
            return std::array<Function, node_count>{
                +[](engine& instance) noexcept { instance.template eval_node<I>(); }...};
        }(std::make_index_sequence<node_count>{});
        for (const auto& action : plan) functions[action.node](*this);
        return outputs(static_cast<Query*>(nullptr));
    }

    template<class Query, class OutputTuple>
    void evaluate_into(OutputTuple& output) { output = evaluate<Query>(); }

    template<class Id>
    const auto& state() const noexcept {
        return std::get<schema_type::template index<Id>()>(states_);
    }

    template<class Id>
    const auto& cached_value() const noexcept {
        constexpr auto i = schema_type::template index<Id>();
        static_assert(!std::is_void_v<typename schema_type::template eval_type<i>>,
                      "cached value requires eval");
        return std::get<i>(values_);
    }
};

} // namespace flexmodel
