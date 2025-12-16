#!/usr/bin/env bash
set -euo pipefail

BASE_SHA="$1"
HEAD_SHA="$2"

mkdir -p artifacts/commit-lists

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
