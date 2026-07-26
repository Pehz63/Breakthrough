# Agent ranking report

Generated 2026-07-26T23:27:45Z. Board `boards/board1.txt`. 6080 games from `ranking/matches_open.jsonl`, 20 rated agents.

> **Reading this table:** it lists every agent ever rated, including RETIRED ones (marked `(retired)`; superseded `@N` identities frozen at old game counts -- their Elo is not current strength). It also spans different SEARCH HEADS, and an agent is search + evaluator, so `ab(d6,tt,ord,nb200k)` and `ab(d6,ord,nb200k)` are different agents whose Elos are not interchangeable. For a current-standings comparison read `ranking/standings.tsv` (active only, grouped by head) instead, fix ONE head, and compare only within this one fit. Full rules: `Docs/benchmarking.md`.

Fit: Bradley-Terry MM refit over the full store, prior 0.5 virtual games per played pair, anchor `rand@1` = Elo 0. `+/-` is one standard error. `cpu/mv` is per-move process CPU time in ms (contention-safe, valid in parallel runs). `eff` = Elo / log2(1 + cpu_us/move), the Elo bought per doubling of per-move compute. `wall/mv` prefers serial games; `*` marks a fallback that includes contended parallel moves. `margin` is the average end-of-game piece lead (own minus opponent). `~` marks agents whose games do not connect to the anchor (rated relative to their own mean of 1000).

## Ratings (active agents)

| rank | Elo | +/- | games | W-L as White | W-L as Black | avg plies | margin | cpu/mv | eff | wall/mv | nodes/mv | state | id |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|
| 1 | 1399 | 21 | 608 | 273-31 | 255-49 | 52 | 1.4 | 165.97 | 81 | 177.70* | 545591* | on | `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` |
| 2 | 1249 | 17 | 608 | 219-85 | 226-78 | 56 | 1.4 | 34.62 | 83 | 40.19* | 104142* | on | `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` |
| 3 | 1215 | 16 | 608 | 217-87 | 205-99 | 60 | 1.3 | 21.71 | 84 | 23.46* | 70968* | on | `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` |
| 4 | 1181 | 16 | 608 | 208-96 | 190-114 | 60 | 0.3 | 20.51 | 82 | 22.20* | 68353* | on | `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` |
| 5 | 1178 | 16 | 608 | 209-95 | 187-117 | 62 | 0.5 | 18.47 | 83 | 20.05* | 61885* | on | `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` |
| 6 | 1166 | 16 | 608 | 196-108 | 191-113 | 54 | 1.5 | 11.59 | 86 | 13.04* | 33775* | on | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` |
| 7 | 1164 | 16 | 608 | 199-105 | 187-117 | 60 | 0.5 | 18.71 | 82 | 20.28* | 62718* | on | `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` |
| 8 | 1136 | 16 | 608 | 196-108 | 169-135 | 61 | 0.3 | 18.91 | 80 | 20.58* | 62904* | on | `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` |
| 9 | 1128 | 16 | 608 | 179-125 | 180-124 | 62 | -0.1 | 369.90 | 61 | 413.01* | 78848* | on | `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` |
| 10 | 1096 | 16 | 608 | 178-126 | 157-147 | 62 | -0.1 | 169.95 | 63 | 184.91* | 77981* | on | `ab(d6,tt,ord,nb200k)@1.learned(s77,ddaa5090)@1.opener(rand,4)@1` |
| 11 | 1081 | 16 | 608 | 158-146 | 166-138 | 62 | -0.4 | 177.31 | 62 | 192.43* | 80268* | on | `ab(d6,tt,ord,nb200k)@1.learned(s78,2fa21eda)@1.opener(rand,4)@1` |
| 12 | 1027 | 16 | 608 | 144-160 | 140-164 | 62 | -0.7 | 401.58 | 55 | 442.03* | 81179* | on | `ab(d6,tt,ord,nb200k)@1.learned(s113,e3cc8b4e)@1.opener(rand,4)@1` |
| 13 | 1025 | 16 | 608 | 128-176 | 154-150 | 63 | -0.7 | 414.18 | 55 | 450.51* | 83697* | on | `ab(d6,tt,ord,nb200k)@1.learned(s79,18f19059)@1.opener(rand,4)@1` |
| 14 | 993 | 16 | 608 | 125-179 | 134-170 | 63 | -0.9 | 401.73 | 53 | 438.83* | 81594* | on | `ab(d6,tt,ord,nb200k)@1.learned(s115,21d7e638)@1.opener(rand,4)@1` |
| 15 | 991 | 16 | 608 | 129-175 | 129-175 | 51 | 1.0 | 0.28 | 122 | 0.32* | 4243* | on | `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` |
| 16 | 986 | 16 | 608 | 131-173 | 123-181 | 62 | -1.1 | 399.47 | 53 | 456.35* | 82998* | on | `ab(d6,tt,ord,nb200k)@1.learned(s110,1466db6c)@1.opener(rand,4)@1` |
| 17 | 903 | 18 | 608 | 98-206 | 102-202 | 53 | -0.4 | 7.96 | 70 | 9.23* | 24837* | on | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` |
| 18 | 842 | 19 | 608 | 76-228 | 89-215 | 50 | 1.5 | 0.01 | 240 | 0.01* | 106* | on | `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` |
| 19 | 323 | 50 | 608 | 15-289 | 17-287 | 36 | -2.2 | 4.25 | 27 | 4.94* | 13623* | on | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` |
| 20 | 0 | (anchor) | 608 | 0-304 | 1-303 | 29 | -3.2 | 0.00 | - | 0.00* | 0* | anchor | `rand@1` |

## Compute efficiency (active agents)

Sorted by per-move CPU time. `*` = on the Elo-vs-compute pareto frontier (no other active agent is both stronger and cheaper).

| cpu ms/mv | Elo | eff | frontier | id |
|---:|---:|---:|:---:|---|
| 0.000 | 0 | - | * | `rand@1` |
| 0.010 | 842 | 240 | * | `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` |
| 0.281 | 991 | 122 | * | `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` |
| 4.246 | 323 | 27 |  | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` |
| 7.957 | 903 | 70 |  | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` |
| 11.589 | 1166 | 86 | * | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` |
| 18.472 | 1178 | 83 | * | `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` |
| 18.710 | 1164 | 82 |  | `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` |
| 18.913 | 1136 | 80 |  | `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` |
| 20.513 | 1181 | 82 | * | `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` |
| 21.710 | 1215 | 84 | * | `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` |
| 34.620 | 1249 | 83 | * | `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` |
| 165.973 | 1399 | 81 | * | `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` |
| 169.950 | 1096 | 63 |  | `ab(d6,tt,ord,nb200k)@1.learned(s77,ddaa5090)@1.opener(rand,4)@1` |
| 177.308 | 1081 | 62 |  | `ab(d6,tt,ord,nb200k)@1.learned(s78,2fa21eda)@1.opener(rand,4)@1` |
| 369.905 | 1128 | 61 |  | `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` |
| 399.474 | 986 | 53 |  | `ab(d6,tt,ord,nb200k)@1.learned(s110,1466db6c)@1.opener(rand,4)@1` |
| 401.585 | 1027 | 55 |  | `ab(d6,tt,ord,nb200k)@1.learned(s113,e3cc8b4e)@1.opener(rand,4)@1` |
| 401.735 | 993 | 53 |  | `ab(d6,tt,ord,nb200k)@1.learned(s115,21d7e638)@1.opener(rand,4)@1` |
| 414.179 | 1025 | 55 |  | `ab(d6,tt,ord,nb200k)@1.learned(s79,18f19059)@1.opener(rand,4)@1` |

