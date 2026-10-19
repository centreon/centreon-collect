#!/usr/bin/env bash
# Delivers CMA assets to a GitHub release.
#
# The assets are published to the download site by the release YAML committed to
# centreon/WebApp-download, not from here.
#
# Modes
#   local  (default): deliver local files matching --file-pattern
#   action:           fetch from the Pulp stable repositories (fallback: re-process existing release assets)
#
# Usage (local):
#   ./deliver-cma-assets.sh \
#     --github-release centreon-monitoring-agent-27.08.3 \
#     --file-pattern "/tmp/packages/centreon-monitoring-agent-27.08*.rpm"
#
# Usage (action — re-run a failed delivery):
#   ./deliver-cma-assets.sh \
#     --mode action \
#     --github-release centreon-monitoring-agent-27.08.3 \
#     --version 27.08.3 \
#     [--distrib el8,el9,el10,windows]
#
# Action mode reads the public indexes Pulp serves (rpm repodata, deb Packages), so it needs
# no Pulp credential. DELIVER_PACKAGES_URL overrides the content host (default
# https://packages.centreon.com).
#
# Tokens (--flag or env var):
#   GITHUB_TOKEN                  GitHub token with write:contents scope
#
# Flags:
#   --dry-run        Log what would happen without executing uploads
#   --force-reupload Re-upload to GitHub release even if the asset is already there
#   --strict         Action mode: fail when a distrib or the windows installer is missing from
#                    Pulp stable, instead of only warning

set -euo pipefail

# ─── Colours ──────────────────────────────────────────────────────────────────
if [[ -t 1 ]]; then
  BOLD='\033[1m' RESET='\033[0m' GREEN='\033[32m' YELLOW='\033[33m'
  BLUE='\033[34m' CYAN='\033[36m' RED='\033[31m' DIM='\033[2m'
else
  BOLD='' RESET='' GREEN='' YELLOW='' BLUE='' CYAN='' RED='' DIM=''
fi

log_header() { echo -e "\n${BOLD}${BLUE}◆ $*${RESET}"; }
log_info()   { echo -e "  ${DIM}ℹ${RESET}  $*"; }
log_ok()     { echo -e "  ${GREEN}✓${RESET}  $*"; }
log_skip()   { echo -e "  ${DIM}→${RESET}  $*"; }
log_upload() { echo -e "  ${CYAN}↑${RESET}  $*"; }
log_warn()   { echo -e "  ${YELLOW}⚠${RESET}  $*"; }
log_error()  { echo -e "  ${RED}✗${RESET}  $*" >&2; }
log_file()   { echo -e "\n  ${BOLD}$*${RESET}"; }

# ─── Config ───────────────────────────────────────────────────────────────────
MODE="${DELIVER_MODE:-local}"
FILE_PATTERN="${DELIVER_FILE_PATTERN:-}"
GITHUB_RELEASE="${DELIVER_GITHUB_RELEASE:-}"
VERSION="${DELIVER_VERSION:-}"
DISTRIBS="${DELIVER_DISTRIB:-}"
DRY_RUN="${DELIVER_DRY_RUN:-false}"
FORCE_REUPLOAD="false"
STRICT="${DELIVER_STRICT:-false}"
REPO="${DELIVER_REPO:-${GITHUB_REPOSITORY:-centreon/centreon-collect}}"
GITHUB_TOKEN="${GITHUB_TOKEN:-}"
PACKAGES_URL="${DELIVER_PACKAGES_URL:-https://packages.centreon.com}"
pulp_found=0
fetch_failed=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --mode)              MODE="$2";               shift 2 ;;
    --file-pattern)      FILE_PATTERN="$2";       shift 2 ;;
    --github-release)    GITHUB_RELEASE="$2";     shift 2 ;;
    --version)           VERSION="$2";            shift 2 ;;
    --distrib)           DISTRIBS="$2";           shift 2 ;;
    --repo)              REPO="$2";               shift 2 ;;
    --github-token)      GITHUB_TOKEN="$2";       shift 2 ;;
    --dry-run)           DRY_RUN="true";          shift   ;;
    --force-reupload)    FORCE_REUPLOAD="true";   shift   ;;
    --strict)            STRICT="true";           shift   ;;
    *) log_error "Unknown argument: $1"; exit 1 ;;
  esac
done

REPO_OWNER="${REPO%%/*}"
REPO_NAME="${REPO##*/}"

