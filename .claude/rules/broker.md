---
paths:
  - "engine/src/broker.cc"
  - "engine/inc/com/centreon/engine/broker.hh"
  - "resources/centreon*.sql"
  - "tests/broker/**"
  - "tests/broker-engine/**"
  - "tests/bam/**"
  - "tests/resources/Broker.py"
  - "packaging/centreon-collect/centreon-broker*"
  - "selinux/centreon-broker/**"
---

# Broker scope

This path belongs to Centreon Broker or to the BBDO contract but lives outside `broker/`. Before changing it, read `docs/architecture/broker.md` and `docs/conventions/broker.md` (they add to `ARCHITECTURE.md` and `docs/conventions/global.md`), and name them in the plan. A change to `bbdo/` or `engine/src/broker.cc` also follows `docs/conventions/engine.md`, and the producer (`engine/src/broker.cc`, `broker/neb/`) and the consumers change in the same pull request.
