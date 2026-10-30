#!/usr/bin/env bash
# PreToolUse hook (Edit|Write|MultiEdit|NotebookEdit).
# Maps the edited file to its component. The first edit of each component in a
# session is refused (exit 2) and stderr lists the architecture, conventions and
# testing files that govern it, so that they are read before anything is written.
# The per-session marker lets the retry and later edits through.
# A failure of the script itself never blocks: it exits 0 with no output.
set -u

input="$(cat 2>/dev/null)" || exit 0
root="${CLAUDE_PROJECT_DIR:-$(pwd)}"

json_field() { # first string value of a JSON key; enough for paths and ids
  printf '%s' "$input" | tr -d '\n' | sed -n "s/.*\"$1\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p" | head -n1
}
file="$(json_field file_path)"
[ -n "$file" ] || file="$(json_field notebook_path)"
[ -n "$file" ] || exit 0
session="$(json_field session_id)"; session="${session:-nosession}"

# Normalise: JSON-escaped Windows separators -> '/', make the path relative to the repo.
file="${file//\\\\//}"; file="${file//\\//}"
root_n="${root//\\//}"
rel="${file#"$root_n"/}"
case "$rel" in /*|[A-Za-z]:/*) exit 0 ;; esac  # outside the repository

arch="docs/architecture"
conv="docs/conventions"
testing="docs/testing.md"

# Component of the file. Order matters: CMA paths outside agent/ come first.
case "$rel" in
  .claude/*|CLAUDE.md|*/CLAUDE.md|CLAUDE.local.md|AGENTS.md|ARCHITECTURE.md|docs/*) exit 0 ;;
  agent/*|engine/modules/opentelemetry/*centreon_agent*|tests/*cma*.robot|tests/resources/Agent.py|packaging/centreon-monitoring-agent*)
    component="cma" ;;
  engine/src/broker.cc|broker/neb/*|bbdo/*|tests/broker-engine/*)
    component="engine-broker" ;;
  common/engine_conf/*|engine/*|tests/engine/*|tests/resources/Engine.py|packaging/centreon-collect/centreon-engine*|selinux/centreon-engine/*)
    component="engine" ;;
  common/*|clib/*) component="shared" ;;
  broker/*|tests/broker/*|tests/bam/*|tests/resources/Broker.py|packaging/centreon-collect/centreon-broker*|selinux/centreon-broker/*)
    component="broker" ;;
  connectors/*) component="connectors" ;;
  gorgone/*) component="gorgone" ;;
  ccc/*) component="ccc" ;;
  tests/*) component="tests" ;;
  *) component="repo" ;;
esac

is_test=0
case "$rel" in tests/*|*/test/*|*/tests/*) is_test=1 ;; esac
is_contract=0
case "$rel" in *agent.proto|engine/modules/opentelemetry/*centreon_agent*|bbdo/*|engine/src/broker.cc|broker/neb/*|engine/enginerpc/*.proto|broker/core/src/broker.proto|common/engine_conf/*.proto) is_contract=1 ;; esac

marker_dir="${TMPDIR:-${TEMP:-/tmp}}/claude-collect-context"
mkdir -p "$marker_dir" 2>/dev/null || exit 0
marker="$marker_dir/${session}-${component}-${is_test}-${is_contract}"
[ -e "$marker" ] && exit 0
: > "$marker" 2>/dev/null

files="ARCHITECTURE.md, $conv/global.md"
note=""
case "$component" in
  cma) files="$files, $arch/cma.md, $conv/cma.md" ;;
  engine) files="$files, $arch/engine.md, $conv/engine.md" ;;
  broker) files="$files, $arch/broker.md, $conv/broker.md" ;;
  engine-broker) files="$files, $arch/engine.md, $conv/engine.md, $arch/broker.md, $conv/broker.md"
    note=" This path is on the Engine-Broker boundary (BBDO events, cbmod): both component files apply." ;;
  shared) note=" This code is shared by Engine, Broker and the CMA: component rules (CMA included) do not apply; build and run the unit tests of every consumer." ;;
  tests) ;;
  repo) note=" This file belongs to no component: global rules only." ;;
  *) note=" Component $component has no architecture or conventions file yet: follow the global conventions and the code around the change." ;;
esac
[ "$is_test" = 1 ] && files="$files, $testing"
[ "$is_contract" = 1 ] && note="$note This is a contract between components (see the architecture): it evolves by addition only, and both sides change in the same pull request."

printf '%s\n' "First edit of component $component in this session ($rel): refused once. Read these files first: $files. Name them in the plan, raise any departure from them with the user, then retry the edit; it will pass.$note" >&2
exit 2
