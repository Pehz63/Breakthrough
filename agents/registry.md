# Agent registry

Append-only union of every agent ever rated (regenerated each tournament rate).
A subset run only ADDS observations, so this stays complete. `changed` flags an
agent whose spec_hash differs from its previous observation (a retrain, a param
change, or a structural bugfix).

| agent | last elo | runs | last run | spec_hash | changed | desc |
|-------|---------:|-----:|----------|-----------|---------|------|
| AB10-Classic-chip | 2794 | 1 | 20260628T041236Z | b4fb89b5c193521c | no | AB10-Classic-chip: AlphaBeta(Classic, d10) |
| AB8-Classic-chip | 2793 | 1 | 20260628T041236Z | 2911e6bb849f9123 | no | AB8-Classic-chip: AlphaBeta(Classic, d8) |
| AB6-Classic-chip | 2793 | 1 | 20260628T041236Z | 182a6a3be0c5f385 | no | AB6-Classic-chip: AlphaBeta(Classic, d6) |
| AB4-Classic-chip | 1767 | 1 | 20260628T041236Z | c2fafc05e1bd2da7 | no | AB4-Classic-chip: AlphaBeta(Classic, d4) |
| AB6-Exp-adv | 752 | 1 | 20260628T041236Z | 56be420f8a7da342 | no | AB6-Exp-adv: AlphaBeta(Experimental, d6) |
| AB10-Exp-adv | 752 | 1 | 20260628T041236Z | b3d4b66ea4928909 | no | AB10-Exp-adv: AlphaBeta(Experimental, d10) |
| AB8-Exp-adv | 751 | 1 | 20260628T041236Z | 48e2b0b7fb6cfa54 | no | AB8-Exp-adv: AlphaBeta(Experimental, d8) |
| LearnedPolicy | -405 | 1 | 20260628T041236Z | 8ab36488892437e8 | no | LearnedPolicy: Policy(LearnedPolicy) slot=1 |
