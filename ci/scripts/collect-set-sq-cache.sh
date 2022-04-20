#!/bin/bash

set -e

echo ""
cp /root/.sonar/cache /src/build/cache
cd /src/build/cache
tar czf "$PROJECT-$VERSION-SQ-source.tar.gz" *

echo "Updating SQ cache"
put_internal_source "centreon-collect" "$PROJECT-SQ-cache-$VERSION" "$PROJECT-$VERSION-SQ-source.tar.gz"
