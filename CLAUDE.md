@AGENTS.md

## Claude Code

Only this file and `AGENTS.md` load at session start. The other files load on demand:

- A folder `CLAUDE.md` imports the architecture and conventions of its component when a file under that folder is read: `agent/`, `engine/`, `broker/`, `bbdo/`, `common/engine_conf/`, and the two `centreon_agent/` folders of the Engine OpenTelemetry module (CMA files).
- `.claude/rules/*.md` (path-scoped) point to the component and testing files for the other paths outside a component folder (`tests/`, packaging, SELinux).
- The `PreToolUse` hook `.claude/hooks/component-context.sh` refuses the first edit of each component in a session and lists the files to read; the retry passes.
- `.claude/settings.json` denies `--no-verify` and force pushes, and asks for confirmation before any edit of `AGENTS.md`, `CLAUDE.md` files, `ARCHITECTURE.md`, `docs/` and `.claude/`.

Skills in `.claude/skills/` follow the same files: `/create-adr` drafts an ADR from `docs/adr/README.md` and opens its own pull request.

The built-in Explore and Plan subagents do not load `CLAUDE.md` or the rules: name the files to read in their prompt.
