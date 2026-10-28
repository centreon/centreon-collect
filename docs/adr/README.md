# Architecture Decision Records — centreon-collect

One decision per file. Each module of the repository has its own folder and its own numbering.

| Module | Folder | Prefix | Area guidance |
| --- | --- | --- | --- |
| Centreon Monitoring Agent (`agent/`) | [`cma/`](cma/) | CMA | [`agent/CLAUDE.md`](../../agent/CLAUDE.md) |
| Engine | `engine/` | ENG | to write |
| Broker | `broker/` | BRK | to write |
| Common libraries (`common/`) | `common/` | CMN | to write |
| Connectors (`connectors/`) | `connectors/` | CON | to write |
| Gorgone | `gorgone/` | GOR | to write |

## Rules

The format is the one of the centreon (web) repository, `centreon/doc/php/adr/template.md`, copied in [`template.md`](template.md).

- Path: `docs/adr/<module>/NNNN-<slug>.md`, numbered per module. An ID (`CMA-NNNN`) is never reused, even after a supersede.
- Title: `<PREFIX>-NNNN: <decision stated as an imperative>`.
- Header: `Status` (`Proposed`, `Accepted`, `Superseded by <ID>`) and `Date` (`YYYY-MM-DD`).
- Sections, in this order:
  - `Context`: the problem, the current practice with the files involved, then the alternatives weighed and why each lost.
  - `Decision`: the rule, in imperative sentences.
  - `Consequences`: what changes, what it costs, the exceptions, and the places where the code does not follow the rule yet. Code that copies such a place is rejected.
  - `When to Amend`: the facts that would reopen the decision.
- Files are cited by path, never by line number (lines drift).
- An ADR changes through a pull request reviewed by the module owner in `CODEOWNERS`. The git log is the history.
