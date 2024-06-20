#!/bin/sh

if [ "$1" -lt "1" ]; then # Final removal
  semodule -r centreon-agent > /dev/null 2>&1 || :
fi
