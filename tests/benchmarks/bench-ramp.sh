#!/bin/bash
# Ramp the active check rate at a rigorously constant configuration, to find where the
# collect chain saturates.
#
# The configuration is generated once per run but never changes shape: 500 hosts and
# 10000 services throughout. The only thing that moves is check_interval, so a point of
# the ramp differs from the previous one by its rate and by nothing else -- neither the
# object count, nor the memory, nor the cost of loading the configuration.
#
# Sub-minute intervals need interval_length=1, since check_interval is a uint32 in
# state.proto and cannot hold a fraction. Every unit in centengine.cfg then becomes a
# second, which the robot compensates for; see ${interval_length} there.
#
# Rate = services / (check_interval * interval_length) checks per second.
#
# Saturation shows up on three signals at once, and one alone proves nothing:
#   - active_checks.service_per_min falls below the requested rate: Engine no longer
#     keeps up with its own schedule;
#   - cpu_ms_per_active_check climbs instead of keeping falling (it drops with the rate
#     as long as the fixed periodic work of the daemons is being amortised);
#   - machine.cpu_avg_pct approaches saturation of the cores actually used.
#
# Beware: bench.py probe deliberately excludes the CPU of the forked children, so the
# fork+exec of the plugin is missing from collect.cpu_total_s but present in
# machine.cpu_avg_pct. At 1000 checks/s that is one to two whole cores of difference.

set -u

LABEL=${LABEL:-ramp}
NB_HOSTS=${NB_HOSTS:-500}
SVC_BY_HOST=${SVC_BY_HOST:-20}
# Seconds, since interval_length is forced to 1 below.
INTERVALS=${INTERVALS:-"300 120 60 30 20 12 10"}
# The default 330s warm-up is calibrated on max_service_check_spread=5 read as five
# minutes. At interval_length=1 the spread is five seconds, so the steady state is
# reached almost at once and a shorter warm-up is legitimate here -- and only here.
WARMUP=${WARMUP:-180}
DURATION=${DURATION:-600}

SERVICES=$(( NB_HOSTS * SVC_BY_HOST ))

echo "ramp '${LABEL}': ${NB_HOSTS} hosts, ${SERVICES} services, warmup ${WARMUP}s, window ${DURATION}s"
echo "points: ${INTERVALS}"
echo

for ci in ${INTERVALS} ; do
    rate=$(( SERVICES / ci ))
    echo "== check_interval=${ci}s -> ${rate} checks/s =="
    ./bench.py run load --test BENCH_LOAD_ACTIVE \
        --var nb_hosts:${NB_HOSTS} \
        --var svc_by_host:${SVC_BY_HOST} \
        --var check_interval:${ci} \
        --var interval_length:1 \
        --var host_checks:False \
        --var warmup:${WARMUP} \
        --var duration:${DURATION} \
        --label "${LABEL}" --allow-dirty || {
            echo "point ${ci}s failed, moving to the next one" >&2
        }
done
