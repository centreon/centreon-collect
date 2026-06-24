#!/usr/bin/env bash
# Delivers CMA assets to a GitHub release and registers them on download.centreon.com.
#
# Modes
#   local  (default): deliver local files matching --file-pattern
#   action:           fetch from Artifactory stable paths (fallback: re-process existing release assets)
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
# Tokens (--flag or env var):
#   GITHUB_TOKEN                  GitHub token with write:contents scope
#   ARTIFACTORY_ACCESS_TOKEN      JFrog token (action mode only)
#   TOKEN_DOWNLOAD_CENTREON_COM   download.centreon.com registration token
#
# Flags:
#   --dry-run        Log what would happen without executing uploads/registrations
#   --force-reupload Re-upload to GitHub release even if the asset is already there

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
REPO="${DELIVER_REPO:-${GITHUB_REPOSITORY:-centreon/centreon-collect}}"
GITHUB_TOKEN="${GITHUB_TOKEN:-}"
ARTIFACTORY_TOKEN="${ARTIFACTORY_ACCESS_TOKEN:-}"
DOWNLOAD_TOKEN="${TOKEN_DOWNLOAD_CENTREON_COM:-}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --mode)              MODE="$2";               shift 2 ;;
    --file-pattern)      FILE_PATTERN="$2";       shift 2 ;;
    --github-release)    GITHUB_RELEASE="$2";     shift 2 ;;
    --version)           VERSION="$2";            shift 2 ;;
    --distrib)           DISTRIBS="$2";           shift 2 ;;
    --repo)              REPO="$2";               shift 2 ;;
    --github-token)      GITHUB_TOKEN="$2";       shift 2 ;;
    --artifactory-token) ARTIFACTORY_TOKEN="$2";  shift 2 ;;
    --download-token)    DOWNLOAD_TOKEN="$2";     shift 2 ;;
    --dry-run)           DRY_RUN="true";          shift   ;;
    --force-reupload)    FORCE_REUPLOAD="true";   shift   ;;
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
  local release_id="$1" file="$2" filename
  filename=$(basename "$file")
  curl --fail --silent \
    -X POST \
    -H "Authorization: Bearer ${GITHUB_TOKEN}" \
    -H "Content-Type: application/octet-stream" \
    -H "X-GitHub-Api-Version: 2022-11-28" \
    --data-binary "@${file}" \
    "https://uploads.github.com/repos/${REPO_OWNER}/${REPO_NAME}/releases/${release_id}/assets?name=${filename}" \
    | jq -r '.browser_download_url'
}

# ─── Artifactory ──────────────────────────────────────────────────────────────
search_artifactory() {
  local version="$1"
  local aql
  aql=$(printf \
    'items.find({"$or":[{"repo":"rpm-standard"},{"repo":"apt-standard"},{"repo":"ubuntu-standard"},{"repo":"installers"}],"name":{"$match":"centreon-monitoring-agent*%s*"},"path":{"$match":"*/stable*"}}).include("name","repo","path")' \
    "$version")
  curl --fail --silent \
    -X POST \
    -H "Authorization: Bearer ${ARTIFACTORY_TOKEN}" \
    -H "Content-Type: text/plain" \
    --data "$aql" \
    "https://packages.centreon.com/artifactory/api/search/aql"
}

download_from_artifactory() {
  local repo="$1" art_path="$2" name="$3" dest_dir="$4"
  curl --fail --silent \
    -H "Authorization: Bearer ${ARTIFACTORY_TOKEN}" \
    "https://packages.centreon.com/artifactory/${repo}/${art_path}/${name}" \
    -o "${dest_dir}/${name}"
  echo "${dest_dir}/${name}"
}

# ─── download.centreon.com ────────────────────────────────────────────────────
file_size() { stat -c '%s' "$1" 2>/dev/null || wc -c < "$1"; }

