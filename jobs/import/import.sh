#!/bin/sh

set -e
set -x

# this script
# - gets a file from centreon-repo-pobox 
# - optionnaly signs it
# - sends it to testing repository
# - and optionnaly promotes it to stable

. `dirname $0`/../common.sh

if [ $# -ne 9 ]; then
  echo "illegal number of parameters, usage: $0 [filename] [base repo] [release] [os] [arch] [product code] [name] [to be signed] [to be stable]"
  echo "  filename      any, the s3 object name of the RPM"
  echo "  base repo     standard | bam | map | mbi | plugin-packs"
  echo "  release       19.10 | 20.04 | 20.10"
  echo "  os            el7 | el8"
  echo "  arch          noarch | x86_64"
  echo "  product       any"
  echo "  group         any, might be $PROJECT-$VERSION-$RELEASE"
  echo "  to be signed  1 if the RPM must be signed before being added to the repo"
  echo "  to be stable  1 if the RPM must be pushed to stable"
fi

S3_BUCKET="centreon-repo-pobox"
S3_OBJECT_URL="s3://$S3_BUCKET/$1"
LOCAL_FILE=$(mktemp)

aws s3 cp "$S3_OBJECT_URL" "$LOCAL_FILE"

# sign if needed
if [ $8 -eq "1" ]; then
  rpmsign --resign "$LOCAL_FILE"
fi

put_testing_rpms "$2" "$3" "$4" "$5" "$6" "$7" "$LOCAL_FILE"

# promote to stable if needed
if [ $9 -eq "1" ]; then
  promote_testing_rpms_to_stable "$2" "$3" "$4" "$5" "$6" "$7"
fi
