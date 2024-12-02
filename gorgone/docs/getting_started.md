# Getting started

## Installation

### From package

Using Centreon standard yum repositories, execute the following command to install Gorgone:

```bash
yum install centreon-gorgone
```

for debian like distribution:

```bash
apt-get install centreon-gorgone
```

### From sources

See the dedicated page [here](/build_from_source.md).


## Configuration

You can retrieve `centcore` configuration, i.e. database hostname and credentials in */etc/centreon/conf.pm*, and build a minimal configuration by applying the [migration procedure](../docs/migration.md).

All directives are available [here](../docs/configuration.md).

## Create the database

Gorgone uses a SQLite database to store all events messages.

If it does not exist, the daemon will automatically create it in the path set by the `gorgone_db_name` configuration directive.

However, you can manualy create it with the database schema:

```bash
sqlite3 -init schema/gorgone_database.sql /var/lib/centreon-gorgone/history.sdb
```

Database schema:

```sql
CREATE TABLE IF NOT EXISTS `gorgone_identity` (
    `id` INTEGER PRIMARY KEY,
    `ctime` int(11) DEFAULT NULL,
    `identity` varchar(2048) DEFAULT NULL,
    `key` varchar(4096) DEFAULT NULL,
    `parent` int(11) DEFAULT '0'
);

CREATE INDEX IF NOT EXISTS idx_gorgone_identity ON gorgone_identity (identity);
CREATE INDEX IF NOT EXISTS idx_gorgone_parent ON gorgone_identity (parent);

CREATE TABLE IF NOT EXISTS `gorgone_history` (
    `id` INTEGER PRIMARY KEY,
    `token` varchar(2048) DEFAULT NULL,
    `code` int(11) DEFAULT NULL,
    `etime` int(11) DEFAULT NULL,
    `ctime` int(11) DEFAULT NULL,
    `instant` int(11) DEFAULT '0',
    `data` TEXT DEFAULT NULL
);

CREATE INDEX IF NOT EXISTS idx_gorgone_history_id ON gorgone_history (id);
CREATE INDEX IF NOT EXISTS idx_gorgone_history_token ON gorgone_history (token);
CREATE INDEX IF NOT EXISTS idx_gorgone_history_etime ON gorgone_history (etime);
CREATE INDEX IF NOT EXISTS idx_gorgone_history_code ON gorgone_history (code);
CREATE INDEX IF NOT EXISTS idx_gorgone_history_ctime ON gorgone_history (ctime);
CREATE INDEX IF NOT EXISTS idx_gorgone_history_instant ON gorgone_history (instant);

CREATE TABLE IF NOT EXISTS `gorgone_synchistory` (
    `id` int(11) DEFAULT NULL,
    `ctime` int(11) DEFAULT NULL,
    `last_id` int(11) DEFAULT NULL
);

CREATE INDEX IF NOT EXISTS idx_gorgone_synchistory_id ON gorgone_synchistory (id);

CREATE TABLE IF NOT EXISTS `gorgone_target_fingerprint` (
  `id` INTEGER PRIMARY KEY,
  `target` varchar(2048) DEFAULT NULL,
  `fingerprint` varchar(4096) DEFAULT NULL
);

CREATE INDEX IF NOT EXISTS idx_gorgone_target_fingerprint_target ON gorgone_target_fingerprint (target);
```

## Launch the daemon

If you are using the package, just launch the service as below:

```bash
systemctl start gorgoned
```

Make sure the daemon is running:

```bash
$ systemctl status gorgoned
● gorgoned.service - Centreon Gorgone
   Loaded: loaded (/etc/systemd/system/gorgoned.service; disabled; vendor preset: disabled)
   Active: active (running) since Mon 2019-09-30 09:36:19 CEST; 2min 29s ago
 Main PID: 5168 (perl)
   CGroup: /system.slice/gorgoned.service
           ├─5168 /usr/bin/perl /usr/bin/gorgoned --config=/etc/centreon-gorgone/config.yaml --logfile=/var/log/centreon/gorgoned.log --severity=info
           ├─5175 gorgone-dbcleaner
           ├─5182 gorgone-action
           ├─5187 gorgone-nodes
           ├─5190 gorgone-legacycmd
           ├─5203 gorgone-proxy
           ├─5204 gorgone-proxy
           ├─5205 gorgone-proxy
           ├─5206 gorgone-proxy
           └─5207 gorgone-proxy

Sep 30 09:36:19 localhost systemd[1]: Started Centreon Gorgone.
```

If you are using the sources, execute the following command:

```bash
perl gorgoned --config=config/config.yaml --severity=info
```

## Full-ZMQ setup

To use Gorgone distributed on multiple servers using ZMQ, follow the example given [here](../docs/communication_modes/client_server_zmq.md).
