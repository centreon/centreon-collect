#!/usr/bin/env bash
set -euo pipefail

# Builds the download-site entries for one monitoring agent release, from the assets actually
# attached to its GitHub release. Reading the release back (rather than reconstructing names)
# is what keeps the published url and its checksum from ever disagreeing.
#
# Emits on stdout the TSV publish-download-release.sh consumes:
#   product  train  state  os  version  file  date  md5  size
# and writes the windows installer's catalog entry to $AGENT_ENTRY_FILE as
#   version<TAB>file_url<TAB>md5<TAB>size

RELEASE_TAG="${RELEASE_TAG:?RELEASE_TAG is not set}"
CMA_VERSION="${CMA_VERSION:?CMA_VERSION is not set}"
REPOSITORY="${REPOSITORY:-centreon/centreon-collect}"
AGENT_ENTRY_FILE="${AGENT_ENTRY_FILE:-agent-entry.tsv}"
WORKDIR="${WORKDIR:-$(mktemp -d)}"

PRODUCT="centreon-monitoring-agent"
TRAIN="${CMA_VERSION%.*}"
STATE="stable"

die() { echo "::error::cma-release-entries: $*" >&2; exit 1; }
# classify() runs inside a command substitution, where `die` would only kill the subshell and
# read back as "not an installer" -- a malformed name would silently drop an OS from the release.
malformed() { echo "::error::cma-release-entries: $*" >&2; return 2; }

[[ "$CMA_VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || die "CMA_VERSION must be MAJOR.MINOR.PATCH (got '$CMA_VERSION')"

# os and version follow WebApp-download's own convention: os is the short distribution form,
# version the long one plus the architecture. agent/25.10/25.10.8.yaml is the reference.
classify() {
  local name="$1" arch os
  case "$name" in
    *.exe)
      printf '%s\t%s-windows\n' "" "$CMA_VERSION"
      return 0
      ;;
    *.rpm)
      [[ "$name" =~ \.(el[0-9]+)\. ]] || malformed "cannot read the el release from '$name'"
      os="${BASH_REMATCH[1]}"
      case "$name" in
        *.x86_64.rpm) arch="amd64" ;;
        *.aarch64.rpm) arch="arm64" ;;
        *) malformed "cannot read the arch from '$name'" ;;
      esac
      printf '%s\t%s-%s-%s\n' "$os" "$CMA_VERSION" "$os" "$arch"
      return 0
      ;;
    *.deb)
      case "$name" in
        *_amd64.deb) arch="amd64" ;;
        *_arm64.deb) arch="arm64" ;;
        *) malformed "cannot read the arch from '$name'" ;;
      esac
      # debian: "-1+deb12u1" up to 25.10.8, "-1.deb12u1" from 25.10.9 - accept both separators
      if [[ "$name" =~ -1[+.]deb([0-9]+)u[0-9]+_ ]]; then
        os="deb${BASH_REMATCH[1]}"
        printf '%s\t%s-debian-%s-%s\n' "$os" "$CMA_VERSION" "${BASH_REMATCH[1]}" "$arch"
        return 0
      fi
      # ubuntu: "-1-0ubuntu.24.04"
      if [[ "$name" =~ -0ubuntu\.([0-9]+\.[0-9]+)_ ]]; then
        os="ubuntu.${BASH_REMATCH[1]}"
        printf '%s\t%s-ubuntu-%s-%s\n' "$os" "$CMA_VERSION" "${BASH_REMATCH[1]}" "$arch"
        return 0
      fi
      malformed "cannot read the distribution from '$name'"
      ;;
    *)
      return 1
      ;;
  esac
}

command -v gh >/dev/null 2>&1 \
  || die "the gh cli is required to read the release assets but is not installed on this runner"
command -v jq >/dev/null 2>&1 || die "jq is required but is not installed on this runner"

mkdir -p "$WORKDIR/assets"

RELEASE_JSON="$WORKDIR/release.json"
gh release view "$RELEASE_TAG" --repo "$REPOSITORY" \
  --json publishedAt,assets > "$RELEASE_JSON" \
  || die "release $RELEASE_TAG not found in $REPOSITORY"

# the site's date field is the release date, not the run date: re-running the publication
# must not change the published metadata
DATE=$(jq -r '.publishedAt' "$RELEASE_JSON")
[[ "$DATE" =~ ^[0-9]{4}-[0-9]{2}-[0-9]{2}T ]] || die "unusable publishedAt '$DATE'"

ASSET_COUNT=$(jq -r '.assets | length' "$RELEASE_JSON")
(( ASSET_COUNT > 0 )) || die "release $RELEASE_TAG has no asset"

echo "[INFO] $ASSET_COUNT asset(s) on $RELEASE_TAG, published $DATE" >&2

: > "$AGENT_ENTRY_FILE"
EMITTED=0

while IFS=$'\t' read -r NAME URL SIZE; do
  [[ -n "$NAME" ]] || continue
  CLASSIFIED=$(classify "$NAME") || CLASSIFY_RC=$?
  case "${CLASSIFY_RC:-0}" in
    0) ;;
    1) echo "[INFO] ignoring $NAME (not a published installer)" >&2; unset CLASSIFY_RC; continue ;;
    *) die "$NAME is an installer but its name could not be parsed - refusing to publish a release with a missing row" ;;
  esac
  OS="${CLASSIFIED%%$'\t'*}"
  VERSION="${CLASSIFIED#*$'\t'}"

  # md5 is not in the api: the asset has to be fetched. The site shows it for manual
  # verification, so it must be the checksum of the very file the url serves.
  LOCAL="$WORKDIR/assets/$NAME"
  curl -fsSL --retry 3 --retry-all-errors -o "$LOCAL" "$URL" \
    || die "could not download $URL"
  ACTUAL_SIZE=$(stat -c %s "$LOCAL")
  [[ "$ACTUAL_SIZE" == "$SIZE" ]] \
    || die "$NAME is $ACTUAL_SIZE bytes but the release api reports $SIZE"
  MD5=$(md5sum "$LOCAL" | cut -d' ' -f1)
  rm -f "$LOCAL"

  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "$PRODUCT" "$TRAIN" "$STATE" "$OS" "$VERSION" "$URL" "$DATE" "$MD5" "$SIZE"
  EMITTED=$((EMITTED + 1))

  if [[ "$NAME" == *.exe ]]; then
    printf '%s\t%s\t%s\t%s\n' "$CMA_VERSION" "$URL" "$MD5" "$SIZE" > "$AGENT_ENTRY_FILE"
  fi
done < <(jq -r '.assets[] | [.name, .url, .size] | @tsv' "$RELEASE_JSON")

(( EMITTED > 0 )) || die "no publishable asset found on $RELEASE_TAG"
[[ -s "$AGENT_ENTRY_FILE" ]] || die "no windows installer (.exe) on $RELEASE_TAG - the Agent tab entry cannot be built"

echo "[INFO] emitted $EMITTED entry(ies)" >&2
