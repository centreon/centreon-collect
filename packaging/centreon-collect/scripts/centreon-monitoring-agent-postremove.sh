#!/bin/sh

case "$1" in
  purge)
    deluser centreon-monitoring-agent || :
    delgroup centreon-monitoring-agent || :
  ;;
esac
