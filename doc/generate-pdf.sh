#!/bin/sh
# Builds nego-engine-broker-<lang>.pdf from the chapters of nego-engine-broker/.
# The chapters are concatenated in order; their tables of contents and
# navigation lines are dropped, and links between chapters become links inside
# the single document.
lang=${1:-fr}
cd "$(dirname "$0")/nego-engine-broker" || exit 1
for f in README-$lang.md [0-9][0-9]-*-$lang.md; do
  sed -e '/^<!-- TOC -->$/,/^<!-- TOC -->$/d' \
      -e '/^\[.*\](README-'"$lang"'\.md)/d' \
      -e 's/]([0-9][0-9]-[a-z-]*-'"$lang"'\.md#/](#/g' \
      -e 's/](README-'"$lang"'\.md)/](#introduction)/g' "$f"
  echo
done | pandoc -o ../nego-engine-broker-$lang.pdf \
  --pdf-engine=xelatex \
  --lua-filter=../mermaid-filter.lua \
  --toc --toc-depth=3 \
  -V lang=$lang -V geometry:margin=2cm -V papersize=a4
