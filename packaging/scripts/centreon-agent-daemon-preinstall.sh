#!/bin/sh

if ! id centreon-agent > /dev/null 2>&1; then
  useradd -r centreon-agent > /dev/null 2>&1
fi

if id -g nagios > /dev/null 2>&1; then
  usermod -a -G centreon-agent nagios
fi

