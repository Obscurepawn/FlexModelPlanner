#pragma once

#include <cstddef>
#include <functional>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>

namespace flexmodel {

namespace detail {
template<class T> inline constexpr bool is_variant = false;
template<class... Ts> inline constexpr bool is_variant<std::variant<Ts...>> = true;

struct replay_ignore {
    template<class... Ts> constexpr void operator()(Ts&&...) const noexcept {}
};

template<bool Interpreted, class Engine, std::ranges::input_range Range, class Observer>
std::size_t replay_range(Engine& engine, Range&& records, Observer&& observer) {
    std::size_t index = 0;
    for (const auto& record : records) {
        auto dispatch = [&](const auto& event) {
            if constexpr (Interpreted) engine.on_interpreted(event);
            else engine.on(event);
            std::invoke(observer, engine, event, index);
        };
        if constexpr (is_variant<std::remove_cvref_t<decltype(record)>>) {
            std::visit(dispatch, record);
        } else {
            dispatch(record);
        }
        ++index;
    }
    return index;
}
} // namespace detail

// Replay uses input order. The observer sees each concrete event after dispatch;
// it may sample outputs using engine.evaluate<Query>(). No timestamps are sorted.
template<class Engine, std::ranges::input_range Range, class Observer = detail::replay_ignore>
std::size_t replay(Engine& engine, Range&& records, Observer&& observer = {}) {
    return detail::replay_range<false>(engine, std::forward<Range>(records),
                                       std::forward<Observer>(observer));
}

template<class Engine, std::ranges::input_range Range, class Observer = detail::replay_ignore>
std::size_t replay_interpreted(Engine& engine, Range&& records, Observer&& observer = {}) {
    return detail::replay_range<true>(engine, std::forward<Range>(records),
                                      std::forward<Observer>(observer));
}

} // namespace flexmodel
