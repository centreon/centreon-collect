#!/bin/sh

getent group centreon-monitoring-agent > /dev/null 2>&1 || groupadd -r centreon-monitoring-agent 2> /dev/null || :
if ! id centreon-monitoring-agent > /dev/null 2>&1; then
  useradd -g centreon-monitoring-agent -r centreon-monitoring-agent > /dev/null 2>&1
fi

if id -g nagios > /dev/null 2>&1; then
  usermod -a -G centreon-monitoring-agent nagios
fi

