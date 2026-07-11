# Docs/Memories

Git-tracked mirror of Claude's per-machine auto-memory for this project, so
the load-bearing context travels with the repo (clone, pull, or any transfer)
instead of living only in one machine's `~/.claude/projects/<hash>/memory/`
store.

Workflow (also recorded in the root `CLAUDE.md` Standing Instructions):

- When Claude saves an auto-memory entry about this project, it mirrors a
  short copy here in the same format (frontmatter + body).
- On a machine whose local auto-memory store is empty or missing entries
  that exist here, Claude rebuilds the local store from this folder before
  starting work.
- The mirror is the portable copy of the load-bearing content, kept short.
- Always-on rules do not live here. Those belong in the root `CLAUDE.md`
  Standing Instructions, which is force-loaded every session on every
  machine.
