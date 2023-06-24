#!/bin/sh

case "$1" in
    remove)
        deluser centreon centreon-broker || true
        deluser centreon-broker || true
        delgroup centreon-broker || true
    ;;
esac

#DEBHELPER#