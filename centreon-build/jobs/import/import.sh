#!/bin/sh

set -e
set -x

# this script
# - gets a file from centreon-repo-pobox
# - optionnaly signs it
# - sends it to testing repository
# - and optionnaly promotes it to stable

. `dirname $0`/../common.sh

REPO_CREDS="ubuntu@srvi-repo.int.centreon.com"
S3_BUCKET="s3://centreon-repo-pobox"
REMOTE_FILE="/tmp/rpm_to_import_$(date +%s).rpm"

PKGNAME=$PKGNAME
BASEREPO=$BASEREPO
SERIE=$SERIE
OS=$OS
ARCH=$ARCH
PRODUCT=$PRODUCT
GROUP=$GROUP
S3_BUCKET_SUBDIR=$S3_BUCKET_SUBDIR

# Check variables.
if [ -z "$PKGNAME" -o -z "$PRODUCT" -o -z "$GROUP" ] ; then
  echo "You need to specify PKGNAME, PRODUCT and GROUP environment variables."
  # Display Help
  echo "This script aims to ease file transfer between repositories"
  echo "  - gets a file from centreon-repo-pobox"
  echo "  - optionnaly signs it"
  echo "  - sends it to testing repository"
  echo "  - and optionnaly promotes it to stable"
  echo
  echo "Syntax: [env_variables] ${0##/*}"
  echo "env:"
  echo "  PKGNAME               Name of the RPM stored in the S3 bucket."
  echo "  BASEREPO              defines the repository, possible : standard | bam | map | mbi | plugin-packs"
  echo "  SERIE                 defines the major version, possible :  19.10 | 20.04 | 20.10 | 21.04"
  echo "  OS                    possible : el7 | el8"
  echo "  ARCH                  possible : noarch | x86_64"
  echo "  PRODUCT               any"
  echo "  GROUP                 any"
  echo "  SIGN_PKG              true | false"
  echo "  PUBLISH_PKG_TO_STABLE true | false"
  exit 1
fi

PKGS=${PKGNAME/,/ }

for pkg in $PKGNAME; do
  LOCAL_FILE="/tmp/$pkg"

  ssh "$REPO_CREDS" aws s3 cp "$S3_BUCKET/$S3_BUCKED_SUBDIR$pkg" "$REMOTE_FILE"

  # sign if needed
  if [ "$PKG_NEEDS_SIGN" = true ]; then
    ssh "$REPO_CREDS" rpm --resign "$REMOTE_FILE"
  fi

  scp "$REPO_CREDS:$REMOTE_FILE" "$LOCAL_FILE"
  ssh "$REPO_CREDS" rm -f "$REMOTE_FILE"

  put_testing_rpms "$BASEREPO" "$SERIE" "$OS" "$ARCH" "$PRODUCT" "$GROUP" "$LOCAL_FILE"

  # promote to stable if needed
  if [ "$PKG_PUBLISH_STABLE" = true ]; then
    promote_testing_rpms_to_stable "$BASEREPO" "$SERIE" "$OS" "$ARCH" "$PRODUCT" "$GROUP"
  fi
done