#pragma once

#include <array>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace flexmodel {

template<class... Ports> struct inputs { using ports = std::tuple<Ports...>; };
template<class Id> struct state_of { using node_id = Id; };
template<class Id> struct value_of { using node_id = Id; };

template<class Event, class Inputs = inputs<>, auto Function = nullptr>
struct on {
    using event_type = Event;
    using inputs_type = Inputs;
    static constexpr auto function = Function;
};
template<class Inputs = inputs<>, auto Function = nullptr>
struct eval {
    using inputs_type = Inputs;
    static constexpr auto function = Function;
};
template<class Id, class Kernel, class... Operations>
struct node {
    using id = Id;
    using kernel = Kernel;
    using operations = std::tuple<Operations...>;
};
template<class... Nodes> struct graph { using nodes = std::tuple<Nodes...>; };
template<class... Ids> struct query { using ids = std::tuple<Ids...>; };
template<class... Ids> struct roots { using ids = std::tuple<Ids...>; };
struct all_nodes {};

enum class action_kind { handle, eval };
struct action {
    std::size_t node{};
    action_kind kind{};
    constexpr bool operator==(const action&) const = default;
};
template<std::size_t N> struct plan {
    std::array<action, 2 * N> actions{};
    std::size_t size{};
    constexpr const action* begin() const noexcept { return actions.data(); }
    constexpr const action* end() const noexcept {
        if constexpr (N == 0) return actions.data();
        else return actions.data() + size;
    }
};

namespace detail {

template<std::size_t N, class F>
constexpr void for_each_index(F&& f) {
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        (f(std::integral_constant<std::size_t, I>{}), ...);
    }(std::make_index_sequence<N>{});
}
template<class Tuple, class F>
constexpr void for_each_type(F&& f) {
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        (f.template operator()<std::tuple_element_t<I, Tuple>>(), ...);
    }(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}
template<class T, class Tuple> struct contains;
template<class T, class... Ts>
struct contains<T, std::tuple<Ts...>> : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};
template<class T, class... Ts>
inline constexpr std::size_t type_count_v = (std::size_t{0} + ... + std::size_t(std::is_same_v<T, Ts>));
template<class Tuple> struct unique_types;
template<class... Ts>
struct unique_types<std::tuple<Ts...>> : std::bool_constant<((type_count_v<Ts, Ts...> == 1) && ...)> {};
template<class... Tuples> struct concat;
template<> struct concat<> { using type = std::tuple<>; };
template<class... Ts> struct concat<std::tuple<Ts...>> { using type = std::tuple<Ts...>; };
template<class... A, class... B, class... Rest>
struct concat<std::tuple<A...>, std::tuple<B...>, Rest...>
    : concat<std::tuple<A..., B...>, Rest...> {};
template<class In, class Out = std::tuple<>> struct unique_tuple;
template<class... Out> struct unique_tuple<std::tuple<>, std::tuple<Out...>> {
    using type = std::tuple<Out...>;
};
template<class T, class... Rest, class... Out>
struct unique_tuple<std::tuple<T, Rest...>, std::tuple<Out...>>
    : unique_tuple<std::tuple<Rest...>, std::conditional_t<(std::is_same_v<T, Out> || ...),
                                                        std::tuple<Out...>, std::tuple<Out..., T>>> {};

template<class Op> struct operation_traits {
    static constexpr bool is_handler = false, is_eval = false, valid = false;
    using event_type = void;
    using events = std::tuple<>;
};
template<class E, class In, auto Fn> struct operation_traits<on<E, In, Fn>> {
    static constexpr bool is_handler = true, is_eval = false, valid = true;
    using event_type = E;
    using events = std::tuple<E>;
};
template<class In, auto Fn> struct operation_traits<eval<In, Fn>> {
    static constexpr bool is_handler = false, is_eval = true, valid = true;
    using event_type = void;
    using events = std::tuple<>;
};
template<class Ops> struct eval_in;
template<> struct eval_in<std::tuple<>> { using type = void; };
template<class Op, class... Rest> struct eval_in<std::tuple<Op, Rest...>> {
    using type = std::conditional_t<operation_traits<Op>::is_eval, Op,
                                   typename eval_in<std::tuple<Rest...>>::type>;
};
template<class Ops, class Event> struct handler_in;
template<class Event> struct handler_in<std::tuple<>, Event> { using type = void; };
template<class Op, class... Rest, class Event>
struct handler_in<std::tuple<Op, Rest...>, Event> {
    using type = std::conditional_t<operation_traits<Op>::is_handler &&
                                   std::is_same_v<typename operation_traits<Op>::event_type, Event>,
                                   Op, typename handler_in<std::tuple<Rest...>, Event>::type>;
};
template<class Node> struct eval_for : eval_in<typename Node::operations> {};
template<class Node, class Event> struct handler_for : handler_in<typename Node::operations, Event> {};
template<class Ops> struct events_in;
template<class... Ops> struct events_in<std::tuple<Ops...>> {
    using type = typename concat<typename operation_traits<Ops>::events...>::type;
};
template<class Node, bool HasEval = !std::is_void_v<typename eval_for<Node>::type>>
struct node_value { using type = std::monostate; };
template<class Node> struct node_value<Node, true> { using type = typename Node::kernel::value_type; };

