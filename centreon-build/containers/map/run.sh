#!/bin/sh

set -e
set -x

service tomcat6 start
tailf /var/log/centreon-studio/centreon-studio.log
