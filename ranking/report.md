# Agent ranking report

Generated 2026-07-04T03:57:42Z. Board `boards/board1.txt`. 264 games from `ranking/matches.jsonl`, 12 rated agents.

Fit: Bradley-Terry MM refit over the full store, prior 0.5 virtual games per played pair, anchor `rand.v1` = Elo 0. `+/-` is one standard error. `*` marks timing that includes parallel-run (contended) moves. `~` marks agents whose games do not connect to the anchor (rated relative to their own mean of 1000).

## Ratings (active agents)

| rank | Elo | +/- | games | W-L-D | ms/move | nodes/move | state | id |
|---:|---:|---:|---:|---:|---:|---:|---|---|
| 1 | 1108 | 91 | 44 | 42-2-0 | 4.88 | 118445 | on | `ab(d6,nb200k).classic(t1,c4,w0,l0).v1` |
| 2 | 1011 | 79 | 44 | 39-5-0 | 5.84 | 30829 | on | `ab(d6,tt,ord,nb200k).classic(t1,c4,w0,l0).v1` |
| 3 | 878 | 71 | 44 | 34-10-0 | 0.15 | 3538 | on | `ab(d4).classic(t1,c4,w0,l0).v1` |
| 4 | 830 | 70 | 44 | 32-12-0 | 0.05 | 1086 | on | `ab(d3).classic(t1,c4,w0,l0).v1` |
| 5 | 830 | 70 | 44 | 32-12-0 | 0.15 | 3568 | on | `ab(d4).classic(t1,c4,w0,l0).dil(r10).v1` |
| 6 | 622 | 68 | 44 | 23-21-0 | 0.49 | 10033 | on | `ab(d4).exp(t1,c4,w0,l0,f2).v1` |
| 7 | 555 | 68 | 44 | 20-24-0 | 0.01 | 116 | on | `ab(d2).classic(t1,c4,w0,l0).v1` |
| 8 | 372 | 70 | 44 | 12-32-0 | 0.00 | 0 | on | `greedy.classic(t2,c10,w3,l2).v1` |
| 9 | 324 | 72 | 44 | 10-34-0 | 0.00 | 0 | on | `smart(4).v1` |
| 10 | 273 | 74 | 44 | 8-36-0 | 0.00 | 0 | on | `greedy.classic(t1,c4,w0,l0).v1` |
| 11 | 0 | (anchor) | 44 | 0-44-0 | 0.00 | 0 | anchor | `rand.v1` |

## Inactive and retired agents

Still rated from their stored games (history is never lost). `off` = in the roster but benched, `gone` = no longer in the roster.

| rank | Elo | +/- | games | W-L-D | ms/move | nodes/move | state | id |
|---:|---:|---:|---:|---:|---:|---:|---|---|
| 1 | 372 | 70 | 44 | 12-32-0 | 0.00 | 0 | off | `tiered.v1` |

## Head-to-head matrix (active agents)

Cell = row agent's score against the column agent, over n games.

| # | agent | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | `ab(d6,nb200k).classic(t1,c4,w0,l0).v1` | - | 75% (4) | 100% (4) | 100% (4) | 75% (4) | 100% (4) | 100% (4) | 100% (4) | 100% (4) | 100% (4) | 100% (4) |
| 2 | `ab(d6,tt,ord,nb200k).classic(t1,c4,w0,l0).v1` | 25% (4) | - | 75% (4) | 100% (4) | 75% (4) | 100% (4) | 100% (4) | 100% (4) | 100% (4) | 100% (4) | 100% (4) |
| 3 | `ab(d4).classic(t1,c4,w0,l0).v1` | 0% (4) | 25% (4) | - | 50% (4) | 75% (4) | 100% (4) | 100% (4) | 100% (4) | 100% (4) | 100% (4) | 100% (4) |
| 4 | `ab(d3).classic(t1,c4,w0,l0).v1` | 0% (4) | 0% (4) | 50% (4) | - | 50% (4) | 100% (4) | 100% (4) | 100% (4) | 100% (4) | 100% (4) | 100% (4) |
| 5 | `ab(d4).classic(t1,c4,w0,l0).dil(r10).v1` | 25% (4) | 25% (4) | 25% (4) | 50% (4) | - | 75% (4) | 100% (4) | 100% (4) | 100% (4) | 100% (4) | 100% (4) |
| 6 | `ab(d4).exp(t1,c4,w0,l0,f2).v1` | 0% (4) | 0% (4) | 0% (4) | 0% (4) | 25% (4) | - | 50% (4) | 100% (4) | 100% (4) | 100% (4) | 100% (4) |
| 7 | `ab(d2).classic(t1,c4,w0,l0).v1` | 0% (4) | 0% (4) | 0% (4) | 0% (4) | 0% (4) | 50% (4) | - | 100% (4) | 100% (4) | 50% (4) | 100% (4) |
| 8 | `greedy.classic(t2,c10,w3,l2).v1` | 0% (4) | 0% (4) | 0% (4) | 0% (4) | 0% (4) | 0% (4) | 0% (4) | - | 50% (4) | 100% (4) | 100% (4) |
| 9 | `smart(4).v1` | 0% (4) | 0% (4) | 0% (4) | 0% (4) | 0% (4) | 0% (4) | 0% (4) | 50% (4) | - | 75% (4) | 100% (4) |
| 10 | `greedy.classic(t1,c4,w0,l0).v1` | 0% (4) | 0% (4) | 0% (4) | 0% (4) | 0% (4) | 0% (4) | 50% (4) | 0% (4) | 25% (4) | - | 100% (4) |
| 11 | `rand.v1` | 0% (4) | 0% (4) | 0% (4) | 0% (4) | 0% (4) | 0% (4) | 0% (4) | 0% (4) | 0% (4) | 0% (4) | - |

