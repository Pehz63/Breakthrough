# Agent ranking report

Generated 2026-07-26T21:56:52Z. Board `boards/board1.txt`. 2912 games from `ranking/matches_open.jsonl`, 14 rated agents.

> **Reading this table:** it lists every agent ever rated, including RETIRED ones (marked `(retired)`; superseded `@N` identities frozen at old game counts -- their Elo is not current strength). It also spans different SEARCH HEADS, and an agent is search + evaluator, so `ab(d6,tt,ord,nb200k)` and `ab(d6,ord,nb200k)` are different agents whose Elos are not interchangeable. For a current-standings comparison read `ranking/standings.tsv` (active only, grouped by head) instead, fix ONE head, and compare only within this one fit. Full rules: `Docs/benchmarking.md`.

Fit: Bradley-Terry MM refit over the full store, prior 0.5 virtual games per played pair, anchor `rand@1` = Elo 0. `+/-` is one standard error. `cpu/mv` is per-move process CPU time in ms (contention-safe, valid in parallel runs). `eff` = Elo / log2(1 + cpu_us/move), the Elo bought per doubling of per-move compute. `wall/mv` prefers serial games; `*` marks a fallback that includes contended parallel moves. `margin` is the average end-of-game piece lead (own minus opponent). `~` marks agents whose games do not connect to the anchor (rated relative to their own mean of 1000).

## Ratings (active agents)

| rank | Elo | +/- | games | W-L as White | W-L as Black | avg plies | margin | cpu/mv | eff | wall/mv | nodes/mv | state | id |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|
| 1 | 1405 | 24 | 416 | 184-24 | 166-42 | 50 | 1.1 | 157.28 | 81 | 171.10* | 524840* | on | `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` |
| 2 | 1250 | 20 | 416 | 140-68 | 145-63 | 54 | 0.9 | 34.09 | 83 | 38.54* | 102045* | on | `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` |
| 3 | 1240 | 20 | 416 | 147-61 | 133-75 | 56 | 0.1 | 19.13 | 87 | 21.23* | 65325* | on | `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` |
| 4 | 1235 | 20 | 416 | 147-61 | 131-77 | 58 | 0.6 | 19.87 | 87 | 21.82* | 67318* | on | `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` |
| 5 | 1211 | 20 | 416 | 140-68 | 126-82 | 58 | 0.1 | 17.18 | 86 | 19.11* | 59462* | on | `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` |
| 6 | 1190 | 20 | 416 | 132-76 | 124-84 | 57 | 0.1 | 17.45 | 84 | 19.34* | 59899* | on | `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` |
| 7 | 1186 | 20 | 416 | 133-75 | 121-87 | 57 | 0.0 | 17.71 | 84 | 19.54* | 59878* | on | `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` |
| 8 | 1178 | 20 | 416 | 130-78 | 120-88 | 51 | 1.1 | 10.72 | 88 | 12.09* | 32583* | on | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` |
| 9 | 1125 | 20 | 416 | 117-91 | 107-101 | 59 | -0.6 | 348.56 | 61 | 390.22* | 77491* | on | `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` |
| 10 | 1003 | 21 | 416 | 85-123 | 83-125 | 49 | -0.2 | 7.55 | 78 | 8.64* | 23827* | on | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` |
| 11 | 996 | 22 | 416 | 81-127 | 84-124 | 47 | 0.7 | 0.25 | 125 | 0.31* | 4266* | on | `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` |
| 12 | 817 | 28 | 416 | 51-157 | 52-156 | 47 | 1.2 | 0.01 | 206 | 0.01* | 106* | on | `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` |
| 13 | 369 | 58 | 416 | 15-193 | 17-191 | 35 | -2.2 | 4.05 | 31 | 4.71* | 13388* | on | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` |
| 14 | 0 | (anchor) | 416 | 0-208 | 1-207 | 27 | -3.1 | 0.00 | - | 0.00* | 0* | anchor | `rand@1` |

## Compute efficiency (active agents)

Sorted by per-move CPU time. `*` = on the Elo-vs-compute pareto frontier (no other active agent is both stronger and cheaper).

| cpu ms/mv | Elo | eff | frontier | id |
|---:|---:|---:|:---:|---|
| 0.000 | 0 | - | * | `rand@1` |
| 0.015 | 817 | 206 | * | `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` |
| 0.252 | 996 | 125 | * | `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` |
| 4.055 | 369 | 31 |  | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` |
| 7.553 | 1003 | 78 | * | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` |
| 10.721 | 1178 | 88 | * | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` |
| 17.183 | 1211 | 86 | * | `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` |
| 17.453 | 1190 | 84 |  | `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` |
| 17.712 | 1186 | 84 |  | `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` |
| 19.128 | 1240 | 87 | * | `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` |
| 19.866 | 1235 | 87 |  | `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` |
| 34.087 | 1250 | 83 | * | `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` |
| 157.277 | 1405 | 81 | * | `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` |
| 348.561 | 1125 | 61 |  | `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` |

