#!/bin/bash
#
# Shared helpers for this repo's docker boot/config-wiring e2e test scripts
# (centreon-engine today; same pattern as centreon/centreon's
# centreon-trapd-common.sh for centreon-centreontrapd/centreon-snmptrapd).

# --- GitHub Step Summary helpers ----------------------------------------
#
# Renders the test's checks as a markdown table in the job's Step Summary
# (visible in the GitHub Actions run UI), on top of the plain-text console
# output these scripts already produce. Usage:
#
#   summary_step_start "Human-readable check name"
#   <run the check>
#   summary_step_pass
#
# Call in order, one pair per check. A check that fails (script exits before
# its matching summary_step_pass) is rendered as failed; checks after it were
# never attempted and simply don't appear in the table. Callers must render
# the table from their own EXIT trap with `_summary_render "<title>" "$rc"`,
# capturing `rc=$?` as the trap's *first* statement so it reflects the
# original exit status rather than the trap's own commands.
#
# Written to $SUMMARY_FRAGMENT_FILE, not $GITHUB_STEP_SUMMARY directly: these
# scripts run as separate matrix legs (one runner each), and GitHub renders
# one Step Summary block per job regardless of what a script writes into it -
# there's no way to merge multiple jobs' summaries into one block from inside
# the job. The docker-test-summary job downloads every leg's fragment as an
# artifact and concatenates them into ONE combined Step Summary instead.
# Falls back to $GITHUB_STEP_SUMMARY when unset, for local/manual runs.
SUMMARY_STEP_NAMES=()
SUMMARY_STEP_STATUS=()

summary_step_start() {
  SUMMARY_STEP_NAMES+=("$1")
  SUMMARY_STEP_STATUS+=("pending")
}

summary_step_pass() {
  local last=$(( ${#SUMMARY_STEP_STATUS[@]} - 1 ))
  SUMMARY_STEP_STATUS[$last]="pass"
}

_summary_render() {
  local title="$1" exit_code="$2"
  {
    echo "### ${title}"
    echo
    echo "| Check | Result |"
    echo "| --- | --- |"
    local i icon
    for i in "${!SUMMARY_STEP_NAMES[@]}"; do
      if [ "${SUMMARY_STEP_STATUS[$i]}" = "pass" ]; then
        icon="✅"
      else
        icon="❌"
      fi
      echo "| ${SUMMARY_STEP_NAMES[$i]} | ${icon} |"
    done
    echo
    if [ "$exit_code" -eq 0 ]; then
      echo "**Result: ✅ PASSED**"
    else
      echo "**Result: ❌ FAILED** (exit code ${exit_code})"
    fi
    echo
  } >> "${SUMMARY_FRAGMENT_FILE:-${GITHUB_STEP_SUMMARY:-/dev/null}}"
}
