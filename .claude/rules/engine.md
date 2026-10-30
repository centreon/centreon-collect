---
paths:
  - "tests/engine/**"
  - "tests/broker-engine/**"
  - "tests/resources/Engine.py"
  - "packaging/centreon-collect/centreon-engine*"
  - "selinux/centreon-engine/**"
---

# Engine scope

This path belongs to Centreon Engine but lives outside `engine/`. Before changing it, read `docs/architecture/engine.md` and `docs/conventions/engine.md` (they add to `ARCHITECTURE.md` and `docs/conventions/global.md`), and name them in the plan. CMA suites (`tests/**/cma*.robot`) follow the CMA files instead.