## Per-agent match history (active agents)

### 1. `ab(d6,nb200k).classic(t1,c4,w0,l0).v1` (Elo 1108 +/- 91)

| opponent | games | W-L-D | score | expected | delta |
|---|---:|---:|---:|---:|---:|
| `ab(d6,tt,ord,nb200k).classic(t1,c4,w0,l0).v1` | 4 | 3-1-0 | 0.75 | 0.64 | +0.11 |
| `ab(d4).classic(t1,c4,w0,l0).v1` | 4 | 4-0-0 | 1.00 | 0.79 | +0.21 |
| `ab(d3).classic(t1,c4,w0,l0).v1` | 4 | 4-0-0 | 1.00 | 0.83 | +0.17 |
| `ab(d4).classic(t1,c4,w0,l0).dil(r10).v1` | 4 | 3-1-0 | 0.75 | 0.83 | -0.08 |
| `ab(d4).exp(t1,c4,w0,l0,f2).v1` | 4 | 4-0-0 | 1.00 | 0.94 | +0.06 |
| `ab(d2).classic(t1,c4,w0,l0).v1` | 4 | 4-0-0 | 1.00 | 0.96 | +0.04 |
| `greedy.classic(t2,c10,w3,l2).v1` | 4 | 4-0-0 | 1.00 | 0.99 | +0.01 |
| `tiered.v1` | 4 | 4-0-0 | 1.00 | 0.99 | +0.01 |
| `smart(4).v1` | 4 | 4-0-0 | 1.00 | 0.99 | +0.01 |
| `greedy.classic(t1,c4,w0,l0).v1` | 4 | 4-0-0 | 1.00 | 0.99 | +0.01 |
| `rand.v1` | 4 | 4-0-0 | 1.00 | 1.00 | +0.00 |

### 2. `ab(d6,tt,ord,nb200k).classic(t1,c4,w0,l0).v1` (Elo 1011 +/- 79)

| opponent | games | W-L-D | score | expected | delta |
|---|---:|---:|---:|---:|---:|
| `ab(d6,nb200k).classic(t1,c4,w0,l0).v1` | 4 | 1-3-0 | 0.25 | 0.36 | -0.11 |
| `ab(d4).classic(t1,c4,w0,l0).v1` | 4 | 3-1-0 | 0.75 | 0.68 | +0.07 |
| `ab(d3).classic(t1,c4,w0,l0).v1` | 4 | 4-0-0 | 1.00 | 0.74 | +0.26 |
| `ab(d4).classic(t1,c4,w0,l0).dil(r10).v1` | 4 | 3-1-0 | 0.75 | 0.74 | +0.01 |
| `ab(d4).exp(t1,c4,w0,l0,f2).v1` | 4 | 4-0-0 | 1.00 | 0.90 | +0.10 |
| `ab(d2).classic(t1,c4,w0,l0).v1` | 4 | 4-0-0 | 1.00 | 0.93 | +0.07 |
| `greedy.classic(t2,c10,w3,l2).v1` | 4 | 4-0-0 | 1.00 | 0.98 | +0.02 |
| `tiered.v1` | 4 | 4-0-0 | 1.00 | 0.98 | +0.02 |
| `smart(4).v1` | 4 | 4-0-0 | 1.00 | 0.98 | +0.02 |
| `greedy.classic(t1,c4,w0,l0).v1` | 4 | 4-0-0 | 1.00 | 0.99 | +0.01 |
| `rand.v1` | 4 | 4-0-0 | 1.00 | 1.00 | +0.00 |

