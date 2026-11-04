# common/engine_conf — Engine configuration model

Owned by Engine although it lives in `common/`: the Engine files below govern it, not only the global conventions. `state.proto` is also parsed by Broker `unified_sql` and its syntax is written by Centreon Web: it evolves by addition only. Adding a directive: `docs/conventions/engine.md` § Adding a configuration directive. Tests: `ut_engine` (`engine/tests/configuration/`), rules in `docs/testing.md`.

@../../docs/architecture/engine.md

@../../docs/conventions/engine.md
