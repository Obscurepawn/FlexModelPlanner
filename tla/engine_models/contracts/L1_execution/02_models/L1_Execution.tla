---------------------------- MODULE L1_Execution ----------------------------
EXTENDS Integers, Sequences, FiniteSets, TLC

\* ============================ var <-> code map ============================
\* MODE: faithful sequential transcription; no fault injection or concurrency.
\* WindowA/WindowB: runtime window arguments, examples/market_kernels.hpp:74-77.
\* Prices/MaxRecords: explicit bounded caller inputs, examples/csv_replay.cpp:42-50.
\* e[i]: independent engine instance state/value slots, include/flexmodel/engine.hpp:130-151.
\*   book/hasBook: symmetric quote mid and caller quote guard,
\*     examples/market_kernels.hpp:19-22, examples/csv_replay.cpp:72-76.
\*   mid: potentially stale value slot until MidKernel.eval,
\*     examples/market_kernels.hpp:28-29.
\*   windows[k].ring/next/count/total: ring storage, insertion position (model 1-based),
\*     valid sample count, running sum, examples/market_kernels.hpp:33-49.
\*   values[k]: evaluator output slot as exact rational numerator/denominator,
\*     examples/market_kernels.hpp:56-57, include/flexmodel/engine.hpp:148-151.
\* mode/phase/cmd/p/pc/calls: active instance and sequential plan dispatch control;
\*     pc denotes NEXT action, calls are ghost execution counters,
\*     include/flexmodel/engine.hpp:160-179, include/flexmodel/engine.hpp:209-240.
\* log/replayIndex/processed: input-order record sequence, next replay record,
\*     and completed-record counts, include/flexmodel/replay.hpp:22-35.
\* outputs[i]: copied query snapshots in requested output order,
\*     include/flexmodel/engine.hpp:194-196, examples/csv_replay.cpp:68-70.
\* samples[i]: GHOST chronological trade differences since last reset, updated
\*     from record + current book, independently of cached mid/ring implementation;
\*     requirement witnessed by examples/market_kernels.hpp:43-57.
\* baseline: GHOST state snapshot for query non-mutation,
\*     include/flexmodel/engine.hpp:148-151, include/flexmodel/engine.hpp:225-240.
\* ACTIONS (all source anchors verified):
\* Choose/StopLive: sequential caller records/end, examples/csv_replay.cpp:60-78,83-88.
\* ReplayStart/FinishRecord: input-order replay, include/flexmodel/replay.hpp:22-35.
\* BookHandle: examples/market_kernels.hpp:20-22.
\* MidEval: examples/market_kernels.hpp:28-29.
\* TradeOne/TradeTwo: examples/market_kernels.hpp:43-49.
\* ResetOne/ResetTwo: examples/market_kernels.hpp:51-54.
\* EvalOne/EvalTwo: examples/market_kernels.hpp:56-57, include/flexmodel/engine.hpp:151.
\* FinishReplay/Halt: finite input termination, include/flexmodel/replay.hpp:34-36.
\* Fixed plans follow actual Graph declarations, examples/market_kernels.hpp:61-72.
\* Abstractions: symmetric integer quotes, rational outputs, bounded records,
\*   callback-atomic actions under single-thread/non-reentrant contract. No IEEE proof.
\* =========================================================================

CONSTANTS WindowA, WindowB, Prices, MaxRecords
ASSUME /\ WindowA \in Nat \ {0}
       /\ WindowB \in Nat \ {0}
       /\ Prices \subseteq Int
       /\ Prices # {}
       /\ MaxRecords \in Nat \ {0}

Instances == {"live", "replay"}
Windows == 1..2
Capacity(k) == IF k = 1 THEN WindowA ELSE WindowB
ActionNames == {"book", "mid", "trade1", "trade2", "reset1", "reset2", "eval1", "eval2"}
QueryChoices == {<<1>>, <<2, 1>>}
Records == {[kind |-> "book", price |-> q, select |-> <<>>] : q \in Prices}
           \cup {[kind |-> "trade", price |-> q, select |-> <<>>] : q \in Prices}
           \cup {[kind |-> "reset", price |-> 0, select |-> <<>>]}
           \cup {[kind |-> "query", price |-> 0, select |-> q] : q \in QueryChoices}
Elements(s) == {s[j] : j \in 1..Len(s)}
Plan(r) == CASE r.kind = "book" -> <<"book">>
            [] r.kind = "trade" -> <<"mid", "trade1", "trade2">>
            [] r.kind = "reset" -> <<"reset1", "reset2">>
            [] OTHER -> (IF 1 \in Elements(r.select) THEN <<"eval1">> ELSE <<>>)
                        \o (IF 2 \in Elements(r.select) THEN <<"eval2">> ELSE <<>>)
Ratio(n, d) == [num |-> n, den |-> IF d = 0 THEN 1 ELSE d]
EqualRatio(a, b) == a.num * b.den = b.num * a.den
InitialWindow(k) == [ring |-> [j \in 1..Capacity(k) |-> 0], next |-> 1,
                     count |-> 0, total |-> 0]
