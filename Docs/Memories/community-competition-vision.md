---
name: community-competition-vision
description: "Zeph's long-term vision to turn Breakthrough's agent-ranking system into a community-wide competitive event with anti-gaming reward design"
metadata:
  type: project
---

Zeph's stated looping goal for the Breakthrough project (2026-07-06): keep producing new agents that dethrone the current top-Elo agent, either by outright beating it or by countering its specific weaknesses. He wants this to grow beyond a solo effort into a community-wide competition, and is considering hosting it (e.g. a UMN club, pitching it as a hackathon, recruiting friends to help build it out).

Reward/design ideas floated so far (all unimplemented, brainstorm stage):
- Reward cheap-to-compute bots, not just high-Elo ones.
- Make the Elo fit robust to outlier losses, e.g. a bot that memorizes one script that beats the champion but otherwise loses everywhere is a degenerate, non-meaningful strategy. Idea: require community members to reproduce/confirm the exploit a few times before it counts, or split it into a separate "gotcha" reward category instead of letting it swing the main leaderboard.
- A second leaderboard measuring cost to reverse-engineer an opponent's weights/parameters (assume access to the existing match library is free, since any bot can already read it).
- Explicit rewards for: dethroning the champion, defending the top spot over time, novel/interesting techniques or discoveries, small creators who make content about their bot (merit-based visibility), and a bonus if a bigger channel (e.g. Two Minute Papers-style) later covers the technique.

**Why:** motivation is to keep the project's core loop (new challenger vs. reigning champion) interesting and scalable past one person, while explicitly designing against Goodhart's-law failure modes (memorized single-game exploits, expensive brute-force bots crowding out clever cheap ones).

**How to apply:** this is vision/brainstorm only, nothing has been decided or built. The existing `rank.exe` system ([[tooling-design-preferences]]: anchored Bradley-Terry refit, per-agent match history, gauntlet mode) is the technical foundation any "outlier-robust Elo" or "second leaderboard" work would extend, not replace. Before implementing anything from this list, confirm with Zeph which piece he wants to prioritize (the anti-gaming Elo mechanism, a second cost-based leaderboard, or the non-technical hosting/community side) rather than building all of it speculatively.
