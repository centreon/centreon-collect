#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-imp-portal-api

# Run analysis.
cd "$PROJECT"
# override missing AMI requirement
sudo apt-get install shellcheck
sonar-scanner