InitialEngine == [book |-> 0, hasBook |-> FALSE, mid |-> 0,
                  windows |-> [k \in Windows |-> InitialWindow(k)],
                  values |-> [k \in Windows |-> Ratio(0, 0)]]
History(state) == [book |-> state.book, hasBook |-> state.hasBook, windows |-> state.windows]
RECURSIVE Sum(_)
Sum(s) == IF Len(s) = 0 THEN 0 ELSE Head(s) + Sum(Tail(s))
Recent(s, k) == IF Len(s) <= Capacity(k) THEN s
                ELSE SubSeq(s, Len(s) - Capacity(k) + 1, Len(s))

VARIABLES e, mode, phase, cmd, p, pc, calls, log, replayIndex,
          processed, outputs, samples, baseline
vars == <<e, mode, phase, cmd, p, pc, calls, log, replayIndex,
          processed, outputs, samples, baseline>>

Init == /\ e = [i \in Instances |-> InitialEngine]
        /\ mode = "live"
        /\ phase = "choose"
        /\ cmd = [kind |-> "reset", price |-> 0, select |-> <<>>]
        /\ p = <<>>
        /\ pc = 1
        /\ calls = [a \in ActionNames |-> 0]
        /\ log = <<>>
        /\ replayIndex = 1
        /\ processed = [i \in Instances |-> 0]
        /\ outputs = [i \in Instances |-> <<>>]
        /\ samples = [i \in Instances |-> <<>>]
        /\ baseline = History(InitialEngine)

\* Environment setup updates the independent expected history before callbacks.
\* Production state is changed only by the corresponding callback action below.
SetUp(i, r) == /\ cmd' = r
               /\ p' = Plan(r)
               /\ pc' = 1
               /\ calls' = [a \in ActionNames |-> 0]
               /\ samples' = [samples EXCEPT ![i] =
                    CASE r.kind = "trade" -> Append(@, r.price - e[i].book)
                      [] r.kind = "reset" -> <<>>
                      [] OTHER -> @]
               /\ baseline' = History(e[i])
               /\ phase' = "execute"

Choose == /\ mode = "live" /\ phase = "choose" /\ Len(log) < MaxRecords
          /\ \E r \in Records :
               /\ (r.kind # "trade" \/ e["live"].hasBook)
               /\ log' = Append(log, r)
               /\ SetUp("live", r)
          /\ UNCHANGED <<e, mode, replayIndex, processed, outputs>>

StopLive == /\ mode = "live" /\ phase = "choose"
            /\ mode' = "replay" /\ phase' = "replayready" /\ replayIndex' = 1
            /\ UNCHANGED <<e, cmd, p, pc, calls, log, processed, outputs, samples, baseline>>

ReplayStart == /\ mode = "replay" /\ phase = "replayready" /\ replayIndex <= Len(log)
               /\ SetUp("replay", log[replayIndex])
               /\ UNCHANGED <<e, mode, log, replayIndex, processed, outputs>>

At(a) == phase = "execute" /\ pc <= Len(p) /\ p[pc] = a
Advance(a) == /\ pc' = pc + 1
              /\ calls' = [calls EXCEPT ![a] = @ + 1]
              /\ UNCHANGED <<mode, phase, cmd, p, log, replayIndex, processed, outputs, samples, baseline>>

BookHandle == /\ At("book")
              /\ e' = [e EXCEPT ![mode].book = cmd.price, ![mode].hasBook = TRUE]
              /\ Advance("book")

MidEval == /\ At("mid")
           /\ e' = [e EXCEPT ![mode].mid = e[mode].book]
           /\ Advance("mid")

RunTrade(k) ==
    LET a == IF k = 1 THEN "trade1" ELSE "trade2"
        w == e[mode].windows[k]
        difference == cmd.price - e[mode].mid
        updated == [ring |-> [w.ring EXCEPT ![w.next] = difference],
                    next |-> (w.next % Capacity(k)) + 1,
                    count |-> IF w.count = Capacity(k) THEN w.count ELSE w.count + 1,
                    total |-> w.total - (IF w.count = Capacity(k) THEN w.ring[w.next] ELSE 0)
                              + difference]
    IN /\ At(a)
       /\ e' = [e EXCEPT ![mode].windows[k] = updated]
       /\ Advance(a)
TradeOne == RunTrade(1)
TradeTwo == RunTrade(2)

RunReset(k) ==
    LET a == IF k = 1 THEN "reset1" ELSE "reset2"
    IN /\ At(a)
       /\ e' = [e EXCEPT ![mode].windows[k].next = 1,
                         ![mode].windows[k].count = 0,
                         ![mode].windows[k].total = 0]
       /\ Advance(a)
ResetOne == RunReset(1)
ResetTwo == RunReset(2)