## Unrated roster agents (no games yet)

- `ab(d6,tt,ord,nb200k)@1.learned(s112,baa2951a)@1.opener(rand,4)@1`
- `ab(d6,tt,ord,nb200k)@1.learned(s114,316257e9)@1.opener(rand,4)@1`

Run `rank.exe play` to schedule their games.

## Head-to-head matrix (active agents)

Cell = row agent's score against the column agent, over n games.

| # | agent | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 20 |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | - | 53% (32) | 81% (32) | 88% (32) | 91% (32) | 78% (32) | 81% (32) | 75% (32) | 84% (32) | 91% (32) | 100% (32) | 94% (32) | 88% (32) | 94% (32) | 84% (32) | 91% (32) | 78% (32) | 100% (32) | 100% (32) | 100% (32) |
| 2 | `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 47% (32) | - | 75% (32) | 41% (32) | 69% (32) | 59% (32) | 53% (32) | 47% (32) | 69% (32) | 94% (32) | 66% (32) | 88% (32) | 81% (32) | 91% (32) | 78% (32) | 81% (32) | 56% (32) | 97% (32) | 100% (32) | 100% (32) |
| 3 | `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 19% (32) | 25% (32) | - | 59% (32) | 59% (32) | 41% (32) | 72% (32) | 78% (32) | 59% (32) | 56% (32) | 81% (32) | 62% (32) | 81% (32) | 81% (32) | 88% (32) | 88% (32) | 91% (32) | 78% (32) | 100% (32) | 100% (32) |
| 4 | `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 12% (32) | 59% (32) | 41% (32) | - | 59% (32) | 56% (32) | 62% (32) | 47% (32) | 59% (32) | 34% (32) | 62% (32) | 53% (32) | 72% (32) | 75% (32) | 91% (32) | 72% (32) | 91% (32) | 97% (32) | 100% (32) | 100% (32) |
| 5 | `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 9% (32) | 31% (32) | 41% (32) | 41% (32) | - | 59% (32) | 62% (32) | 62% (32) | 59% (32) | 59% (32) | 53% (32) | 69% (32) | 69% (32) | 81% (32) | 84% (32) | 75% (32) | 91% (32) | 91% (32) | 100% (32) | 100% (32) |
| 6 | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 22% (32) | 41% (32) | 59% (32) | 44% (32) | 41% (32) | - | 34% (32) | 31% (32) | 75% (32) | 66% (32) | 69% (32) | 81% (32) | 75% (32) | 69% (32) | 72% (32) | 69% (32) | 66% (32) | 97% (32) | 100% (32) | 100% (32) |
| 7 | `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 19% (32) | 47% (32) | 28% (32) | 38% (32) | 38% (32) | 66% (32) | - | 56% (32) | 44% (32) | 69% (32) | 59% (32) | 66% (32) | 72% (32) | 72% (32) | 81% (32) | 69% (32) | 94% (32) | 91% (32) | 100% (32) | 100% (32) |
| 8 | `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 25% (32) | 53% (32) | 22% (32) | 53% (32) | 38% (32) | 69% (32) | 44% (32) | - | 38% (32) | 50% (32) | 38% (32) | 50% (32) | 72% (32) | 72% (32) | 78% (32) | 66% (32) | 88% (32) | 88% (32) | 100% (32) | 100% (32) |
| 9 | `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 16% (32) | 31% (32) | 41% (32) | 41% (32) | 41% (32) | 25% (32) | 56% (32) | 62% (32) | - | 69% (32) | 72% (32) | 69% (32) | 56% (32) | 72% (32) | 47% (32) | 84% (32) | 81% (32) | 59% (32) | 100% (32) | 100% (32) |
| 10 | `ab(d6,tt,ord,nb200k)@1.learned(s77,ddaa5090)@1.opener(rand,4)@1` | 9% (32) | 6% (32) | 44% (32) | 66% (32) | 41% (32) | 34% (32) | 31% (32) | 50% (32) | 31% (32) | - | 62% (32) | 56% (32) | 66% (32) | 69% (32) | 56% (32) | 62% (32) | 91% (32) | 72% (32) | 100% (32) | 100% (32) |
| 11 | `ab(d6,tt,ord,nb200k)@1.learned(s78,2fa21eda)@1.opener(rand,4)@1` | 0% (32) | 34% (32) | 19% (32) | 38% (32) | 47% (32) | 31% (32) | 41% (32) | 62% (32) | 28% (32) | 38% (32) | - | 69% (32) | 59% (32) | 81% (32) | 59% (32) | 66% (32) | 69% (32) | 72% (32) | 100% (32) | 100% (32) |
| 12 | `ab(d6,tt,ord,nb200k)@1.learned(s113,e3cc8b4e)@1.opener(rand,4)@1` | 6% (32) | 12% (32) | 38% (32) | 47% (32) | 31% (32) | 19% (32) | 34% (32) | 50% (32) | 31% (32) | 44% (32) | 31% (32) | - | 56% (32) | 41% (32) | 34% (32) | 66% (32) | 81% (32) | 66% (32) | 100% (32) | 100% (32) |
| 13 | `ab(d6,tt,ord,nb200k)@1.learned(s79,18f19059)@1.opener(rand,4)@1` | 12% (32) | 19% (32) | 19% (32) | 28% (32) | 31% (32) | 25% (32) | 28% (32) | 28% (32) | 44% (32) | 34% (32) | 41% (32) | 44% (32) | - | 59% (32) | 44% (32) | 59% (32) | 91% (32) | 75% (32) | 100% (32) | 100% (32) |
| 14 | `ab(d6,tt,ord,nb200k)@1.learned(s115,21d7e638)@1.opener(rand,4)@1` | 6% (32) | 9% (32) | 19% (32) | 25% (32) | 19% (32) | 31% (32) | 28% (32) | 28% (32) | 28% (32) | 31% (32) | 19% (32) | 59% (32) | 41% (32) | - | 56% (32) | 53% (32) | 91% (32) | 66% (32) | 100% (32) | 100% (32) |
| 15 | `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 16% (32) | 22% (32) | 12% (32) | 9% (32) | 16% (32) | 28% (32) | 19% (32) | 22% (32) | 53% (32) | 44% (32) | 41% (32) | 66% (32) | 56% (32) | 44% (32) | - | 41% (32) | 31% (32) | 88% (32) | 100% (32) | 100% (32) |
| 16 | `ab(d6,tt,ord,nb200k)@1.learned(s110,1466db6c)@1.opener(rand,4)@1` | 9% (32) | 19% (32) | 12% (32) | 28% (32) | 25% (32) | 31% (32) | 31% (32) | 34% (32) | 16% (32) | 38% (32) | 34% (32) | 34% (32) | 41% (32) | 47% (32) | 59% (32) | - | 78% (32) | 56% (32) | 100% (32) | 100% (32) |
| 17 | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 22% (32) | 44% (32) | 9% (32) | 9% (32) | 9% (32) | 34% (32) | 6% (32) | 12% (32) | 19% (32) | 9% (32) | 31% (32) | 19% (32) | 9% (32) | 9% (32) | 69% (32) | 22% (32) | - | 91% (32) | 100% (32) | 100% (32) |
| 18 | `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 0% (32) | 3% (32) | 22% (32) | 3% (32) | 9% (32) | 3% (32) | 9% (32) | 12% (32) | 41% (32) | 28% (32) | 28% (32) | 34% (32) | 25% (32) | 34% (32) | 12% (32) | 44% (32) | 9% (32) | - | 97% (32) | 100% (32) |
| 19 | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 3% (32) | - | 97% (32) |
| 20 | `rand@1` | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 0% (32) | 3% (32) | - |

## Per-agent match history (active agents)

### 1. `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` (Elo 1399 +/- 21)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 17-15 | 0.53 | 0.70 | -0.17 | 63 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 26-6 | 0.81 | 0.74 | +0.07 | 55 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 28-4 | 0.88 | 0.78 | +0.10 | 58 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 29-3 | 0.91 | 0.78 | +0.12 | 55 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 25-7 | 0.78 | 0.79 | -0.01 | 52 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 26-6 | 0.81 | 0.79 | +0.02 | 57 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 24-8 | 0.75 | 0.82 | -0.07 | 56 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 27-5 | 0.84 | 0.83 | +0.02 | 55 |
| `ab(d6,tt,ord,nb200k)@1.learned(s77,ddaa5090)@1.opener(rand,4)@1` | 32 | 29-3 | 0.91 | 0.85 | +0.05 | 58 |
| `ab(d6,tt,ord,nb200k)@1.learned(s78,2fa21eda)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.86 | +0.14 | 57 |
| `ab(d6,tt,ord,nb200k)@1.learned(s113,e3cc8b4e)@1.opener(rand,4)@1` | 32 | 30-2 | 0.94 | 0.89 | +0.04 | 58 |
| `ab(d6,tt,ord,nb200k)@1.learned(s79,18f19059)@1.opener(rand,4)@1` | 32 | 28-4 | 0.88 | 0.90 | -0.02 | 60 |
| `ab(d6,tt,ord,nb200k)@1.learned(s115,21d7e638)@1.opener(rand,4)@1` | 32 | 30-2 | 0.94 | 0.91 | +0.03 | 56 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 27-5 | 0.84 | 0.91 | -0.07 | 48 |
| `ab(d6,tt,ord,nb200k)@1.learned(s110,1466db6c)@1.opener(rand,4)@1` | 32 | 29-3 | 0.91 | 0.92 | -0.01 | 57 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 25-7 | 0.78 | 0.95 | -0.16 | 50 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.96 | +0.04 | 43 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 32 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 23 |

