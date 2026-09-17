#!/bin/bash
# Walk the two axes of the BAM startup benchmark, separately.
#
# The whole question this campaign answers is whether the two axes behave alike. At
# startup BAM reads its entire configuration from the database; four of the five steps
# read BAM tables, and the fifth reads the whole service table of the platform. So:
#
#   - the SERVICES axis grows the platform and leaves the BAM configuration alone.
#     Only "mapping_ms" should move. If the other four move as well, something else
#     scales with the platform and the diagnosis is incomplete.
#   - the BA axis grows the BAM configuration and leaves the platform alone. The four
#     BAM steps should move and "mapping_ms" should not -- it reads the same table
#     either way.
#
# If mapping_ms does not separate from the rest along the SERVICES axis, the
# optimisation behind this campaign is not worth writing. That is a result too.
#
# Each point restarts from an empty configuration database, so points are independent
# and can be run in any order -- or one at a time, on a laptop, which is the intent.

set -u

LABEL=${LABEL:-$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo bam)}
SVC_BY_HOST=${SVC_BY_HOST:-20}

# The services axis, in hosts: 10k, 50k and 200k services at 20 services per host.
# Drop 10000 from the list on a machine where the largest point does not fit; cbd holds
# the mapping of every service in memory, and nothing else here grows.
HOST_POINTS=${HOST_POINTS:-"500 2500 10000"}
# The BA configuration held constant along that axis.
BASE_NB_BA=${BASE_NB_BA:-100}

# The BA axis, at a platform size held constant.
BA_POINTS=${BA_POINTS:-"50 200 800"}
BASE_NB_HOSTS=${BASE_NB_HOSTS:-2500}

KPI_PER_BA=${KPI_PER_BA:-5}
BOOLEXP_PER_BA=${BOOLEXP_PER_BA:-1}

run_point() {
    local hosts=$1 bas=$2
    echo "== $(( hosts * SVC_BY_HOST )) services, ${bas} BAs =="
    ./bench.py run bam-startup --test BENCH_BAM_STARTUP \
        --var nb_hosts:${hosts} \
        --var svc_by_host:${SVC_BY_HOST} \
        --var nb_ba:${bas} \
        --var kpi_per_ba:${KPI_PER_BA} \
        --var boolexp_per_ba:${BOOLEXP_PER_BA} \
        --label "${LABEL}" --allow-dirty || {
            echo "point (${hosts} hosts, ${bas} BAs) failed, moving to the next one" >&2
        }
}

echo "bam-startup '${LABEL}'"
echo
echo "--- services axis (${BASE_NB_BA} BAs throughout) ---"
for hosts in ${HOST_POINTS} ; do
    run_point "${hosts}" "${BASE_NB_BA}"
done

echo
echo "--- BA axis ($(( BASE_NB_HOSTS * SVC_BY_HOST )) services throughout) ---"
for bas in ${BA_POINTS} ; do
    run_point "${BASE_NB_HOSTS}" "${bas}"
done
