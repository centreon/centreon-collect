#!/usr/bin/env bash
# Publishes release metadata to download.centreon.com by committing to
# centreon/WebApp-download and opening a PR against its default branch.
#
# Vendored from centreon/centreon-images (PR #379) and adapted for the monitoring agent: the S3
# sidecar is gone (CMA assets live on the GitHub releases, not in a bucket) and the catalog.yaml
# agent block is edited alongside the release YAML, because the Agent tab renders only that block
# -- the release rows are stored and validated but displayed nowhere.
# Contract: delivery-tooling docs/download-website/{asset-publication,cma}.md
set -euo pipefail

readonly SCRIPT_NAME="${0##*/}"
# resolved once: the catalog helper sits next to this script, and the commit phase cd's away
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

WEBAPP_REPO="${WEBAPP_REPO:-centreon/WebApp-download}"
WEBAPP_BASE="${WEBAPP_BASE:-develop}"
ENTRIES_FILE=""
OUT_NAME=""
BRANCH=""
COMMIT_MESSAGE=""
PR_TITLE=""
PR_BODY_FILE=""
PR_LABEL="release"
DRY_RUN="false"
AGENT_VERSION=""
AGENT_FILE_URL=""
AGENT_MD5=""
AGENT_SIZE=""
WORKDIR=""

log()      { printf '%s\n' "$*"; }
log_ok()   { printf '\033[0;32m✓\033[0m %s\n' "$*"; }
log_skip() { printf '\033[0;33m-\033[0m %s\n' "$*"; }
die()      { printf '\033[0;31m✗\033[0m %s\n' "$*" >&2; exit 1; }

cleanup() { [[ -n "$WORKDIR" && -d "$WORKDIR" ]] && rm -rf "$WORKDIR"; }
trap cleanup EXIT

# Recap of where the metadata went, so a run page shows the outcome without
# anyone reading the log. Falls back to stdout when run outside Actions.
write_summary() {
  local pr_state="$1"
  {
    echo "### Download site entry"
    echo ""
    echo "| | |"
    echo "|---|---|"
    echo "| Repository | \`${WEBAPP_REPO}\` (\`${WEBAPP_BASE}\`) |"
    echo "| File | \`${out_rel}\` |"
    echo "| Agent catalog | ${catalog_line:-not edited} |"
    echo "| Entries | ${entry_count} |"
    echo "| Branch | \`${BRANCH}\` |"
    echo "| Pull request | ${pr_state} |"
    echo "| Validation | ${validate_line:-not run} |"
    echo ""
  } | tee -a "${GITHUB_STEP_SUMMARY:-/dev/null}"
}

