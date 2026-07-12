# Strength benchmarking instrument (feedback, 2026-07-12)

Agent strength comparisons run on the FULL main roster (`rank.exe gauntlet`
with the default `ranking/roster.txt`), never on the small
`ranking/climb_roster.txt` pool, which exists only as the hill climber's cheap
fitness function.

Why: the developer set this convention after climb-pool ablations produced
weak conclusions. The main pool's diversity is the point: variants never play
each other, but they are compared through the same broad frozen opponents.
The gauntlet identity artifact (theory 19) reached ~200 Elo on the climb pool
vs ~95 on the main pool.

How to apply: vary one thing between compared candidate IDs, run at least two
`--seed` replicates, read differences against the replicate spread. Use
pairgen byte-level comparison for near-identical agents, full refits for
permanent ratings. Full text: `Docs/benchmarking.md`, "Measuring strength".
