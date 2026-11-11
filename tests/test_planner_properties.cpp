#include <flexmodel/planner.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdio>

namespace fm = flexmodel;

namespace {
constexpr std::size_t actions = 4;
constexpr std::size_t nodes = actions / 2;

// A consteval diamond verifies that the same builder is usable during compilation.
consteval bool compile_time_diamond() {
    fm::detail::action_graph<nodes> graph{};
    graph.exists.fill(true);
    graph.needs[1][0] = true;
    graph.needs[2][0] = true;
    graph.needs[3][1] = true;
    graph.needs[3][2] = true;
    fm::detail::plan_builder<nodes> builder{graph};
    builder.visit(3);
    return builder.result.size == 4 &&
        builder.result.actions == std::array<fm::action, 4>{{
            {0, fm::action_kind::handle}, {1, fm::action_kind::handle},
            {0, fm::action_kind::eval}, {1, fm::action_kind::eval}}};
}
static_assert(compile_time_diamond());

// The oracle uses bit-set reachability and Kahn's indegree removal, rather than
// the production DFS and its gray/black marks. Bits encode consumer -> prerequisite.
struct Oracle {
    unsigned reachable = 0;
    bool cycle = false;
};
Oracle oracle(const std::array<unsigned, actions>& requirements, unsigned roots) {
    unsigned reachable = roots;
    unsigned previous;
    do {
        previous = reachable;
        for (std::size_t consumer = 0; consumer < actions; ++consumer)
            if (reachable & (1U << consumer)) reachable |= requirements[consumer];
    } while (reachable != previous);

    std::array<unsigned, actions> indegree{};
    std::array<std::size_t, actions> ready{};
    std::size_t head = 0, tail = 0;
    for (std::size_t consumer = 0; consumer < actions; ++consumer) {
        indegree[consumer] = std::popcount(requirements[consumer] & reachable);
        if ((reachable & (1U << consumer)) && indegree[consumer] == 0)
            ready[tail++] = consumer;
    }
    while (head < tail) {
        const auto prerequisite = ready[head++];
        for (std::size_t consumer = 0; consumer < actions; ++consumer)
            if ((reachable & (1U << consumer)) &&
                (requirements[consumer] & (1U << prerequisite))) {
                if (--indegree[consumer] == 0) ready[tail++] = consumer;
            }
    }
    return {reachable, head != static_cast<std::size_t>(std::popcount(reachable))};
}

const char* check_valid_plan(const fm::plan<nodes>& plan, unsigned reachable,
                             const std::array<unsigned, actions>& requirements) {
    if (plan.size != static_cast<std::size_t>(std::popcount(reachable))) return "wrong action count";
    unsigned seen = 0;
    std::array<std::size_t, actions> position{};
    for (std::size_t i = 0; i < plan.size; ++i) {
        const auto action = plan.actions[i];
        if (action.node >= nodes) return "invalid node index";
        const auto id = action.node + (action.kind == fm::action_kind::eval ? nodes : 0);
        if (seen & (1U << id)) return "duplicate action";
        seen |= 1U << id;
        position[id] = i;
    }
    if (seen != reachable) return "wrong reachable action set";
    for (std::size_t consumer = 0; consumer < actions; ++consumer)
        if (reachable & (1U << consumer))
            for (std::size_t prerequisite = 0; prerequisite < actions; ++prerequisite)
                if ((requirements[consumer] & (1U << prerequisite)) &&
                    position[prerequisite] >= position[consumer]) return "prerequisite scheduled too late";
    return nullptr;
}
} // namespace

int main() {
    std::size_t cases = 0, acyclic = 0, cyclic = 0, failures = 0;
    for (unsigned edges = 0; edges < (1U << (actions * actions)); ++edges) {
        std::array<unsigned, actions> requirements{};
        fm::detail::action_graph<nodes> graph{};
        graph.exists.fill(true);
        for (std::size_t consumer = 0; consumer < actions; ++consumer) {
            requirements[consumer] = (edges >> (consumer * actions)) & 0xFU;
            for (std::size_t prerequisite = 0; prerequisite < actions; ++prerequisite)
                graph.needs[consumer][prerequisite] = requirements[consumer] & (1U << prerequisite);
        }
        for (unsigned roots = 0; roots < (1U << actions); ++roots) {
            ++cases;
            const auto expected = oracle(requirements, roots);
            fm::detail::plan_builder<nodes> builder{graph};
            bool rejected = false;
            try {
                for (std::size_t root = 0; root < actions; ++root)
                    if (roots & (1U << root)) builder.visit(root);
            } catch (const char*) {
                rejected = true;
            } catch (...) {
                std::printf("Unexpected exception for graph=0x%04x, roots=0x%x\n", edges, roots);
                return 1;
            }
            expected.cycle ? ++cyclic : ++acyclic;
            const char* error = nullptr;
            if (rejected != expected.cycle) error = "cycle decision differs from Kahn oracle";
            else if (!rejected) error = check_valid_plan(builder.result, expected.reachable, requirements);
            if (error) {
                ++failures;
                if (failures <= 10)
                    std::printf("FAIL graph=0x%04x, roots=0x%x: %s\n", edges, roots, error);
            }
        }
    }
    std::printf("Exhaustive planner: %zu cases, %zu acyclic, %zu cyclic, %zu failures\n",
                cases, acyclic, cyclic, failures);
    return cases == 1048576 && failures == 0 ? 0 : 1;
}