# ─── Validation ───────────────────────────────────────────────────────────────
[[ -z "$GITHUB_RELEASE" ]] && { log_error "--github-release is required"; exit 1; }
[[ -z "$GITHUB_TOKEN"   ]] && { log_error "GITHUB_TOKEN or --github-token is required"; exit 1; }

# ─── GitHub API ───────────────────────────────────────────────────────────────
gh_api() {
  local method="$1" endpoint="$2"
  curl --fail --silent --show-error --max-time 30 \
    -X "$method" \
    -H "Authorization: Bearer ${GITHUB_TOKEN}" \
    -H "Accept: application/vnd.github+json" \
    -H "X-GitHub-Api-Version: 2022-11-28" \
    "${@:3}" \
    "https://api.github.com${endpoint}"
}

get_release() {
  gh_api GET "/repos/${REPO_OWNER}/${REPO_NAME}/releases/tags/${GITHUB_RELEASE}"
}

list_asset_names() {
  local release_id="$1" page=1
  while true; do
    local batch
    batch=$(gh_api GET "/repos/${REPO_OWNER}/${REPO_NAME}/releases/${release_id}/assets?per_page=100&page=${page}" | jq -r '.[].name')
    echo "$batch"
    [[ $(echo "$batch" | wc -l) -lt 100 ]] && break
    (( page++ ))
  done
}

upload_asset() {
  local release_id="$1" file="$2" filename encoded_name
  filename=$(basename "$file")
  # Percent-encode: an unescaped "+" in the query string reaches GitHub as a
  # space, which it then stores as "." (deb "1+deb12u1" became "1.deb12u1").
  encoded_name=$(jq -rn --arg name "$filename" '$name|@uri')
  curl --fail --silent \
    -X POST \
    -H "Authorization: Bearer ${GITHUB_TOKEN}" \
    -H "Content-Type: application/octet-stream" \
    -H "X-GitHub-Api-Version: 2022-11-28" \
    --data-binary "@${file}" \
    "https://uploads.github.com/repos/${REPO_OWNER}/${REPO_NAME}/releases/${release_id}/assets?name=${encoded_name}" \
    | jq -r '.browser_download_url'
}

# ─── Pulp ─────────────────────────────────────────────────────────────────────
# Stable CMA content under the Pulp domain paths: rpm and deb in the "standard"
# domain, the windows installer in the "default" installers family. Only the
# packages local mode delivers are matched (no debuginfo/selinux), so a re-run
# uploads exactly what a normal run would. Content redirects to signed CDN urls.
pulp_get() {
  curl --fail --silent --show-error --location --max-time 300 --retry 3 --retry-delay 5 "$@"
}

# A missing distrib drops its row from the download site: an error in strict mode.
report_missing() {
  if [[ "$STRICT" == "true" ]]; then
    log_error "$*"
    (( fetch_failed++ )) || true
  else
    log_warn "$*"
  fi
}

http_status() {
  curl --silent --location --head --max-time 60 -o /dev/null -w '%{http_code}' "$1" || true
}

fetch_to_tmp() {
  local url="$1" name d match
  name="${url##*/}"
  (( pulp_found++ )) || true
  # Filter by --distrib if provided
  if [[ -n "$DISTRIBS" ]]; then
    match=false
    IFS=',' read -ra distrib_list <<< "$DISTRIBS"
    for d in "${distrib_list[@]}"; do
      if [[ "$d" == "windows" && "$name" == *.exe ]] || [[ "$name" == *"${d}"* ]]; then
        match=true; break
      fi
    done
    [[ "$match" == "false" ]] && { log_skip "Filtered: ${name}"; return 0; }
  fi
  if pulp_get "$url" -o "${TMP_DIR}/${name}"; then
    log_info "Downloaded: ${name}"
    FILES+=("${TMP_DIR}/${name}")
  else
    log_error "Failed to download ${url}"
    (( fetch_failed++ )) || true
  fi
}

