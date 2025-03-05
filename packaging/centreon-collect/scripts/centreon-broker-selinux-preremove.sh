#!/bin/sh

if [ "$1" -lt "1" ]; then # Final removal
  semodule -r centreon-broker > /dev/null 2>&1 || :
fi
