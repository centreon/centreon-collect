#!/bin/sh

if [ "$1" -lt "1" ]; then # Final removal
  semodule -r centreon-monitoring-agent > /dev/null 2>&1 || :
fi
