#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-documentation

# Get release.
cd "$PROJECT"
COMMIT=`git log -1 HEAD --pretty=format:%h`
now=`date +%s`
export RELEASE="$now.$COMMIT"

# Get committer.
COMMITTER=`git show --format='%cN <%cE>' HEAD | head -n 1`

# Analyze assets.
for lang in en fr ; do
  cd "$lang"
  find assets -type f | sort -u > "../../existing_assets_$lang.txt"
  find . -name '*.md' | xargs -d '\n' cat | grep -o -e 'assets/[0-9a-zA-Z./_\\-]*' | sort -u > "../../used_assets_$lang.txt"
  diff "../../existing_assets_$lang.txt" "../../used_assets_$lang.txt" > "../../assets_diff_$lang.txt"
  cd ..
done
cd ..

# Generate properties files for downstream jobs.
cat > source.properties << EOF
PROJECT=$PROJECT
RELEASE=$RELEASE
COMMIT=$COMMIT
COMMITTER=$COMMITTER
EOF
