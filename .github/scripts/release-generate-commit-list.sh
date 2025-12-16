#!/usr/bin/env bash
set -euo pipefail

# Inputs
# BASE_SHA is latest stable release bundle tag commit sha
# HEAD_SHA is release or hotfix branch head sha
BASE_SHA="$1"
HEAD_SHA="$2"

# Prepare list of commit-list
mkdir -p artifacts/commit-lists

# Generate list of commits for components
while read -r component; do
  echo "Generating commit list for $component"

  paths="${COMPONENT_PATHS[$component]}"

  if [ -z "$paths" ]; then
    echo "No paths defined for $component, skipping"
    continue
  fi

  git log \
    --pretty=format:'%h %s' \
    "$BASE_SHA..$HEAD_SHA" \
    -- $paths \
    > "artifacts/commit-lists/${component}.txt"

done < artifacts/components.txt