template<class Port> struct port_traits { static constexpr bool valid = false; };
template<class Id> struct port_traits<state_of<Id>> {
    using id = Id;
    static constexpr bool valid = true, is_value = false;
};
template<class Id> struct port_traits<value_of<Id>> {
    using id = Id;
    static constexpr bool valid = true, is_value = true;
};

template<class Graph> struct schema;
template<class... Nodes> struct schema<graph<Nodes...>> {
    using nodes = std::tuple<Nodes...>;
    using ids = std::tuple<typename Nodes::id...>;
    using state_tuple = std::tuple<typename Nodes::kernel::state_type...>;
    using value_tuple = std::tuple<typename node_value<Nodes>::type...>;
    using events = typename unique_tuple<typename concat<typename events_in<typename Nodes::operations>::type...>::type>::type;
    static constexpr std::size_t node_count = sizeof...(Nodes);
    template<std::size_t I> using node_type = std::tuple_element_t<I, nodes>;
    template<std::size_t I> using eval_type = typename eval_for<node_type<I>>::type;
    template<std::size_t I, class E> using handler_type = typename handler_for<node_type<I>, E>::type;
    template<class Id> static consteval std::size_t index() {
        static_assert(contains<Id, ids>::value, "dependency absent: node ID is not in graph");
        constexpr std::array<bool, node_count> matches{std::is_same_v<Id, typename Nodes::id>...};
        for (std::size_t i = 0; i < node_count; ++i) if (matches[i]) return i;
        return 0; // Keep secondary diagnostics bounded after the assertion above.
    }
};

template<class Port, class Graph> struct input_type;
template<class Id, class Graph> struct input_type<state_of<Id>, Graph> {
    using type = std::tuple_element_t<schema<Graph>::template index<Id>(), typename schema<Graph>::state_tuple>;
};
template<class Id, class Graph> struct input_type<value_of<Id>, Graph> {
    using type = std::tuple_element_t<schema<Graph>::template index<Id>(), typename schema<Graph>::value_tuple>;
};
template<class Port, class Graph> using input_type_t = typename input_type<Port, Graph>::type;

template<class Graph> consteval bool validate_metadata() {
    using S = schema<Graph>;
    static_assert(unique_types<typename S::ids>::value, "duplicate node id");
    for_each_type<typename S::nodes>([]<class Node>() {
        using Ops = typename Node::operations;
        constexpr auto eval_count = [] {
            std::size_t count = 0;
            for_each_type<Ops>([&]<class Op>() { count += operation_traits<Op>::is_eval; });
            return count;
        }();
        static_assert(eval_count <= 1, "duplicate eval");
        for_each_type<Ops>([]<class Op>() {
            static_assert(operation_traits<Op>::valid, "invalid operation descriptor");
            if constexpr (operation_traits<Op>::is_handler) {
                using Event = typename Op::event_type;
                static_assert(!std::is_void_v<Event>, "handler event must not be void");
                constexpr auto count = [] {
                    std::size_t n = 0;
                    for_each_type<Ops>([&]<class Other>() {
                        n += operation_traits<Other>::is_handler &&
                             std::is_same_v<Event, typename operation_traits<Other>::event_type>;
                    });
                    return n;
                }();
                static_assert(count == 1, "duplicate handler");
            }
            if constexpr (operation_traits<Op>::valid) {
                for_each_type<typename Op::inputs_type::ports>([]<class Port>() {
                    static_assert(port_traits<Port>::valid, "invalid input port");
                    if constexpr (port_traits<Port>::valid) {
                        using Id = typename port_traits<Port>::id;
                        constexpr auto i = S::template index<Id>();
                        if constexpr (port_traits<Port>::is_value) {
                            static_assert(!std::is_void_v<typename S::template eval_type<i>>,
                                          "value dependency requires eval");
                        }
                    }
                });
            }
        });
    });
    return true;
}

} // namespace detail
} // namespace flexmodel
