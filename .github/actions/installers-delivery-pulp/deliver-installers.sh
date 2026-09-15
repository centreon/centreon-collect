#!/usr/bin/env bash
set -euo pipefail

# Delivers the monitoring agent Windows installer to the pulp "installers" family.
# Model and rationale: delivery-tooling docs/pulp/concepts/installers-repositories.md

# shellcheck source=.github/scripts/pulp/api.sh
source "$(dirname "$0")/../../scripts/pulp/api.sh"

# an unset org variable is forwarded as an empty string, overriding the default
PULP_URL="${PULP_URL:-https://pulp-api.int.centreon.com}"
PULP_CONTENT_URL="${PULP_CONTENT_URL:-https://packages.int.centreon.com}"
# not edition-scoped, so the installers family lives in the shared "default" domain
PULP_DOMAIN="default"

MODULE_NAME="${MODULE_NAME:?MODULE_NAME is not set}"
MAJOR_VERSION="${MAJOR_VERSION:?MAJOR_VERSION is not set}"
STABILITY="${STABILITY:?STABILITY is not set}"
INSTALLER_PATH="${INSTALLER_PATH:?INSTALLER_PATH is not set}"
INSTALLER_NAME="${INSTALLER_NAME:?INSTALLER_NAME is not set}"
# the "os" segment of the relative path; windows today, kept as an input so a
# future linux installer does not need a second script
INSTALLER_OS="${INSTALLER_OS:-windows}"

RELEASE_TYPE="${RELEASE_TYPE:-}"

# same segments as the rpm/deb repositories (see pulp-repository-properties/properties.sh), and
# deliberately unlike the flat artifactory installers layout: there a release and a hotfix of one
# major share ".../testing/windows/" and --sync-deletes makes the second delivery erase the first.
case "$STABILITY" in
  unstable)
    STABILITY_SEGMENT="unstable"
    ;;
  testing)
    if [[ "$RELEASE_TYPE" != "release" && "$RELEASE_TYPE" != "hotfix" ]]; then
      echo "::error::stability is 'testing' but release_type is '${RELEASE_TYPE:-empty}'. The testing tier is split into testing-release and testing-hotfix, so there is no repository to deliver to."
      exit 1
    fi
    STABILITY_SEGMENT="testing-$RELEASE_TYPE"
    ;;
  *)
    echo "::error::deliver-installers only writes the delivery tiers, got stability '$STABILITY'. Stable is reached through promote-installers.sh on a tag."
    exit 1
    ;;
esac

[[ -f "$INSTALLER_PATH" ]] || { echo "::error::installer not found at $INSTALLER_PATH"; exit 1; }

BASE_PATH="installers/$MODULE_NAME/$MAJOR_VERSION/$STABILITY_SEGMENT"
REPOSITORY_NAME="${BASE_PATH//\//-}"
# deliberately free of the stability: both tiers store the unit at the same
# relative path, which makes promotion a content add of the SAME unit
RELATIVE_PATH="$INSTALLER_OS/$INSTALLER_NAME"

INSTALLER_SIZE=$(stat -c %s "$INSTALLER_PATH")
INSTALLER_SHA256=$(sha256sum "$INSTALLER_PATH" | cut -d' ' -f1)

echo "::group::Delivering $INSTALLER_NAME to $REPOSITORY_NAME"
echo "Repository:    $REPOSITORY_NAME"
echo "Base path:     $BASE_PATH"
echo "Relative path: $RELATIVE_PATH"
echo "Size:          $INSTALLER_SIZE bytes"
echo "sha256:        $INSTALLER_SHA256"

# fail closed: provisioning is create-repos-installers.yml's job in delivery-tooling,
# a delivery must never create the repository it writes to
if ! pulp_resource_exists "repositories/file/file" "$REPOSITORY_NAME"; then
  echo "::error::pulp repository $REPOSITORY_NAME does not exist. Provision the major first with the create-repos-installers workflow in centreon/delivery-tooling."
  exit 1
fi

refresh_pulp_token
REPOSITORY_HREF=$(pulp file repository show --name "$REPOSITORY_NAME" | jq -r '.pulp_href')
[[ -n "$REPOSITORY_HREF" && "$REPOSITORY_HREF" != "null" ]] || {
  echo "::error::could not resolve the href of $REPOSITORY_NAME"
  exit 1
}

# content identity is (sha256, relative_path): an identical re-delivery resolves to the
# unit that is already there, so a re-run is a no-op rather than a duplicate
EXISTING_HREF=$(
  content_curl -fsSL "$PULP_URL/$PULP_DOMAIN/api/v3/content/file/files/?sha256=$INSTALLER_SHA256&relative_path=$RELATIVE_PATH" 2>/dev/null |
    jq -r '.results[0].pulp_href // empty'
) || EXISTING_HREF=""

