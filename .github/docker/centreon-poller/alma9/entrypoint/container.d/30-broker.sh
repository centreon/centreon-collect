#!/bin/sh

# Start Centreon Broker daemon (cbd). Never wired up before: the RPM
# providing it (centreon-broker-cbd) was missing from the packages
# copied into the docker build context, so cbd was never installed and
# never started, even though centengine's embedded broker output
# (central-module.json) expects it listening on localhost:5669.
systemctl start cbd