### 2. `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` (Elo 1249 +/- 17)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 15-17 | 0.47 | 0.30 | +0.17 | 63 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 24-8 | 0.75 | 0.55 | +0.20 | 59 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.60 | -0.19 | 58 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 22-10 | 0.69 | 0.60 | +0.09 | 60 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.62 | -0.02 | 61 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 17-15 | 0.53 | 0.62 | -0.09 | 57 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 15-17 | 0.47 | 0.66 | -0.19 | 59 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 22-10 | 0.69 | 0.67 | +0.02 | 58 |
| `ab(d6,tt,ord,nb200k)@1.learned(s77,ddaa5090)@1.opener(rand,4)@1` | 32 | 30-2 | 0.94 | 0.71 | +0.23 | 60 |
| `ab(d6,tt,ord,nb200k)@1.learned(s78,2fa21eda)@1.opener(rand,4)@1` | 32 | 21-11 | 0.66 | 0.72 | -0.07 | 62 |
| `ab(d6,tt,ord,nb200k)@1.learned(s113,e3cc8b4e)@1.opener(rand,4)@1` | 32 | 28-4 | 0.88 | 0.78 | +0.09 | 57 |
| `ab(d6,tt,ord,nb200k)@1.learned(s79,18f19059)@1.opener(rand,4)@1` | 32 | 26-6 | 0.81 | 0.78 | +0.03 | 62 |
| `ab(d6,tt,ord,nb200k)@1.learned(s115,21d7e638)@1.opener(rand,4)@1` | 32 | 29-3 | 0.91 | 0.81 | +0.09 | 55 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 25-7 | 0.78 | 0.82 | -0.03 | 52 |
| `ab(d6,tt,ord,nb200k)@1.learned(s110,1466db6c)@1.opener(rand,4)@1` | 32 | 26-6 | 0.81 | 0.82 | -0.01 | 61 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 18-14 | 0.56 | 0.88 | -0.32 | 56 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 31-1 | 0.97 | 0.91 | +0.06 | 53 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 37 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 31 |

