#!/usr/bin/env bash
set -euo pipefail

# Promotes the monitoring agent Windows installer from testing to stable in the pulp
# "installers" family. No bytes move: because the relative path carries no stability,
# both repositories reference the SAME content unit.
# Model and rationale: delivery-tooling docs/pulp/concepts/installers-repositories.md

# shellcheck source=.github/scripts/pulp/api.sh
source "$(dirname "$0")/../../scripts/pulp/api.sh"

# an unset org variable is forwarded as an empty string, overriding the default
PULP_URL="${PULP_URL:-https://pulp-api.int.centreon.com}"
PULP_CONTENT_URL="${PULP_CONTENT_URL:-https://packages.int.centreon.com}"
PULP_DOMAIN="default"

MODULE_NAME="${MODULE_NAME:?MODULE_NAME is not set}"
MAJOR_VERSION="${MAJOR_VERSION:?MAJOR_VERSION is not set}"
INSTALLER_NAME="${INSTALLER_NAME:?INSTALLER_NAME is not set}"
INSTALLER_OS="${INSTALLER_OS:-windows}"

SOURCE_STABILITY="testing"
TARGET_STABILITY="stable"

SOURCE_BASE_PATH="installers/$MODULE_NAME/$MAJOR_VERSION/$SOURCE_STABILITY"
TARGET_BASE_PATH="installers/$MODULE_NAME/$MAJOR_VERSION/$TARGET_STABILITY"
SOURCE_REPOSITORY="${SOURCE_BASE_PATH//\//-}"
TARGET_REPOSITORY="${TARGET_BASE_PATH//\//-}"
RELATIVE_PATH="$INSTALLER_OS/$INSTALLER_NAME"

echo "::group::Promoting $INSTALLER_NAME"
echo "From: $SOURCE_REPOSITORY"
echo "To:   $TARGET_REPOSITORY"
echo "Path: $RELATIVE_PATH"

for REPOSITORY in "$SOURCE_REPOSITORY" "$TARGET_REPOSITORY"; do
  if ! pulp_resource_exists "repositories/file/file" "$REPOSITORY"; then
    echo "::error::pulp repository $REPOSITORY does not exist. Provision the major first with the create-repos-installers workflow in centreon/delivery-tooling."
    exit 1
  fi
done

refresh_pulp_token
SOURCE_HREF=$(pulp file repository show --name "$SOURCE_REPOSITORY" | jq -r '.pulp_href')
TARGET_HREF=$(pulp file repository show --name "$TARGET_REPOSITORY" | jq -r '.pulp_href')

SOURCE_VERSION=$(content_curl -fsSL "$PULP_URL$SOURCE_HREF" | jq -r '.latest_version_href // empty')
[[ -n "$SOURCE_VERSION" ]] || { echo "::error::$SOURCE_REPOSITORY has no repository version to promote from"; exit 1; }

# the unit is looked up by its relative path in the SOURCE repository version, so the
# promoted file is provably the one that was tested, not a same-named rebuild
CONTENT_HREF=$(
  content_curl -fsSL "$PULP_URL/$PULP_DOMAIN/api/v3/content/file/files/?repository_version=$SOURCE_VERSION&relative_path=$RELATIVE_PATH" 2>/dev/null |
    jq -r '.results[0].pulp_href // empty'
) || CONTENT_HREF=""

if [[ -z "$CONTENT_HREF" ]]; then
  echo "::error::$RELATIVE_PATH is not in $SOURCE_REPOSITORY. The testing delivery of this build must run before its promotion."
  exit 1
fi

CONTENT_SHA256=$(content_curl -fsSL "$PULP_URL$CONTENT_HREF" | jq -r '.sha256 // empty')
echo "Content unit: $CONTENT_HREF"
echo "sha256:       $CONTENT_SHA256"

# add_content_units is idempotent, so a re-run of a partially failed promotion is safe
MODIFY_BODY=$(mktemp)
jq -nc --arg href "$CONTENT_HREF" '{add_content_units: [$href]}' > "$MODIFY_BODY"
TASK_HREF=$(start_modify_task "$PULP_URL${TARGET_HREF}modify/" "$MODIFY_BODY")
rm -f "$MODIFY_BODY"
wait_task_race "$TASK_HREF" || { echo "::error::could not add $RELATIVE_PATH to $TARGET_REPOSITORY"; exit 1; }
echo "::endgroup::"

SERVED_URL="$PULP_CONTENT_URL/$PULP_DOMAIN/$TARGET_BASE_PATH/$RELATIVE_PATH"

echo "::group::Verifying $SERVED_URL"
VERIFY_STATUS="not served"
for attempt in 1 2 3 4 5 6; do
  HTTP_CODE=$(curl -sL -o /dev/null -w '%{http_code}' --max-time 60 "$SERVED_URL" || echo 000)
  if [[ "$HTTP_CODE" == "200" ]]; then
    VERIFY_STATUS="served (HTTP 200)"
    break
  fi
  echo "[INFO] attempt $attempt/6: HTTP $HTTP_CODE, retrying"
  sleep 10
done
echo "$VERIFY_STATUS"
echo "::endgroup::"

{
  echo "## Windows installer promoted to stable on Pulp"
  echo ""
  echo "| | |"
  echo "|---|---|"
  echo "| From | \`$SOURCE_REPOSITORY\` |"
  echo "| To | \`$TARGET_REPOSITORY\` |"
  echo "| Relative path | \`$RELATIVE_PATH\` |"
  echo "| Content unit | \`$CONTENT_HREF\` (same unit, no bytes moved) |"
  echo "| sha256 | \`$CONTENT_SHA256\` |"
  echo "| Served | [$SERVED_URL]($SERVED_URL) |"
  echo "| Verification | $VERIFY_STATUS |"
  echo ""
} | tee -a "${GITHUB_STEP_SUMMARY:-/dev/null}"

if [[ "$VERIFY_STATUS" != "served (HTTP 200)" ]]; then
  echo "::error::$INSTALLER_NAME is not served at $SERVED_URL after the promotion."
  exit 1
fi

echo "Promoted $INSTALLER_NAME to $TARGET_REPOSITORY."
