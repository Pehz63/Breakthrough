# Evidence-tethered claims (feedback, 2026-07-12)

Strong claims in results docs and `Docs/theories.md` must be tethered to the
evidence that actually tested them, and interpretation or mechanism stories
must be explicitly labeled as hypotheses, not written as findings.

Why: the developer caught a results-doc claim that PST noise was "refuted as a
tie-breaker even at dominated ratios" when the -800 Elo data point came from a
configuration where the noise exceeded a chip (not dominated at all), and the
"re-sorts near-equal branches" mechanism was a guess written as a conclusion.
Their words: a strong claim "should be made with evidence. The appropriate
docs should have explored such a claim and not guessed it."

How to apply: before writing a claim, check the cited measurement actually
tested THAT claim (config, scale, instrument). Separate "measured" from
"interpreted"; prefix mechanism stories with "hypothesis:". Where a cheap test
can turn a guess into evidence (like the dominance/order-preservation walk in
`tests/test_eval.cpp`), write the test instead of the guess.
