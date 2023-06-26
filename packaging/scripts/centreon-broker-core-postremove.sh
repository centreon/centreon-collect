#!/bin/sh

case "$1" in
  remove)
    deluser centreon centreon-broker || :
    deluser centreon-broker || :
    delgroup centreon-broker || :
  ;;
esac