usage() {
  cat <<EOF
Usage: $SCRIPT_NAME --entries FILE --out-name NAME [options]

Required:
  --entries FILE       TSV file, one release entry per line (see below)
  --out-name NAME      release YAML filename, e.g. 25.10-20260600-alma9.yaml
  --branch NAME        branch to create in $WEBAPP_REPO
  --commit-message MSG commit message
  --pr-title TITLE     pull request title

Optional:
  --pr-body-file FILE  pull request body (default: generated)
  --pr-label LABEL     label to apply (default: $PR_LABEL, "" to skip)
  --agent-version V    monitoring agent version whose windows installer is featured on the
                       Agent tab, e.g. 25.10.9 (omit to leave catalog.yaml untouched)
  --agent-file-url URL windows installer url for that catalog entry
  --agent-md5 MD5      windows installer md5
  --agent-size BYTES   windows installer size
  --dry-run            print what would happen, mutate nothing
  --help

Entry TSV columns (tab-separated, no header):
  product  train  state  os  version  file  date  md5  size

  os may be empty (the windows installer carries an empty one). All entries
  must belong to the same product group and train.

Environment:
  WEBAPP_REPO   target repo (default: centreon/WebApp-download)
  WEBAPP_BASE   base branch  (default: develop)
  GH_TOKEN      token with contents:write and pull_requests:write on WEBAPP_REPO
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --entries)        ENTRIES_FILE="${2:?--entries needs a value}"; shift 2 ;;
    --out-name)       OUT_NAME="${2:?--out-name needs a value}"; shift 2 ;;
    --branch)         BRANCH="${2:?--branch needs a value}"; shift 2 ;;
    --commit-message) COMMIT_MESSAGE="${2:?--commit-message needs a value}"; shift 2 ;;
    --pr-title)       PR_TITLE="${2:?--pr-title needs a value}"; shift 2 ;;
    --pr-body-file)   PR_BODY_FILE="${2:?--pr-body-file needs a value}"; shift 2 ;;
    --pr-label)       PR_LABEL="${2-}"; shift 2 ;;
    --agent-version)  AGENT_VERSION="${2:?--agent-version needs a value}"; shift 2 ;;
    --agent-file-url) AGENT_FILE_URL="${2:?--agent-file-url needs a value}"; shift 2 ;;
    --agent-md5)      AGENT_MD5="${2:?--agent-md5 needs a value}"; shift 2 ;;
    --agent-size)     AGENT_SIZE="${2:?--agent-size needs a value}"; shift 2 ;;
    --dry-run)        DRY_RUN="true"; shift ;;
    --help|-h)        usage; exit 0 ;;
    *)                die "unknown option: $1 (see --help)" ;;
  esac
done

[[ -n "$ENTRIES_FILE" ]]    || die "--entries is required (see --help)"
[[ -f "$ENTRIES_FILE" ]]    || die "entries file not found: $ENTRIES_FILE"
[[ -n "$OUT_NAME" ]]        || die "--out-name is required"
[[ -n "$BRANCH" ]]          || die "--branch is required"
[[ -n "$COMMIT_MESSAGE" ]]  || die "--commit-message is required"
[[ -n "$PR_TITLE" ]]        || die "--pr-title is required"
[[ "$OUT_NAME" == *.yaml ]] || die "--out-name must end in .yaml: $OUT_NAME"

for tool in git curl; do
  command -v "$tool" >/dev/null 2>&1 || die "$tool is required but not installed"
done

# ---------------------------------------------------------------------------
# Parse and validate entries
# ---------------------------------------------------------------------------
# Parallel arrays; bash 4 has no array-of-struct. Index i is one release entry.
declare -a E_PRODUCT E_TRAIN E_STATE E_OS E_VERSION E_FILE E_DATE E_MD5 E_SIZE
entry_count=0
line_no=0

while IFS= read -r line || [[ -n "$line" ]]; do
  line_no=$((line_no + 1))
  if [[ -z "${line//[[:space:]]/}" || "$line" == \#* ]]; then
    continue
  fi

  # Tab counts as IFS whitespace, so `read -r a b c` collapses runs of tabs and
  # an empty os column would shift every later field. mapfile keeps empties.
  mapfile -t -d $'\t' cols < <(printf '%s' "$line")

  local_ctx="entry on line $line_no"
  [[ "${#cols[@]}" -ge 9 ]] \
    || die "$local_ctx: expected 9 or 10 tab-separated columns, got ${#cols[@]}"

  product="${cols[0]-}"; train="${cols[1]-}";   state="${cols[2]-}"
  os="${cols[3]-}";      version="${cols[4]-}"; file="${cols[5]-}"
  date="${cols[6]-}";    md5="${cols[7]-}";     size="${cols[8]-}"

  [[ -n "$product" ]] || die "$local_ctx: product is required"
  [[ -n "$train"   ]] || die "$local_ctx: train is required"
  [[ -n "$version" ]] || die "$local_ctx: version is required"
  [[ -n "$file"    ]] || die "$local_ctx: file is required"
  [[ -n "$date"    ]] || die "$local_ctx: date is required (schema has no default)"

  # Mirror src/lib/schema.js: state enum, UTC date shape, md5 hex, integer size.
  case "${state:=stable}" in
    stable|rc|beta) ;;
    *) die "$local_ctx: state must be stable, rc or beta (got '$state')" ;;
  esac
  [[ "$date" =~ ^[0-9]{4}-[0-9]{2}-[0-9]{2}(T[0-9]{2}:[0-9]{2}(:[0-9]{2})?Z)?$ ]] \
    || die "$local_ctx: date must be YYYY-MM-DD or YYYY-MM-DDTHH:MM:SSZ (got '$date')"
  [[ "$md5" =~ ^[0-9a-f]{32}$ ]] \
    || die "$local_ctx: md5 must be 32 lowercase hex chars (got '$md5')"
  [[ "$size" =~ ^[0-9]+$ ]] \
    || die "$local_ctx: size must be a non-negative integer in bytes (got '$size')"
  [[ "$size" -gt 0 ]] \
    || die "$local_ctx: size is 0 - refusing to publish an empty artifact"

  E_PRODUCT+=("$product"); E_TRAIN+=("$train");   E_STATE+=("$state")
  E_OS+=("$os");           E_VERSION+=("$version"); E_FILE+=("$file")
  E_DATE+=("$date");       E_MD5+=("$md5");       E_SIZE+=("$size")
  entry_count=$((entry_count + 1))
