# Architecture Decision Records

One file per structuring decision: `docs/adr/NNNN-<slug>.md`, numbered continuously in this repository, written in English. The existing design is described in [`ARCHITECTURE.md`](../../ARCHITECTURE.md) and the component files, not here.

With Claude Code, `/create-adr` drafts the record from this file and opens its pull request.

## When to write one

At least one of these is true:

- several serious options were debated;
- going back is expensive (migration, compatibility between agent, Engine and Broker versions, customer impact);
- the choice will be questioned and the reason must stay traceable.

Choosing a protocol, a language or an exchange format is an ADR. A style rule is a convention.

## Lifecycle

- Written before or with the pull request that implements it, validated by the people the decision concerns.
- `Proposed` when the pull request opens, `Accepted` when it merges. An accepted ADR is never rewritten: only its status changes.
- A changed decision is a new ADR that marks the old one `Superseded by ADR-NNNN`, and updates the architecture or conventions sections it affects.
- The matching architecture or conventions section states the rule in one line and links the ADR.
- An ADR written after the fact for a decision already in place gives the real date of the decision.

## Template

```markdown
# ADR-NNNN: [Decision stated as an imperative]

- **Status:** Proposed | Accepted | Superseded by ADR-NNNN
- **Date:** YYYY-MM-DD

## Context

[The problem, the current practice and why it is not settled. Then the alternatives weighed and why each lost.]

## Decision

[The rule, in a few imperative sentences.]

## Consequences

- [What changes, what it costs, the exceptions.]

## When to Amend

- [The facts that would reopen this decision.]
```

- Title: the decision as an imperative ("Send agent check results as OTLP"), not the topic ("Result format").
- Context includes the rejected alternatives and why: that is what keeps the debate closed.
- When to Amend: objective facts that would justify a new ADR (end of support of a distribution, new protocol version...).
- Compatible with Nygard and MADR: MADR's "Considered Options" and "Decision Drivers" go into Context.

## Records

None yet.
