#!/bin/sh

set -e
set -x

# Run SSH service.
ssh -D &

# Otherwise it is a classical Centreon Web container.
/usr/share/centreon/container.sh