done < "$ENTRIES_FILE"

[[ "$entry_count" -gt 0 ]] || die "no entries found in $ENTRIES_FILE"

train="${E_TRAIN[0]}"
for i in "${!E_TRAIN[@]}"; do
  [[ "${E_TRAIN[$i]}" == "$train" ]] \
    || die "all entries must share one train: '${E_TRAIN[$i]}' != '$train'"
done

log "→ $entry_count entry(ies) for train $train"

# ---------------------------------------------------------------------------
# Clone the target repo and run pre-flight checks against its catalog
# ---------------------------------------------------------------------------
WORKDIR="$(mktemp -d)"
repo_dir="$WORKDIR/webapp-download"

clone_url="https://github.com/${WEBAPP_REPO}.git"

# Authenticate with a per-invocation header instead of a credentialed remote
# URL: `git -c` before the subcommand is not persisted, so the token never
# lands in .git/config, which is the cwd the target repo's own code runs in.
git_auth=()
if [[ -n "${GH_TOKEN:-}" ]]; then
  auth_basic="$(printf 'x-access-token:%s' "$GH_TOKEN" | base64 -w0)"
  # Actions masks the literal secret, not anything derived from it, so register
  # the encoded form too: it is reversible back to a working token.
  if [[ -n "${GITHUB_ACTIONS:-}" ]]; then
    echo "::add-mask::$auth_basic"
  fi
  auth_header="Authorization: Basic $auth_basic"
  git_auth=(-c "http.extraHeader=$auth_header")
fi
authed_git() { git "${git_auth[@]}" "$@"; }

# A missing repo grant on a private repo answers 404, not 403 - say so plainly.
if ! authed_git clone --quiet --depth 1 --branch "$WEBAPP_BASE" "$clone_url" "$repo_dir" 2>"$WORKDIR/clone.err"; then
  if grep -qiE 'not found|could not read' "$WORKDIR/clone.err"; then
    die "cannot clone ${WEBAPP_REPO}@${WEBAPP_BASE}. On a private repo a missing write grant reports 'not found', not a permission error - check the token's access to ${WEBAPP_REPO}."
  fi
  cat "$WORKDIR/clone.err" >&2
  die "cannot clone ${WEBAPP_REPO}@${WEBAPP_BASE}"
fi
log_ok "cloned ${WEBAPP_REPO}@${WEBAPP_BASE}"

