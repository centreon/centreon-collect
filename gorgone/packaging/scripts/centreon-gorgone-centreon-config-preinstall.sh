#!/bin/bash

if ! getent passwd centreon > /dev/null 2>&1; then
  useradd -g centreon -m -d /var/spool/centreon -r centreon 2> /dev/null
fi

if ! getent group centreon > /dev/null 2>&1; then
  groupadd -r centreon
fi
