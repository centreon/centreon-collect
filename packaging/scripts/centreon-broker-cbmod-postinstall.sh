#!/bin/sh

getent passwd centreon-engine &>/dev/null && usermod -a -G centreon-broker centreon-engine
getent group centreon-engine &>/dev/null && usermod -a -G centreon-engine centreon-broker
