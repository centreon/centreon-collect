# centreon-collect

The architecture file below is the entry point. It links the architecture and conventions file of each component, and the testing rules.

@.claude/architecture/global.md

@.claude/conventions/global.md

Component files load on demand: `agent/CLAUDE.md` and `.claude/rules/*.md` (path-scoped) point to them when a matching file is read, and a `PreToolUse` hook (`.claude/hooks/component-context.sh`) names the governing files on the first edit of each component in a session.

## Working agreements for Claude-produced work

- Before changing code, read the global conventions, the architecture and conventions files of the component you touch, and the testing rules. Name them in the plan.
- If the change has to depart from one of these files, stop and ask before writing code.
- A component without its own files follows the global conventions and the code around the change. Rules written for one component never apply to another.
- When a review remark or a correction states a convention that no file covers, propose the sentence to add to the matching architecture or conventions file.
- One change per pull request, opened as a draft. No construct the author cannot explain in review.