# Reuse an open release branch so each per-OS run of one build accumulates into
# the same file and PR; the site only shows the newest build per product, so a
# build has to land complete or its other OS rows disappear.
branch_exists="false"
if authed_git ls-remote --exit-code --heads "$clone_url" "$BRANCH" >/dev/null 2>&1; then
  branch_exists="true"
  authed_git -C "$repo_dir" fetch --quiet --depth 1 origin "$BRANCH" \
    || die "branch $BRANCH exists on ${WEBAPP_REPO} but could not be fetched"
  authed_git -C "$repo_dir" checkout --quiet -B "$BRANCH" FETCH_HEAD
  log_ok "reusing existing branch $BRANCH"
fi

catalog="$repo_dir/src/data/catalog.yaml"
[[ -f "$catalog" ]] || die "$catalog missing - the target repo layout changed"

# Reads catalog.yaml with sed rather than a YAML parser: only flat product keys
# and version ids are needed, and this keeps the script dependency-free.
catalog_products() {
  sed -n '/^products:/,/^[a-z_]*:/p' "$catalog" | sed -n 's/^  \([a-z0-9][a-z0-9._-]*\):[[:space:]]*$/\1/p'
}
catalog_trains() {
  sed -n '/^versions:/,/^[a-z_]*:/p' "$catalog" | sed -n 's/.*id:[[:space:]]*"\([^"]*\)".*/\1/p'
}
product_group() {
  sed -n "/^  $1:\$/,/^  [a-z0-9][a-z0-9._-]*:\$/p" "$catalog" \
    | sed -n 's/^    group:[[:space:]]*"\?\([a-z]*\)"\?[[:space:]]*$/\1/p' | head -1
}

known_trains="$(catalog_trains)"
grep -qxF "$train" <<<"$known_trains" || die \
  "train '$train' is not declared in ${WEBAPP_REPO} src/data/catalog.yaml (versions:). Adding a train is a human decision - open a catalog PR first. Known trains: $(tr '\n' ' ' <<<"$known_trains")"

known_products="$(catalog_products)"
group=""
for i in "${!E_PRODUCT[@]}"; do
  p="${E_PRODUCT[$i]}"
  grep -qxF "$p" <<<"$known_products" || die \
    "product '$p' is not declared in ${WEBAPP_REPO} src/data/catalog.yaml (products:). Adding a product is a human decision - open a catalog PR first."
  g="$(product_group "$p")"
  [[ -n "$g" ]] || die "product '$p' has no group in catalog.yaml"
  if [[ -z "$group" ]]; then
    group="$g"
  elif [[ "$g" != "$group" ]]; then
    die "all entries must share one product group: '$p' is '$g', expected '$group'"
  fi
done
log_ok "catalog pre-flight passed (train $train, group $group)"

# The site tab enum and the on-disk folder differ for widgets only.
case "$group" in
  appliances|packages|agent) group_dir="$group" ;;
  custom)                    group_dir="widgets" ;;
  *) die "unhandled product group '$group' - teach this script its folder" ;;
esac

out_rel="src/data/releases/${group_dir}/${train}/${OUT_NAME}"
out_path="$repo_dir/$out_rel"

# ---------------------------------------------------------------------------
# Emit the release YAML
# ---------------------------------------------------------------------------
# Key order and quoting match scripts/rm-add-vm.mjs so generated files are
# indistinguishable from release-manager output.
render_entry() {
  local i="$1"
  printf -- '- product: "%s"\n' "${E_PRODUCT[$i]}"
  printf -- '  train: "%s"\n'   "${E_TRAIN[$i]}"
  printf -- '  state: "%s"\n'   "${E_STATE[$i]}"
  printf -- '  os: "%s"\n'      "${E_OS[$i]}"
  printf -- '  version: "%s"\n' "${E_VERSION[$i]}"
  printf -- '  file: "%s"\n'    "${E_FILE[$i]}"
  printf -- '  date: "%s"\n'    "${E_DATE[$i]}"
  printf -- '  md5: "%s"\n'     "${E_MD5[$i]}"
  printf -- '  size: %s\n'      "${E_SIZE[$i]}"
  printf -- '  enabled: true\n'
}

