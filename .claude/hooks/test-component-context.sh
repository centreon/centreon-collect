#!/usr/bin/env bash
# Tests for component-context.sh, against the folder CLAUDE.md files of this
# repository. Needs bash only:
#   .claude/hooks/test-component-context.sh
set -euo pipefail

HOOKS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${HOOKS_DIR}/../.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT
export TMPDIR="${TMP}"

failures=0
total=0

check() {
  local expected="$1" actual="$2" label="$3"
  total=$((total + 1))
  if [[ "${actual}" != "${expected}" ]]; then
    failures=$((failures + 1))
    printf 'FAIL (expected %s, got %s): %s\n' "${expected}" "${actual}" "${label}"
  fi
}

# hook <expected exit> <event> <session> <repo-relative file> <label>
hook() {
  local expected="$1" event="$2" session="$3" file="$4" label="$5" rc=0
  printf '{"session_id":"%s","hook_event_name":"%s","tool_name":"Edit","tool_input":{"file_path":"%s"}}' \
    "${session}" "${event}" "${ROOT}/${file}" |
    CLAUDE_PROJECT_DIR="${ROOT}" bash "${HOOKS_DIR}/component-context.sh" > "${TMPDIR}/out" 2> "${TMPDIR}/err" || rc=$?
  check "${expected}" "${rc}" "${label}"
}
says() { grep -qF -- "$1" "${TMPDIR}/err" && echo yes || echo no; }

# agent/ without a Read: every file listed.
hook 2 PreToolUse s1 agent/src/main.cc 'agent/ without Read: first edit refused'
check yes "$(says 'docs/architecture/cma.md')" 'agent/ without Read: lists cma.md'
check yes "$(says 'docs/conventions/cma.md')" 'agent/ without Read: lists conventions/cma.md'
check yes "$(says 'ARCHITECTURE.md, docs/conventions/global.md')" 'agent/ without Read: lists the global files'
hook 0 PreToolUse s1 agent/src/scheduler.cc 'agent/: second edit passes'

# agent/ after a Read: only what agent/CLAUDE.md does not import.
hook 0 PostToolUse s2 agent/src/main.cc 'Read under agent/: never blocks'
check 0 "$(wc -c < "${TMPDIR}/err" | tr -d ' ')" 'Read under agent/: no output'
hook 2 PreToolUse s2 agent/src/scheduler.cc 'agent/ after Read: first edit refused'
check no "$(says 'cma.md')" 'agent/ after Read: cma files not listed'
check yes "$(says 'Read these files first: ARCHITECTURE.md, docs/conventions/global.md.')" 'agent/ after Read: global files listed'
check yes "$(says 'agent/CLAUDE.md are already loaded')" 'agent/ after Read: says why'
hook 0 PreToolUse s2 agent/src/scheduler.cc 'agent/ after Read: second edit passes'
hook 2 PreToolUse s2 agent/test/scheduler_test.cc 'agent/ test after Read: refused once'
check yes "$(says 'docs/conventions/global.md, docs/testing.md.')" 'agent/ test after Read: lists testing.md only beyond global'

# A Read elsewhere does not mark agent/.
hook 0 PostToolUse s3 engine/src/main.cc 'Read under engine/'
hook 2 PreToolUse s3 agent/src/main.cc 'agent/ after a Read of engine/: refused'
check yes "$(says 'docs/architecture/cma.md')" 'agent/ after a Read of engine/: cma.md listed'

# Contract: agent.proto.
hook 0 PostToolUse s4 agent/proto/agent.proto 'Read of agent.proto'
hook 2 PreToolUse s4 agent/proto/agent.proto '.proto: refused'
check yes "$(says 'This is a contract between components')" '.proto: contract note'
check no "$(says 'cma.md')" '.proto after Read: cma files not listed'

# Engine side of the CMA: nested CLAUDE.md files.
f=engine/modules/opentelemetry/src/centreon_agent/agent_impl.cc
hook 0 PostToolUse s5 "${f}" 'Read under engine centreon_agent/'
hook 2 PreToolUse s5 "${f}" 'engine centreon_agent/: refused'
check no "$(says 'cma.md')" 'engine centreon_agent/ after Read: cma files not listed'
check yes "$(says 'centreon_agent/CLAUDE.md')" 'engine centreon_agent/ after Read: names its CLAUDE.md'

# bbdo/: broker files imported, engine files still listed.
hook 0 PostToolUse s6 bbdo/events.hh 'Read under bbdo/'
hook 2 PreToolUse s6 bbdo/events.hh 'bbdo/: refused'
check no "$(says 'docs/architecture/broker.md')" 'bbdo/ after Read: broker files not listed'
check yes "$(says 'docs/architecture/engine.md, docs/conventions/engine.md.')" 'bbdo/ after Read: engine files listed'

# Engine-Broker boundary under engine/ and broker/: unchanged.
hook 0 PostToolUse s7 engine/src/broker.cc 'Read of engine/src/broker.cc'
hook 2 PreToolUse s7 engine/src/broker.cc 'engine/src/broker.cc: refused'
check yes "$(says 'docs/architecture/engine.md, docs/conventions/engine.md, docs/architecture/broker.md')" 'engine/src/broker.cc: both sides listed'

# Shared code.
hook 0 PostToolUse s8 common/log_v2/log_v2.cc 'Read under common/'
hook 2 PreToolUse s8 common/log_v2/log_v2.cc 'common/: refused'
check yes "$(says 'shared by Engine, Broker and the CMA')" 'common/: shared code note'

# tests/: unchanged, testing.md listed.
hook 0 PostToolUse s9 tests/broker/sql.robot 'Read under tests/'
hook 2 PreToolUse s9 tests/broker/sql.robot 'tests/: refused'
check yes "$(says 'docs/testing.md')" 'tests/: lists testing.md'
check yes "$(says 'docs/architecture/broker.md')" 'tests/: lists the component files'

# Framework files: never refused.
hook 0 PreToolUse s10 AGENTS.md 'AGENTS.md: not refused'
hook 0 PreToolUse s10 docs/testing.md 'docs/: not refused'
hook 0 PreToolUse s10 agent/CLAUDE.md 'folder CLAUDE.md: not refused'
hook 0 PreToolUse s10 .claude/settings.json '.claude/: not refused'

# Invalid input never blocks.
rc=0; printf 'not json' | CLAUDE_PROJECT_DIR="${ROOT}" bash "${HOOKS_DIR}/component-context.sh" > /dev/null 2>&1 || rc=$?
check 0 "${rc}" 'invalid JSON: exit 0'
rc=0; CLAUDE_PROJECT_DIR="${ROOT}" bash "${HOOKS_DIR}/component-context.sh" < /dev/null > /dev/null 2>&1 || rc=$?
check 0 "${rc}" 'empty input: exit 0'
rc=0; printf '{"hook_event_name":"PreToolUse","tool_input":{"file_path":"/elsewhere/x.cc"}}' |
  CLAUDE_PROJECT_DIR="${ROOT}" bash "${HOOKS_DIR}/component-context.sh" > /dev/null 2>&1 || rc=$?
check 0 "${rc}" 'file outside the repository: exit 0'

printf '%d/%d checks passed\n' "$((total - failures))" "${total}"
((failures == 0))
