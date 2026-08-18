# Gumbel AlphaZero bot -- Slice 2: search-shape knobs (cvisit/cscale/m)

## Context

Pass 2 (`plans/gumbel-mcts-plan-3-amber-thicket.md` /
`...-results-3-amber-thicket.md`) found sims has an interior optimum but
identified only ONE lever for game-time compute. In discussion after Pass 2
shipped, the developer asked whether alpha-beta search or any other kind of
"depth" could apply to Gumbel MCTS, and then specifically whether
`ai_gumbel.cpp`'s hardcoded search-shape constants (`kGumbelCVisit`,
`kGumbelCScale`, `kGumbelM` -- the root Gumbel-top-k candidate count) could
become per-agent levers the way `sims` already is. The developer then asked
directly: **"Please update the gumbel agents to a new version that supports
more configurations. Then try to identify best values for these in the same
way you explored values for the training."**

This plan covers both halves: (1) exposing the three constants as per-agent
roster knobs, and (2) a screening-level sweep to find good values, run
against an already-trained Pass-2 checkpoint with no retraining.

## Design

### Part 1: exposing the knobs

Investigated via a read-only Explore pass over `ai_gumbel.h`/`.cpp`,
`agents.h`/`.cpp`, `ranking.cpp` (the `ab(deep=K,...)` multi-parameter head
as the precedent for growing an ID grammar), and the existing test coverage.
Key finding: `gumbelSigma`/`gumbelSelectAction`/`gumbelHalvingRounds` are
already pure, explicitly-parameterized functions; only `gumbelSearch` (and
`gumbelSimulate`'s recursive non-root descent, and `gumbelImprovedPolicy`)
hardcode the three constants at four call sites.

Two designs were considered:

1. **Thread new parameters through `gumbelSearch`'s signature**, transported
   from `AgentSpec` to the explorer via the generic `params[]` int array
   (the channel `gaz` already repurposes for `modelSlot`).
2. **New global state** (`g_gumbelCVisit`/`g_gumbelCScale`/`g_gumbelRootM`),
   set/restored by `agentChooseMove` around the explorer call -- exactly
   mirroring how `AlphaBeta`'s own per-search toggles
   (`g_useTT`/`g_useMoveOrder`/`g_aspirationWindow`) already work.

Design 2 was chosen after reading `agents.cpp`'s `agentChooseMove`: it
already saves/sets/restores a block of globals unconditionally around every
explorer call, for exactly this purpose ("per-search feature toggles...so an
agent can enable/disable an optimization for ablation comparisons").
`gumbelSimulate`'s non-root read is inside RECURSION, so threading new
parameters through its call signature would touch every stack frame; a
global sidesteps that entirely and needed zero signature changes anywhere
(`gumbelSearch`, `gumbelExplore`, and every existing direct caller --
`ml_gumbelzero.cpp`'s trainer, `tests/test_gumbel.cpp`'s positional-arg
test -- are untouched).

**ID grammar:** `gaz(sims=N[,cvisit=C][,cscale=S][,m=M])@1`, mirroring
`ab(deep=K,...)`'s pattern exactly: `sims=` stays the mandatory first
positional argument, the three new flags are single-spelling label-table
entries (no legacy alias, matching `risk=`/`sims=`'s own precedent),
appended by `rankAgentId` only when non-default so a bare `gaz(sims=N)@1`
id is byte-identical to before. `cscale=` is spelled in TENTHS
(`cscale=10` -> 1.0), matching `learned()`'s existing `risk=<tenths>`
convention, so no new floating-point parsing primitive was needed anywhere
in `ranking.cpp`.

**No version bump** on the `gaz` row in `g_rkExplorers`: an id with none of
the new flags present is unaffected (the globals default to the paper's own
values, 50/1.0/16, matching what was previously hardcoded), the same way
`ab()`'s own optional flags (`tt`, `ord`, `qs`, `margin=`, `nodes=`, ...)
accumulated over the project's history without ever bumping `ab`'s version.

**Scope boundary, stated explicitly:** this is a SERVING-time change only.
`ml_gumbelzero.cpp`'s self-play trainer calls `gumbelSearch` directly
(bypassing `agentChooseMove`), so it keeps training against the paper
defaults, unaffected. Whether to also expose these as training-time
hyperparameters (reopening the six-axis Pass-2 grid with 2-3 more axes) is
a separate, larger decision, not folded into this change.

New `AgentSpec` fields: `gumbelCVisit` (int, default 50), `gumbelCScaleTenths`
(int, default 10), `gumbelRootM` (int, default 16) -- plain ints, not
doubles, so there is no floating-point round-trip concern anywhere in the ID
codec's canonical-form check.

### Part 2: identifying good values

Framed the same way Pass 2 framed the sims question: a small random-search
draw over the joint space (not one-axis-at-a-time, per the project's
established Bergstra & Bengio 2012 precedent), rated via a self-contained
round robin rather than a gauntlet against the full ~162-agent pool -- the
whole pool comparison would have poor resolution here, since Pass 2 already
found this regime loses to the plain chip counter, so every config's win
rate against strong external agents would be near zero regardless of search
shape.

**Design:** one fixed already-trained checkpoint (slot746, Pass 2 Round B's
R17 at seed 2117, rung 4000 -- the strongest Gumbel-Zero checkpoint
currently on disk) and one fixed `sims=200` (Pass 2's found productive
range), varying only `cvisit`/`cscale`/`m`. 8 random draws over curated
value sets (`cvisit` in {10,25,50,100,150,200}, `cscale` tenths in
{2,5,10,20,30}, `m` in {2,4,8,16,24,32}) plus REF (the paper defaults),
played in a full round robin against each other and a `rand@1` anchor, into
a dedicated scratch store (`ranking/matches_gaz_knobs.jsonl`), never the
canonical ladder -- mirroring the ad hoc `matches_ab6_vs_gz.jsonl` precedent
from the direct chip-counter match, and Pass 2's own dedicated screening
store for the same never-touch-the-ladder reason.

## Verification

1. `.\tools\run_tests.ps1 -Build` passes, including new tests: an ID
   round-trip battery for the grown `gaz(...)` grammar (defaults elided,
   each flag independently non-default, duplicate/unknown-flag rejection),
   a knob-validation test proving `m` actually narrows the root candidate
   set under an identical sim budget (not just that the code compiles), and
   an `agentChooseMove` save/restore hygiene test.
2. The cvisit/cscale/m sweep plays into its own scratch store and is rated
   with its own self-contained Bradley-Terry fit (`rand@1` anchor), never
   touching `ranking/matches.jsonl` or the canonical `ratings.tsv`/
   `standings.tsv`.
3. Screening only: this identifies which values are worth carrying forward,
   not a strength or certification claim -- this regime already lost 32-0 to
   the plain chip counter in Pass 2, and nothing here retrains or re-tests
   that result.
