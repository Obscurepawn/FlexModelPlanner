# FlexModelPlanner Specification

This document specifies the public API, type requirements, compile-time validation rules, scheduling semantics, and runtime guarantees of **FlexModelPlanner** (`namespace flexmodel`, umbrella header `<flexmodel/flexmodel.hpp>`).

---

## 1. Header Synopsis

```cpp
namespace flexmodel {

// Port and graph descriptors (<flexmodel/meta.hpp>)
template<class... Ports> struct inputs;
template<class Id> struct state_of;
template<class Id> struct value_of;

template<class Event, class Inputs = inputs<>, auto Function = nullptr> struct on;
template<class Inputs = inputs<>, auto Function = nullptr> struct eval;

template<class Id, class Kernel, class... Operations> struct node;
template<class... Nodes> struct graph;
template<class... Ids> struct query;
template<class... Ids> struct roots;
struct all_nodes {};

// Static plan representation (<flexmodel/meta.hpp>)
enum class action_kind { handle, eval };

struct action {
    std::size_t node{};
    action_kind kind{};
    constexpr bool operator==(const action&) const = default;
};

template<std::size_t N>
struct plan {
    std::array<action, 2 * N> actions{};
    std::size_t size{};
    constexpr const action* begin() const noexcept;
    constexpr const action* end() const noexcept;
};

// Execution engine (<flexmodel/engine.hpp>)
template<class Graph, class Roots = all_nodes>
class engine {
public:
    using state_tuple = /* std::tuple<typename Nodes::kernel::state_type...> */;
    using value_tuple = /* std::tuple<ValueOrMonostate...> */;
    static constexpr std::size_t node_count = /* sizeof...(Nodes) */;

    engine() requires std::is_default_constructible_v<state_tuple>;
    explicit engine(state_tuple states);

    template<class Event> static consteval auto event_plan();
    template<class Query> static consteval auto query_plan();

    template<class Event> void on(const Event& event) noexcept;
    template<class Event> void on_interpreted(const Event& event) noexcept;

    template<class Query> auto evaluate() noexcept;
    template<class Query> auto evaluate_interpreted() noexcept;
    template<class Query, class OutputTuple> void evaluate_into(OutputTuple& output);

    template<class Id> const auto& state() const noexcept;
    template<class Id> const auto& cached_value() const noexcept;
};

// Plan formatting (<flexmodel/inspect.hpp>)
template<std::size_t N>
void dump_plan(const plan<N>& plan, std::ostream& output,
               std::span<const std::string_view> names = {});

template<class Graph, std::size_t N>
void dump_plan(std::ostream& output, const plan<N>& plan);

// Stream replay (<flexmodel/replay.hpp>)
template<class Engine, std::ranges::input_range Range, class Observer = /* no-op */>
std::size_t replay(Engine& engine, Range&& records, Observer&& observer = {});

template<class Engine, std::ranges::input_range Range, class Observer = /* no-op */>
std::size_t replay_interpreted(Engine& engine, Range&& records, Observer&& observer = {});

} // namespace flexmodel
```

---

## 2. Type and Concept Requirements

### 2.1 Node Identifiers (`Id`)

- Each `node<Id, Kernel, Operations...>` is uniquely identified within a `graph<Nodes...>` by its `Id` type. Multiple nodes may share the same `Kernel` type under distinct `Id` types.
- `Id` may be an incomplete or empty tag type. If `Id::name` is a valid expression convertible to `std::string_view`, `dump_plan<Graph>(output, plan)` uses it as the human-readable node label; otherwise it falls back to `"node#<i>"`.

### 2.2 Kernels (`Kernel`)

- Every `Kernel` must provide a nested type `Kernel::state_type`.
  - `state_type` must be move-constructible when constructing `engine(state_tuple)`.
  - `state_type` is required to be default-constructible only when `engine()`'s default constructor is instantiated.
- If a node declares an `eval<...>` operation, its `Kernel` must also provide `Kernel::value_type`:
  - `std::is_default_constructible_v<value_type>` must be `true`.
  - `std::is_nothrow_assignable_v<value_type&, value_type>` must be `true`.
- Nodes without an `eval<...>` operation do not require `value_type`; their corresponding element in `engine::value_tuple` is `std::monostate`.

### 2.3 Input Ports (`inputs<Ports...>`)

Operations declare read-only dependencies through `inputs<Ports...>`, where each port is one of:

| Port descriptor | Resolved callback parameter type | Precondition |
|---|---|---|
| `state_of<ProducerId>` | `const ProducerKernel::state_type&` | `ProducerId` exists in `Graph` |
| `value_of<ProducerId>` | `const ProducerKernel::value_type&` | `ProducerId` exists in `Graph` and declares `eval<...>` |