### 3. `ab(d4).classic(t1,c4,w0,l0).v1` (Elo 878 +/- 71)

| opponent | games | W-L-D | score | expected | delta |
|---|---:|---:|---:|---:|---:|
| `ab(d6,nb200k).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.21 | -0.21 |
| `ab(d6,tt,ord,nb200k).classic(t1,c4,w0,l0).v1` | 4 | 1-3-0 | 0.25 | 0.32 | -0.07 |
| `ab(d3).classic(t1,c4,w0,l0).v1` | 4 | 2-2-0 | 0.50 | 0.57 | -0.07 |
| `ab(d4).classic(t1,c4,w0,l0).dil(r10).v1` | 4 | 3-1-0 | 0.75 | 0.57 | +0.18 |
| `ab(d4).exp(t1,c4,w0,l0,f2).v1` | 4 | 4-0-0 | 1.00 | 0.81 | +0.19 |
| `ab(d2).classic(t1,c4,w0,l0).v1` | 4 | 4-0-0 | 1.00 | 0.87 | +0.13 |
| `greedy.classic(t2,c10,w3,l2).v1` | 4 | 4-0-0 | 1.00 | 0.95 | +0.05 |
| `tiered.v1` | 4 | 4-0-0 | 1.00 | 0.95 | +0.05 |
| `smart(4).v1` | 4 | 4-0-0 | 1.00 | 0.96 | +0.04 |
| `greedy.classic(t1,c4,w0,l0).v1` | 4 | 4-0-0 | 1.00 | 0.97 | +0.03 |
| `rand.v1` | 4 | 4-0-0 | 1.00 | 0.99 | +0.01 |

### 4. `ab(d3).classic(t1,c4,w0,l0).v1` (Elo 830 +/- 70)

| opponent | games | W-L-D | score | expected | delta |
|---|---:|---:|---:|---:|---:|
| `ab(d6,nb200k).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.17 | -0.17 |
| `ab(d6,tt,ord,nb200k).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.26 | -0.26 |
| `ab(d4).classic(t1,c4,w0,l0).v1` | 4 | 2-2-0 | 0.50 | 0.43 | +0.07 |
| `ab(d4).classic(t1,c4,w0,l0).dil(r10).v1` | 4 | 2-2-0 | 0.50 | 0.50 | +0.00 |
| `ab(d4).exp(t1,c4,w0,l0,f2).v1` | 4 | 4-0-0 | 1.00 | 0.77 | +0.23 |
| `ab(d2).classic(t1,c4,w0,l0).v1` | 4 | 4-0-0 | 1.00 | 0.83 | +0.17 |
| `greedy.classic(t2,c10,w3,l2).v1` | 4 | 4-0-0 | 1.00 | 0.93 | +0.07 |
| `tiered.v1` | 4 | 4-0-0 | 1.00 | 0.93 | +0.07 |
| `smart(4).v1` | 4 | 4-0-0 | 1.00 | 0.95 | +0.05 |
| `greedy.classic(t1,c4,w0,l0).v1` | 4 | 4-0-0 | 1.00 | 0.96 | +0.04 |
| `rand.v1` | 4 | 4-0-0 | 1.00 | 0.99 | +0.01 |

### 5. `ab(d4).classic(t1,c4,w0,l0).dil(r10).v1` (Elo 830 +/- 70)

| opponent | games | W-L-D | score | expected | delta |
|---|---:|---:|---:|---:|---:|
| `ab(d6,nb200k).classic(t1,c4,w0,l0).v1` | 4 | 1-3-0 | 0.25 | 0.17 | +0.08 |
| `ab(d6,tt,ord,nb200k).classic(t1,c4,w0,l0).v1` | 4 | 1-3-0 | 0.25 | 0.26 | -0.01 |
| `ab(d4).classic(t1,c4,w0,l0).v1` | 4 | 1-3-0 | 0.25 | 0.43 | -0.18 |
| `ab(d3).classic(t1,c4,w0,l0).v1` | 4 | 2-2-0 | 0.50 | 0.50 | +0.00 |
| `ab(d4).exp(t1,c4,w0,l0,f2).v1` | 4 | 3-1-0 | 0.75 | 0.77 | -0.02 |
| `ab(d2).classic(t1,c4,w0,l0).v1` | 4 | 4-0-0 | 1.00 | 0.83 | +0.17 |
| `greedy.classic(t2,c10,w3,l2).v1` | 4 | 4-0-0 | 1.00 | 0.93 | +0.07 |
| `tiered.v1` | 4 | 4-0-0 | 1.00 | 0.93 | +0.07 |
| `smart(4).v1` | 4 | 4-0-0 | 1.00 | 0.95 | +0.05 |
| `greedy.classic(t1,c4,w0,l0).v1` | 4 | 4-0-0 | 1.00 | 0.96 | +0.04 |
| `rand.v1` | 4 | 4-0-0 | 1.00 | 0.99 | +0.01 |

### 6. `ab(d4).exp(t1,c4,w0,l0,f2).v1` (Elo 622 +/- 68)

| opponent | games | W-L-D | score | expected | delta |
|---|---:|---:|---:|---:|---:|
| `ab(d6,nb200k).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.06 | -0.06 |
| `ab(d6,tt,ord,nb200k).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.10 | -0.10 |
| `ab(d4).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.19 | -0.19 |
| `ab(d3).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.23 | -0.23 |
| `ab(d4).classic(t1,c4,w0,l0).dil(r10).v1` | 4 | 1-3-0 | 0.25 | 0.23 | +0.02 |
| `ab(d2).classic(t1,c4,w0,l0).v1` | 4 | 2-2-0 | 0.50 | 0.60 | -0.10 |
| `greedy.classic(t2,c10,w3,l2).v1` | 4 | 4-0-0 | 1.00 | 0.81 | +0.19 |
| `tiered.v1` | 4 | 4-0-0 | 1.00 | 0.81 | +0.19 |
| `smart(4).v1` | 4 | 4-0-0 | 1.00 | 0.85 | +0.15 |
| `greedy.classic(t1,c4,w0,l0).v1` | 4 | 4-0-0 | 1.00 | 0.88 | +0.12 |
| `rand.v1` | 4 | 4-0-0 | 1.00 | 0.97 | +0.03 |

### 7. `ab(d2).classic(t1,c4,w0,l0).v1` (Elo 555 +/- 68)

| opponent | games | W-L-D | score | expected | delta |
|---|---:|---:|---:|---:|---:|
| `ab(d6,nb200k).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.04 | -0.04 |
| `ab(d6,tt,ord,nb200k).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.07 | -0.07 |
| `ab(d4).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.13 | -0.13 |
| `ab(d3).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.17 | -0.17 |
| `ab(d4).classic(t1,c4,w0,l0).dil(r10).v1` | 4 | 0-4-0 | 0.00 | 0.17 | -0.17 |
| `ab(d4).exp(t1,c4,w0,l0,f2).v1` | 4 | 2-2-0 | 0.50 | 0.40 | +0.10 |
| `greedy.classic(t2,c10,w3,l2).v1` | 4 | 4-0-0 | 1.00 | 0.74 | +0.26 |
| `tiered.v1` | 4 | 4-0-0 | 1.00 | 0.74 | +0.26 |
| `smart(4).v1` | 4 | 4-0-0 | 1.00 | 0.79 | +0.21 |
| `greedy.classic(t1,c4,w0,l0).v1` | 4 | 2-2-0 | 0.50 | 0.84 | -0.34 |
| `rand.v1` | 4 | 4-0-0 | 1.00 | 0.96 | +0.04 |

### 8. `greedy.classic(t2,c10,w3,l2).v1` (Elo 372 +/- 70)

| opponent | games | W-L-D | score | expected | delta |
|---|---:|---:|---:|---:|---:|
| `ab(d6,nb200k).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.01 | -0.01 |
| `ab(d6,tt,ord,nb200k).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.02 | -0.02 |
| `ab(d4).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.05 | -0.05 |
| `ab(d3).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.07 | -0.07 |
| `ab(d4).classic(t1,c4,w0,l0).dil(r10).v1` | 4 | 0-4-0 | 0.00 | 0.07 | -0.07 |
| `ab(d4).exp(t1,c4,w0,l0,f2).v1` | 4 | 0-4-0 | 0.00 | 0.19 | -0.19 |
| `ab(d2).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.26 | -0.26 |
| `tiered.v1` | 4 | 2-2-0 | 0.50 | 0.50 | +0.00 |
| `smart(4).v1` | 4 | 2-2-0 | 0.50 | 0.57 | -0.07 |
| `greedy.classic(t1,c4,w0,l0).v1` | 4 | 4-0-0 | 1.00 | 0.64 | +0.36 |
| `rand.v1` | 4 | 4-0-0 | 1.00 | 0.89 | +0.11 |

