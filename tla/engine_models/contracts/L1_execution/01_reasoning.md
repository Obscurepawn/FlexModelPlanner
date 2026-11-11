# L1 modeling decisions

The production execution boundary is one callback. Calls within each event/query execute in the fixed source-derived plan, and the next caller record cannot start until all actions and output collection finish. This follows the ordinary C++ fold-expression call sequence and the shipped sequential callers. The model introduces no interleavings between callbacks of different operations on one instance.

The concrete plans are book = `[book.handle]`; trade = `[mid.eval, recentA.handle, recentB.handle]`; reset = `[recentA.reset, recentB.reset]`; query = the requested recent-feature evaluators in graph declaration order. `outputs` preserves the caller's requested order separately. These plans are read from the graph declarations, not independently replanned in this level; L0 and the exhaustive C++ Kahn-oracle test address generic planning.

Each instance owns a book, potentially stale midpoint cache, two rings, and two output slots. Ring positions are modeled as 1-based indices; C++ positions are 0-based. The update `next % capacity + 1` is the corresponding index translation. Reset leaves ring cells untouched and only resets position, count and sum, exactly as the example does. Cached midpoint and output slots are also intentionally not invalidated by reset or unrelated events.

The independent oracle is a chronological list of trade-minus-book-mid samples since reset. It is updated when a caller record starts, using the current book rather than the cached midpoint. The production ring is updated later using the cached midpoint. Consequently a wrong ordering or stale-mid read changes actual data and violates the oracle comparison, not merely an epoch label. At operation boundaries the ring count, running sum, insertion position and retained cells must match the oracle. During a callback sequence the oracle is allowed to be ahead until the full event completes.

Queries have a history snapshot and must preserve it through every evaluator action. At completion, selected output ratios must equal the independently recomputed oracle ratios. Ghost per-action counters check exact once-per-plan execution; equal-price trades are never merged. Live query outputs are retained as copied values. Replay consumes the completed log in order from a distinct initial engine, checking query-prefix equality and final complete engine equality.

## Explicit abstractions and limits

- Symmetric integer quotes preserve the midpoint-read ordering and ring-history concerns. They do not test non-symmetric quote arithmetic.
- Values are exact rational numerator/denominator pairs. This preserves the mathematical average and distinguishes stale or missing samples, but excludes IEEE floating-point rounding, overflow, NaN and infinity. This level is not a floating-point correctness claim.
- Two windows exercise same-kernel/different-ID state isolation. Sizes are actual supported runtime parameters, reduced to 1/2 and 2/3. Maximum record counts 5 and 6 permit quote, enough repeated trades to evict both windows, and a following query.
- Inputs are bounded to prices `{0,1}` and bounded record sequences. Both repeated and changed values occur, and negative trade-minus-mid samples remain possible. Passing finite configurations is bounded evidence, not a proof for every stream length or graph size.
- Generated and interpreted C++ paths both consume the same plan. This level models their common sequential action semantics, not instruction selection, optimizer behavior, pointer binding or allocation freedom. C++ tests cover their actual executions and allocation hooks.
- CSV parsing is outside this level. The model starts with successfully parsed typed records and respects the preceding-book guard. The Python test driver invoked by `make test` validates malformed-input rejection separately.
- Allocation, initialization exceptions, borrowed-reference misuse, C++ lifetime/aliasing, arbitrary custom kernels, thread misuse and external global effects are outside the model. No unverified transition is treated as production behavior.

Source surface denominator: 11 transition families (caller choose/end; replay start; book update; midpoint eval; history trade; history reset; history eval; record completion/output capture; replay completion; terminal stutter), with trade/reset/eval parameterized over the two node instances. All 11 are represented; this is design-surface coverage, not binary line coverage. The variable/action source map is at the top of `02_models/L1_Execution.tla`.
