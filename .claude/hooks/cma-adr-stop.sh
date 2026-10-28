#!/usr/bin/env bash
# Stop hook. If the session edited files in the Centreon Monitoring Agent scope
# since the last check, keep Claude working once to review the change against
# the ADRs before ending the turn. Never loops: it blocks at most once per batch
# of edits and lets the turn end when stop_hook_active is true.
set -u

input="$(cat 2>/dev/null)" || exit 0
json_field() {
  printf '%s' "$input" | tr -d '\n' | sed -n "s/.*\"$1\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p" | head -n1
}
case "$(printf '%s' "$input" | tr -d '\n ')" in *'"stop_hook_active":true'*) exit 0 ;; esac
session="$(json_field session_id)"; session="${session:-nosession}"
touched="${TMPDIR:-/tmp}/cma-adr-hook/$session.touched"
[ -s "$touched" ] || exit 0

files="$(sort -u "$touched" | head -n 30 | tr '\n' ' ')"
: > "$touched"
reason="Files in the CMA scope were changed: $files. Before ending: delegate to the cma-adr-reviewer subagent on the current diff (git diff), or if unavailable check the diff yourself against the Decision of every governing ADR and the blocking criteria of docs/adr/cma/ARCHITECTURE.md. Report each rule as OK / violated / not applicable, and fix any blocking violation."
esc="$(printf '%s' "$reason" | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g')"
printf '{"decision":"block","reason":"%s"}\n' "$esc"
exit 0
