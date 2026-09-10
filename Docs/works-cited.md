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
