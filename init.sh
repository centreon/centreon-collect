#!/bin/bash

(return 0 2>/dev/null) && sourced=1 || sourced=0

if (( sourced == 0 )) ; then
  echo "Please execute this script with:"
  echo ". init.sh"
  exit 1
fi

if [[ "$PATH" =~ "vcpkg" ]] ; then
  echo "Already initialized"
fi

export VCPKG_ROOT=$PWD/vcpkg
export PATH=$VCPKG_ROOT:$PATH