RunEval(k) ==
    LET a == IF k = 1 THEN "eval1" ELSE "eval2"
        w == e[mode].windows[k]
    IN /\ At(a)
       /\ e' = [e EXCEPT ![mode].values[k] = Ratio(w.total, w.count)]
       /\ Advance(a)
EvalOne == RunEval(1)
EvalTwo == RunEval(2)

FinishRecord == /\ phase = "execute" /\ pc > Len(p)
                /\ outputs' = IF cmd.kind = "query"
                              THEN [outputs EXCEPT ![mode] =
                                   Append(@, [j \in 1..Len(cmd.select) |-> e[mode].values[cmd.select[j]]])]
                              ELSE outputs
                /\ processed' = [processed EXCEPT ![mode] = @ + 1]
                /\ phase' = IF mode = "live" THEN "choose" ELSE "replayready"
                /\ replayIndex' = IF mode = "replay" THEN replayIndex + 1 ELSE replayIndex
                /\ UNCHANGED <<e, mode, cmd, p, pc, calls, log, samples, baseline>>

FinishReplay == /\ mode = "replay" /\ phase = "replayready" /\ replayIndex > Len(log)
                /\ phase' = "done"
                /\ UNCHANGED <<e, mode, cmd, p, pc, calls, log, replayIndex, processed, outputs, samples, baseline>>
Halt == /\ phase = "done" /\ UNCHANGED vars

Next == Choose \/ StopLive \/ ReplayStart \/ BookHandle \/ MidEval
        \/ TradeOne \/ TradeTwo \/ ResetOne \/ ResetTwo \/ EvalOne \/ EvalTwo
        \/ FinishRecord \/ FinishReplay \/ Halt
Spec == Init /\ [][Next]_vars

TypeOK == /\ mode \in Instances
          /\ phase \in {"choose", "execute", "replayready", "done"}
          /\ log \in Seq(Records) /\ Len(log) <= MaxRecords
          /\ replayIndex \in 1..(MaxRecords + 1)
          /\ processed \in [Instances -> 0..MaxRecords]
          /\ pc \in 1..4
          /\ p \in Seq(ActionNames) /\ Len(p) <= 3
          /\ calls \in [ActionNames -> 0..1]
          /\ \A i \in Instances :
               /\ e[i].book \in Prices \cup {0}
               /\ e[i].mid \in Prices \cup {0}
               /\ e[i].hasBook \in BOOLEAN
               /\ samples[i] \in Seq(Int) /\ Len(samples[i]) <= MaxRecords
               /\ \A k \in Windows :
                    /\ e[i].windows[k].ring \in [1..Capacity(k) -> Int]
                    /\ e[i].windows[k].next \in 1..Capacity(k)
                    /\ e[i].windows[k].count \in 0..Capacity(k)
                    /\ e[i].windows[k].total \in Int
                    /\ e[i].values[k].num \in Int
                    /\ e[i].values[k].den \in 1..Capacity(k)

Stable(i) == i # mode \/ phase # "execute" \/ pc > Len(p)
RingMatches(i, k) ==
    LET s == samples[i]
        w == e[i].windows[k]
        recent == Recent(s, k)
    IN /\ w.count = Len(recent)
       /\ w.total = Sum(recent)
       /\ w.next = (Len(s) % Capacity(k)) + 1
       /\ \A j \in 1..Len(s) :
            j > Len(s) - Capacity(k) => w.ring[((j - 1) % Capacity(k)) + 1] = s[j]
HistoryMatchesOracle == \A i \in Instances : Stable(i) => \A k \in Windows : RingMatches(i, k)
FreshMidAtTrade == (At("trade1") \/ At("trade2")) => e[mode].mid = e[mode].book
QueryDoesNotMutateHistory ==
    (phase = "execute" /\ cmd.kind = "query") => History(e[mode]) = baseline
QueriedValuesMatchOracle ==
    (phase = "execute" /\ cmd.kind = "query" /\ pc > Len(p)) =>
        \A k \in Elements(cmd.select) :
            LET recent == Recent(samples[mode], k)
            IN EqualRatio(e[mode].values[k], Ratio(Sum(recent), Len(recent)))
ActionsExactlyOnce ==
    (phase = "execute" /\ pc > Len(p)) =>
        /\ \A a \in ActionNames : calls[a] = IF a \in Elements(p) THEN 1 ELSE 0
        /\ (cmd.kind = "trade" => calls["trade1"] = 1 /\ calls["trade2"] = 1 /\ calls["mid"] = 1)
        /\ (cmd.kind = "reset" => calls["reset1"] = 1 /\ calls["reset2"] = 1 /\ calls["mid"] = 0)
ReplayQueryParity == outputs["replay"] = SubSeq(outputs["live"], 1, Len(outputs["replay"]))
FinalReplayParity == phase = "done" =>
    /\ e["live"] = e["replay"]
    /\ outputs["live"] = outputs["replay"]
    /\ processed["live"] = Len(log)
    /\ processed["replay"] = Len(log)
=============================================================================
