#!/bin/sh

case "$1" in
  purge)
    deluser centreon-broker || :
    delgroup centreon-broker || :
  ;;
esac
