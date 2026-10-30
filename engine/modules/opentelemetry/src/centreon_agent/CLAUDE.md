# Engine side of the Centreon Monitoring Agent contract

This folder and `inc/com/centreon/engine/modules/opentelemetry/centreon_agent/` are the Contracts zone of the CMA: the CMA files below govern them, not `docs/conventions/engine.md`, even though `engine/CLAUDE.md` also loads here. A change to `agent/proto/agent.proto` or to the OTLP encoding changes the agent and this folder in the same pull request.

Tests: `ut_engine` (`engine/tests/opentelemetry/`), `tests/broker-engine/cma*.robot`; rules in `docs/testing.md` § Centreon Monitoring Agent.

@../../../../../docs/architecture/cma.md

@../../../../../docs/conventions/cma.md
