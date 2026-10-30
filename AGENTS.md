# centreon-collect — rules for coding agents

Collection side of Centreon: monitoring agent (CMA), Engine, Broker, shared libraries, connectors, Gorgone. C++17 on Linux, C++20 for the CMA on Windows only.

## Files to read before changing code

| File | Read when |
|---|---|
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | Always: components, contracts, review criteria, build, CI |
| [`docs/conventions/global.md`](docs/conventions/global.md) | Always |
| `docs/architecture/<component>.md`, `docs/conventions/<component>.md` | The component has them (see the Components table of `ARCHITECTURE.md`) |
| [`docs/testing.md`](docs/testing.md) | The change needs a test, touches a test or CI |
| [`docs/adr/`](docs/adr/README.md) | The change takes or reopens a structuring decision |

## Working agreements

- Find the components the change touches, read the files above for each, and name them in the plan with the sections that govern the change. A plan without them is rejected.
- If the change has to depart from one of these files, stop and ask before writing code. Never work around a rule.
- A component without its own files follows the global conventions and the code around the change. Rules written for one component never apply to another.
- During a code task, update the architecture description your change makes wrong (flow, contract, component), in the same pull request. Never change a rule (convention, review criterion, test rule, agent wiring): propose it in its own pull request. This holds for a rule that a review remark or a correction states and that no file covers: propose the sentence for the matching file.
- Every edit of `AGENTS.md`, `CLAUDE.md` files, `ARCHITECTURE.md`, `docs/` and `.claude/` asks for confirmation (`.claude/settings.json`). Start Claude Code from the repository root so that these rules match.
- One change per pull request, opened as a draft, targeting `develop`. No construct the author cannot explain in review.

## Skills, subagents and scripts

- Reference these files by path, never copy their rules: a copy drifts the day the file changes. Steps and output format specific to a skill stay in the skill.
- A review checks the code against these files and the review criteria of `ARCHITECTURE.md`, not against rules of its own.
- A subagent does not see the conversation that started it: its prompt names the files to read.

## Commands

```bash
# Linux, from the repository root (vcpkg setup: README.md § Compilation)
cmake -B build -DVCPKG_OVERLAY_TRIPLETS=custom-triplets -DVCPKG_TARGET_TRIPLET=x64-linux \
      -DVCPKG_OVERLAY_PORTS=overlays -DWITH_TESTING=On -S .
cmake --build build --target ut_<component>
build/tests/ut_<component> --gtest_filter='<suite>.*'
clang-format -i <changed files>     # not checked by CI
```

Robot tests: `tests/README.md`, then `./robot.sh <suite>.robot` from `tests/`.