### 3. `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` (Elo 1215 +/- 16)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 6-26 | 0.19 | 0.26 | -0.07 | 55 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 8-24 | 0.25 | 0.45 | -0.20 | 59 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.55 | +0.04 | 66 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.55 | +0.04 | 68 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.57 | -0.16 | 62 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 23-9 | 0.72 | 0.57 | +0.15 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 25-7 | 0.78 | 0.61 | +0.17 | 70 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.62 | -0.03 | 63 |
| `ab(d6,tt,ord,nb200k)@1.learned(s77,ddaa5090)@1.opener(rand,4)@1` | 32 | 18-14 | 0.56 | 0.66 | -0.10 | 62 |
| `ab(d6,tt,ord,nb200k)@1.learned(s78,2fa21eda)@1.opener(rand,4)@1` | 32 | 26-6 | 0.81 | 0.68 | +0.13 | 69 |
| `ab(d6,tt,ord,nb200k)@1.learned(s113,e3cc8b4e)@1.opener(rand,4)@1` | 32 | 20-12 | 0.62 | 0.75 | -0.12 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s79,18f19059)@1.opener(rand,4)@1` | 32 | 26-6 | 0.81 | 0.75 | +0.06 | 63 |
| `ab(d6,tt,ord,nb200k)@1.learned(s115,21d7e638)@1.opener(rand,4)@1` | 32 | 26-6 | 0.81 | 0.78 | +0.03 | 63 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 28-4 | 0.88 | 0.78 | +0.09 | 55 |
| `ab(d6,tt,ord,nb200k)@1.learned(s110,1466db6c)@1.opener(rand,4)@1` | 32 | 28-4 | 0.88 | 0.79 | +0.09 | 63 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 29-3 | 0.91 | 0.86 | +0.05 | 57 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 25-7 | 0.78 | 0.90 | -0.11 | 54 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.99 | +0.01 | 40 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 32 |

### 4. `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` (Elo 1181 +/- 16)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 4-28 | 0.12 | 0.22 | -0.10 | 58 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.40 | +0.19 | 58 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.45 | -0.04 | 66 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.50 | +0.09 | 68 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 18-14 | 0.56 | 0.52 | +0.04 | 59 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 20-12 | 0.62 | 0.52 | +0.10 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 15-17 | 0.47 | 0.56 | -0.10 | 66 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.58 | +0.02 | 66 |
| `ab(d6,tt,ord,nb200k)@1.learned(s77,ddaa5090)@1.opener(rand,4)@1` | 32 | 11-21 | 0.34 | 0.62 | -0.28 | 66 |
| `ab(d6,tt,ord,nb200k)@1.learned(s78,2fa21eda)@1.opener(rand,4)@1` | 32 | 20-12 | 0.62 | 0.64 | -0.01 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s113,e3cc8b4e)@1.opener(rand,4)@1` | 32 | 17-15 | 0.53 | 0.71 | -0.18 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s79,18f19059)@1.opener(rand,4)@1` | 32 | 23-9 | 0.72 | 0.71 | +0.01 | 69 |
| `ab(d6,tt,ord,nb200k)@1.learned(s115,21d7e638)@1.opener(rand,4)@1` | 32 | 24-8 | 0.75 | 0.75 | +0.00 | 66 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 29-3 | 0.91 | 0.75 | +0.16 | 49 |
| `ab(d6,tt,ord,nb200k)@1.learned(s110,1466db6c)@1.opener(rand,4)@1` | 32 | 23-9 | 0.72 | 0.75 | -0.04 | 69 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 29-3 | 0.91 | 0.83 | +0.07 | 56 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 31-1 | 0.97 | 0.88 | +0.09 | 49 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.99 | +0.01 | 35 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 28 |

### 5. `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` (Elo 1178 +/- 16)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 3-29 | 0.09 | 0.22 | -0.12 | 55 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 10-22 | 0.31 | 0.40 | -0.09 | 60 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.45 | -0.04 | 68 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.50 | -0.09 | 68 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.52 | +0.08 | 62 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 20-12 | 0.62 | 0.52 | +0.11 | 67 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 20-12 | 0.62 | 0.56 | +0.06 | 70 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.57 | +0.02 | 73 |
| `ab(d6,tt,ord,nb200k)@1.learned(s77,ddaa5090)@1.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.62 | -0.02 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s78,2fa21eda)@1.opener(rand,4)@1` | 32 | 17-15 | 0.53 | 0.64 | -0.10 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s113,e3cc8b4e)@1.opener(rand,4)@1` | 32 | 22-10 | 0.69 | 0.70 | -0.02 | 75 |
| `ab(d6,tt,ord,nb200k)@1.learned(s79,18f19059)@1.opener(rand,4)@1` | 32 | 22-10 | 0.69 | 0.71 | -0.02 | 70 |
| `ab(d6,tt,ord,nb200k)@1.learned(s115,21d7e638)@1.opener(rand,4)@1` | 32 | 26-6 | 0.81 | 0.74 | +0.07 | 72 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 27-5 | 0.84 | 0.75 | +0.10 | 54 |
| `ab(d6,tt,ord,nb200k)@1.learned(s110,1466db6c)@1.opener(rand,4)@1` | 32 | 24-8 | 0.75 | 0.75 | -0.00 | 69 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 29-3 | 0.91 | 0.83 | +0.08 | 58 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 29-3 | 0.91 | 0.87 | +0.03 | 52 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.99 | +0.01 | 36 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 29 |

### 6. `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` (Elo 1166 +/- 16)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 7-25 | 0.22 | 0.21 | +0.01 | 52 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.38 | +0.02 | 61 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.43 | +0.16 | 62 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 14-18 | 0.44 | 0.48 | -0.04 | 59 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.48 | -0.08 | 62 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 11-21 | 0.34 | 0.50 | -0.16 | 58 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 10-22 | 0.31 | 0.54 | -0.23 | 56 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 24-8 | 0.75 | 0.55 | +0.20 | 64 |
| `ab(d6,tt,ord,nb200k)@1.learned(s77,ddaa5090)@1.opener(rand,4)@1` | 32 | 21-11 | 0.66 | 0.60 | +0.06 | 62 |
| `ab(d6,tt,ord,nb200k)@1.learned(s78,2fa21eda)@1.opener(rand,4)@1` | 32 | 22-10 | 0.69 | 0.62 | +0.07 | 59 |
| `ab(d6,tt,ord,nb200k)@1.learned(s113,e3cc8b4e)@1.opener(rand,4)@1` | 32 | 26-6 | 0.81 | 0.69 | +0.12 | 61 |
| `ab(d6,tt,ord,nb200k)@1.learned(s79,18f19059)@1.opener(rand,4)@1` | 32 | 24-8 | 0.75 | 0.69 | +0.06 | 62 |
| `ab(d6,tt,ord,nb200k)@1.learned(s115,21d7e638)@1.opener(rand,4)@1` | 32 | 22-10 | 0.69 | 0.73 | -0.04 | 60 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 23-9 | 0.72 | 0.73 | -0.01 | 50 |
| `ab(d6,tt,ord,nb200k)@1.learned(s110,1466db6c)@1.opener(rand,4)@1` | 32 | 22-10 | 0.69 | 0.74 | -0.05 | 57 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 21-11 | 0.66 | 0.82 | -0.16 | 51 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 31-1 | 0.97 | 0.87 | +0.10 | 43 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.99 | +0.01 | 30 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 22 |