Input ports determine callback parameter order left-to-right. Duplicate ports within an `inputs<...>` list pass separate `const&` arguments to the same underlying slot while scheduling the prerequisite action at most once.

### 2.4 Operations and Callbacks (`on`, `eval`)

- **Event handler** (`on<Event, Inputs, Function>`):
  - `Event` must not be `void`. At most one `on<Event, ...>` per `Event` type may appear on a single `node`.
  - When `Function` is `nullptr` (the default), dispatch calls `Kernel::handle(state, event, inputs...)`. Otherwise dispatch invokes `std::invoke(Function, state, event, inputs...)`; if `Function` is a pointer type, it must be non-null.
  - Signature requirement: invocable with `(State&, const Event&, const InputTypes&...)`, returning exact type `void`, and declared `noexcept` (`noexcept(...)` / `std::is_nothrow_invocable_v`).
- **Evaluator** (`eval<Inputs, Function>`):
  - At most one `eval<...>` may appear on a single `node`.
  - When `Function` is `nullptr`, dispatch calls `Kernel::eval(state, inputs...)`. Otherwise dispatch invokes `std::invoke(Function, state, inputs...)`; if `Function` is a pointer type, it must be non-null.
  - Signature requirement: invocable with `(const State&, const InputTypes&...)`, returning exact type `Kernel::value_type` (`std::is_same_v`, without implicit conversion), and declared `noexcept`.

---

## 3. Graph Validation and Static Planning

### 3.1 Eager Compile-Time Validation

Instantiating `engine<Graph, Roots>` validates the entire `Graph` at compile time, including nodes outside `Roots` and event types never dispatched at runtime. Compilation fails with a `static_assert` or constant-evaluation diagnostic on any of the following:

- Duplicate `Id` types in `graph<Nodes...>`.
- Any `node` operation other than `on<...>` or `eval<...>`, or `on<void, ...>`.
- Duplicate `on<Event, ...>` for the same `Event` on a node, or more than one `eval<...>` on a node.
- Any port other than `state_of<Id>` or `value_of<Id>`, an unknown `Id` not in `Graph`, or `value_of<Id>` targeting a node without `eval<...>`.
- Null callback pointer, mismatched parameter types (including non-`const` state in `eval`, non-`const` event, or non-`const` inputs), wrong return type, or non-`noexcept` callback.
- Any cycle in a declared event's action dependency graph or in the evaluator query dependency graph.

### 3.2 Root Selection and Live Closure (`Roots`)

- `all_nodes` marks all `N` nodes in `Graph` as live.
- `roots<Ids...>` marks each listed `Id` live (repeated IDs are idempotent; unknown IDs are ill-formed) and closes under structural reachability across **all** declared handler and evaluator input ports of live nodes.
- Only live nodes are scheduled in `event_plan<Event>()` and `query_plan<Query>()`. Inactive nodes remain present in `state_tuple` and `value_tuple` to keep type layout independent of root selection.

### 3.3 Action Graph and Topological Ordering

For a graph of `N` nodes, each plan operates over at most `2N` actions: `handle(i)` (`i ∈ [0, N)`) and `eval(i)` (`i ∈ [0, N)`).

1. **Event plan (`event_plan<Event>()`)**:
   - `handle(i)` exists iff node `i` declares `on<Event, ...>`. `eval(i)` exists iff node `i` declares `eval<...>`.
   - If action `C` reads `state_of<P>` and `handle(P)` exists for `Event`, `C` depends on `handle(P)`.
   - If action `C` reads `value_of<P>`, `C` depends on `eval(P)`, and additionally depends on `handle(P)` if `handle(P)` exists for `Event`.
   - If both `eval(i)` and `handle(i)` exist for `Event`, `eval(i)` depends on `handle(i)` so an evaluator always observes its own node's updated state for the current event.
   - Roots of the traversal are all `handle(i)` where node `i` is live (`i = 0 .. N-1`). Consequently, handlers and evaluators may interleave within an event plan, and evaluators not needed by any live handler for `Event` do not run.
   - Unregistered `Event` types (no node declares `on<Event, ...>`) are valid and yield an empty `plan<N>` (`size == 0`).

2. **Query plan (`query_plan<query<Ids...>>()`)**:
   - `Ids...` must be unique, live under `Roots`, and each target node must declare `eval<...>`.
   - No `handle(i)` actions exist in a query plan. `eval(C)` depends on `eval(P)` for every `value_of<P>` in `C`'s evaluator inputs (`state_of<P>` reads current state directly with no action prerequisite).
   - Roots of the traversal are the requested evaluators visited in ascending graph node index `0 .. N-1` (independent of the tuple order in `query<Ids...>`).

