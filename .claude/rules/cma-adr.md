---
paths:
  - "agent/**"
  - "engine/modules/opentelemetry/**/centreon_agent/**"
  - "common/grpc/**"
  - "common/process/**"
  - "common/crypto/**"
  - "tests/broker-engine/cma*.robot"
---

# Centreon Monitoring Agent: ADRs are mandatory

This file belongs to the Centreon Monitoring Agent (CMA) scope or to a library it shares.

- Before the first edit in this scope, read `docs/adr/cma/ARCHITECTURE.md`, then every ADR that its section 6 table lists for the change at hand. Name the ADRs you read in your plan.
- Apply each Decision of those ADRs. Before you call the work done, check every Guardrail of those ADRs against the diff and report the result line by line.
- Do not copy a pattern listed under "Known deviations".
- If the change has to depart from an ADR, stop and ask before writing code. Do not work around the ADR.
- Proto (CMA-0004) and OTLP (CMA-0005) changes include the Engine side in `engine/modules/opentelemetry/src/centreon_agent/`.
- A change under `common/` also impacts Engine and Broker: build and test them (CMA-0001 rule 8).
