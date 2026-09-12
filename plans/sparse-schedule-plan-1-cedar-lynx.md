# Plan: rate agents on a sparse opponent graph instead of a round robin

Written 2026-09-12, during Round 4's rung 16. Nothing here has been run yet.
The developer's question: a round robin's game count grows with the square of
the pool, while the games one agent needs for a given rating precision should
not depend on how many other agents exist. Could each agent play a random
subset of opponents instead?

## What a round robin costs today

Round 4 (`plans/track-migration-plan-1-slate-kestrel.md`) plays every pair
touching one of 198 category agents in a pool of 426 active agents:

| quantity | value |
|---|---|
| pairs scheduled | about 65,000 (198 x 197 / 2 among the cohort, plus 198 x 228 against the rest) |
| games scheduled by rung 16 alone (8 more per pair) | 477,944 |
| games per agent at rung 8, median per cell (`ranking/rungs/cores_r8.tsv`) | 2,734 to 3,592 |
| printed pm at rung 8, median per cell | 7 to 8 Elo |
| measured throughput at rung 16 | about 17,700 games per hour on 10 workers |

Adding a core adds 6 category agents, and each plays every other active
agent, so the next core costs about 6 x 426 x 16 = 41,000 games at 16 per
pair, and that number rises with every agent the pool gains.

## What the theory says

- **Shah, Balakrishnan, Bradley, Parekh, Ramchandran, Wainwright (JMLR
  2016)**, read from the full text. For Bradley-Terry and Thurstone models
  the squared error of the whole score vector is governed by the spectrum of
  the comparison graph's Laplacian. With d items and n comparisons in total,
  the complete graph's minimax risk is Theta(d^2 / n), and a constant-degree
  expander's is the same order: "The evaluation of this class of graphs with
  respect to the minimax risk is identical to that of complete graphs". Per
  item that is Theta(d / n), which depends only on comparisons per item. The
  paper then notes that "the degree-k expander requires n >= kd samples while
  the complete graph requires n >= (d choose 2) samples, so in practical
  applications at least for small sample sizes we should prefer a low-degree
  expander". Paths and cycles are strictly worse (Theta(d^3 / n) or more), so
  the graph must be well connected, not merely connected. A random graph of
  moderate degree is an expander with high probability.
- **Fisher information per game is p(1 - p)** (`Docs/ranking-workflow.md`),
  so per-agent SE is about 173.7 / sqrt(G x mean p(1 - p)) for G games.
  Round 4's pool averages 0.1642. At that value G = 500 gives 19.4 Elo,
  G = 1,000 gives 13.7, G = 2,000 gives 9.7. None of this depends on N.
- **Chiang et al. 2024 (Chatbot Arena)**, read from the HTML full text.
  Over 50 models and about 240,000 votes, about 8,000 per model, so a sparse
  and uneven comparison graph. Bradley-Terry by maximum likelihood with
  sandwich standard errors. Pairs are chosen by an active rule that favours
  the pairs whose confidence intervals shrink most, which their Figure 7
  reports needs 54% fewer samples than random sampling for estimating the
  win matrix.
- **Heckel, Shah, Ramchandran, Wainwright (Annals of Statistics 2019)**,
  abstract only. An active procedure that picks the next pair from current
  confidence intervals recovers a ranking with a number of comparisons
  optimal up to log factors.
- **Hunter (Annals of Statistics 2004)**, abstract only. The MM algorithm for
  Bradley-Terry maximum likelihood converges to the unique estimate under
  stated conditions. The condition usually quoted for this (not read from
  the paper here) is that for every split of the agents into two groups,
  some agent in each group has beaten some agent in the other.

So the developer's intuition matches the theory: at equal total games, a
well-connected sparse graph rates every agent about as well as the round
robin, and the round robin only adds the requirement that every pair be
filled.

## Where this project differs from the theory

1. **Deterministic pairs cap information per opponent.** Two agents that
   consume no `rand()` replay one game per colour, so the scheduler stops at 2
   games per pair (`pairGameTarget`, `Docs/benchmarking.md` defect 3). Every
   openless category agent is deterministic, so against the other openless
   agents its information comes from the number of opponents, not from games
   per pair. An openless agent meeting 64 deterministic opponents has 128
   distinct games from them, which at p(1 - p) = 0.164 is an SE of about 38
   Elo from those games alone. For these agents the round robin's breadth is
   doing real work, and a sparse design has to give them more opponents or
   diversify the start (which would change what the openless division
   measures).
