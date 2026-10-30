---
paths:
  - "common/engine_conf/**"
  - "tests/engine/**"
  - "tests/broker-engine/**"
  - "tests/resources/Engine.py"
  - "packaging/centreon-collect/centreon-engine*"
  - "selinux/centreon-engine/**"
---

# Engine scope

This path belongs to Centreon Engine but lives outside `engine/`. Before changing it, read `docs/architecture/engine.md` and `docs/conventions/engine.md` (they add to `ARCHITECTURE.md` and `docs/conventions/global.md`), and name them in the plan. `common/engine_conf/state.proto` is also parsed by Broker `unified_sql`: it evolves by addition only. CMA suites (`tests/**/cma*.robot`) follow the CMA files instead.
