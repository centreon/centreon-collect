#!/bin/sh

set -x

# Get project.
PROJECT=centreon-agent

# Remove old reports.
rm -f /tmp/ut.xml

# Install dependencies.
cd /root/go/src/github.com/centreon/$PROJECT/
go get github.com/shirou/w32

# Run unit tests.
go test ./... -v 2>&1 | go-junit-report > /tmp/ut.xml