### 7. `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` (Elo 1164 +/- 16)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 6-26 | 0.19 | 0.21 | -0.02 | 57 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 15-17 | 0.47 | 0.38 | +0.09 | 57 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 9-23 | 0.28 | 0.43 | -0.15 | 68 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 12-20 | 0.38 | 0.48 | -0.10 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 12-20 | 0.38 | 0.48 | -0.11 | 67 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 21-11 | 0.66 | 0.50 | +0.16 | 58 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 18-14 | 0.56 | 0.54 | +0.02 | 71 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 14-18 | 0.44 | 0.55 | -0.11 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s77,ddaa5090)@1.opener(rand,4)@1` | 32 | 22-10 | 0.69 | 0.60 | +0.09 | 67 |
| `ab(d6,tt,ord,nb200k)@1.learned(s78,2fa21eda)@1.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.62 | -0.02 | 67 |
| `ab(d6,tt,ord,nb200k)@1.learned(s113,e3cc8b4e)@1.opener(rand,4)@1` | 32 | 21-11 | 0.66 | 0.69 | -0.03 | 66 |
| `ab(d6,tt,ord,nb200k)@1.learned(s79,18f19059)@1.opener(rand,4)@1` | 32 | 23-9 | 0.72 | 0.69 | +0.03 | 67 |
| `ab(d6,tt,ord,nb200k)@1.learned(s115,21d7e638)@1.opener(rand,4)@1` | 32 | 23-9 | 0.72 | 0.73 | -0.01 | 66 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 26-6 | 0.81 | 0.73 | +0.08 | 50 |
| `ab(d6,tt,ord,nb200k)@1.learned(s110,1466db6c)@1.opener(rand,4)@1` | 32 | 22-10 | 0.69 | 0.74 | -0.05 | 67 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 30-2 | 0.94 | 0.82 | +0.12 | 53 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 29-3 | 0.91 | 0.86 | +0.04 | 52 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.99 | +0.01 | 40 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 29 |

### 8. `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` (Elo 1136 +/- 16)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 8-24 | 0.25 | 0.18 | +0.07 | 56 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 17-15 | 0.53 | 0.34 | +0.19 | 59 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 7-25 | 0.22 | 0.39 | -0.17 | 70 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 17-15 | 0.53 | 0.44 | +0.10 | 66 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 12-20 | 0.38 | 0.44 | -0.06 | 70 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 22-10 | 0.69 | 0.46 | +0.23 | 56 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 14-18 | 0.44 | 0.46 | -0.02 | 71 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 12-20 | 0.38 | 0.51 | -0.14 | 70 |
| `ab(d6,tt,ord,nb200k)@1.learned(s77,ddaa5090)@1.opener(rand,4)@1` | 32 | 16-16 | 0.50 | 0.56 | -0.06 | 69 |
| `ab(d6,tt,ord,nb200k)@1.learned(s78,2fa21eda)@1.opener(rand,4)@1` | 32 | 12-20 | 0.38 | 0.58 | -0.20 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s113,e3cc8b4e)@1.opener(rand,4)@1` | 32 | 16-16 | 0.50 | 0.65 | -0.15 | 70 |
| `ab(d6,tt,ord,nb200k)@1.learned(s79,18f19059)@1.opener(rand,4)@1` | 32 | 23-9 | 0.72 | 0.65 | +0.06 | 66 |
| `ab(d6,tt,ord,nb200k)@1.learned(s115,21d7e638)@1.opener(rand,4)@1` | 32 | 23-9 | 0.72 | 0.70 | +0.02 | 66 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 25-7 | 0.78 | 0.70 | +0.08 | 52 |
| `ab(d6,tt,ord,nb200k)@1.learned(s110,1466db6c)@1.opener(rand,4)@1` | 32 | 21-11 | 0.66 | 0.70 | -0.05 | 69 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 28-4 | 0.88 | 0.79 | +0.08 | 57 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 28-4 | 0.88 | 0.84 | +0.03 | 52 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.99 | +0.01 | 35 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 28 |

### 9. `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` (Elo 1128 +/- 16)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 5-27 | 0.16 | 0.17 | -0.02 | 55 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 10-22 | 0.31 | 0.33 | -0.02 | 58 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.38 | +0.03 | 63 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.42 | -0.02 | 66 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.43 | -0.02 | 73 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 8-24 | 0.25 | 0.45 | -0.20 | 64 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 18-14 | 0.56 | 0.45 | +0.11 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 20-12 | 0.62 | 0.49 | +0.14 | 70 |
| `ab(d6,tt,ord,nb200k)@1.learned(s77,ddaa5090)@1.opener(rand,4)@1` | 32 | 22-10 | 0.69 | 0.55 | +0.14 | 71 |
| `ab(d6,tt,ord,nb200k)@1.learned(s78,2fa21eda)@1.opener(rand,4)@1` | 32 | 23-9 | 0.72 | 0.57 | +0.15 | 67 |
| `ab(d6,tt,ord,nb200k)@1.learned(s113,e3cc8b4e)@1.opener(rand,4)@1` | 32 | 22-10 | 0.69 | 0.64 | +0.05 | 67 |
| `ab(d6,tt,ord,nb200k)@1.learned(s79,18f19059)@1.opener(rand,4)@1` | 32 | 18-14 | 0.56 | 0.64 | -0.08 | 69 |
| `ab(d6,tt,ord,nb200k)@1.learned(s115,21d7e638)@1.opener(rand,4)@1` | 32 | 23-9 | 0.72 | 0.69 | +0.03 | 68 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 15-17 | 0.47 | 0.69 | -0.22 | 58 |
| `ab(d6,tt,ord,nb200k)@1.learned(s110,1466db6c)@1.opener(rand,4)@1` | 32 | 27-5 | 0.84 | 0.69 | +0.15 | 70 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 26-6 | 0.81 | 0.78 | +0.03 | 61 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.84 | -0.24 | 58 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.99 | +0.01 | 41 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 30 |

