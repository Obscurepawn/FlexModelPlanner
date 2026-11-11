# L1 TLC runs

Toolchain: TLC 1.7.4 from the workspace-pinned jar; Java 21; one worker. No symmetry reduction, state constraint, simulation or truncated search was used. Both configurations completed with an empty queue. The primary and scaling runs each enabled action coverage.

Commands run from `/home/obscure/FlexModelPlanner`:

```sh
bash /mnt/c/Users/10255/.codex/skills/vibe-tla/scripts/run_tlc.sh \
  tla/engine_models/contracts/L1_execution/02_models/L1_Execution.tla \
  --det --coverage > tla/engine_models/contracts/L1_execution/primary.log 2>&1

bash /mnt/c/Users/10255/.codex/skills/vibe-tla/scripts/run_tlc.sh \
  tla/engine_models/contracts/L1_execution/02_models/L1_Execution.tla \
  tla/engine_models/contracts/L1_execution/02_models/L1_Execution_Scale.cfg \
  --det --coverage > tla/engine_models/contracts/L1_execution/scaling.log 2>&1
```

| Configuration | Windows | Maximum records | Generated states | Distinct states | Depth | Runner wall time | Verdict |
|---|---|---:|---:|---:|---:|---:|---|
| `L1_Execution.cfg` | 1, 2 | 5 | 251,059 | 234,373 | 49 | 10 s | PASS |
| `L1_Execution_Scale.cfg` | 2, 3 | 6 | 2,008,707 | 1,891,422 | 59 | 62 s | PASS |

The TLC-internal finish durations were 9 seconds and 61 seconds; the table uses the skill runner's end-to-end elapsed seconds. Combined runner wall time is 72 seconds. These are verification run durations, not engine performance measurements. State counts include caller choices, per-callback dispatch, copied output observations and a complete second replay.

## Action coverage

TLC aggregates the two parameterized node instances of `RunTrade`, `RunReset` and `RunEval`. The exact-once invariant also checks that both required handlers complete within each respective plan. Every instrumented action family generated states in both runs; `Halt` reaches already-seen states and is therefore `0:positive`, not dead coverage.

| Action | Primary generated | Scaling generated |
|---|---:|---:|
| Init | 1 | 1 |
| Choose | 16,685 | 117,284 |
| StopLive | 9,986 | 69,175 |
| ReplayStart | 48,207 | 403,341 |
| BookHandle | 21,272 | 168,394 |
| MidEval | 11,408 | 99,584 |
| RunTrade | 22,816 | 199,168 |
| RunReset | 17,008 | 136,358 |
| RunEval | 25,512 | 204,537 |
| FinishRecord | 58,192 | 472,515 |
| FinishReplay | 9,986 | 69,175 |
| Halt | 9,986 | 69,175 |

Raw logs are `primary.log` and `scaling.log`. The first executable revision parsed and completed without a counterexample; no invariant was weakened after a failing run. `run_tlc.sh` removed TLC scratch state directories after each run. Portable reruns are registered in `tla/models.tsv` for `bash tla/check.sh`.
