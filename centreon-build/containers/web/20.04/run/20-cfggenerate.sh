#!/bin/sh

# Generate configuration
centreon -d -u admin -p centreon -a POLLERGENERATE -v 1
