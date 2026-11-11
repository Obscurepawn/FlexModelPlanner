#pragma once

#include "meta.hpp"

#include <array>
#include <cstddef>
#include <ostream>
#include <span>
#include <string_view>

namespace flexmodel {

// Diagnostic formatting is deliberately outside dispatch and may allocate in
// the supplied stream. Missing names fall back to stable graph node indices.
template<std::size_t N>
void dump_plan(const plan<N>& plan, std::ostream& output,
               std::span<const std::string_view> names = {}) {
    for (std::size_t i = 0; i < plan.size; ++i) {
        const auto& action = plan.actions[i];
        const std::size_t node_index = action.node;
        output << i << ": ";
        if (node_index < names.size() && !names[node_index].empty()) output << names[node_index];
        else output << "node#" << node_index;
        output << (action.kind == action_kind::handle ? ".handle\n" : ".eval\n");
    }
}

// Graph-aware convenience overload: an ID may expose static constexpr name.
template<class Graph, std::size_t N>
void dump_plan(std::ostream& output, const plan<N>& plan) {
    using Schema = detail::schema<Graph>;
    static_assert(N == Schema::node_count, "plan graph size mismatch");
    std::array<std::string_view, N> names{};
    detail::for_each_index<N>([&](auto index) {
        using Id = typename Schema::template node_type<index.value>::id;
        if constexpr (requires { std::string_view{Id::name}; }) names[index] = std::string_view{Id::name};
    });
    dump_plan(plan, output, names);
}

} // namespace flexmodel
