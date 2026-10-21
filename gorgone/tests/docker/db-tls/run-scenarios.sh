#!/bin/bash
# MON-211445 end to end check: start gorgone with several DSN against MariaDB with TLS
# required (ON) and/or not required (OFF), and report what really happened:
# connection result seen in the gorgone logs + TLS cipher seen by the server.
#
# usage: ./run-scenarios.sh [ON|OFF|all]     (default: all)
#   GORGONE_SRC=/path/to/gorgone ./run-scenarios.sh   to test another version of the code
#
# The "expected" column is the behaviour with the MON-211445 fix.
cd "$(dirname "$0")" || exit 1
modes="${1:-all}"
[ "$modes" = "all" ] && modes="ON OFF"

[ -f certs/ca.pem ] || ./gen-certs.sh >/dev/null

CA="mariadb_ssl_ca_file=/certs/ca.pem"
BASE="host=db;port=3306;database=centreon"

# name | DSN | expected when TLS required (ON) | expected when TLS not required (OFF)
SCENARIOS=(
    "mysql, no TLS option|mysql:$BASE|refused|clear"
    "mysql, mysql_ssl=1 (ticket)|mysql:$BASE;mysql_ssl=1;mysql_ssl_ca_file=/certs/ca.pem;mysql_ssl_verify_server_cert=1|refused|refused"
    "MariaDB, TLS + CA check|MariaDB:$BASE;mariadb_ssl=1;$CA;mariadb_ssl_verify_server_cert=1|tls|tls"
    "MariaDB, TLS + wrong CA|MariaDB:$BASE;mariadb_ssl=1;mariadb_ssl_ca_file=/certs/wrong-ca.pem;mariadb_ssl_verify_server_cert=1|refused|refused"
    "MariaDB, no TLS option|MariaDB:$BASE|refused|clear"
)

# wait for the nodes module to reach the database (or fail), print "<result>|<detail>"
run_one() {
    local dsn="$1" logs error cipher
    GORGONE_DSN="$dsn" docker compose up -d --no-deps --force-recreate gorgone >/dev/null 2>&1
    for _ in $(seq 1 30); do
        logs=$(docker compose logs --no-log-prefix gorgone 2>/dev/null)
        if grep -q "\[nodes\] retrieved" <<< "$logs"; then
            cipher=$(./check-tls.sh 2>/dev/null | awk -F'\t' 'NR > 1 { print $4; exit }')
            docker compose stop gorgone >/dev/null 2>&1
            if [ -z "$cipher" ] || [ "$cipher" = "(clear)" ]; then
                echo "clear|connected without TLS"
            else
                echo "tls|$cipher"
            fi
            return
        fi
        error=$(grep -m1 -oP "cannot connect to database '[^']*': \K.*?(?= \(caller)" <<< "$logs")
        if [ -n "$error" ]; then
            docker compose stop gorgone >/dev/null 2>&1
            echo "refused|$error"
            return
        fi
        sleep 1
    done
    docker compose stop gorgone >/dev/null 2>&1
    echo "timeout|no result after 30s, see: docker compose logs gorgone"
}

failed=0
printf "\ngorgone code: %s\n" "$(realpath "${GORGONE_SRC:-../../..}")"
for mode in $modes; do
    printf "\n=== MariaDB with TLS required: %s ===\n" "$mode"
    docker compose down -v >/dev/null 2>&1
    DB_TLS_REQUIRED=$mode docker compose up -d --wait db >/dev/null 2>&1 || { echo "database did not start"; exit 1; }
    printf "%-32s %-9s %-9s %-6s %s\n" "scenario" "expected" "result" "status" "detail"
    for scenario in "${SCENARIOS[@]}"; do
        IFS='|' read -r name dsn expected_on expected_off <<< "$scenario"
        expected=$expected_on
        [ "$mode" = "OFF" ] && expected=$expected_off
        IFS='|' read -r result detail <<< "$(DB_TLS_REQUIRED=$mode run_one "$dsn")"
        status=PASS
        if [ "$result" != "$expected" ]; then status=FAIL; failed=1; fi
        printf "%-32s %-9s %-9s %-6s %s\n" "$name" "$expected" "$result" "$status" "$detail"
    done
done
docker compose down -v >/dev/null 2>&1
exit $failed
