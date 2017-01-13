#!/bin/sh

set -e
set -x

# Install the reporting schema.
/usr/share/centreon-bi/bin/centreonBIETL -c

while true ; do
  sleep 10
done
