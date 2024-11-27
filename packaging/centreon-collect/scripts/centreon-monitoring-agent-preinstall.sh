#!/bin/sh

if ! id centreon-monitoring-agent > /dev/null 2>&1; then
  useradd -r centreon-monitoring-agent > /dev/null 2>&1
fi

if id -g nagios > /dev/null 2>&1; then
  usermod -a -G centreon-monitoring-agent nagios
fi

