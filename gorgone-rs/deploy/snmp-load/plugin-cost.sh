#!/bin/sh
# Cost of one plugin execution, in isolation: COUNT sequential runs of each
# command inside the engine image, CPU from the container's cgroup
# (cpu.stat usage_usec), wall time, and the maximum RSS of one run.
#
#   docker run --rm --network snmp-load_default --entrypoint sh \
#     -v "$PWD/plugin-cost.sh:/plugin-cost.sh:ro" snmp-load-engine /plugin-cost.sh AGENT_IP [COUNT]
set -eu
agent=$1
count=${2:-200}
rs=/usr/lib/centreon/plugins/centreon-plugin-rust-snmp
col=/usr/share/centreon/rs-collections/applications-protocol-snmp
pl=/usr/lib/centreon/plugins/centreon_linux_snmp.pl
perl_args="--plugin=os::linux::snmp::plugin --hostname=$agent --snmp-version=2c --snmp-community=public"

usage() { sed -n 's/^usage_usec //p' /sys/fs/cgroup/cpu.stat; }
now_ms() { date +%s%3N; }

measure() {
  name=$1
  shift
  echo "$name: $("$@" | head -c 150)"
  cpu0=$(usage)
  t0=$(now_ms)
  i=0
  while [ "$i" -lt "$count" ]; do
    "$@" >/dev/null || true
    i=$((i + 1))
  done
  t1=$(now_ms)
  cpu1=$(usage)
  rss=$(/usr/bin/time -f %M "$@" 2>&1 >/dev/null | tail -1)
  awk -v n="$count" -v c=$((cpu1 - cpu0)) -v w=$((t1 - t0)) -v r="$rss" -v name="$name" \
    'BEGIN { printf "%-14s cpu %6.1f ms/run  wall %6.1f ms/run  max rss %6.1f MiB\n", name, c / n / 1000, w / n, r / 1024 }'
}

measure rust-cpu $rs -H "$agent" -v 2c -c public -j $col/cpu.json --warning-avg 80 --critical-avg 90
measure rust-storage $rs -H "$agent" -v 2c -c public -j $col/storage.json --warning-prct 80 --critical-prct 90
# shellcheck disable=SC2086
measure perl-cpu $pl $perl_args --mode=cpu --warning-average=80 --critical-average=90
# shellcheck disable=SC2086
measure perl-storage $pl $perl_args --mode=storage --warning-usage=80 --critical-usage=90
measure true /bin/true