### 10. `ab(d6,tt,ord,nb200k)@1.learned(s77,ddaa5090)@1.opener(rand,4)@1` (Elo 1096 +/- 16)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 3-29 | 0.09 | 0.15 | -0.05 | 58 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 2-30 | 0.06 | 0.29 | -0.23 | 60 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 14-18 | 0.44 | 0.34 | +0.10 | 62 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 21-11 | 0.66 | 0.38 | +0.28 | 66 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.38 | +0.02 | 68 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 11-21 | 0.34 | 0.40 | -0.06 | 62 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 10-22 | 0.31 | 0.40 | -0.09 | 67 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 16-16 | 0.50 | 0.44 | +0.06 | 69 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 10-22 | 0.31 | 0.45 | -0.14 | 71 |
| `ab(d6,tt,ord,nb200k)@1.learned(s78,2fa21eda)@1.opener(rand,4)@1` | 32 | 20-12 | 0.62 | 0.52 | +0.10 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s113,e3cc8b4e)@1.opener(rand,4)@1` | 32 | 18-14 | 0.56 | 0.60 | -0.03 | 67 |
| `ab(d6,tt,ord,nb200k)@1.learned(s79,18f19059)@1.opener(rand,4)@1` | 32 | 21-11 | 0.66 | 0.60 | +0.06 | 70 |
| `ab(d6,tt,ord,nb200k)@1.learned(s115,21d7e638)@1.opener(rand,4)@1` | 32 | 22-10 | 0.69 | 0.64 | +0.04 | 73 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 18-14 | 0.56 | 0.65 | -0.08 | 61 |
| `ab(d6,tt,ord,nb200k)@1.learned(s110,1466db6c)@1.opener(rand,4)@1` | 32 | 20-12 | 0.62 | 0.65 | -0.03 | 73 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 29-3 | 0.91 | 0.75 | +0.15 | 63 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 23-9 | 0.72 | 0.81 | -0.09 | 57 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.99 | +0.01 | 39 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 33 |

### 11. `ab(d6,tt,ord,nb200k)@1.learned(s78,2fa21eda)@1.opener(rand,4)@1` (Elo 1081 +/- 16)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.14 | -0.14 | 57 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 11-21 | 0.34 | 0.28 | +0.07 | 62 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 6-26 | 0.19 | 0.32 | -0.13 | 69 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 12-20 | 0.38 | 0.36 | +0.01 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 15-17 | 0.47 | 0.36 | +0.10 | 68 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 10-22 | 0.31 | 0.38 | -0.07 | 59 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.38 | +0.02 | 67 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 20-12 | 0.62 | 0.42 | +0.20 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 9-23 | 0.28 | 0.43 | -0.15 | 67 |
| `ab(d6,tt,ord,nb200k)@1.learned(s77,ddaa5090)@1.opener(rand,4)@1` | 32 | 12-20 | 0.38 | 0.48 | -0.10 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s113,e3cc8b4e)@1.opener(rand,4)@1` | 32 | 22-10 | 0.69 | 0.58 | +0.11 | 70 |
| `ab(d6,tt,ord,nb200k)@1.learned(s79,18f19059)@1.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.58 | +0.01 | 70 |
| `ab(d6,tt,ord,nb200k)@1.learned(s115,21d7e638)@1.opener(rand,4)@1` | 32 | 26-6 | 0.81 | 0.62 | +0.19 | 73 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.63 | -0.03 | 55 |
| `ab(d6,tt,ord,nb200k)@1.learned(s110,1466db6c)@1.opener(rand,4)@1` | 32 | 21-11 | 0.66 | 0.63 | +0.02 | 70 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 22-10 | 0.69 | 0.74 | -0.05 | 62 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 23-9 | 0.72 | 0.80 | -0.08 | 56 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.99 | +0.01 | 39 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 31 |

### 12. `ab(d6,tt,ord,nb200k)@1.learned(s113,e3cc8b4e)@1.opener(rand,4)@1` (Elo 1027 +/- 16)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 2-30 | 0.06 | 0.11 | -0.04 | 58 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 4-28 | 0.12 | 0.22 | -0.09 | 57 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 12-20 | 0.38 | 0.25 | +0.12 | 68 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 15-17 | 0.47 | 0.29 | +0.18 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 10-22 | 0.31 | 0.30 | +0.02 | 75 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 6-26 | 0.19 | 0.31 | -0.12 | 61 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 11-21 | 0.34 | 0.31 | +0.03 | 66 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 16-16 | 0.50 | 0.35 | +0.15 | 70 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 10-22 | 0.31 | 0.36 | -0.05 | 67 |
| `ab(d6,tt,ord,nb200k)@1.learned(s77,ddaa5090)@1.opener(rand,4)@1` | 32 | 14-18 | 0.44 | 0.40 | +0.03 | 67 |
| `ab(d6,tt,ord,nb200k)@1.learned(s78,2fa21eda)@1.opener(rand,4)@1` | 32 | 10-22 | 0.31 | 0.42 | -0.11 | 70 |
| `ab(d6,tt,ord,nb200k)@1.learned(s79,18f19059)@1.opener(rand,4)@1` | 32 | 18-14 | 0.56 | 0.50 | +0.06 | 69 |
| `ab(d6,tt,ord,nb200k)@1.learned(s115,21d7e638)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.55 | -0.14 | 73 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 11-21 | 0.34 | 0.55 | -0.21 | 59 |
| `ab(d6,tt,ord,nb200k)@1.learned(s110,1466db6c)@1.opener(rand,4)@1` | 32 | 21-11 | 0.66 | 0.56 | +0.10 | 67 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 26-6 | 0.81 | 0.67 | +0.14 | 61 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 21-11 | 0.66 | 0.74 | -0.09 | 58 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.98 | +0.02 | 40 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 30 |

