---
paths:
  - "agent/**"
  - "engine/modules/opentelemetry/**/centreon_agent/**"
  - "tests/**/cma*.robot"
  - "tests/resources/Agent.py"
  - "packaging/centreon-monitoring-agent*/**"
  - "packaging/centreon-monitoring-agent*"
---

# Centreon Monitoring Agent scope

This path belongs to the Centreon Monitoring Agent (CMA). Before changing it, read `.claude/architecture/cma.md` and `.claude/conventions/cma.md` (they add to the global files loaded by `CLAUDE.md`), and name them in the plan. A change to `agent/proto/agent.proto`, the OTLP encoding or `engine/modules/opentelemetry/**/centreon_agent/` changes both the agent and Engine in the same pull request.
