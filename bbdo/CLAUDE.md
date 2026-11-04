# bbdo/ — BBDO contract between Engine (`cbmod`) and Broker

Read by peers of other versions, by retention files and by Lua scripts. The Broker files below govern it; a new or changed event also follows `docs/conventions/engine.md` § Adding a Broker event or field, and the producer (`engine/src/broker.cc`, `broker/neb/`) and the consumers change in the same pull request. Tests: `ut_broker`, `ut_engine`, `tests/broker-engine/`; rules in `docs/testing.md`.

@../docs/architecture/broker.md

@../docs/conventions/broker.md
