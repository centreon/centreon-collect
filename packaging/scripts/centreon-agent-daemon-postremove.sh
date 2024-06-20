#!/bin/sh

case "$1" in
  purge)
    deluser centreon-agent || :
    delgroup centreon-agent || :
  ;;
esac
