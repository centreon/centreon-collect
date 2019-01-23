#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Check arguments.
if [ -z "$VERSION" ] ; then
  echo "You need to specify VERSION environment variable."
  exit 1
fi

# Download composer.
rm -f composer
curl -o composer "https://getcomposer.org/download/$VERSION/composer.phar"

# Send composer to the cache.
scp composer "$REPO_CREDS":/srv/cache/tools/composer
