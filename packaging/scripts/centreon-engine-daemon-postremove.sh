#!/bin/sh

case "$1" in
  purge)
    deluser centreon-engine || :
    delgroup centreon-engine || :
  ;;
esac
