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

- Before the first edit in this scope, read `docs/adr/cma/ARCHITECTURE.md`, then every section that the change touches and the ADR each section names. Name the ADRs you read in your plan.
- Apply the Decision of each of those ADRs. Before you call the work done, check the diff against every sentence of those Decisions and report the result line by line.
- Do not copy a place the Consequences list as an exception, a known defect or "not yet followed".
- If the change matches a "When to Amend" condition of an ADR, say so before writing code.
- If the change has to depart from an ADR, stop and ask before writing code. Do not work around the ADR.
- Proto (CMA-0004) and OTLP (CMA-0005) changes include the Engine side in `engine/modules/opentelemetry/src/centreon_agent/`.
- A change under `common/` also impacts Engine and Broker: build and test them (CMA-0001 rule 8).