3. **Deterministic DFS**:
   - Plans are emitted in post-order depth-first traversal: prerequisites run before dependents, and prerequisites of an action are scanned in index order `handle(0 .. N-1)` followed by `eval(0 .. N-1)`.
   - Each action executes **at most once** per plan.
   - Cycles across the union of different events are permitted as long as every individual `Event`'s action graph and the evaluator query graph are acyclic.

### 3.4 Event-History Preservation

- Every `on(event)` call executes `handle(i)` once for every live node `i` subscribing to `Event`, even if consecutive events carry identical payloads or no query is issued between events.
- Every `evaluate<Query>()` call executes all `eval` actions in `query_plan<Query>()` without mutating node states or advancing event history.
- The framework performs **no** implicit value-equality cutoff, event coalescing, cross-event memoization, or cross-query caching.

---

## 4. Runtime Execution, Lifetimes, and Replay

### 4.1 Instance Ownership and Movability

- `engine<Graph, Roots>` stores `state_tuple` and `value_tuple` directly as member subobjects. Default construction value-initializes both tuples; `engine(state_tuple)` move-initializes `states_` and value-initializes `values_`.
- Plans (`plan<N>`) contain only integer node indices and `action_kind` tags—never pointers or references to an `engine` instance. Moving an `engine` transfers its `states_` and `values_` according to their move constructors and never invalidates static plans.

### 4.2 Queries, State Inspection, and Reference Lifetimes

- `evaluate<query<Ids...>>()` executes `query_plan<query<Ids...>>()` once and returns `std::tuple<const Value&...>` ordered according to `Ids...`.
  - Returned references borrow directly from `engine`'s internal `values_` slots. They remain valid until the `engine` is moved or destroyed, and their pointees may be overwritten by any subsequent `on(...)` (if an event plan evaluates that node) or `evaluate<...>()` call.
- `evaluate_into<Query>(OutputTuple& out)` evaluates `Query` and assigns the resulting tuple into `out`. Unlike `evaluate<Query>()`, `evaluate_into` may throw or allocate if `OutputTuple`'s assignment operator does so.
- `state<Id>() const noexcept` returns a read-only `const State&` to `Id`'s state slot.
- `cached_value<Id>() const noexcept` (requires `Id` to have `eval<...>`) returns a read-only `const Value&` to `Id`'s output slot without running `eval`. Prior to the first evaluation of `Id`, this slot holds a value-initialized `value_type`; after subsequent events, it may hold a stale value until `Id` is evaluated again.

### 4.3 Static vs. Interpreted Dispatch

- `on(event)` and `evaluate<Query>()` unroll `plan<N>` at compile time via `std::index_sequence` fold expressions into direct static calls.
- `on_interpreted(event)` and `evaluate_interpreted<Query>()` step through the exact same `plan<N>` array at runtime via a `constexpr` table of generated function pointers. They exist as differential-testing baselines for code generation, not as independent planning oracles.

### 4.4 Memory, Exception, and Thread-Safety Guarantees

- **Hot-path allocation**: Plan construction happens at compile time. `on(...)` and `evaluate<Query>()` perform zero dynamic memory allocations inside the framework; end-to-end zero allocation holds whenever user `Kernel` callbacks and `value_type` move-assignments do not allocate.
- **Exception safety**: `on`, `on_interpreted`, `evaluate`, `evaluate_interpreted`, `state`, and `cached_value` are all `noexcept`.
- **Concurrency**: An individual `engine` instance is single-threaded and non-reentrant (callbacks must not invoke methods on the owning `engine`). Distinct `engine` instances share only immutable `constexpr` metadata and may execute concurrently on separate threads without synchronization.

### 4.5 Replay and Plan Inspection

- `replay(engine, records, observer)` (and `replay_interpreted`) iterates `records` (`std::ranges::input_range`) once in input order without timestamp sorting:
  - If the range element type is a `std::variant<...>`, the active alternative is unwrapped with `std::visit`; otherwise the element is dispatched directly.
  - For each element at zero-based position `index`, it calls `engine.on(event)` (or `engine.on_interpreted(event)`) followed by `std::invoke(observer, engine, event, index)`.
  - Returns the total number of processed elements (`std::size_t`).
- `dump_plan(plan, output, names)` writes one line per scheduled action in `[0, plan.size)` formatted as `"<i>: <name>.<handle|eval>\n"`, using `names[action.node]` when non-empty and in range, or `"node#<action.node>"` otherwise. Formatting writes to `std::ostream` and is intended for diagnostics outside latency-critical paths.
