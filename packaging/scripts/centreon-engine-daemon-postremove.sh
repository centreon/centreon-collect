#!/bin/sh

case "$1" in
  purge)
    deluser centreon centreon-engine || :
    deluser centreon-engine || :
    delgroup centreon-engine || :
  ;;
esac
