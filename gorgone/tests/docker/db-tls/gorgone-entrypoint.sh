#!/bin/bash
# Start gorgoned from the mounted source code (/usr/src/gorgone) with the nodes module,
# which connects to the centreon database as soon as it starts.
set -e

mkdir -p /tmp/gorgone
cat > /tmp/gorgone/config.yaml <<EOF
name: config.yaml
description: MON-211445 database TLS test
configuration:
  centreon:
    database:
      db_configuration:
        dsn: "${GORGONE_DSN}"
        username: centreon
        password: centreon
      db_realtime:
        dsn: "${GORGONE_DSN}"
        username: centreon
        password: centreon
  gorgone:
    gorgonecore:
      id: 1
      internal_com_type: ipc
      internal_com_path: /tmp/gorgone/routing.ipc
      gorgone_db_type: SQLite
      gorgone_db_name: dbname=/tmp/gorgone/history.sdb
      privkey: /tmp/gorgone/rsakey.priv.pem
      pubkey: /tmp/gorgone/rsakey.pub.pem
    modules:
      - name: nodes
        package: "gorgone::modules::centreon::nodes::hooks"
        enable: true
EOF

echo "DSN: ${GORGONE_DSN}"
perl -MDBD::mysql -MDBD::MariaDB -e 'printf("DBD::mysql %s, DBD::MariaDB %s\n", $DBD::mysql::VERSION, $DBD::MariaDB::VERSION)'
exec perl /usr/src/gorgone/gorgoned --config=/tmp/gorgone/config.yaml --severity=info --logfile=/dev/stdout --flushoutput
