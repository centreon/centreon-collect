# centreon-collect

Monitoring collection stack of Centreon: Engine (`engine/`), Broker (`broker/`), Centreon Monitoring Agent (`agent/`), Gorgone (`gorgone/`), connectors (`connectors/`), shared C++ libraries (`common/`). Robot integration tests in `tests/`.

## Where the rules are

- Architecture Decision Records: `docs/adr/` (index `docs/adr/README.md`), one folder per module. A module with ADRs also has an architecture reference (`docs/adr/<module>/ARCHITECTURE.md`) and an area `CLAUDE.md`.
- Centreon Monitoring Agent: `agent/CLAUDE.md`, which imports `docs/adr/cma/ARCHITECTURE.md`. Hooks in `.claude/settings.json` inject the Decision of the governing ADRs before each edit in that scope and ask for a review before the turn ends. Reviewer subagent: `cma-adr-reviewer`.
- Modules without ADRs: follow the surrounding code. CMA rules do not apply outside the CMA scope.

## Working agreements for Claude-produced work

- Before changing code in a module that has ADRs, name in the plan the ADRs that govern the change. If the change has to depart from one, stop and ask before writing code.
- Before declaring a change in the CMA scope done, run the `cma-adr-reviewer` subagent on the diff and fix what it reports as blocking.
- When a review remark or a correction states a convention that no ADR covers, propose the ADR sentence that would have prevented it.
- One change per PR, opened as a draft. No construct the author cannot explain in review.