fetch_rpm() {
  local distrib="$1" base primary primary_xml href found=0
  base="${PACKAGES_URL}/standard/rpm/${MAJOR}/${distrib}/stable/x86_64"
  # no stable repository for this major/distrib: nothing to fetch, like an empty search
  if [[ "$(http_status "${base}/repodata/repomd.xml")" == "404" ]]; then
    report_missing "No ${distrib} stable repository for ${MAJOR}"
    return 0
  fi
  if ! primary=$(pulp_get "${base}/repodata/repomd.xml" | grep -m1 -o 'repodata/[^"]*primary\.xml[^"]*') \
    || ! primary_xml=$(pulp_get "${base}/${primary}" | gunzip -c); then
    log_error "Cannot read the ${distrib} index at ${base}"
    (( fetch_failed++ )) || true
    return 0
  fi
  while IFS= read -r href; do
    if [[ "${href##*/}" == "centreon-monitoring-agent-${VERSION}-"*.rpm ]]; then
      fetch_to_tmp "${base}/${href}"
      found=1
    fi
  done < <(grep -o 'location href="[^"]*"' <<< "$primary_xml" | sed 's/^location href="//; s/"$//' || true)
  [[ "$found" -eq 1 ]] || report_missing "No CMA ${VERSION} in ${distrib} stable"
}

fetch_deb() {
  local repo="$1" distrib="$2" base arch index packages filename found=0
  base="${PACKAGES_URL}/standard/${repo}"
  for arch in amd64 arm64; do
    index="${base}/dists/${distrib}-${MAJOR}-stable/main/binary-${arch}/Packages"
    if [[ "$(http_status "$index")" == "404" ]]; then
      log_warn "No ${distrib} ${arch} stable suite for ${MAJOR}"
      continue
    fi
    if ! packages=$(pulp_get "$index"); then
      log_error "Cannot read the ${distrib} ${arch} index at ${base}"
      (( fetch_failed++ )) || true
      continue
    fi
    while IFS= read -r filename; do
      [[ -z "$filename" ]] && continue
      fetch_to_tmp "${base}/${filename}"
      found=1
    done < <(awk -v RS= -v ver="${VERSION}-" '
      /(^|\n)Package: centreon-monitoring-agent\n/ {
        v = ""; f = ""
        n = split($0, line, "\n")
        for (i = 1; i <= n; i++) {
          if (line[i] ~ /^Version: /)  v = substr(line[i], 10)
          if (line[i] ~ /^Filename: /) f = substr(line[i], 11)
        }
        if (index(v, ver) == 1) print f
      }' <<< "$packages")
  done
  [[ "$found" -eq 1 ]] || report_missing "No CMA ${VERSION} in ${distrib} stable"
}

fetch_installer() {
  local url code
  url="${PACKAGES_URL}/default/installers/monitoring-agent/${MAJOR}/stable/windows/centreon-monitoring-agent-${VERSION}.exe"
  code=$(http_status "$url")
  case "$code" in
    200) fetch_to_tmp "$url" ;;
    404) report_missing "No CMA ${VERSION} windows installer in stable" ;;
    *)   log_error "Cannot reach ${url} (HTTP ${code:-network-error})"; (( fetch_failed++ )) || true ;;
  esac
}

# ─── Main ─────────────────────────────────────────────────────────────────────
echo -e "\n${BOLD}${BLUE}CMA Asset Delivery${RESET}"
echo -e "  Mode    : ${CYAN}${MODE}${RESET}"
echo -e "  Release : ${CYAN}${GITHUB_RELEASE}${RESET}"
echo -e "  Repo    : ${DIM}${REPO}${RESET}"
[[ "$DRY_RUN" == "true" ]] && echo -e "  ${YELLOW}DRY RUN — no uploads or registrations will be performed${RESET}"

# 1. Find the GitHub release
log_header "GitHub release"
log_info "Looking up tag: ${GITHUB_RELEASE}"
if ! release_json=$(get_release 2>&1); then
  log_error "Release not found or API error: ${release_json}"
  exit 1
fi
release_id=$(echo "$release_json" | jq -r '.id')
release_name=$(echo "$release_json" | jq -r '.name')
log_ok "\"${release_name}\" (id ${release_id})"

# 2. List existing assets (idempotency)
existing_names=$(list_asset_names "$release_id")
existing_count=$(echo "$existing_names" | grep -c . || true)
log_info "${existing_count} asset(s) already on release"

# 3. Collect files
log_header "Collecting files"
declare -a FILES=()