# Merge rather than truncate, the way rm-add-vm.mjs does: one file per build can
# be written by several per-OS runs, and a re-run must replace its own rows
# instead of duplicating them. Keyed on product+os, which is what the site
# de-duplicates on.
chunk_dir="$WORKDIR/chunks"
mkdir -p "$chunk_dir"
if [[ -f "$out_path" ]]; then
  awk -v dir="$chunk_dir" '
    /^- / { n++; f = sprintf("%s/%04d.existing", dir, n) }
    n     { print > f }
  ' "$out_path"
  kept=0
  for chunk in "$chunk_dir"/*.existing; do
    [[ -e "$chunk" ]] || continue
    c_product="$(sed -n 's/^- product: "\(.*\)"$/\1/p' "$chunk" | head -1)"
    c_os="$(sed -n 's/^  os: "\(.*\)"$/\1/p' "$chunk" | head -1)"
    superseded="false"
    for i in "${!E_PRODUCT[@]}"; do
      if [[ "$c_product" == "${E_PRODUCT[$i]}" && "$c_os" == "${E_OS[$i]}" ]]; then
        superseded="true"
        break
      fi
    done
    if [[ "$superseded" == "true" ]]; then
      rm -f "$chunk"
    else
      mv "$chunk" "${chunk%.existing}.keep"
      kept=$((kept + 1))
    fi
  done
  log "→ merging into an existing $OUT_NAME ($kept entry(ies) kept)"
fi

for i in "${!E_PRODUCT[@]}"; do
  render_entry "$i" >"$chunk_dir/$(printf '%s|%s' "${E_PRODUCT[$i]}" "${E_OS[$i]}").new"
done

mkdir -p "$(dirname "$out_path")"
: >"$out_path"
# Sort by the rendered product then os, matching rm-add-vm.mjs's chunk sort.
while IFS= read -r chunk; do
  cat "$chunk" >>"$out_path"
done < <(
  for chunk in "$chunk_dir"/*.keep "$chunk_dir"/*.new; do
    [[ -e "$chunk" ]] || continue
    printf '%s\t%s\t%s\n' \
      "$(sed -n 's/^- product: "\(.*\)"$/\1/p' "$chunk" | head -1)" \
      "$(sed -n 's/^  os: "\(.*\)"$/\1/p' "$chunk" | head -1)" \
      "$chunk"
  done | LC_ALL=C sort -t$'\t' -k1,1 -k2,2 | cut -f3
)

log_ok "wrote $out_rel"

# ---------------------------------------------------------------------------
# The Agent tab: catalog.yaml is the only rendered surface for the monitoring agent
# ---------------------------------------------------------------------------
catalog_rel="src/data/catalog.yaml"
catalog_line="not edited"
if [[ -n "$AGENT_VERSION" ]]; then
  [[ -n "$AGENT_FILE_URL" && -n "$AGENT_MD5" && -n "$AGENT_SIZE" ]] \
    || die "--agent-version needs --agent-file-url, --agent-md5 and --agent-size"
  agent_out="$WORKDIR/catalog-agent.log"
  if ! python3 "$SCRIPT_DIR/catalog-agent-entry.py" \
    --catalog "$catalog" --version "$AGENT_VERSION" --file-url "$AGENT_FILE_URL" \
    --md5 "$AGENT_MD5" --size "$AGENT_SIZE" >"$agent_out" 2>&1; then
    cat "$agent_out" >&2
    die "could not update the agent block of $catalog_rel"
  fi
  cat "$agent_out"
  catalog_line="$(grep -v '^featured_count=' "$agent_out" | head -1)"
  featured_count="$(sed -n 's/^featured_count=//p' "$agent_out" | head -1)"
  # the tab features one build per supported train; a third entry is a site change, not a release
  if [[ -n "$featured_count" && "$featured_count" != "2" ]]; then
    catalog_line="$catalog_line — **$featured_count featured entries, review before merging**"
  fi
  log_ok "updated $catalog_rel"
fi
log "--- $out_rel ---"
cat "$out_path"
log "--- end ---"

# ---------------------------------------------------------------------------
# Gate on pnpm validate
# ---------------------------------------------------------------------------
# The target repo runs validate only in a push-triggered workflow that is not a
# required check, so this is the real gate. Prefer local node, else a container.

# pnpm 11's binary links against libatomic.so.1, which is absent from slim
# images and some runners; the target repo's own workflows install it too.
ensure_libatomic() {
  ldconfig -p 2>/dev/null | grep -q 'libatomic\.so\.1' && return 0
  command -v apt-get >/dev/null 2>&1 || return 1
  sudo -n apt-get update -qq >/dev/null 2>&1 || return 1
  sudo -n apt-get install -y -qq libatomic1 >/dev/null 2>&1
}

validate_catalog() {
  command -v pnpm >/dev/null 2>&1 || { echo "pnpm is not on PATH"; return 1; }
  local pnpm_major
  pnpm_major="$(pnpm --version 2>/dev/null | cut -d. -f1)"
  [[ "$pnpm_major" =~ ^[0-9]+$ ]] || { echo "cannot read the pnpm version"; return 1; }
  [[ "$pnpm_major" -ge 11 ]] || { echo "pnpm $pnpm_major is older than the required 11"; return 1; }
  ensure_libatomic || echo "warning: libatomic1 is missing and could not be installed; pnpm may fail to start"
  # This runs the target repo's code, so hand it neither the token nor its
  # lifecycle scripts: --ignore-scripts skips the root pre/post/prepare hooks,
  # and the validator is invoked directly rather than through `pnpm validate`.
  (
    cd "$repo_dir"
    env -u GH_TOKEN -u GITHUB_TOKEN pnpm install --frozen-lockfile --ignore-scripts \
      --config.trustPolicy=no-downgrade \
      --config.minimumReleaseAge=2880 \
      --config.blockExoticSubdeps=true || exit 1
    env -u GH_TOKEN -u GITHUB_TOKEN node scripts/validate.mjs
  )
}

log "→ running pnpm validate"
validate_out="$WORKDIR/validate.log"
if ! validate_catalog >"$validate_out" 2>&1; then
  cat "$validate_out" >&2
  die "pnpm validate failed - nothing was pushed. This needs pnpm >=11 on PATH (the calling action provisions it via pnpm/action-setup) and the libatomic1 package."
fi

grep -q 'Catalogue valide' "$validate_out" \
  || { cat "$validate_out" >&2; die "pnpm validate did not report a valid catalog"; }
validate_line="$(grep 'Catalogue valide' "$validate_out" | head -1 | sed 's/^[^A-Za-z]*//')"
log_ok "$validate_line"


