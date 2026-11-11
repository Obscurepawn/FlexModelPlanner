---------------------------- MODULE L0_Planning ----------------------------
EXTENDS Naturals, Sequences, FiniteSets, TLC

\* var <-> code map (faithful transcription; no fault injection)
\* m: typed metadata, bounded test input (include/flexmodel/meta.hpp:12-34).
\* roots/mode: selected roots/event/query (include/flexmodel/planner.hpp:52-78,137-157).
\* live, pass, row, col: in-place N-pass loops (include/flexmodel/planner.hpp:56-75).
\* marks: 0=new,1=gray,2=black (include/flexmodel/planner.hpp:26-35).
\* stack: recursive visit frames/dep cursor (include/flexmodel/planner.hpp:29-34).
\* order: result.actions, cursor: root scan (include/flexmodel/planner.hpp:36-37,112-118,147-157).
\* phase: closure, DFS, return or compile-time rejection.
\* Init/ClosureStep: include/flexmodel/planner.hpp:56-75.
\* RootSkip/RootEnter: include/flexmodel/planner.hpp:112-118,155-157.
\* DependencySkip/DependencyEnter: include/flexmodel/planner.hpp:29-34.
\* Cycle: gray-node call at include/flexmodel/planner.hpp:31; the named
\* non-constexpr rejection helper is include/flexmodel/planner.hpp:9-13.
\* FinishAction: include/flexmodel/planner.hpp:35-37.
\* FinishPlan/Terminal: include/flexmodel/planner.hpp:118,157.
\* Needs/Seeds: include/flexmodel/planner.hpp:80-109,112-118,147-157.
\* Concurrency: one synchronous builder; no injected faults or C++ type checker.

CONSTANTS NodeCount, EventCount, MaxPorts
Nodes == 1..NodeCount
Events == 1..EventCount
Actions == 1..(2 * NodeCount)
Ports == {[consumer |-> c, producer |-> p, op |-> o, view |-> v] :
           c \in Nodes, p \in Nodes, o \in 0..EventCount, v \in {"state", "value"}}
PortSets == {{}} \cup {{p} : p \in Ports} \cup
            (IF MaxPorts = 2 THEN {{p, q} : p \in Ports, q \in Ports} ELSE {})
Schemas == {s \in [handlers : [Events -> SUBSET Nodes], evals : SUBSET Nodes,
                   ports : PortSets] :
    \A p \in s.ports :
       /\ (IF p.op = 0 THEN p.consumer \in s.evals ELSE p.consumer \in s.handlers[p.op])
       /\ (p.view = "value" => p.producer \in s.evals)}
VARIABLES m, roots, mode, live, pass, row, col, phase, marks, stack, order, cursor
vars == <<m, roots, mode, live, pass, row, col, phase, marks, stack, order, cursor>>
NodeNeeds == [n \in Nodes |-> {p.producer : p \in {p \in m.ports : p.consumer = n}}]
Handlers == IF mode = 0 THEN {} ELSE m.handlers[mode]
Exists == Handlers \cup {NodeCount + n : n \in m.evals}
Inputs(a) == {p \in m.ports :
    p.consumer = (IF a > NodeCount THEN a - NodeCount ELSE a) /\
    p.op = (IF a > NodeCount THEN 0 ELSE mode)}
Needs == [a \in Actions |->
    {p.producer : p \in {p \in Inputs(a) : p.producer \in Handlers}} \cup
    {NodeCount + p.producer : p \in {p \in Inputs(a) : p.view = "value"}} \cup
    (IF a > NodeCount /\ a - NodeCount \in Handlers THEN {a - NodeCount} ELSE {})]
Seeds == IF mode = 0 THEN {NodeCount + n : n \in roots \cap m.evals}
         ELSE live \cap Handlers
RECURSIVE Closure(_, _, _)
Closure(edges, start, steps) == IF steps = 0 THEN start
    ELSE LET previous == Closure(edges, start, steps - 1)
         IN previous \cup UNION {edges[a] : a \in previous}
Reachable == Closure(Needs, Seeds, 2 * NodeCount)
Cyclic == \E a \in Reachable : a \in Closure(Needs, Needs[a], 2 * NodeCount)
Members(sequence) == {sequence[i] : i \in 1..Len(sequence)}
Init ==
    /\ m \in Schemas /\ roots \in SUBSET Nodes /\ mode \in 0..EventCount
    /\ live = roots /\ pass = 1 /\ row = 1 /\ col = 1 /\ phase = "closure"
    /\ marks = [a \in Actions |-> 0] /\ stack = <<>> /\ order = <<>> /\ cursor = 1