### 13. `ab(d6,tt,ord,nb200k)@1.learned(s79,18f19059)@1.opener(rand,4)@1` (Elo 1025 +/- 16)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 4-28 | 0.12 | 0.10 | +0.02 | 60 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 6-26 | 0.19 | 0.22 | -0.03 | 62 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 6-26 | 0.19 | 0.25 | -0.06 | 63 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 9-23 | 0.28 | 0.29 | -0.01 | 69 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 10-22 | 0.31 | 0.29 | +0.02 | 70 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 8-24 | 0.25 | 0.31 | -0.06 | 62 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 9-23 | 0.28 | 0.31 | -0.03 | 67 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 9-23 | 0.28 | 0.35 | -0.06 | 66 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 14-18 | 0.44 | 0.36 | +0.08 | 69 |
| `ab(d6,tt,ord,nb200k)@1.learned(s77,ddaa5090)@1.opener(rand,4)@1` | 32 | 11-21 | 0.34 | 0.40 | -0.06 | 70 |
| `ab(d6,tt,ord,nb200k)@1.learned(s78,2fa21eda)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.42 | -0.01 | 70 |
| `ab(d6,tt,ord,nb200k)@1.learned(s113,e3cc8b4e)@1.opener(rand,4)@1` | 32 | 14-18 | 0.44 | 0.50 | -0.06 | 69 |
| `ab(d6,tt,ord,nb200k)@1.learned(s115,21d7e638)@1.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.55 | +0.05 | 72 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 14-18 | 0.44 | 0.55 | -0.11 | 59 |
| `ab(d6,tt,ord,nb200k)@1.learned(s110,1466db6c)@1.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.56 | +0.04 | 70 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 29-3 | 0.91 | 0.67 | +0.24 | 60 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 24-8 | 0.75 | 0.74 | +0.01 | 58 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.98 | +0.02 | 40 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 33 |

### 14. `ab(d6,tt,ord,nb200k)@1.learned(s115,21d7e638)@1.opener(rand,4)@1` (Elo 993 +/- 16)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 2-30 | 0.06 | 0.09 | -0.03 | 56 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 3-29 | 0.09 | 0.19 | -0.09 | 55 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 6-26 | 0.19 | 0.22 | -0.03 | 63 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 8-24 | 0.25 | 0.25 | -0.00 | 66 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 6-26 | 0.19 | 0.26 | -0.07 | 72 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 10-22 | 0.31 | 0.27 | +0.04 | 60 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 9-23 | 0.28 | 0.27 | +0.01 | 66 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 9-23 | 0.28 | 0.30 | -0.02 | 66 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 9-23 | 0.28 | 0.31 | -0.03 | 68 |
| `ab(d6,tt,ord,nb200k)@1.learned(s77,ddaa5090)@1.opener(rand,4)@1` | 32 | 10-22 | 0.31 | 0.36 | -0.04 | 73 |
| `ab(d6,tt,ord,nb200k)@1.learned(s78,2fa21eda)@1.opener(rand,4)@1` | 32 | 6-26 | 0.19 | 0.38 | -0.19 | 73 |
| `ab(d6,tt,ord,nb200k)@1.learned(s113,e3cc8b4e)@1.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.45 | +0.14 | 73 |
| `ab(d6,tt,ord,nb200k)@1.learned(s79,18f19059)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.45 | -0.05 | 72 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 18-14 | 0.56 | 0.50 | +0.06 | 61 |
| `ab(d6,tt,ord,nb200k)@1.learned(s110,1466db6c)@1.opener(rand,4)@1` | 32 | 17-15 | 0.53 | 0.51 | +0.02 | 72 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 29-3 | 0.91 | 0.63 | +0.28 | 62 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 21-11 | 0.66 | 0.70 | -0.05 | 60 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.98 | +0.02 | 41 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 33 |

### 15. `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` (Elo 991 +/- 16)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 5-27 | 0.16 | 0.09 | +0.07 | 48 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 7-25 | 0.22 | 0.18 | +0.03 | 52 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 4-28 | 0.12 | 0.22 | -0.09 | 55 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 3-29 | 0.09 | 0.25 | -0.16 | 49 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 5-27 | 0.16 | 0.25 | -0.10 | 54 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 9-23 | 0.28 | 0.27 | +0.01 | 50 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 6-26 | 0.19 | 0.27 | -0.08 | 50 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 7-25 | 0.22 | 0.30 | -0.08 | 52 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 17-15 | 0.53 | 0.31 | +0.22 | 58 |
| `ab(d6,tt,ord,nb200k)@1.learned(s77,ddaa5090)@1.opener(rand,4)@1` | 32 | 14-18 | 0.44 | 0.35 | +0.08 | 61 |
| `ab(d6,tt,ord,nb200k)@1.learned(s78,2fa21eda)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.37 | +0.03 | 55 |
| `ab(d6,tt,ord,nb200k)@1.learned(s113,e3cc8b4e)@1.opener(rand,4)@1` | 32 | 21-11 | 0.66 | 0.45 | +0.21 | 59 |
| `ab(d6,tt,ord,nb200k)@1.learned(s79,18f19059)@1.opener(rand,4)@1` | 32 | 18-14 | 0.56 | 0.45 | +0.11 | 59 |
| `ab(d6,tt,ord,nb200k)@1.learned(s115,21d7e638)@1.opener(rand,4)@1` | 32 | 14-18 | 0.44 | 0.50 | -0.06 | 61 |
| `ab(d6,tt,ord,nb200k)@1.learned(s110,1466db6c)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.51 | -0.10 | 60 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 10-22 | 0.31 | 0.62 | -0.31 | 44 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 28-4 | 0.88 | 0.70 | +0.17 | 45 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.98 | +0.02 | 29 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 21 |

### 16. `ab(d6,tt,ord,nb200k)@1.learned(s110,1466db6c)@1.opener(rand,4)@1` (Elo 986 +/- 16)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 3-29 | 0.09 | 0.08 | +0.01 | 57 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 6-26 | 0.19 | 0.18 | +0.01 | 61 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 4-28 | 0.12 | 0.21 | -0.09 | 63 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 9-23 | 0.28 | 0.25 | +0.04 | 69 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 8-24 | 0.25 | 0.25 | +0.00 | 69 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 10-22 | 0.31 | 0.26 | +0.05 | 57 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 10-22 | 0.31 | 0.26 | +0.05 | 67 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 11-21 | 0.34 | 0.30 | +0.05 | 69 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 5-27 | 0.16 | 0.31 | -0.15 | 70 |
| `ab(d6,tt,ord,nb200k)@1.learned(s77,ddaa5090)@1.opener(rand,4)@1` | 32 | 12-20 | 0.38 | 0.35 | +0.03 | 73 |
| `ab(d6,tt,ord,nb200k)@1.learned(s78,2fa21eda)@1.opener(rand,4)@1` | 32 | 11-21 | 0.34 | 0.37 | -0.02 | 70 |
| `ab(d6,tt,ord,nb200k)@1.learned(s113,e3cc8b4e)@1.opener(rand,4)@1` | 32 | 11-21 | 0.34 | 0.44 | -0.10 | 67 |
| `ab(d6,tt,ord,nb200k)@1.learned(s79,18f19059)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.44 | -0.04 | 70 |
| `ab(d6,tt,ord,nb200k)@1.learned(s115,21d7e638)@1.opener(rand,4)@1` | 32 | 15-17 | 0.47 | 0.49 | -0.02 | 72 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 19-13 | 0.59 | 0.49 | +0.10 | 60 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 25-7 | 0.78 | 0.62 | +0.17 | 61 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 18-14 | 0.56 | 0.70 | -0.13 | 56 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.98 | +0.02 | 36 |
| `rand@1` | 32 | 32-0 | 1.00 | 1.00 | +0.00 | 30 |

