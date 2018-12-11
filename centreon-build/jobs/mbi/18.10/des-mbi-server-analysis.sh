#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-bi-server

# Copy reports and run analysis.
cd "$PROJECT"
sonar-scanner
