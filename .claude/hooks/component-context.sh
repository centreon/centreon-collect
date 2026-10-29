#!/usr/bin/env bash
# PreToolUse hook (Edit|Write|MultiEdit|NotebookEdit).
# Maps the edited file to its component and, on the first edit of that component
# in the session, reminds the model which architecture, conventions and testing
# files govern it (paths only, not their content). Later edits stay silent.
# Never blocks: any failure exits 0 with no output.
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

arch=".claude/architecture"
conv=".claude/conventions"
testing=".claude/ci/testing.md"

# Component of the file. Order matters: CMA paths outside agent/ come first.
case "$rel" in
  .claude/*|CLAUDE.md|*/CLAUDE.md|CLAUDE.local.md) exit 0 ;;
  agent/*|engine/modules/opentelemetry/*centreon_agent*|tests/*cma*.robot|tests/resources/Agent.py|packaging/centreon-monitoring-agent*)
    component="cma" ;;
  common/*|clib/*) component="shared" ;;
  engine/*) component="engine" ;;
  broker/*) component="broker" ;;
  bbdo/*) component="bbdo" ;;
  connectors/*) component="connectors" ;;
  gorgone/*) component="gorgone" ;;
  ccc/*) component="ccc" ;;
  tests/*) component="tests" ;;
  *) component="repo" ;;
esac

is_test=0
case "$rel" in tests/*|*/test/*|*/tests/*) is_test=1 ;; esac
is_contract=0
case "$rel" in *agent.proto|engine/modules/opentelemetry/*centreon_agent*|bbdo/*.proto) is_contract=1 ;; esac

marker_dir="${TMPDIR:-${TEMP:-/tmp}}/claude-collect-context"
mkdir -p "$marker_dir" 2>/dev/null || exit 0
marker="$marker_dir/${session}-${component}-${is_test}-${is_contract}"
[ -e "$marker" ] && exit 0
: > "$marker" 2>/dev/null

files="$arch/global.md, $conv/global.md"
note=""
case "$component" in
  cma) files="$files, $arch/cma.md, $conv/cma.md" ;;
  shared) note=" This code is shared by Engine, Broker and the CMA: component rules (CMA included) do not apply; build and run the unit tests of every consumer." ;;
  tests) ;;
  repo) note=" This file belongs to no component: global rules only." ;;
  *) note=" Component $component has no architecture or conventions file yet: follow the global conventions and the code around the change." ;;
esac
[ "$is_test" = 1 ] && files="$files, $testing"
[ "$is_contract" = 1 ] && note="$note This is a contract between components (see the architecture): it evolves by addition only, and both sides change in the same pull request."

msg="Repository context for $rel (component: $component). Files that govern this path: $files. The working agreements of CLAUDE.md require these files to be read before the edit, and a change that departs from them to be raised with the user first.$note"
msg="${msg//\\/\\\\}"; msg="${msg//\"/\\\"}"
printf '{"hookSpecificOutput":{"hookEventName":"PreToolUse","additionalContext":"%s"}}\n' "$msg"
exit 0
