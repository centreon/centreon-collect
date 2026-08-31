#!/bin/bash
# Ramp the passive result rate at a rigorously constant configuration, to find where the
# collect chain saturates when no plugin is ever forked.
#
# The companion of bench-ramp.sh
#
# The configuration never changes shape: 500 hosts and 10000 services throughout. Only
# passive_rate moves. check_interval is irrelevant here -- the services are passive, so
# Engine schedules nothing and the injector sets the pace on its own.
#
# What the chain is measured on, since no fork pollutes the figure: Engine reading the
# external command FIFO and processing a result, cbd carrying it, unified_sql writing
# it, rrd storing it. That is the number to compare between two versions.
#
# Saturation shows up on three signals, and one alone proves nothing:
#   - results_submitted falls below rate x (warmup + duration + 10): the FIFO filled up
#     and the injector blocked on write, which is Engine failing to drain it;
#   - cpu_ms_per_result climbs instead of staying flat;
#   - machine.cpu_avg_pct approaches saturation of the cores actually used.
#
# The injector itself is not a suspect: 0.28 us per result, 0.1% of a core at 4000/s.

set -u

LABEL=${LABEL:-ramp-passive}
NB_HOSTS=${NB_HOSTS:-500}
SVC_BY_HOST=${SVC_BY_HOST:-20}
RATES=${RATES:-"20 100 250 500 1000 2000 4000"}
# Left at the defaults of the suite on purpose: it is what makes the first point of the
# ramp readable against the runs already in bench.db.
WARMUP=${WARMUP:-330}
DURATION=${DURATION:-600}

echo "passive ramp '${LABEL}': ${NB_HOSTS} hosts, $(( NB_HOSTS * SVC_BY_HOST )) services"
echo "rates: ${RATES} results/s -- warmup ${WARMUP}s, window ${DURATION}s"
echo

for rate in ${RATES} ; do
    echo "== passive_rate=${rate} results/s =="
    ./bench.py run load --test BENCH_LOAD_PASSIVE \
        --var nb_hosts:${NB_HOSTS} \
        --var svc_by_host:${SVC_BY_HOST} \
        --var passive_rate:${rate} \
        --var warmup:${WARMUP} \
        --var duration:${DURATION} \
        --label "${LABEL}" --allow-dirty || {
            echo "point ${rate}/s failed, moving to the next one" >&2
        }
done
