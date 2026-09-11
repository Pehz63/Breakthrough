# Works cited

External research referenced in this project's docs and decisions. Separate
from `Docs/theories.md` (this project's own testable claims) and
`Docs/terminology.md` (project vocabulary).

**Bergstra, J. and Bengio, Y. (2012). "Random Search for Hyper-Parameter
Optimization." Journal of Machine Learning Research, 13, 281-305.**
https://jmlr.org/papers/v13/bergstra12a.html -- For a fixed evaluation budget,
sampling every hyperparameter independently and simultaneously finds better
configurations than grid search or changing one hyperparameter at a time,
because when only a few axes actually matter the latter two waste most of
their budget resolving the ones that do not. Basis for
`Docs/model-training-playbook.md`'s Pass-2 sweep design (random search over
the joint space), replacing an earlier one-axis-at-a-time proposal for the
TD-Leaf hyperparameter sweep.

**Cohen-Solal, Q. (2026). "Learning to Play Two-Player Perfect-Information
Games without Knowledge." Journal of Machine Learning Research, 27(130),
1-64.** arXiv 2008.01188 v5. https://jmlr.org/papers/v27/25-2259.html --
Learns a value network by reinforcement with no policy and no domain
knowledge (the Athénan program). Runs on 11 games including Breakthrough.
Each experiment trains several combinations and reports each one's final
win percentage in a round-robin against the others, averaged over 32 or 48
repetitions. Breakthrough rows, read from the arXiv v5 text:
- Table 2, learning target: tree learning 78.1% (MCTS) / 79.2% (iterative
  deepening alpha-beta), root learning 45.5% / 50.6%, terminal learning
  22.3% / 14.3%.
- Table 3, search used while learning: Descent 86.5%, Unbounded Best-First
  Minimax 72.5%, MCTS and alpha-beta with tree learning 45.5% / 47.6%, with
  root learning 19.2% / 21.0%.
- Table 4, terminal reward: plain win/loss 39.0%, additive depth (win fast,
  lose slow) 69.5%, multiplicative depth 40.4%, cumulative mobility 43.9%,
  presence (piece count) 48.5%.
Its Table 11 lists Athénan with the Computer Olympiad Breakthrough gold in
each year 2020-2024. The closest prior study to this project's
cross-technique replication goal
(`Docs/Memories/breakthrough-replication-study-goal.md`). Its percentages
are internal to each experiment's pool and do not rate the combinations
against outside programs.

**Lorentz, R. and Zosa, T. E. (2017). "Machine Learning in the Game of
Breakthrough." Advances in Computer Games 15, LNCS 10664, 140-150.**
https://doi.org/10.1007/978-3-319-71649-7_12 -- Abstract only (full text
not read): TD learning inside an MCTS program reached nearly the strength
of Wanderer, a strong hand-tuned Breakthrough program, and CNNs trained on
Wanderer's moves produced a program stronger than Wanderer.

**Soemers, D. J. N. J., Mella, V., Browne, C. and Teytaud, O. (2021). "Deep
Learning for General Game Playing with Ludii and Polygames." ICGA Journal,
43, 146-161.** arXiv 2101.09562. -- AlphaZero-style training through
Polygames on Ludii games. Table 1: Breakthrough trained 20 hours on 8 GPUs,
MCTS with the trained model at 40 iterations per move won 100.00% against
untrained MCTS at 800 iterations per move. A sanity result against a weak
baseline, not a strength rating.

**Veness, J., Silver, D., Blair, A. and Uther, W. (2009). "Bootstrapping
from Game Tree Search." Advances in Neural Information Processing Systems
22.** https://papers.nips.cc/paper_files/paper/2009/hash/389bc7bb1e1c2a5e7e147703232a88f6-Abstract.html
Updates a linear evaluator (1812 mostly binary chess features) toward the
values an alpha-beta search computes, at every searched node (TreeStrap)
rather than one node (TD-Leaf, RootStrap). Self-play from small random
weights, fixed 1m+1s time control, update time charged against thinking
time, step size tuned per method, one training run per method. Table 2,
best Elo with 95% CI in a BayesElo tournament of about 16,000 games, untrained
anchored at 250: TreeStrap(alpha-beta) 2157 +/- 31, TreeStrap(minimax)
1807 +/- 32, RootStrap(alpha-beta) 1362 +/- 59, TD-Leaf 1068 +/- 36,
untrained 250 +/- 63. Claim C1 of `plans/replication-study-plan-1-brass-lectern.md`.

**Baxter, J., Tridgell, A. and Weaver, L. (1999). "TDLeaf(lambda):
Combining Temporal Difference Learning with Game-Tree Search."** arXiv
cs/9901001. Abstract only: TD-Leaf(lambda) applies TD(lambda) at the leaf
of the principal variation. KnightCap rose from 1650 to 2100 on FICS in 308
games. Compared against TD(lambda) and TD-directed(lambda) in chess and
backgammon. Veness et al. 2009 note the KnightCap weights started from
expert values. The basis of `train.exe tdleaf`.

**Jones, A. L. (2021). "Scaling Scaling Laws with Board Games."** arXiv
2104.03113. Abstract only: AlphaZero on Hex at several board sizes. The
strength reachable at fixed compute degrades predictably as the board grows,
and train-time and test-time compute trade off against each other.

**Henderson, P., Islam, R., Bachman, P., Pineau, J., Precup, D. and Meger,
D. (2018). "Deep Reinforcement Learning that Matters." AAAI 2018.** arXiv
1709.06560. Abstract only: seed variance, implementation details and
reporting practice change RL comparisons enough to reverse conclusions, and
the paper proposes reporting guidelines. The basis for the replication
study's equal-tuning-budget protocol.

**Agarwal, R., Schwarzer, M., Castro, P. S., Courville, A. and Bellemare,
M. G. (2021). "Deep Reinforcement Learning at the Edge of the Statistical
Precipice." NeurIPS 2021.** arXiv 2108.13264. Abstract only: point estimates
over few runs are unreliable. Recommends interval estimates, performance
profiles, the interquartile mean and stratified bootstrap confidence
intervals. The reporting standard the replication study follows.

**Danihelka, I., Pohlen, T., Rowland, M., Hessel, M., Ozair, S., Silver, D.
and van Hasselt, H. (2022). "Policy improvement by planning with Gumbel."
ICLR 2022.** https://openreview.net/forum?id=bERaNdoegnO -- Replaces
AlphaZero's Dirichlet-noise root exploration with Gumbel-top-k sampling and
Sequential Halving, and replaces PUCT's non-root action selection with a
deterministic rule whose visit distribution converges to
`softmax(logits + sigma(completedQ))`, giving a provable policy-improvement
guarantee at very low simulation counts (tens, not thousands). Basis for the
`GumbelMCTS` explorer and the `joint` (value + policy) model type,
`src/ai_gumbel.cpp` / `src/ml_model.h`; see `ML.md`'s "Gumbel MCTS" section.