## Head-to-head matrix (active agents)

Cell = row agent's score against the column agent, over n games.

| # | agent | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | - | 53% (32) | 88% (32) | 81% (32) | 91% (32) | 81% (32) | 75% (32) | 78% (32) | 84% (32) | 78% (32) | 84% (32) | 100% (32) | 100% (32) | 100% (32) |
| 2 | `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 47% (32) | - | 41% (32) | 75% (32) | 69% (32) | 53% (32) | 47% (32) | 59% (32) | 69% (32) | 56% (32) | 78% (32) | 97% (32) | 100% (32) | 100% (32) |
| 3 | `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 12% (32) | 59% (32) | - | 41% (32) | 59% (32) | 62% (32) | 47% (32) | 56% (32) | 59% (32) | 91% (32) | 91% (32) | 97% (32) | 100% (32) | 100% (32) |
| 4 | `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 19% (32) | 25% (32) | 59% (32) | - | 59% (32) | 72% (32) | 78% (32) | 41% (32) | 59% (32) | 91% (32) | 88% (32) | 78% (32) | 100% (32) | 100% (32) |
| 5 | `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 9% (32) | 31% (32) | 41% (32) | 41% (32) | - | 62% (32) | 62% (32) | 59% (32) | 59% (32) | 91% (32) | 84% (32) | 91% (32) | 100% (32) | 100% (32) |
| 6 | `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 19% (32) | 47% (32) | 38% (32) | 28% (32) | 38% (32) | - | 56% (32) | 66% (32) | 44% (32) | 94% (32) | 81% (32) | 91% (32) | 100% (32) | 100% (32) |
| 7 | `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 25% (32) | 53% (32) | 53% (32) | 22% (32) | 38% (32) | 44% (32) | - | 69% (32) | 38% (32) | 88% (32) | 78% (32) | 88% (32) | 100% (32) | 100% (32) |
| 8 | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 22% (32) | 41% (32) | 44% (32) | 59% (32) | 41% (32) | 34% (32) | 31% (32) | - | 75% (32) | 66% (32) | 72% (32) | 97% (32) | 100% (32) | 100% (32) |
| 9 | `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 16% (32) | 31% (32) | 41% (32) | 41% (32) | 41% (32) | 56% (32) | 62% (32) | 25% (32) | - | 81% (32) | 47% (32) | 59% (32) | 100% (32) | 100% (32) |
| 10 | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 22% (32) | 44% (32) | 9% (32) | 9% (32) | 9% (32) | 6% (32) | 12% (32) | 34% (32) | 19% (32) | - | 69% (32) | 91% (32) | 100% (32) | 100% (32) |
| 11 | `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 16% (32) | 22% (32) | 9% (32) | 12% (32) | 16% (32) | 19% (32) | 22% (32) | 28% (32) | 53% (32) | 31% (32) | - | 88% (32) | 100% (32) | 100% (32) |
| 12 | `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 0% (32) | 3% (32) | 3% (32) | 22% (32) | 9% (32) | 9% (32) | 12% (32) | 3% (32) | 41% (32) | 9% (32) | 12% (32) | - | 97% (32) | 100% (32) |
| 13 | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 3% (32) | - | 97% (32) |
| 14 | `rand@1` | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 3% (32) | - |

## Per-agent match history (active agents)

### 1. `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` (Elo 1405 +/- 24)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 17-15 | 0.53 | 0.71 | -0.18 | 63 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 28-4 | 0.88 | 0.72 | +0.15 | 58 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 26-6 | 0.81 | 0.73 | +0.09 | 55 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 29-3 | 0.91 | 0.75 | +0.15 | 55 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 26-6 | 0.81 | 0.77 | +0.04 | 57 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 24-8 | 0.75 | 0.78 | -0.03 | 56 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 25-7 | 0.78 | 0.79 | -0.01 | 52 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 27-5 | 0.84 | 0.83 | +0.01 | 55 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 25-7 | 0.78 | 0.91 | -0.13 | 50 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 27-5 | 0.84 | 0.91 | -0.07 | 48 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.97 | +0.03 | 43 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 32 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 23 |

### 2. `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` (Elo 1250 +/- 20)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 15-17 | 0.47 | 0.29 | +0.18 | 63 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.52 | -0.11 | 58 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 24-8 | 0.75 | 0.52 | +0.23 | 59 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 22-10 | 0.69 | 0.56 | +0.13 | 60 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 17-15 | 0.53 | 0.59 | -0.05 | 57 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 15-17 | 0.47 | 0.59 | -0.12 | 59 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.60 | -0.01 | 61 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 22-10 | 0.69 | 0.67 | +0.01 | 58 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 18-14 | 0.56 | 0.81 | -0.24 | 56 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 25-7 | 0.78 | 0.81 | -0.03 | 52 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 31-1 | 0.97 | 0.92 | +0.04 | 53 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.99 | +0.01 | 37 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 31 |

### 3. `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` (Elo 1240 +/- 20)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 4-28 | 0.12 | 0.28 | -0.15 | 58 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.48 | +0.11 | 58 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.51 | -0.10 | 66 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.54 | +0.05 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 20-12 | 0.62 | 0.57 | +0.05 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 15-17 | 0.47 | 0.58 | -0.11 | 66 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 18-14 | 0.56 | 0.59 | -0.03 | 59 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.66 | -0.07 | 66 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 29-3 | 0.91 | 0.80 | +0.11 | 56 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 29-3 | 0.91 | 0.80 | +0.10 | 49 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 31-1 | 0.97 | 0.92 | +0.05 | 49 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.99 | +0.01 | 35 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 28 |

### 4. `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` (Elo 1235 +/- 20)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 6-26 | 0.19 | 0.27 | -0.09 | 55 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 8-24 | 0.25 | 0.48 | -0.23 | 59 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.49 | +0.10 | 66 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.54 | +0.06 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 23-9 | 0.72 | 0.56 | +0.15 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 25-7 | 0.78 | 0.57 | +0.21 | 70 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.58 | -0.18 | 62 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.65 | -0.06 | 63 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 29-3 | 0.91 | 0.79 | +0.11 | 57 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 28-4 | 0.88 | 0.80 | +0.08 | 55 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 25-7 | 0.78 | 0.92 | -0.14 | 54 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.99 | +0.01 | 40 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 32 |

### 5. `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` (Elo 1211 +/- 20)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 3-29 | 0.09 | 0.25 | -0.15 | 55 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 10-22 | 0.31 | 0.44 | -0.13 | 60 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.46 | -0.05 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.46 | -0.06 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 20-12 | 0.62 | 0.53 | +0.10 | 67 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 20-12 | 0.62 | 0.54 | +0.09 | 70 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.55 | +0.05 | 62 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.62 | -0.03 | 73 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 29-3 | 0.91 | 0.77 | +0.14 | 58 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 27-5 | 0.84 | 0.77 | +0.07 | 54 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 29-3 | 0.91 | 0.91 | -0.00 | 52 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.99 | +0.01 | 36 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 29 |

### 6. `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` (Elo 1190 +/- 20)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 6-26 | 0.19 | 0.23 | -0.04 | 57 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 15-17 | 0.47 | 0.41 | +0.05 | 57 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 12-20 | 0.38 | 0.43 | -0.05 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 9-23 | 0.28 | 0.44 | -0.15 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 12-20 | 0.38 | 0.47 | -0.10 | 67 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 18-14 | 0.56 | 0.51 | +0.06 | 71 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 21-11 | 0.66 | 0.52 | +0.14 | 58 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 14-18 | 0.44 | 0.59 | -0.16 | 68 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 30-2 | 0.94 | 0.75 | +0.19 | 53 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 26-6 | 0.81 | 0.75 | +0.06 | 50 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 29-3 | 0.91 | 0.90 | +0.01 | 52 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.99 | +0.01 | 40 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 29 |

### 7. `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` (Elo 1186 +/- 20)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 8-24 | 0.25 | 0.22 | +0.03 | 56 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 17-15 | 0.53 | 0.41 | +0.12 | 59 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 17-15 | 0.53 | 0.42 | +0.11 | 66 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 7-25 | 0.22 | 0.43 | -0.21 | 70 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 12-20 | 0.38 | 0.46 | -0.09 | 70 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 14-18 | 0.44 | 0.49 | -0.06 | 71 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 22-10 | 0.69 | 0.51 | +0.18 | 56 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 12-20 | 0.38 | 0.59 | -0.21 | 70 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 28-4 | 0.88 | 0.74 | +0.13 | 57 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 25-7 | 0.78 | 0.75 | +0.03 | 52 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 28-4 | 0.88 | 0.89 | -0.02 | 52 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.99 | +0.01 | 35 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 28 |

### 8. `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` (Elo 1178 +/- 20)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 7-25 | 0.22 | 0.21 | +0.01 | 52 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.40 | +0.01 | 61 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 14-18 | 0.44 | 0.41 | +0.03 | 59 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.42 | +0.18 | 62 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.45 | -0.05 | 62 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 11-21 | 0.34 | 0.48 | -0.14 | 58 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 10-22 | 0.31 | 0.49 | -0.18 | 56 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 24-8 | 0.75 | 0.58 | +0.17 | 64 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 21-11 | 0.66 | 0.73 | -0.08 | 51 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 23-9 | 0.72 | 0.74 | -0.02 | 50 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 31-1 | 0.97 | 0.89 | +0.08 | 43 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.99 | +0.01 | 30 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 22 |

### 9. `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` (Elo 1125 +/- 20)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 5-27 | 0.16 | 0.17 | -0.01 | 55 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 10-22 | 0.31 | 0.33 | -0.01 | 58 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.34 | +0.07 | 66 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.35 | +0.06 | 63 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.38 | +0.03 | 73 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 18-14 | 0.56 | 0.41 | +0.16 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 20-12 | 0.62 | 0.41 | +0.21 | 70 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 8-24 | 0.25 | 0.42 | -0.17 | 64 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 26-6 | 0.81 | 0.67 | +0.14 | 61 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 15-17 | 0.47 | 0.68 | -0.21 | 58 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.86 | -0.26 | 58 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.99 | +0.01 | 41 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 30 |

### 10. `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` (Elo 1003 +/- 21)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 7-25 | 0.22 | 0.09 | +0.13 | 50 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 14-18 | 0.44 | 0.19 | +0.24 | 56 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 3-29 | 0.09 | 0.20 | -0.11 | 56 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 3-29 | 0.09 | 0.21 | -0.11 | 57 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 3-29 | 0.09 | 0.23 | -0.14 | 58 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 2-30 | 0.06 | 0.25 | -0.19 | 53 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 4-28 | 0.12 | 0.26 | -0.13 | 57 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 11-21 | 0.34 | 0.27 | +0.08 | 51 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 6-26 | 0.19 | 0.33 | -0.14 | 61 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 22-10 | 0.69 | 0.51 | +0.18 | 44 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 29-3 | 0.91 | 0.75 | +0.16 | 49 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.97 | +0.03 | 29 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 21 |

### 11. `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` (Elo 996 +/- 22)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 5-27 | 0.16 | 0.09 | +0.07 | 48 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 7-25 | 0.22 | 0.19 | +0.03 | 52 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 3-29 | 0.09 | 0.20 | -0.10 | 49 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 4-28 | 0.12 | 0.20 | -0.08 | 55 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 5-27 | 0.16 | 0.23 | -0.07 | 54 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 6-26 | 0.19 | 0.25 | -0.06 | 50 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 7-25 | 0.22 | 0.25 | -0.03 | 52 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 9-23 | 0.28 | 0.26 | +0.02 | 50 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 17-15 | 0.53 | 0.32 | +0.21 | 58 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 10-22 | 0.31 | 0.49 | -0.18 | 44 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 28-4 | 0.88 | 0.74 | +0.14 | 45 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.97 | +0.03 | 29 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 21 |

### 12. `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` (Elo 817 +/- 28)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.03 | -0.03 | 43 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 1-31 | 0.03 | 0.08 | -0.04 | 53 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 1-31 | 0.03 | 0.08 | -0.05 | 49 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 7-25 | 0.22 | 0.08 | +0.14 | 54 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 3-29 | 0.09 | 0.09 | +0.00 | 52 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 3-29 | 0.09 | 0.10 | -0.01 | 52 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 4-28 | 0.12 | 0.11 | +0.02 | 52 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 1-31 | 0.03 | 0.11 | -0.08 | 43 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.14 | +0.26 | 58 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 3-29 | 0.09 | 0.25 | -0.16 | 49 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 4-28 | 0.12 | 0.26 | -0.14 | 45 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 31-1 | 0.97 | 0.93 | +0.04 | 33 |
| `rand@1` | 32 | 32-0 | 1.00 | 0.99 | +0.01 | 24 |

### 13. `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` (Elo 369 +/- 58)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 32 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.01 | -0.01 | 37 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.01 | -0.01 | 35 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.01 | -0.01 | 40 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.01 | -0.01 | 36 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.01 | -0.01 | 40 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.01 | -0.01 | 35 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.01 | -0.01 | 30 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.01 | -0.01 | 41 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.03 | -0.03 | 29 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.03 | -0.03 | 29 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 1-31 | 0.03 | 0.07 | -0.04 | 33 |
| `rand@1` | 32 | 31-1 | 0.97 | 0.89 | +0.08 | 36 |

### 14. `rand@1` (Elo 0 (anchor))

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 23 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 31 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 28 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 32 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 29 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 29 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 28 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 22 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 30 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 21 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 21 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.01 | -0.01 | 24 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 1-31 | 0.03 | 0.11 | -0.08 | 36 |