if [[ "$MODE" == "local" ]]; then
  [[ -z "$FILE_PATTERN" ]] && { log_error "--file-pattern is required in local mode"; exit 1; }
  while IFS= read -r -d '' f; do
    FILES+=("$f")
  done < <(find "$(dirname "$FILE_PATTERN")" -maxdepth 1 \
    -name "$(basename "$FILE_PATTERN")" -print0 2>/dev/null || true)
  [[ ${#FILES[@]} -eq 0 ]] && { log_warn "No files matched: ${FILE_PATTERN}"; exit 0; }
  log_info "${#FILES[@]} file(s) matched ${FILE_PATTERN}"

elif [[ "$MODE" == "action" ]]; then
  [[ -z "$VERSION" ]] && { log_error "--version is required in action mode"; exit 1; }
  MAJOR="${VERSION%.*}"

  TMP_DIR=$(mktemp -d)
  trap 'rm -rf "$TMP_DIR"' EXIT

  # Pulp has no cross-repository search like the former AQL: walk the stable
  # repositories the CMA is delivered to (monitoring-agent.yml matrix)
  log_info "Searching Pulp (${PACKAGES_URL}) for CMA ${VERSION} in stable repositories…"
  for d in el8 el9 el10; do fetch_rpm "$d"; done
  for d in bookworm trixie; do fetch_deb apt-stable "$d"; done
  for d in jammy noble; do fetch_deb ubuntu-stable "$d"; done
  fetch_installer
  log_info "${pulp_found} item(s) found on Pulp"

  if [[ "$pulp_found" -eq 0 && "$fetch_failed" -eq 0 ]]; then
    log_info "No Pulp results — re-downloading ${existing_count} existing release asset(s)"
    while IFS=$'\t' read -r asset_name download_url; do
      [[ -z "$asset_name" ]] && continue
      log_info "Downloading: ${asset_name}"
      curl --fail --silent \
        -H "Authorization: Bearer ${GITHUB_TOKEN}" \
        "$download_url" -o "${TMP_DIR}/${asset_name}"
      FILES+=("${TMP_DIR}/${asset_name}")
    done < <(echo "$release_json" | jq -r '.assets[] | [.name, .browser_download_url] | @tsv' 2>/dev/null || true)
  fi
  log_info "${#FILES[@]} file(s) ready to process"

else
  log_error "Unknown mode: ${MODE} (expected local or action)"; exit 1
fi

# 4. Process each file
log_header "Processing assets"
# an unreadable Pulp index or a failed download counts as a failure
uploaded=0; skipped=0; failed=$fetch_failed

SUMMARY_ROWS=""

for file in "${FILES[@]}"; do
  filename=$(basename "$file")
  log_file "$filename"

  on_release=$(echo "$existing_names" | grep -Fx "$filename" || true)
  download_url=""

  # GitHub release upload
  if [[ -n "$on_release" && "$FORCE_REUPLOAD" != "true" ]]; then
    download_url="https://github.com/${REPO}/releases/download/${GITHUB_RELEASE}/${filename}"
    log_skip "Already on release — skipping upload"
    SUMMARY_ROWS+="| \`$filename\` | already present | [link]($download_url) |"$'\n'
    (( skipped++ )) || true
  elif [[ "$DRY_RUN" == "true" ]]; then
    download_url="https://github.com/${REPO}/releases/download/${GITHUB_RELEASE}/${filename}"
    log_skip "[dry-run] Would upload to GitHub release"
    SUMMARY_ROWS+="| \`$filename\` | would upload (dry run) | — |"$'\n'
    (( uploaded++ )) || true
  else
    log_upload "Uploading to GitHub release…"
    if download_url=$(upload_asset "$release_id" "$file"); then
      log_ok "Uploaded → ${download_url}"
      SUMMARY_ROWS+="| \`$filename\` | uploaded | [link]($download_url) |"$'\n'
      (( uploaded++ )) || true
    else
      log_error "Upload failed for ${filename}"
      SUMMARY_ROWS+="| \`$filename\` | **upload failed** | — |"$'\n'
      (( failed++ )) || true
      continue
    fi
  fi

done

# 5. Summary
{
  echo "## GitHub release assets — ${GITHUB_RELEASE}"
  echo ""
  echo "| Asset | Result | Download |"
  echo "|---|---|---|"
  printf '%s' "$SUMMARY_ROWS"
  echo ""
  echo "Uploaded ${uploaded} · skipped ${skipped} · failed ${failed}"
  echo ""
} | tee -a "${GITHUB_STEP_SUMMARY:-/dev/null}" > /dev/null

echo -e "\n${BOLD}Summary${RESET}"
echo -e "  Uploaded : ${GREEN}${uploaded}${RESET}"
echo -e "  Skipped  : ${DIM}${skipped}${RESET}"
if [[ "$failed" -gt 0 ]]; then
  echo -e "  Failed   : ${RED}${failed}${RESET}"
  exit 1
else
  echo -e "  Failed   : ${failed}"
fi
