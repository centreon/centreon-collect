#!/usr/bin/env bash
# Install the CMA ADR kit into a local centreon-collect clone without committing it.
# Files are copied, then listed in the clone's .git/info/exclude (local, never pushed).
#
# Usage:
#   ./install-adr-local.sh <path-to-centreon-collect> [--force]   install / update
#   ./install-adr-local.sh <path-to-centreon-collect> --uninstall remove the kit
#   ./install-adr-local.sh <path-to-centreon-collect> --status    show what is installed
set -euo pipefail

KIT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="${1:?usage: $0 <path-to-centreon-collect> [--force|--uninstall|--status]}"
MODE="${2:-install}"

FILES=(
  "CLAUDE.md"
  ".claude/settings.json"
  ".claude/hooks/cma-adr-context.sh"
  ".claude/hooks/cma-adr-stop.sh"
  ".claude/agents/cma-adr-reviewer.md"
  "agent/CLAUDE.md"
  ".claude/rules/cma-adr.md"
  "docs/adr/README.md"
  "docs/adr/template.md"
  "docs/adr/cma/README.md"
  "docs/adr/cma/ARCHITECTURE.md"
)
while IFS= read -r f; do FILES+=("${f#"$KIT_DIR"/}"); done \
  < <(find "$KIT_DIR/docs/adr/cma" -name '[0-9][0-9][0-9][0-9]-*.md' | sort)

MARK_BEGIN="# >>> cma-adr local kit >>>"
MARK_END="# <<< cma-adr local kit <<<"

cd "$REPO"
git rev-parse --is-inside-work-tree >/dev/null 2>&1 || { echo "not a git clone: $REPO" >&2; exit 1; }
[ -d agent ] && [ -f agent/CMakeLists.txt ] || { echo "not centreon-collect (agent/ missing): $REPO" >&2; exit 1; }
EXCLUDE="$(git rev-parse --git-path info/exclude)"
mkdir -p "$(dirname "$EXCLUDE")"; touch "$EXCLUDE"

strip_block() { sed -i "/^${MARK_BEGIN}\$/,/^${MARK_END}\$/d" "$EXCLUDE"; }

case "$MODE" in
  --status)
    for f in "${FILES[@]}"; do
      if [ -f "$f" ]; then
        if git ls-files --error-unmatch "$f" >/dev/null 2>&1; then s="TRACKED (in repo)";
        elif git check-ignore -q "$f"; then s="installed, ignored";
        else s="installed, NOT ignored"; fi
      else s="absent"; fi
      printf '%-55s %s\n' "$f" "$s"
    done
    exit 0 ;;
  --uninstall)
    for f in "${FILES[@]}"; do
      if [ -f "$f" ] && ! git ls-files --error-unmatch "$f" >/dev/null 2>&1; then rm -v "$f"; fi
    done
    rmdir -p docs/adr/cma .claude/rules .claude/hooks .claude/agents 2>/dev/null || true
    strip_block
    echo "uninstalled"; exit 0 ;;
  install|--force) ;;
  *) echo "unknown option: $MODE" >&2; exit 1 ;;
esac

# Never overwrite a file that is tracked by git, nor a local file unless --force.
for f in "${FILES[@]}"; do
  if git ls-files --error-unmatch "$f" >/dev/null 2>&1; then
    echo "refused: $f is tracked by git in this clone (the repo now has its own version)" >&2; exit 1
  fi
  if [ -f "$f" ] && [ "$MODE" != "--force" ] && ! cmp -s "$KIT_DIR/$f" "$f"; then
    echo "refused: $f exists and differs; rerun with --force to overwrite" >&2; exit 1
  fi
done

for f in "${FILES[@]}"; do
  mkdir -p "$(dirname "$f")"
  cp "$KIT_DIR/$f" "$f"
done

strip_block
{ echo "$MARK_BEGIN"; printf '/%s\n' "${FILES[@]}"; echo "$MARK_END"; } >> "$EXCLUDE"

echo "installed ${#FILES[@]} files into $(pwd)"
if [ -n "$(git status --porcelain -- "${FILES[@]}")" ]; then
  echo "WARNING: some kit files still appear in git status" >&2; exit 1
fi
echo "git status: clean for kit files"
echo "note: 'git clean -fdX' or '-fdx' deletes ignored files, including this kit. Rerun this script after it."
