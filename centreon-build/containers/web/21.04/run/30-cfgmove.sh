#!/bin/sh

# Move configuration to proper place.
centreon -d -u admin -p centreon -a CFGMOVE -v 1
