#!/usr/bin/env bash
# PreToolUse hook (Edit|Write|MultiEdit|NotebookEdit).
# When the target file is in the Centreon Monitoring Agent scope, inject the
# Decision of the ADRs that govern it. Each ADR Decision is injected once per
# session; later edits only name the ADRs. Also records that the session touched
# the CMA scope, for the Stop hook (cma-adr-stop.sh).
# Never blocks: any failure exits 0 with no output.
set -u

input="$(cat 2>/dev/null)" || exit 0
root="${CLAUDE_PROJECT_DIR:-$(pwd)}"
adr_dir="$root/docs/adr/cma"
[ -d "$adr_dir" ] || exit 0

json_field() { # first string value of a JSON key, unescaped enough for paths and ids
  printf '%s' "$input" | tr -d '\n' | sed -n "s/.*\"$1\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p" | head -n1
}
file="$(json_field file_path)"
[ -n "$file" ] || file="$(json_field notebook_path)"
[ -n "$file" ] || exit 0
session="$(json_field session_id)"; session="${session:-nosession}"

# Normalise: JSON-escaped Windows separators -> '/', make path relative to the repo.
file="${file//\\\\//}"; file="${file//\\//}"
root_n="${root//\\//}"
rel="${file#"$root_n"/}"
case "$rel" in /*|[A-Za-z]:/*) rel="${rel#*centreon-collect/}" ;; esac

adrs=""
add() { for a in "$@"; do case " $adrs " in *" $a "*) ;; *) adrs="$adrs $a" ;; esac; done; }
case "$rel" in
  agent/*|engine/modules/opentelemetry/*centreon_agent*|common/grpc/*|common/process/*|common/crypto/*|tests/broker-engine/cma*) ;;
  *) exit 0 ;;
esac
case "$rel" in */CMakeLists.txt|vcpkg.json) add 0001 ;; esac
case "$rel" in agent/native_linux/*|agent/native_windows/*) add 0001 0008 ;; esac
case "$rel" in *bireactor*|*streaming_client*|*streaming_server*) add 0003 ;; esac
case "$rel" in agent/proto/*|*agent_info*) add 0004 ;; esac
case "$rel" in *scheduler*) add 0005 0006 ;; esac
case "$rel" in agent/src/check.cc|agent/inc/com/centreon/agent/check.hh|*check_exec*) add 0007 ;; esac
case "$rel" in *native_check*|*drive_size*|*check_health*|*filter*|*/check_*) add 0008 ;; esac
case "$rel" in *grpc_ca_bootstrap*|agent/src/main*.cc) add 0009 ;; esac
case "$rel" in *config*|agent/installer/*|agent/conf/*) add 0010 ;; esac
case "$rel" in *log.cc|*log.hh) add 0011 ;; esac
case "$rel" in agent/test/*|tests/broker-engine/cma*) add 0013 ;; esac
case "$rel" in engine/modules/opentelemetry/*) add 0004 0005 ;; esac
case "$rel" in common/*) add 0001 0002 0009 ;; esac
case "$rel" in *.cc|*.hh) add 0002 0012 ;; esac
add 0013

state_dir="${TMPDIR:-/tmp}/cma-adr-hook"
mkdir -p "$state_dir" 2>/dev/null || exit 0
seen="$state_dir/$session.seen"
touch "$seen" "$state_dir/$session.touched" 2>/dev/null || exit 0
printf '%s\n' "$rel" >> "$state_dir/$session.touched"

ctx="CMA scope: $rel. Governing ADRs:"
for a in $adrs; do ctx="$ctx CMA-$a"; done
ctx="$ctx. Apply their Decision; do not copy a place their Consequences list as not yet followed. Reference: docs/adr/cma/ARCHITECTURE.md."
for a in $adrs; do
  grep -qx "$a" "$seen" && continue
  f="$(ls "$adr_dir"/"$a"-*.md 2>/dev/null | head -n1)"
  [ -n "$f" ] || continue
  title="$(head -n1 "$f" | sed 's/^# //')"
  decision="$(awk '/^## Decision/{f=1;next} /^## /{f=0} f' "$f")"
  ctx="$ctx

=== $title (Decision) ===
$decision"
  printf '%s\n' "$a" >> "$seen"
done
ctx="${ctx:0:9500}"

esc="$(printf '%s' "$ctx" | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/\t/\\t/g' | awk 'BEGIN{ORS="\\n"} {print}')"
printf '{"hookSpecificOutput":{"hookEventName":"PreToolUse","additionalContext":"%s"}}\n' "$esc"
exit 0