2. **Opponent mix is a second error source.** Ratings here depend on which
   styles an agent met: the 2026-08-01 TD-Leaf cohort beat `learned-other`
   agents 70.5% but the chip counter 63.6% (`tools/CLAUDE.md`,
   `rank.exe matchup`). A random subset of opponents adds variance from that
   mix, which the fit's pm does not include. Stratifying the draw (so many
   opponents from each Elo band and each regime) should hold it down.
3. **Title claims need dense pairs at the top.** `CLAUDE.md`'s ranking rule
   2 requires every contender pair at >= 32 games, since conclusions have
   inverted between 8 and 32. A sparse design for the bulk of the pool and a
   dense block among contenders is the natural split.
4. **Connection to the `rand@1` anchor.** The low end of the roster exists to
   spread the ladder. Any design has to keep every agent connected to it
   through well-separated opponents, not only near-equal ones.

## Methods considered

| method | games | what it gives up |
|---|---|---|
| round robin (today) | grows with the square of the pool | nothing, at the highest cost |
| random opponent graph, degree k, one joint fit | N x k x g / 2, linear in N | per-agent error from the opponent mix, deterministic agents' distinct games |
| stratified random graph (k opponents spread over Elo bands and regimes) | same | needs a prior rating to stratify by (the previous fit) |
| fixed reference panel, pinned fit (Workflow A, the replication study's design) | N x panel size x g, linear | cohort agents meet each other only through the panel, and every rating is relative to the panel's style mix |
| adaptive: a sparse first pass, then games where the uncertainty that matters is largest (Chatbot Arena, Heckel 2019, the per-pair ladder idea in `plans/ranking-run-scheduling-plan-1-tidal-lantern.md`) | smallest for a given target | bookkeeping, and the design becomes data-dependent |
| Swiss-style pairing near equal ratings | linear | at most 0.25 / 0.1642 = 1.5x more information per game than today, and it weakens connection to far-apart agents |

## The test: subsample Round 4's own games, no new play

Round 4's rung-16 store holds, for every cohort agent, up to 16 games against
each of 425 opponents. Every sparse design is a subset of that, so each can
be fitted and compared against the full data without playing a game.

- **Designs.** Opponents per agent k in {8, 16, 32, 64, 128, 425}, crossed
  with games per pair g in {2, 4, 8, 16} (the first g games of each pair in
  store order, so colours stay balanced). Opponents drawn at random, and a
  second series drawn stratified by the rung-8 Elo band and by regime. Each
  design at 3 random draws.
- **Fit.** The same fit Round 4's convergence test uses: `rank.exe rate
  --pin ranking/rungs/pin_source_standings_20260906.tsv` over a store made of
  every non-cohort row plus the design's cohort rows (an index file listing a
  shared part and the design's part, so nothing large is copied per design).
- **Measured against the full rung-16 pinned fit, over the 198 agents and
  the 33 cores:** Spearman rho, RMS and maximum Elo difference, the 6 cell
  champions, the core-order criteria of `analysis/rung_convergence.py`, and
  the ratio of each agent's actual difference to its printed pm (whether pm
  stays honest in a sparse design).
- **Measured against itself:** two draws at the same (k, g) with no pair in
  common, fitted separately. Their disagreement is the design's own error
  with no shared data, which the comparison against the full fit cannot
  give, since a subset agrees with the data it came from.
- **Reported by games per agent**, so every design sits on one axis next to
  the round robin at the same budget. Openless agents are reported
  separately, because of point 1.

Runs after Round 4's rung 16 finishes, since the fits are CPU load and the
`time=25ms` track's games are sensitive to it.

## Decision the test feeds

Whether to add a sparse mode to the scheduler: `rank.exe play --opponents K`
(and a stratified variant), choosing each agent's opponents from a hash of
the run seed and the two ids, so the graph is fixed across ladder rungs and
across runs, an agent joining later gets its own K edges, and existing edges
never change. Rule 2's dense contender block would stay as it is. That is a
scheduler change with tests, planned after the numbers are in, not before.
