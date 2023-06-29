#!/bin/sh

getent passwd centreon-engine > /dev/null 2>&1 && usermod -a -G centreon-broker centreon-engine
getent group centreon-engine > /dev/null 2>&1 && usermod -a -G centreon-engine centreon-broker