ClosureStep ==
    /\ phase = "closure"
    /\ live' = IF row \in live /\ col \in NodeNeeds[row] THEN live \cup {col} ELSE live
    /\ IF col < NodeCount THEN
           /\ col' = col + 1 /\ UNCHANGED <<row, pass, phase>>
       ELSE IF row < NodeCount THEN
           /\ col' = 1 /\ row' = row + 1 /\ UNCHANGED <<pass, phase>>
       ELSE IF pass < NodeCount THEN
           /\ col' = 1 /\ row' = 1 /\ pass' = pass + 1 /\ UNCHANGED phase
       ELSE /\ phase' = "dfs" /\ UNCHANGED <<col, row, pass>>
    /\ UNCHANGED <<m, roots, mode, marks, stack, order, cursor>>
RootSkip ==
    /\ phase = "dfs" /\ stack = <<>> /\ cursor \in Actions
    /\ (cursor \notin Seeds \/ marks[cursor] = 2)
    /\ cursor' = cursor + 1
    /\ UNCHANGED <<m, roots, mode, live, pass, row, col, phase, marks, stack, order>>
RootEnter ==
    /\ phase = "dfs" /\ stack = <<>> /\ cursor \in Seeds /\ marks[cursor] = 0
    /\ marks' = [marks EXCEPT ![cursor] = 1]
    /\ stack' = <<[action |-> cursor, next |-> 1]>> /\ cursor' = cursor + 1
    /\ UNCHANGED <<m, roots, mode, live, pass, row, col, phase, order>>
DependencySkip ==
    /\ phase = "dfs" /\ Len(stack) > 0
    /\ LET frame == stack[Len(stack)] IN
       /\ frame.next \in Actions
       /\ (frame.next \notin Needs[frame.action] \/ marks[frame.next] = 2)
       /\ stack' = [stack EXCEPT ![Len(stack)].next = @ + 1]
    /\ UNCHANGED <<m, roots, mode, live, pass, row, col, phase, marks, order, cursor>>
DependencyEnter ==
    /\ phase = "dfs" /\ Len(stack) > 0
    /\ LET frame == stack[Len(stack)] IN
       /\ frame.next \in Actions /\ frame.next \in Needs[frame.action]
       /\ marks[frame.next] = 0
       /\ marks' = [marks EXCEPT ![frame.next] = 1]
       /\ stack' = Append([stack EXCEPT ![Len(stack)].next = @ + 1],
                          [action |-> frame.next, next |-> 1])
    /\ UNCHANGED <<m, roots, mode, live, pass, row, col, phase, order, cursor>>
Cycle ==
    /\ phase = "dfs" /\ Len(stack) > 0
    /\ LET frame == stack[Len(stack)] IN
       /\ frame.next \in Actions /\ frame.next \in Needs[frame.action]
       /\ marks[frame.next] = 1
    /\ phase' = "error"
    /\ UNCHANGED <<m, roots, mode, live, pass, row, col, marks, stack, order, cursor>>
FinishAction ==
    /\ phase = "dfs" /\ Len(stack) > 0
    /\ LET frame == stack[Len(stack)] IN
       /\ frame.next = 2 * NodeCount + 1
       /\ marks' = [marks EXCEPT ![frame.action] = 2]
       /\ order' = Append(order, frame.action)
    /\ stack' = SubSeq(stack, 1, Len(stack) - 1)
    /\ UNCHANGED <<m, roots, mode, live, pass, row, col, phase, cursor>>
FinishPlan ==
    /\ phase = "dfs" /\ stack = <<>> /\ cursor = 2 * NodeCount + 1
    /\ phase' = "done"
    /\ UNCHANGED <<m, roots, mode, live, pass, row, col, marks, stack, order, cursor>>
Terminal == phase \in {"done", "error"} /\ UNCHANGED vars
Next == ClosureStep \/ RootSkip \/ RootEnter \/ DependencySkip \/ DependencyEnter \/
        Cycle \/ FinishAction \/ FinishPlan \/ Terminal
Spec == Init /\ [][Next]_vars /\ WF_vars(Next)
TypeOK == /\ live \subseteq Nodes /\ marks \in [Actions -> 0..2]
          /\ order \in Seq(Actions) /\ Len(order) <= 2 * NodeCount
          /\ phase \in {"closure", "dfs", "error", "done"}
LiveExact == phase # "closure" => live = Closure(NodeNeeds, roots, NodeCount)
NoDuplicates == Cardinality(Members(order)) = Len(order)
PrerequisitesFirst == \A i \in 1..Len(order) :
    Needs[order[i]] \subseteq Members(SubSeq(order, 1, i - 1))
OnlyRequired == phase # "closure" => Members(order) \subseteq Reachable \cap Exists
Complete == phase = "done" => (~Cyclic /\ Members(order) = Reachable)
CycleSound == phase = "error" => Cyclic
Terminates == <>(phase \in {"done", "error"})
=============================================================================
