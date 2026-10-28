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

- Path: `docs/adr/<module>/NNNN-<slug>.md`. `NNNN` is never reused, even after a supersede.
- Sections, in this order: Status, Context, Decision, Consequences, Guardrails, References.
- `Decision` uses MUST / MUST NOT / SHOULD (RFC 2119). A line that review would not reject is not a decision.
- `Guardrails` lists what review rejects in a Claude-produced PR, one line each, verifiable.
- `References` cites files, never line numbers (they drift).
- Status values: `Proposed`, `Accepted`, `Superseded by <id>`, `Deprecated`.
- An ADR is changed by a PR, reviewed by the module owner in `CODEOWNERS`. The git log is the history: no dates in the body.
- Known deviations between the code and an ADR are listed in the ADR under `Known deviations`. Code that copies a listed deviation is rejected.

## Template

```markdown
# CMA-NNNN — <title>

- Status: Proposed
- Owner: @centreon/owners-cpp
- Scope: <paths>

## Context
<forces, constraints, facts from the code>

## Decision
1. <MUST / MUST NOT rule>

## Consequences
<what this costs, what it enables>

## Guardrails
- <what review rejects>

## Known deviations
- <file: deviation> (optional)

## References
- <files>
```
