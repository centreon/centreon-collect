# Gorgone database TLS end to end test (MON-211445)

Gorgone (Debian 13 Perl drivers: DBD::mysql 4.053 and DBD::MariaDB 1.22 built against
libmariadb3 11.8) connects to a MariaDB 11.8 server with TLS. Gorgone runs from the source code
mounted in the container, with the `nodes` module which reads the `centreon` database at startup.

| File                    | Role                                                                            |
| :---------------------- | :------------------------------------------------------------------------------ |
| `docker-compose.yml`    | `db` (MariaDB with TLS) and `gorgone` (trixie image, code mounted as a volume) |
| `gen-certs.sh`          | test CA, server certificate for `db`, and an unrelated `wrong-ca.pem`          |
| `db-init/01-centreon.sh`| minimal `centreon` database and `centreon` user (`REQUIRE SSL` when TLS is ON) |
| `gorgone-entrypoint.sh` | writes the gorgone configuration with `GORGONE_DSN` and starts gorgoned        |
| `check-tls.sh`          | TLS cipher of the `centreon` sessions, read on the server side                 |
| `run-scenarios.sh`      | runs every DSN in the 3 database modes (ON, OFF, NOTLS) and reports the results |

Variables:

| Variable          | Default                                                        | Description                                       |
| :---------------- | :------------------------------------------------------------- | :------------------------------------------------ |
| `DB_TLS_REQUIRED` | `ON`                                                           | `ON`: `require_secure_transport` + `REQUIRE SSL` |
| `DB_TLS`          | `ON`                                                           | `OFF`: the server does not offer TLS (use with `DB_TLS_REQUIRED=OFF`) |
| `GORGONE_DSN`     | `MariaDB:` DSN with TLS and CA verification                    | DSN used by gorgone                               |
| `GORGONE_SRC`     | `../../..` (the `gorgone` directory of this repository)        | gorgone source code to test                       |
| `GORGONE_IMAGE`   | `docker.centreon.com/centreon/gorgone-testing-trixie:release-26.09-next` | image with the Debian 13 drivers  |
| `DB_IMAGE`        | `mariadb:11.8`                                                 | database server                                   |

All commands are run from this directory.

## Run every scenario

```shell
./gen-certs.sh                 # once
./run-scenarios.sh             # modes ON, OFF and NOTLS (or ./run-scenarios.sh ON|OFF|NOTLS)
```

The `expected` column is the behaviour with the fix, the `detail` column gives the error or the TLS cipher
seen by the server. To compare with the code before the fix:

```shell
mkdir -p /tmp/gorgone-develop
git -C ../../.. archive origin/develop -- gorgoned gorgone | tar -x -C /tmp/gorgone-develop
GORGONE_SRC=/tmp/gorgone-develop ./run-scenarios.sh
```

## Manual run

```shell
# clean environment (the database is only initialised on the first start)
docker compose down -v

# database with TLS required (ON) or not required (OFF)
DB_TLS_REQUIRED=ON docker compose up -d --wait db

# gorgone with a DSN (keep the same DB_TLS_REQUIRED, or the database is recreated)
DB_TLS_REQUIRED=ON GORGONE_DSN='MariaDB:host=db;port=3306;database=centreon;mariadb_ssl=1;mariadb_ssl_ca_file=/certs/ca.pem;mariadb_ssl_verify_server_cert=1' \
    docker compose up -d --no-deps --force-recreate gorgone
docker compose logs -f gorgone     # "[nodes] retrieved 1 nodes from DB" = connected
./check-tls.sh                     # cipher of the gorgone session, "(clear)" = no TLS

# ticket error with DBD::mysql: "SSL connection error: Enforcing SSL encryption is not supported"
DB_TLS_REQUIRED=ON GORGONE_DSN='mysql:host=db;port=3306;database=centreon;mysql_ssl=1;mysql_ssl_ca_file=/certs/ca.pem' \
    docker compose up -d --no-deps --force-recreate gorgone

# switch to TLS not required: always restart from a clean database
docker compose down -v
DB_TLS_REQUIRED=OFF docker compose up -d --wait db

# stop and remove everything
docker compose down -v
```
