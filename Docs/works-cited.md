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
