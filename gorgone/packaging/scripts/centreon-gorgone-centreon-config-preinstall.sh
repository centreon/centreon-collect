#!/bin/bash

if ! getent group centreon > /dev/null 2>&1; then
  groupadd -r centreon
fi