if [[ -n "$EXISTING_HREF" ]]; then
  echo "[INFO] content unit already exists ($EXISTING_HREF), adding it to $REPOSITORY_NAME"
  CONTENT_HREF="$EXISTING_HREF"
  MODIFY_BODY=$(mktemp)
  jq -nc --arg href "$CONTENT_HREF" '{add_content_units: [$href]}' > "$MODIFY_BODY"
  TASK_HREF=$(start_modify_task "$PULP_URL${REPOSITORY_HREF}modify/" "$MODIFY_BODY")
  rm -f "$MODIFY_BODY"
  wait_task_race "$TASK_HREF" || { echo "::error::could not add the existing unit to $REPOSITORY_NAME"; exit 1; }
  CONTENT_ACTION="reused"
else
  echo "[INFO] uploading $INSTALLER_NAME"
  # uploading THROUGH the repository is required, not a convenience: the stock
  # file content-create policy is has_required_repo_perms_on_upload, which object-checks
  # the repository named in the request and rejects a repository-less create
  TASK_HREF=$(pulp_upload -X POST \
    -F "file=@$INSTALLER_PATH" \
    -F "relative_path=$RELATIVE_PATH" \
    -F "repository=$REPOSITORY_HREF" \
    "$PULP_URL/$PULP_DOMAIN/api/v3/content/file/files/")
  wait_task_race "$TASK_HREF" || { echo "::error::upload of $INSTALLER_NAME failed"; exit 1; }
  CONTENT_HREF=$(
    content_curl -fsSL "$PULP_URL$TASK_HREF" | jq -r '.created_resources[] | select(contains("/content/file/files/"))' | head -1
  )
  CONTENT_ACTION="uploaded"
fi

[[ -n "$CONTENT_HREF" ]] || { echo "::error::could not resolve the delivered content href"; exit 1; }

# The artifactory side keeps a single build in testing (jf rt upload --sync-deletes) and the
# file plugin has no --retain-package-versions, so superseded builds are removed explicitly.
echo "[INFO] removing superseded installers from $REPOSITORY_NAME"
SUPERSEDED=$(
  content_curl -fsSL "$PULP_URL$REPOSITORY_HREF" 2>/dev/null | jq -r '.latest_version_href // empty'
)
if [[ -n "$SUPERSEDED" ]]; then
  mapfile -t STALE_HREFS < <(
    content_curl -fsSL "$PULP_URL/$PULP_DOMAIN/api/v3/content/file/files/?repository_version=$SUPERSEDED&limit=1000" 2>/dev/null |
      jq -r --arg keep "$CONTENT_HREF" --arg prefix "$INSTALLER_OS/" \
        '.results[] | select(.pulp_href != $keep) | select(.relative_path | startswith($prefix)) | .pulp_href'
  )
  if ((${#STALE_HREFS[@]} > 0)); then
    printf '[INFO] removing %d superseded unit(s): %s\n' "${#STALE_HREFS[@]}" "${STALE_HREFS[*]}"
    MODIFY_BODY=$(mktemp)
    jq -nc --args '{remove_content_units: $ARGS.positional}' "${STALE_HREFS[@]}" > "$MODIFY_BODY"
    TASK_HREF=$(start_modify_task "$PULP_URL${REPOSITORY_HREF}modify/" "$MODIFY_BODY")
    rm -f "$MODIFY_BODY"
    wait_task_race "$TASK_HREF" || echo "::warning::could not remove superseded installers from $REPOSITORY_NAME"
  else
    echo "[INFO] nothing superseded to remove"
  fi
fi

echo "::endgroup::"

SERVED_URL="$PULP_CONTENT_URL/$PULP_DOMAIN/$BASE_PATH/$RELATIVE_PATH"

echo "::group::Verifying $SERVED_URL"
# --autopublish makes the new repository version servable without an explicit publication,
# but the content app needs a moment to pick it up
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
  echo "## Windows installer delivered to Pulp"
  echo ""
  echo "| | |"
  echo "|---|---|"
  echo "| Repository | \`$REPOSITORY_NAME\` |"
  echo "| Relative path | \`$RELATIVE_PATH\` |"
  echo "| Content unit | \`$CONTENT_HREF\` ($CONTENT_ACTION) |"
  echo "| Size | $INSTALLER_SIZE bytes |"
  echo "| sha256 | \`$INSTALLER_SHA256\` |"
  echo "| Served | [$SERVED_URL]($SERVED_URL) |"
  echo "| Verification | $VERIFY_STATUS |"
  echo ""
} | tee -a "${GITHUB_STEP_SUMMARY:-/dev/null}"

if [[ "$VERIFY_STATUS" != "served (HTTP 200)" ]]; then
  echo "::error::$INSTALLER_NAME is not served at $SERVED_URL after the delivery."
  exit 1
fi

echo "content_href=$CONTENT_HREF" >> "${GITHUB_OUTPUT:-/dev/null}"
echo "Delivered $INSTALLER_NAME to $REPOSITORY_NAME."