### 17. `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` (Elo 903 +/- 18)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 7-25 | 0.22 | 0.05 | +0.16 | 50 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 14-18 | 0.44 | 0.12 | +0.32 | 56 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 3-29 | 0.09 | 0.14 | -0.05 | 57 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 3-29 | 0.09 | 0.17 | -0.07 | 56 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 3-29 | 0.09 | 0.17 | -0.08 | 58 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 11-21 | 0.34 | 0.18 | +0.16 | 51 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 2-30 | 0.06 | 0.18 | -0.12 | 53 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 4-28 | 0.12 | 0.21 | -0.08 | 57 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 6-26 | 0.19 | 0.22 | -0.03 | 61 |
| `ab(d6,tt,ord,nb200k)@1.learned(s77,ddaa5090)@1.opener(rand,4)@1` | 32 | 3-29 | 0.09 | 0.25 | -0.15 | 63 |
| `ab(d6,tt,ord,nb200k)@1.learned(s78,2fa21eda)@1.opener(rand,4)@1` | 32 | 10-22 | 0.31 | 0.26 | +0.05 | 62 |
| `ab(d6,tt,ord,nb200k)@1.learned(s113,e3cc8b4e)@1.opener(rand,4)@1` | 32 | 6-26 | 0.19 | 0.33 | -0.14 | 61 |
| `ab(d6,tt,ord,nb200k)@1.learned(s79,18f19059)@1.opener(rand,4)@1` | 32 | 3-29 | 0.09 | 0.33 | -0.24 | 60 |
| `ab(d6,tt,ord,nb200k)@1.learned(s115,21d7e638)@1.opener(rand,4)@1` | 32 | 3-29 | 0.09 | 0.37 | -0.28 | 62 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 22-10 | 0.69 | 0.38 | +0.31 | 44 |
| `ab(d6,tt,ord,nb200k)@1.learned(s110,1466db6c)@1.opener(rand,4)@1` | 32 | 7-25 | 0.22 | 0.38 | -0.17 | 61 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 29-3 | 0.91 | 0.59 | +0.32 | 49 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 32-0 | 1.00 | 0.97 | +0.03 | 29 |
| `rand@1` | 32 | 32-0 | 1.00 | 0.99 | +0.01 | 21 |

### 18. `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` (Elo 842 +/- 19)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.04 | -0.04 | 43 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 1-31 | 0.03 | 0.09 | -0.06 | 53 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 7-25 | 0.22 | 0.10 | +0.11 | 54 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 1-31 | 0.03 | 0.12 | -0.09 | 49 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 3-29 | 0.09 | 0.13 | -0.03 | 52 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 1-31 | 0.03 | 0.13 | -0.10 | 43 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 3-29 | 0.09 | 0.14 | -0.04 | 52 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 4-28 | 0.12 | 0.16 | -0.03 | 52 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 13-19 | 0.41 | 0.16 | +0.24 | 58 |
| `ab(d6,tt,ord,nb200k)@1.learned(s77,ddaa5090)@1.opener(rand,4)@1` | 32 | 9-23 | 0.28 | 0.19 | +0.09 | 57 |
| `ab(d6,tt,ord,nb200k)@1.learned(s78,2fa21eda)@1.opener(rand,4)@1` | 32 | 9-23 | 0.28 | 0.20 | +0.08 | 56 |
| `ab(d6,tt,ord,nb200k)@1.learned(s113,e3cc8b4e)@1.opener(rand,4)@1` | 32 | 11-21 | 0.34 | 0.26 | +0.09 | 58 |
| `ab(d6,tt,ord,nb200k)@1.learned(s79,18f19059)@1.opener(rand,4)@1` | 32 | 8-24 | 0.25 | 0.26 | -0.01 | 58 |
| `ab(d6,tt,ord,nb200k)@1.learned(s115,21d7e638)@1.opener(rand,4)@1` | 32 | 11-21 | 0.34 | 0.30 | +0.05 | 60 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 4-28 | 0.12 | 0.30 | -0.17 | 45 |
| `ab(d6,tt,ord,nb200k)@1.learned(s110,1466db6c)@1.opener(rand,4)@1` | 32 | 14-18 | 0.44 | 0.30 | +0.13 | 56 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 3-29 | 0.09 | 0.41 | -0.32 | 49 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 31-1 | 0.97 | 0.95 | +0.02 | 33 |
| `rand@1` | 32 | 32-0 | 1.00 | 0.99 | +0.01 | 24 |

### 19. `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` (Elo 323 +/- 50)

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 32 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 37 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.01 | -0.01 | 40 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.01 | -0.01 | 35 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.01 | -0.01 | 36 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.01 | -0.01 | 30 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.01 | -0.01 | 40 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.01 | -0.01 | 35 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.01 | -0.01 | 41 |
| `ab(d6,tt,ord,nb200k)@1.learned(s77,ddaa5090)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.01 | -0.01 | 39 |
| `ab(d6,tt,ord,nb200k)@1.learned(s78,2fa21eda)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.01 | -0.01 | 39 |
| `ab(d6,tt,ord,nb200k)@1.learned(s113,e3cc8b4e)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.02 | -0.02 | 40 |
| `ab(d6,tt,ord,nb200k)@1.learned(s79,18f19059)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.02 | -0.02 | 40 |
| `ab(d6,tt,ord,nb200k)@1.learned(s115,21d7e638)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.02 | -0.02 | 41 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.02 | -0.02 | 29 |
| `ab(d6,tt,ord,nb200k)@1.learned(s110,1466db6c)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.02 | -0.02 | 36 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.03 | -0.03 | 29 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 1-31 | 0.03 | 0.05 | -0.02 | 33 |
| `rand@1` | 32 | 31-1 | 0.97 | 0.87 | +0.10 | 36 |

### 20. `rand@1` (Elo 0 (anchor))

| opponent | games | W-L | score | expected | delta | avg plies |
|---|---:|---:|---:|---:|---:|---:|
| `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 23 |
| `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 31 |
| `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 32 |
| `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 28 |
| `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 29 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 22 |
| `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 29 |
| `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 28 |
| `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 30 |
| `ab(d6,tt,ord,nb200k)@1.learned(s77,ddaa5090)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 33 |
| `ab(d6,tt,ord,nb200k)@1.learned(s78,2fa21eda)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 31 |
| `ab(d6,tt,ord,nb200k)@1.learned(s113,e3cc8b4e)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 30 |
| `ab(d6,tt,ord,nb200k)@1.learned(s79,18f19059)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 33 |
| `ab(d6,tt,ord,nb200k)@1.learned(s115,21d7e638)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 33 |
| `ab(d4)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 21 |
| `ab(d6,tt,ord,nb200k)@1.learned(s110,1466db6c)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.00 | -0.00 | 30 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d4)@1.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.01 | -0.01 | 21 |
| `ab(d2)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` | 32 | 0-32 | 0.00 | 0.01 | -0.01 | 24 |
| `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r63)@1.opener(rand,4)@1` | 32 | 1-31 | 0.03 | 0.13 | -0.10 | 36 |

