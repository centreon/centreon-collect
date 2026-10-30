@AGENTS.md

## Claude Code

Only this file and `AGENTS.md` load at session start. The other files load on demand:

- `agent/CLAUDE.md`, `engine/CLAUDE.md` and `broker/CLAUDE.md` import the architecture and conventions of their component when a file under their folder is read.
- `.claude/rules/*.md` (path-scoped) point to the component and testing files when a matching file is read or edited, including component paths outside their folder (`bbdo/`, `common/engine_conf/`, `tests/`, packaging).
- The `PreToolUse` hook `.claude/hooks/component-context.sh` names the governing files on the first edit of each component in a session. It never blocks.

Skills in `.claude/skills/` follow the same files: `/create-adr` drafts an ADR from `docs/adr/README.md` and opens its own pull request.

The built-in Explore and Plan subagents may not load `CLAUDE.md` or the rules: name the files to read in their prompt.