# ---------------------------------------------------------------------------
# Commit, push, open the PR
# ---------------------------------------------------------------------------
if [[ "$DRY_RUN" == "true" ]]; then
  log_skip "[dry-run] would create branch : $BRANCH"
  log_skip "[dry-run] would commit        : $COMMIT_MESSAGE"
  log_skip "[dry-run] would open PR       : $PR_TITLE"
  log_skip "[dry-run] would target        : ${WEBAPP_REPO} ${WEBAPP_BASE} <- $out_rel"
  [[ -n "$AGENT_VERSION" ]] && log_skip "[dry-run] would also commit   : $catalog_rel"
  write_summary "not opened (dry run)"
  log_ok "dry run complete, nothing pushed"
  exit 0
fi

[[ -n "${GH_TOKEN:-}" ]] || die "GH_TOKEN is required to push and open a PR"
command -v gh >/dev/null 2>&1 || die "gh cli is required to open the pull request"

cd "$repo_dir"

# The target ruleset sets require_extra_approval_for_unattributed_changes, so
# the committer has to resolve to the token's own account. Ask the API rather
# than hardcode it, since a GitHub App token commits under a different identity.
commit_name="${GIT_AUTHOR_NAME:-}"
commit_email="${GIT_AUTHOR_EMAIL:-}"
if [[ -z "$commit_name" ]]; then
  if identity="$(gh api user --jq '"\(.login) \(.id)"' 2>/dev/null)" && [[ -n "$identity" ]]; then
    commit_name="${identity%% *}"
    commit_email="${identity##* }+${commit_name}@users.noreply.github.com"
  else
    # An App installation token cannot read /user; fall back to its bot identity.
    commit_name="github-actions[bot]"
    commit_email="41898282+github-actions[bot]@users.noreply.github.com"
  fi
