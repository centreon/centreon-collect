#!/bin/sh

rm -f /tmp/docker.ready

# centengine logs to this file (log_v2_logger=file); 10-log-tail_background.sh
# streams it to stdout. Clear it so tail -F starts clean on restart.
> /var/log/centreon-engine/centengine.log 2>/dev/null || true
rm -rf /var/log/centreon-engine/archives/* 2>/dev/null || true

# NOTE: the trixie image refreshes apt package lists here so 40-engine.sh/
# 41-custom-deps.sh can install plugins at runtime. This image has no apk-repo
# equivalent for that dynamic install path yet (see the Dockerfile's "KNOWN
# GAP" comment) - those scripts are intentionally not shipped, so there is
# nothing to refresh.
