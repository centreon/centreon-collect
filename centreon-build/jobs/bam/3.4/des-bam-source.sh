#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

#
# This script will generate sources from the local clone of the project
# repository. These sources will then be pushed to the internal
# repository (srvi-repo) and used in downstream jobs, thanks to the
# property file generated at the end of the script.
#

# Project.
PROJECT=centreon-bam-server
tar czf "$PROJECT-git.tar.gz" "$PROJECT"

# Get version.
cd "$PROJECT"
VERSION=`grep mod_release www/modules/$PROJECT/conf.php | cut -d '"' -f 4`
export VERSION="$VERSION"

# Get release.
COMMIT=`git log -1 HEAD --pretty=format:%h`
now=`date +%s`
export RELEASE="$now.$COMMIT"

# Get committer.
COMMITTER=`git show --format='%cN <%cE>' HEAD | head -n 1`

# Create source tarballs (f*cking .gitattributes).
git archive --prefix="$PROJECT-$VERSION/" HEAD | gzip > "../$PROJECT-$VERSION.tar.gz"
echo '/* -export-ignore' > .git/info/attributes
git archive --prefix="$PROJECT-$VERSION-full/" HEAD | gzip > "../$PROJECT-$VERSION-full.tar.gz"
rm -f .git/info/attributes
cd ..

# Encrypt source archive.
for phpversion in 53 54 ; do
  curl -F "file=@$PROJECT-$VERSION.tar.gz" -F "version=$phpversion" -F "modulename=$PROJECT" 'http://encode.int.centreon.com/api/' -o "$PROJECT-$VERSION-php$phpversion.tar.gz"
done

# Send it to srvi-repo.
put_internal_source "bam" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION.tar.gz"
put_internal_source "bam" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION-full.tar.gz"
put_internal_source "bam" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION-php53.tar.gz"
put_internal_source "bam" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION-php54.tar.gz"
put_internal_source "bam" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-git.tar.gz"

# Generate properties files for downstream jobs.
cat > source.properties << EOF
PROJECT=$PROJECT
VERSION=$VERSION
RELEASE=$RELEASE
COMMIT=$COMMIT
COMMITTER=$COMMITTER
EOF
