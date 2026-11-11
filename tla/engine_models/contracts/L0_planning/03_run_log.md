# L0 Planning — run log

TLC 1.7.4, Java 21, one worker, `--coverage`, local runs on 2026-11-11. Commands used the skill runner; the portable equivalent is `bash tla/check.sh` (manifest lists each configuration). No model property failed.

```sh
bash /mnt/c/Users/10255/.codex/skills/vibe-tla/scripts/run_tlc.sh tla/engine_models/contracts/L0_planning/02_models/L0_Planning.tla --det --coverage
bash /mnt/c/Users/10255/.codex/skills/vibe-tla/scripts/run_tlc.sh tla/engine_models/contracts/L0_planning/02_models/L0_Planning.tla tla/engine_models/contracts/L0_planning/02_models/L0_Planning_two_inputs.cfg --det --coverage
bash /mnt/c/Users/10255/.codex/skills/vibe-tla/scripts/run_tlc.sh tla/engine_models/contracts/L0_planning/02_models/L0_Planning.tla tla/engine_models/contracts/L0_planning/02_models/L0_Planning_two_events.cfg --det --coverage
```

| Configuration | Initial | Generated | Distinct | Depth | TLC time | Wrapper elapsed | Verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| primary: 2 nodes, 1 event, <=1 port | 960 | 17,288 | 16,238 | 29 | 1 s | 2 s | PASS |
| two_inputs: 2 nodes, 1 event, <=2 ports | 4,368 | 78,710 | 73,894 | 29 | 4 s | 5 s | PASS |
| two_events: 2 nodes, 2 events, <=2 ports | 49,728 | 895,016 | 839,836 | 29 | 35 s | 36 s | PASS |

Times are measured rounded TLC/runner wall times, not performance benchmarks. The three recorded wrapper runs total 43 seconds; development/editing time is not included.

## Action coverage
Counts are generated states, the second number of TLC's distinct:generated pair. Every named action generated states in every configuration. Terminal stuttering has zero new distinct states but positive generated coverage.

| Action | Primary | Two inputs | Two events |
|---|---:|---:|---:|
| Init | 960 | 4,368 | 49,728 |
| ClosureStep | 7,680 | 34,944 | 397,824 |
| RootSkip | 3,118 | 12,953 | 150,864 |
| RootEnter | 606 | 3,132 | 34,344 |
| DependencySkip | 2,398 | 11,620 | 129,400 |
| DependencyEnter | 40 | 387 | 3,580 |
| Cycle | 80 | 826 | 7,216 |
| FinishAction | 566 | 2,570 | 29,820 |
| FinishPlan | 880 | 3,542 | 42,512 |
| Terminal | 960 | 4,368 | 49,728 |

Scratch states were removed by the runner. Fresh portable rerun logs are written to `build-tla/`.