register_download() {
  local file="$1" download_url="$2"
  local filename; filename=$(basename "$file")

  # Parse: product-MAJOR.MINOR.PATCH[...].ext
  local product major_ver minor_ver extension
  if [[ "$filename" =~ ^([a-zA-Z0-9-]+)[-_]([0-9]+\.[0-9]+)\.([0-9]+).*\.([a-z]+)$ ]]; then
    product="${BASH_REMATCH[1]}"
    major_ver="${BASH_REMATCH[2]}"
    minor_ver="${BASH_REMATCH[3]}"
    extension="${BASH_REMATCH[4]}"
  else
    log_warn "Cannot parse metadata from ${filename} — skipping download.centreon.com"
    return 0
  fi

  # Distrib suffix
  local distrib_suffix=""
  if [[ "$filename" =~ \.el([0-9]+)\. ]]; then
    distrib_suffix="-el${BASH_REMATCH[1]}"
  elif [[ "$filename" =~ ~deb([0-9]+) ]]; then
    distrib_suffix="-debian-${BASH_REMATCH[1]}"
  elif [[ "$filename" =~ ubuntu[._]([0-9]+\.[0-9]+) ]]; then
    distrib_suffix="-ubuntu-${BASH_REMATCH[1]}"
  elif [[ "$filename" =~ \.exe$ ]]; then
    distrib_suffix="-windows"
  fi

  # Arch suffix
  local arch_suffix=""
  if [[ "$filename" =~ (amd64|x86_64) ]]; then
    arch_suffix="-amd64"
  elif [[ "$filename" =~ (arm64|aarch64) ]]; then
    arch_suffix="-arm64"
  fi

  local file_hash file_sz version_str
  file_hash=$(md5sum "$file" | cut -d' ' -f1)
  file_sz=$(file_size "$file")
  version_str="${major_ver}.${minor_ver}${distrib_suffix}${arch_suffix}"

  if [[ "$DRY_RUN" == "true" ]]; then
    log_skip "[dry-run] Would register: ${product} ${version_str}"
    return 0
  fi

  local response
  response=$(curl --get --fail --silent \
    "https://download.centreon.com/api/" \
    --data-urlencode "token=${DOWNLOAD_TOKEN}" \
    --data-urlencode "product=${product}" \
    --data-urlencode "release=${major_ver}" \
    --data-urlencode "version=${version_str}" \
    --data-urlencode "extension=${extension}" \
    --data-urlencode "md5=${file_hash}" \
    --data-urlencode "size=${file_sz}" \
    --data-urlencode "ddos=0" \
    --data-urlencode "dryrun=0" \
    --data-urlencode "release_url=${download_url}")
  log_ok "Registered on download.centreon.com${response:+: ${response}}"
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
  [[ -z "$VERSION" ]]           && { log_error "--version is required in action mode"; exit 1; }
  [[ -z "$ARTIFACTORY_TOKEN" ]] && { log_error "ARTIFACTORY_ACCESS_TOKEN or --artifactory-token is required in action mode"; exit 1; }

  TMP_DIR=$(mktemp -d)
  trap 'rm -rf "$TMP_DIR"' EXIT

  log_info "Searching Artifactory for CMA ${VERSION} in stable paths…"
  results_json=""
  results_json=$(search_artifactory "$VERSION") || log_warn "Artifactory search failed"

  result_count=$(echo "$results_json" | jq '.results | length' 2>/dev/null || echo 0)
  log_info "${result_count} item(s) found on Artifactory"

  if [[ "$result_count" -eq 0 ]]; then
    log_info "No Artifactory results — re-downloading ${existing_count} existing release asset(s)"
    while IFS=$'\t' read -r asset_name download_url; do
      [[ -z "$asset_name" ]] && continue
      log_info "Downloading: ${asset_name}"
      curl --fail --silent \
        -H "Authorization: Bearer ${GITHUB_TOKEN}" \
        "$download_url" -o "${TMP_DIR}/${asset_name}"
      FILES+=("${TMP_DIR}/${asset_name}")
    done < <(echo "$release_json" | jq -r '.assets[] | [.name, .browser_download_url] | @tsv' 2>/dev/null || true)
  else
    while IFS=$'\t' read -r name repo art_path; do
      [[ -z "$name" ]] && continue
      # Filter by --distrib if provided
      if [[ -n "$DISTRIBS" ]]; then
        match=false
        IFS=',' read -ra distrib_list <<< "$DISTRIBS"
        for d in "${distrib_list[@]}"; do
          if [[ "$d" == "windows" && "$name" == *.exe ]] || [[ "$name" == *"${d}"* ]]; then
            match=true; break
          fi
        done
        [[ "$match" == "false" ]] && { log_skip "Filtered: ${name}"; continue; }
      fi
      local_path=$(download_from_artifactory "$repo" "$art_path" "$name" "$TMP_DIR") \
        && FILES+=("$local_path") \
        || log_error "Failed to download ${name}"
    done < <(echo "$results_json" | jq -r '.results[] | [.name, .repo, .path] | @tsv')
  fi
  log_info "${#FILES[@]} file(s) ready to process"

else
  log_error "Unknown mode: ${MODE} (expected local or action)"; exit 1
fi

# 4. Process each file
log_header "Processing assets"
uploaded=0; skipped=0; failed=0

for file in "${FILES[@]}"; do
  filename=$(basename "$file")
  log_file "$filename"

  on_release=$(echo "$existing_names" | grep -Fx "$filename" || true)
  download_url=""

  # GitHub release upload
  if [[ -n "$on_release" && "$FORCE_REUPLOAD" != "true" ]]; then
    download_url="https://github.com/${REPO}/releases/download/${GITHUB_RELEASE}/${filename}"
    log_skip "Already on release — skipping upload"
    (( skipped++ )) || true
  elif [[ "$DRY_RUN" == "true" ]]; then
    download_url="https://github.com/${REPO}/releases/download/${GITHUB_RELEASE}/${filename}"
    log_skip "[dry-run] Would upload to GitHub release"
    (( uploaded++ )) || true
  else
    log_upload "Uploading to GitHub release…"
    if download_url=$(upload_asset "$release_id" "$file"); then
      log_ok "Uploaded → ${download_url}"
      (( uploaded++ )) || true
    else
      log_error "Upload failed for ${filename}"
      (( failed++ )) || true
      continue
    fi
  fi

  # download.centreon.com registration
  if [[ -n "$DOWNLOAD_TOKEN" ]]; then
    if register_download "$file" "$download_url"; then
      : # logged inside the function
    else
      log_error "download.centreon.com registration failed for ${filename}"
      (( failed++ )) || true
    fi
  fi
done

# 5. Summary
echo -e "\n${BOLD}Summary${RESET}"
echo -e "  Uploaded : ${GREEN}${uploaded}${RESET}"
echo -e "  Skipped  : ${DIM}${skipped}${RESET}"
if [[ "$failed" -gt 0 ]]; then
  echo -e "  Failed   : ${RED}${failed}${RESET}"
  exit 1
else
  echo -e "  Failed   : ${failed}"
fi
