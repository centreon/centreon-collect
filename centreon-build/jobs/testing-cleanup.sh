#!/bin/sh

set -e
set -x

# SSH command.
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'

# Check arguments.
if [ -z "$VERSION" -o -z "$PROJECT" ] ; then
  echo "You need to specify VERSION and PROJECT environment variables."
  exit 1
fi

# Get repo and architecture based on project.
case "$PROJECT" in
  awie|dsm|lm|open-tickets|plugins|ppm|web|widgets)
    arch=noarch
    repo=standard
    ;;
  autodisco|broker|clib|connector|engine|nrpe)
    arch=x86_64
    repo=standard
    ;;
  bam)
    arch=noarch
    repo=bam
    ;;
  map-server|map-web)
    arch=noarch
    repo=map
    ;;
  bi-engine|bi-etl|bi-report|bi-reporting-server|bi-server)
    arch=noarch
    repo=mbi
    PROJECT=centreon-$PROJECT
    ;;
  packs)
    arch=noarch
    repo=plugin-packs
    ;;
  *)
    echo "Unsupported project $PROJECT."
    exit 1
    ;;
esac

# Clean REPO.
$SSH_REPO rm -rf "/srv/sources/$repo/testing/$PROJECT/*"
$SSH_REPO rm -rf "/srv/yum/$repo/$VERSION/el7/testing/$arch/$PROJECT/*"
$SSH_REPO createrepo "/srv/yum/$repo/$VERSION/el7/testing/$arch"
