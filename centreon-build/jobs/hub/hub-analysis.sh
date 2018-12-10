#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-hub-ui

# Run analysis.
cd "$PROJECT"
sonar-scanner
