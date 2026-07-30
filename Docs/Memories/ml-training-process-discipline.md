---
name: ml-training-process-discipline
description: "Never one-shot a model training study; 3-pass process, min 3 seeds, consistent rungs, present the config grid and be interactive about findings"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 292a8b1b-8d5f-4201-91e5-512e51507f7b
  modified: 2026-07-30T06:56:21.942Z
---

Full procedure lives in the project doc `Docs/model-training-playbook.md`
(read in full before any ML training work in this repo) -- this memory is the
durable cross-session reminder that it exists and why.

**The rule.** Never treat a single working configuration as a finished study.
Three passes, always: (1) sanity -- one config, verify the plumbing and the
instrument actually measure what they claim; (2) broad sweep -- vary
hyperparameters widely, learn the shape of the problem, minimum 3 seeds per
configuration (only reduce after seeing results say it's safe), and the SAME
checkpoint/rung list across every configuration in one study, no exceptions;
(3) optimize -- use pass 2's findings to build a specific best config, or hand
off to an automated search (this project already has one,
`tools/hill_climb.ps1`) that runs until it stops improving.

Two more standing requirements layered on top: present the Pass-2 configuration
grid to the developer as its OWN clearly labeled thing before running it (not
buried in a plans document), and be interactive at every pass boundary and at
final results -- surface findings, invite pushback, don't just hand over a
written doc and call it done.

**Why:** developer correction, 2026-07-30, after a TD-Leaf self-play cohort
study ran with 1-2 seeds on most arms and an inconsistent rung ladder (one
arm got 5 checkpoints, others got 2, so the arms that skipped the game count
that turned out to matter most could never be fairly compared). Direct quotes:
"you try to 1-shot the task by guessing a configuration and running it and
saying 'ok done'... I don't want passing work, I want a configuration that
will make it shine and do its best," and "Be more interactive with me... I
tend to notice many things you don't."

**How to apply:** any time a training run, hyperparameter sweep, or new agent
config is in scope in this repo, read the playbook first, not just this
summary. [[tdleaf-online-selfplay]] is the worked-example incident this rule
came from.
