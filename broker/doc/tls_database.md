# TLS/SSL Database Connection Configuration
Centreon Broker now supports secured TLS 1.2/1.3 connections to MySQL/MariaDB databases. This feature allows you to encrypt the communication between Centreon Broker and the database servers.

## Configuration Parameters

The following parameters can be added to any database output configuration (unified_sql, sql, storage) in your Centreon Broker configuration file:

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `db_ssl_enabled` | boolean | No | `false` | Enable SSL/TLS for database connection |
| `db_ssl_ca` | string | No | - | Path to the Certificate Authority (CA) certificate file in PEM format |
| `db_ssl_cert` | string | No | - | Path to the client certificate file in PEM format |
| `db_ssl_key` | string | No | - | Path to the client private key file in PEM format |
| `db_tls_version` | string | No | `TLSv1.3` | TLS protocol version(s) to use (e.g., "TLSv1.2", "TLSv1.3", "TLSv1.2,TLSv1.3") |
| `db_ssl_verify_cert` | boolean | No | `true` | Enable certificate verification (identity check) |

A boolean can be given as a JSON boolean (`true`) or as a string (`"true"`, `"yes"`, `"1"`, `"false"`, `"no"`, `"0"`).

### Minimal TLS Configuration (Server Authentication Only)

If you only want to verify the server's identity without client certificate authentication:

```json
{
  "centreon-broker-unified-sql": {
    "type": "unified_sql",
    "db_type": "mysql",
    "db_host": "database.example.com",
    "db_port": "3306",
    "db_user": "centreon",
    "db_password": "password",
    "db_name": "centreon_storage",
    "db_ssl_enabled": "true",
    "db_ssl_ca": "/etc/centreon-broker/ssl/ca-cert.pem",
    "db_ssl_verify_cert": "true"
  }
}
```

### TLS without Certificate Verification

```json
{
  "centreon-broker-unified-sql": {
    "type": "unified_sql",
    "db_type": "mysql",
    "db_host": "database.example.com",
    "db_port": "3306",
    "db_user": "centreon",
    "db_password": "password",
    "db_name": "centreon_storage",
    "db_ssl_enabled": "true",
    "db_ssl_verify_cert": "false"
  }
}
```

With `db_ssl_enabled` set to `true`, the broker never falls back to an unencrypted connection, even when `db_ssl_verify_cert` is `false`:
- Over TCP, the broker reads the server greeting first. If the server does not support TLS, the connection is refused before any credential is sent: `TLS is enabled but the database server <host>:<port> does not support it, connection refused before sending credentials.`
- Over the unix socket (`db_host` set to `localhost`), the connection is refused right after it is established: `TLS is enabled but the database server does not support it, unencrypted connection refused.`

Without certificate verification, the connection is encrypted but the server identity is not checked, so it does not protect against an attacker who can intercept the traffic. Use `db_ssl_verify_cert` set to `true` with `db_ssl_ca` for that.
### MariaDB/MySQL Server Configuration

Configure your MariaDB/MySQL server to support SSL connections:

### Database Configuration 
mariadb (`/etc/mysql/mariadb.conf.d/99-tls.cnf`)
mysql (`/etc/my.cnf.d/99-tls.cnf`)

```ini
[mysqld]
ssl_ca=/path/to/ca-cert.pem
ssl_cert=/path/to/server-cert.pem
ssl_key=/path/to/server-key.pem

# Optional: Require SSL for specific users
# In MySQL/MariaDB console:
# ALTER USER 'centreon'@'%' REQUIRE SSL;
# Or for mutual TLS authentication:
# ALTER USER 'centreon'@'%' REQUIRE X509;
```

Restart MariaDB after configuration changes:
```bash
systemctl restart mariadb
```

### Verify Server SSL Configuration

```bash
mysql -u root -p -e "SHOW VARIABLES LIKE '%ssl%';"
```

You should see `have_ssl` set to `YES`.

## Verification

### Check Broker Logs

After enabling SSL, check the Centreon Broker logs for SSL-related messages:

```bash
grep -i ssl /var/log/centreon-broker/central-broker-master.log
```

Expected log entries:
```
[2026-02-17 10:00:00] [info] SSL/TLS enabled for database connection
```

**Test connection manually**:
```bash
mysql --ssl-ca=/path/to/ca-cert.pem \
         --ssl-cert=/path/to/client-cert.pem \
         --ssl-key=/path/to/client-key.pem \
         -h <host> -u <user> -p
```
