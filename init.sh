#!/bin/bash
# A little script to source before compiling.
# It just sets VCPKG_ROOT and PATH so that make/ninja won't fail.

# To be sure the script is sourced.
(return 0 2>/dev/null) && sourced=1 || sourced=0

if (( sourced == 0 )) ; then
  echo "Please execute this script with:"
  echo ". init.sh"
  exit 1
fi

# To be sure the script has not already been launched.
if [[ "$PATH" =~ "vcpkg" ]] ; then
  echo "Already initialized"
fi

# The main purpose.
export VCPKG_ROOT=$PWD/vcpkg
export PATH=$VCPKG_ROOT:$PATH
