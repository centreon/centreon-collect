#!/bin/sh

deluser centreon centreon-broker || true
deluser centreon-broker || true
delgroup centreon-broker || true