fi
git config user.name  "$commit_name"
git config user.email "$commit_email"
log "→ committing as $commit_name <$commit_email>"

if [[ "$branch_exists" != "true" ]]; then
  git checkout --quiet -b "$BRANCH"
fi
git add "$out_rel"
[[ -n "$AGENT_VERSION" ]] && git add "$catalog_rel"

if git diff --cached --quiet; then
  write_summary "not opened (no change)"
  log_skip "no change to commit - $out_rel already matches ${WEBAPP_BASE}"
  exit 0
fi

git commit --quiet -m "$COMMIT_MESSAGE"
authed_git push --quiet origin "HEAD:refs/heads/$BRANCH"
log_ok "pushed $BRANCH"

if [[ -z "$PR_BODY_FILE" ]]; then
  PR_BODY_FILE="$WORKDIR/pr-body.md"
  {
    printf '## Summary\n'
    printf -- '- Adds %d release entry(ies) for train `%s` to `%s`\n' "$entry_count" "$train" "$out_rel"
    if [[ -n "$AGENT_VERSION" ]]; then
      printf -- '- Features windows installer `%s` on the Agent tab (`%s`)\n' "$AGENT_VERSION" "$catalog_rel"
    fi
    printf -- '- md5 and size are computed from the published GitHub release assets\n'
    if [[ -n "${GITHUB_SERVER_URL:-}" && -n "${GITHUB_REPOSITORY:-}" && -n "${GITHUB_RUN_ID:-}" ]]; then
      printf -- '- Produced by [run %s](%s/%s/actions/runs/%s) - see its summary for the files, size and md5\n' \
        "$GITHUB_RUN_ID" "$GITHUB_SERVER_URL" "$GITHUB_REPOSITORY" "$GITHUB_RUN_ID"
    fi
    printf '\n## Test plan\n'
    printf -- '- [x] `pnpm validate` passed in the publishing pipeline before this PR was opened\n'
    printf -- '- [ ] Preview build shows the new rows for train `%s`\n' "$train"
  } >"$PR_BODY_FILE"
fi

export GH_TOKEN
# A second per-OS run of the same build pushes to a branch that already has a
# PR; update it instead of failing on create.
pr_url="$(gh pr list --repo "$WEBAPP_REPO" --head "$BRANCH" --state open \
  --json url --jq '.[0].url // empty' 2>/dev/null || true)"
if [[ -n "$pr_url" ]]; then
  log_ok "updated existing $pr_url"
else
  pr_url="$(gh pr create \
    --repo "$WEBAPP_REPO" \
    --base "$WEBAPP_BASE" \
    --head "$BRANCH" \
    --title "$PR_TITLE" \
    --body-file "$PR_BODY_FILE")"
  log_ok "opened $pr_url"
fi

if [[ -n "$PR_LABEL" ]]; then
  gh pr edit "$pr_url" --add-label "$PR_LABEL" >/dev/null 2>&1 \
    || log_skip "could not apply label '$PR_LABEL' (it may not exist in ${WEBAPP_REPO})"
fi

write_summary "$pr_url"
log_ok "release metadata published - a human review and squash merge are required"
