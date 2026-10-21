#!/bin/bash
# MON-211445 end to end check: start gorgone with several DSN against MariaDB and report what
# really happened: connection result seen in the gorgone logs + TLS cipher seen by the server.
#
# Database modes:
#   ON     TLS offered and required by the server
#   OFF    TLS offered, clear connections accepted
#   NOTLS  TLS not offered at all (what a man in the middle removing TLS looks like):
#          the TLS DSN must refuse to connect instead of falling back to a clear connection
#
# usage: ./run-scenarios.sh [ON|OFF|NOTLS|all]     (default: all)
#   GORGONE_SRC=/path/to/gorgone ./run-scenarios.sh   to test another version of the code
#
# The "expected" column is the behaviour with the MON-211445 fix.
cd "$(dirname "$0")" || exit 1
modes="${1:-all}"
[ "$modes" = "all" ] && modes="ON OFF NOTLS"

[ -f certs/ca.pem ] || ./gen-certs.sh >/dev/null

BASE="host=db;port=3306;database=centreon"

# name | DSN | expected with ON | expected with OFF | expected with NOTLS
SCENARIOS=(
    "mysql, no TLS option|mysql:$BASE|refused|clear|clear"
    "mysql, mysql_ssl=1 (ticket)|mysql:$BASE;mysql_ssl=1;mysql_ssl_ca_file=/certs/ca.pem;mysql_ssl_verify_server_cert=1|refused|refused|refused"
    "mysql, ssl_optional + verify|mysql:$BASE;mysql_ssl=1;mysql_ssl_optional=1;mysql_ssl_ca_file=/certs/ca.pem;mysql_ssl_verify_server_cert=1|tls|tls|refused"
    "mysql, ssl_optional no verify|mysql:$BASE;mysql_ssl=1;mysql_ssl_optional=1;mysql_ssl_ca_file=/certs/ca.pem|tls|tls|clear"
    "MariaDB, TLS + CA check|MariaDB:$BASE;mariadb_ssl=1;mariadb_ssl_ca_file=/certs/ca.pem;mariadb_ssl_verify_server_cert=1|tls|tls|refused"
    "MariaDB, TLS + wrong CA|MariaDB:$BASE;mariadb_ssl=1;mariadb_ssl_ca_file=/certs/wrong-ca.pem;mariadb_ssl_verify_server_cert=1|refused|refused|refused"
    "MariaDB, no TLS option|MariaDB:$BASE|refused|clear|clear"
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
    case "$mode" in
        ON) export DB_TLS=ON DB_TLS_REQUIRED=ON; column=3 ;;
        OFF) export DB_TLS=ON DB_TLS_REQUIRED=OFF; column=4 ;;
        NOTLS) export DB_TLS=OFF DB_TLS_REQUIRED=OFF; column=5 ;;
        *) echo "unknown mode $mode"; exit 1 ;;
    esac
    printf "\n=== database mode %s (TLS offered: %s, TLS required: %s) ===\n" "$mode" "$DB_TLS" "$DB_TLS_REQUIRED"
    docker compose down -v >/dev/null 2>&1
    docker compose up -d --wait db >/dev/null 2>&1 || { echo "database did not start"; exit 1; }
    printf "%-32s %-9s %-9s %-6s %s\n" "scenario" "expected" "result" "status" "detail"
    for scenario in "${SCENARIOS[@]}"; do
        IFS='|' read -r -a fields <<< "$scenario"
        name=${fields[0]}
        expected=${fields[$((column - 1))]}
        IFS='|' read -r result detail <<< "$(run_one "${fields[1]}")"
        status=PASS
        if [ "$result" != "$expected" ]; then status=FAIL; failed=1; fi
        printf "%-32s %-9s %-9s %-6s %s\n" "$name" "$expected" "$result" "$status" "$detail"
    done
done
docker compose down -v >/dev/null 2>&1
exit $failed