### 9. `smart(4).v1` (Elo 324 +/- 72)

| opponent | games | W-L-D | score | expected | delta |
|---|---:|---:|---:|---:|---:|
| `ab(d6,nb200k).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.01 | -0.01 |
| `ab(d6,tt,ord,nb200k).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.02 | -0.02 |
| `ab(d4).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.04 | -0.04 |
| `ab(d3).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.05 | -0.05 |
| `ab(d4).classic(t1,c4,w0,l0).dil(r10).v1` | 4 | 0-4-0 | 0.00 | 0.05 | -0.05 |
| `ab(d4).exp(t1,c4,w0,l0,f2).v1` | 4 | 0-4-0 | 0.00 | 0.15 | -0.15 |
| `ab(d2).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.21 | -0.21 |
| `greedy.classic(t2,c10,w3,l2).v1` | 4 | 2-2-0 | 0.50 | 0.43 | +0.07 |
| `tiered.v1` | 4 | 1-3-0 | 0.25 | 0.43 | -0.18 |
| `greedy.classic(t1,c4,w0,l0).v1` | 4 | 3-1-0 | 0.75 | 0.57 | +0.18 |
| `rand.v1` | 4 | 4-0-0 | 1.00 | 0.87 | +0.13 |

### 10. `greedy.classic(t1,c4,w0,l0).v1` (Elo 273 +/- 74)

| opponent | games | W-L-D | score | expected | delta |
|---|---:|---:|---:|---:|---:|
| `ab(d6,nb200k).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.01 | -0.01 |
| `ab(d6,tt,ord,nb200k).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.01 | -0.01 |
| `ab(d4).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.03 | -0.03 |
| `ab(d3).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.04 | -0.04 |
| `ab(d4).classic(t1,c4,w0,l0).dil(r10).v1` | 4 | 0-4-0 | 0.00 | 0.04 | -0.04 |
| `ab(d4).exp(t1,c4,w0,l0,f2).v1` | 4 | 0-4-0 | 0.00 | 0.12 | -0.12 |
| `ab(d2).classic(t1,c4,w0,l0).v1` | 4 | 2-2-0 | 0.50 | 0.16 | +0.34 |
| `greedy.classic(t2,c10,w3,l2).v1` | 4 | 0-4-0 | 0.00 | 0.36 | -0.36 |
| `tiered.v1` | 4 | 1-3-0 | 0.25 | 0.36 | -0.11 |
| `smart(4).v1` | 4 | 1-3-0 | 0.25 | 0.43 | -0.18 |
| `rand.v1` | 4 | 4-0-0 | 1.00 | 0.83 | +0.17 |

### 11. `rand.v1` (Elo 0 (anchor))

| opponent | games | W-L-D | score | expected | delta |
|---|---:|---:|---:|---:|---:|
| `ab(d6,nb200k).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.00 | -0.00 |
| `ab(d6,tt,ord,nb200k).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.00 | -0.00 |
| `ab(d4).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.01 | -0.01 |
| `ab(d3).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.01 | -0.01 |
| `ab(d4).classic(t1,c4,w0,l0).dil(r10).v1` | 4 | 0-4-0 | 0.00 | 0.01 | -0.01 |
| `ab(d4).exp(t1,c4,w0,l0,f2).v1` | 4 | 0-4-0 | 0.00 | 0.03 | -0.03 |
| `ab(d2).classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.04 | -0.04 |
| `greedy.classic(t2,c10,w3,l2).v1` | 4 | 0-4-0 | 0.00 | 0.11 | -0.11 |
| `tiered.v1` | 4 | 0-4-0 | 0.00 | 0.11 | -0.11 |
| `smart(4).v1` | 4 | 0-4-0 | 0.00 | 0.13 | -0.13 |
| `greedy.classic(t1,c4,w0,l0).v1` | 4 | 0-4-0 | 0.00 | 0.17 | -0.17 |

