#!/bin/sh

case "$1" in
  purge)
    deluser centreon centreon-broker || :
    deluser centreon-broker || :
    delgroup centreon-broker || :
  ;;
esac
