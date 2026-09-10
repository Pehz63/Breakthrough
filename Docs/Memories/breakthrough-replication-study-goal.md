---
name: breakthrough-replication-study-goal
description: "Project research goal set 2026-09-10: a paper replicating published game-AI training techniques on Breakthrough under one controlled, compute-matched protocol (a cross-comparison, meta-analysis style)"
metadata:
  type: project
---

Research goal, stated 2026-09-10: a paper that replicates training techniques published on other board games on Breakthrough and compares them against each other under one controlled protocol. The contribution is (1) which published findings transfer to Breakthrough and (2) a cross-comparison of techniques the literature measured only in separate papers, games, and budgets. It refines [[community-competition-vision]]'s dethrone loop. A stronger agent is expected as a by-product.

Closest prior art: Cohen-Solal (JMLR 27, 2026, arXiv 2008.01188 v5) compares tree/root/terminal learning, Descent vs other searches, and six terminal reward heuristics on 11 games including Breakthrough (round-robin win percentages among the combinations). Lorentz and Zosa (ACG 2017): TD inside MCTS and expert-move CNNs on Breakthrough. Nothing found on Breakthrough for TD-Leaf, alpha-beta TreeStrap, search-score distillation, KataGo auxiliary targets, or potential-based shaping.

**Why:** a controlled cross-comparison on one game is missing from the literature, and the project already has the instrument (matched-budget heads, full-roster anchored refit, measured seed-noise band, the 4-pass playbook).

**How to apply:** frame each training study as a replication of a named published technique. Cite it in `Docs/works-cited.md` with its original domain, protocol, and result, list the adaptations, then measure on the same head, compute budget, and full-roster refit as every other technique. Record failures too, "did not transfer" is a finding.
