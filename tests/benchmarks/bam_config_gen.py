#!/usr/bin/python3
#
# Copyright 2026 Centreon
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# For more information : contact@centreon.com
#
"""Fill the Centreon configuration database at platform scale, for the BAM
startup benchmark.

Why not tests/resources/db_conf.py: it builds the configuration database *from*
an Engine configuration, object by object, one INSERT per row (see
create_conf_db). At fifty thousand services that is a hundred thousand
round-trips, and the benchmark would spend its afternoon populating rather than
measuring. Here the rows are batched, and nothing is derived from an Engine
configuration -- because nothing needs to be.

That last point is what makes this benchmark cheap enough for a laptop.
reader_v2 reads the *database*, never the .cfg files, so measuring how long cbd
takes to load a BAM configuration needs a populated database and a cbd, and no
centengine at all. The expensive half of a platform-sized test is the one we can
leave out.

The generated shape, for the record:

  - `hosts` hosts named host_1..host_N, each owning `services_by_host` services
    named service_1..service_M -- global, sequential ids, like the test suite's
    own generator, so that a configuration produced here reads like the others;
  - one _Module_BAM_1 host carrying one ba_<id> virtual service per BA, without
    which reader_v2 refuses the whole configuration ("BA %d has no associated
    service");
  - `bas` BAs, each with `service_kpis_per_ba` service KPIs and
    `boolexps_per_ba` boolean rules, every rule naming two services by name --
    which is what gives the name-to-id resolution something to resolve.
"""

import sys
import time

import pymysql.cursors

try:
    from robot.libraries.BuiltIn import BuiltIn
except ImportError:  # pragma: no cover - only when run outside of robot
    BuiltIn = None

# Rows per INSERT. Large enough that the round-trips stop mattering, small
# enough to stay well under max_allowed_packet on the default MariaDB of the
# test container.
BATCH = 5000

DB_HOST = "localhost"
DB_PORT = 3306
DB_USER = "centreon"
DB_PASS = "centreon"
DB_NAME_CONF = "centreon"


def _load_db_settings():
    """Read the database coordinates from the suite's own variables.

    The benchmark has no business defining where the database lives: the test
    suite already says so, and a second definition would drift. The defaults
    above are only for running this file by hand, outside of robot.

    Called when a keyword runs, and not when the library is imported. Importing
    a library happens before the suite's own resources are in scope, and reading
    the variables there raised -- which robot reports as "No keyword with name
    ... found", a message that says nothing about the real cause. The variables
    are already in scope by the time a keyword runs, since the suite imports
    ../resources/import.resource.
    """
    global DB_HOST, DB_PORT, DB_USER, DB_PASS, DB_NAME_CONF
    if BuiltIn is None:
        return
    try:
        builtin = BuiltIn()
        settings = {
            "DB_HOST": builtin.get_variable_value("${DBHost}"),
            "DB_PORT": builtin.get_variable_value("${DBPort}"),
            "DB_USER": builtin.get_variable_value("${DBUser}"),
            "DB_PASS": builtin.get_variable_value("${DBPass}"),
            "DB_NAME_CONF": builtin.get_variable_value("${DBNameConf}"),
        }
    except Exception:  # noqa: BLE001 - outside robot, or variables not in scope
        return
    # A variable that is not set comes back as None; keeping the default beats
    # connecting to "None".
    if settings["DB_HOST"] is not None:
        DB_HOST = settings["DB_HOST"]
    if settings["DB_PORT"] is not None:
        DB_PORT = int(settings["DB_PORT"])
    if settings["DB_USER"] is not None:
        DB_USER = settings["DB_USER"]
    if settings["DB_PASS"] is not None:
        DB_PASS = settings["DB_PASS"]
    if settings["DB_NAME_CONF"] is not None:
        DB_NAME_CONF = settings["DB_NAME_CONF"]

# Emptied before every run, in dependency order. DELETE and not TRUNCATE: the
# foreign keys of a real Centreon schema make TRUNCATE fail on several of these
# even with the checks turned off for the session.
_TABLES = ("mod_bam_kpi", "mod_bam_boolean", "mod_bam_poller_relations",
           "mod_bam", "host_service_relation", "service", "host",
           "ns_host_relation", "nagios_server", "timeperiod", "meta_service")


def _connect():
    """Open a connection to the configuration database."""
    return pymysql.connect(host=DB_HOST, port=int(DB_PORT), user=DB_USER,
                           password=DB_PASS, database=DB_NAME_CONF,
                           charset='utf8mb4',
                           cursorclass=pymysql.cursors.DictCursor)


def _insert_many(cursor, query: str, rows: list):
    """Send rows in batches, so that one step is a handful of round-trips."""
    for start in range(0, len(rows), BATCH):
        cursor.executemany(query, rows[start:start + BATCH])


# Tables whose statistics matter to the queries BAM sends, and whose plans
# should not depend on when the last bulk insert happened.
_ANALYZED = ("host", "service", "host_service_relation", "mod_bam",
             "mod_bam_kpi", "mod_bam_boolean")


def _settle(connection, max_wait: int = 180) -> int:
    """Wait until the server has digested the inserts, and refresh statistics.

    Measured, and it cost a wrong conclusion before it was: a cbd started right
    after 400k rows were inserted spends its queries waiting on I/O while InnoDB
    is still flushing dirty pages. The BAM configuration load then measured
    319 ms at 200k services where the same work, on a settled server, takes
    10 ms -- a thirty-fold error, attributed to the product instead of to the
    benchmark. A point that does not wait is measuring the populating.

    Stability rather than a fixed threshold: the resting number of dirty pages
    depends on the server's configuration and on what else it is doing, so a
    threshold picked here would either never be reached or be reached at once.

    Args:
        connection: an open connection to the configuration database.
        max_wait (int, optional): seconds to wait at most. Defaults to 180.

    Returns:
        The number of dirty pages left, for the record.
    """
    with connection.cursor() as cursor:
        for table in _ANALYZED:
            cursor.execute(f"ANALYZE TABLE {table}")
            cursor.fetchall()
    # No FLUSH TABLES here: it needs the RELOAD privilege, which the suite's
    # "centreon" user does not have, and waiting for the flush to finish is
    # what this function is about anyway.

    dirty = -1
    stable = 0
    deadline = time.time() + max_wait
    while time.time() < deadline:
        with connection.cursor() as cursor:
            cursor.execute("SHOW GLOBAL STATUS LIKE "
                           "'Innodb_buffer_pool_pages_dirty'")
            row = cursor.fetchone()
        previous = dirty
        dirty = int(row["Value"]) if row else 0
        # Three readings within ten pages of each other: the flushing is over.
        if previous >= 0 and abs(previous - dirty) <= 10:
            stable += 1
            if stable >= 3:
                return dirty
        else:
            stable = 0
        time.sleep(1)
    return dirty


def ctn_bam_bench_populate(hosts: int, services_by_host: int, bas: int,
                           service_kpis_per_ba: int = 5,
                           boolexps_per_ba: int = 1,
                           metas: int = 0,
                           poller_id: int = 1) -> dict:
    """Build a platform-sized BAM configuration in the database.

    Args:
        hosts (int): how many hosts to create.
        services_by_host (int): services per host.
        bas (int): how many BAs.
        service_kpis_per_ba (int, optional): service KPIs per BA. Defaults to 5.
        boolexps_per_ba (int, optional): boolean rules per BA, each naming two
            services. Defaults to 1.
        metas (int, optional): how many meta-service KPIs to create, each with
            its meta_<id> virtual service. Defaults to 0 -- meta-services are a
            legacy shape, so they are opt-in rather than part of every point.
        poller_id (int, optional): the poller the BAs are bound to, which has to
            be the poller_id of the central cbd or reader_v2 filters them all
            out. Defaults to 1.

    Returns:
        A dict of what was created, to be filed as the parameters of the run.
    """
    hosts = int(hosts)
    services_by_host = int(services_by_host)
    bas = int(bas)
    service_kpis_per_ba = int(service_kpis_per_ba)
    boolexps_per_ba = int(boolexps_per_ba)
    metas = int(metas)
    services = hosts * services_by_host
    if services <= 0 or bas <= 0:
        raise ValueError("a BAM benchmark needs at least one service and one BA")

    _load_db_settings()
    connection = _connect()
    with connection:
        with connection.cursor() as cursor:
            # Off for the session only: the generated ids are consistent by
            # construction, and checking them row by row is most of the cost.
            cursor.execute("SET FOREIGN_KEY_CHECKS=0")
            cursor.execute("SET UNIQUE_CHECKS=0")
            for table in _TABLES:
                cursor.execute(f"DELETE FROM {table}")
                cursor.execute(f"ALTER TABLE {table} AUTO_INCREMENT = 1")

            cursor.execute("INSERT INTO nagios_server (id, name) "
                           "VALUES (%s, %s)", (poller_id, "poller0"))
            # reader_v2 loads the timeperiods as a dimension, and mod_bam rows
            # point at this one.
            cursor.execute(
                "INSERT INTO timeperiod (tp_id, tp_name, tp_alias, tp_sunday, "
                "tp_monday, tp_tuesday, tp_wednesday, tp_thursday, tp_friday, "
                "tp_saturday) VALUES (1, '24x7', '24x7', '00:00-24:00', "
                "'00:00-24:00', '00:00-24:00', '00:00-24:00', '00:00-24:00', "
                "'00:00-24:00', '00:00-24:00')")

            _insert_many(cursor,
                         "INSERT INTO host (host_id, host_name, host_alias, "
                         "host_address, host_register, host_activate) "
                         "VALUES (%s, %s, %s, %s, '1', '1')",
                         [(i, f"host_{i}", f"host_{i}", "127.0.0.1")
                          for i in range(1, hosts + 1)])

            # Global, sequential service ids: host_1 owns service_1..service_M,
            # host_2 the next M, like the suite's generator numbers them.
            service_rows = []
            relation_rows = []
            sid = 1
            for hid in range(1, hosts + 1):
                for _ in range(services_by_host):
                    service_rows.append((sid, f"service_{sid}"))
                    relation_rows.append((hid, sid))
                    sid += 1
            _insert_many(cursor,
                         "INSERT INTO service (service_id, "
                         "service_description, service_register, "
                         "service_activate) VALUES (%s, %s, '1', '1')",
                         service_rows)
            _insert_many(cursor,
                         "INSERT INTO host_service_relation (host_host_id, "
                         "service_service_id) VALUES (%s, %s)",
                         relation_rows)

            bam_host_id = hosts + 1
            cursor.execute(
                "INSERT INTO host (host_id, host_name, host_register, "
                "host_activate) VALUES (%s, '_Module_BAM_1', '2', '1')",
                (bam_host_id,))

            _insert_many(cursor,
                         "INSERT INTO mod_bam (ba_id, name, state_source, "
                         "activate, id_reporting_period, level_w, level_c, "
                         "id_notification_period, notifications_enabled, "
                         "event_handler_enabled, inherit_kpi_downtimes) "
                         "VALUES (%s, %s, 1, '1', 1, 80, 70, 1, '0', '0', '1')",
                         [(ba, f"ba-{ba}") for ba in range(1, bas + 1)])
            _insert_many(cursor,
                         "INSERT INTO mod_bam_poller_relations (ba_id, "
                         "poller_id) VALUES (%s, %s)",
                         [(ba, poller_id) for ba in range(1, bas + 1)])

            # The virtual service of every BA, on _Module_BAM_1. reader_v2
            # throws if a single BA is missing one, so this is not decoration.
            virtual_rows = []
            virtual_relations = []
            for ba in range(1, bas + 1):
                vsid = services + ba
                virtual_rows.append((vsid, f"ba_{ba}", f"ba-{ba}"))
                virtual_relations.append((bam_host_id, vsid))
            _insert_many(cursor,
                         "INSERT INTO service (service_id, "
                         "service_description, display_name, "
                         "service_register, service_activate) "
                         "VALUES (%s, %s, %s, '2', '1')",
                         virtual_rows)
            _insert_many(cursor,
                         "INSERT INTO host_service_relation (host_host_id, "
                         "service_service_id) VALUES (%s, %s)",
                         virtual_relations)

            # Service KPIs, spread over the whole platform rather than packed
            # into the first hosts: a mapping lookup that always hits the same
            # few entries would be a cache benchmark, not this one.
            kpi_rows = []
            picked = 0
            for ba in range(1, bas + 1):
                for _ in range(service_kpis_per_ba):
                    target = picked * 7919 % services + 1  # a prime, to scatter
                    picked += 1
                    hid = (target - 1) // services_by_host + 1
                    kpi_rows.append((hid, target, ba))
            _insert_many(cursor,
                         "INSERT INTO mod_bam_kpi (host_id, service_id, id_ba, "
                         "drop_warning, drop_critical, drop_unknown, "
                         "config_type, activate) "
                         "VALUES (%s, %s, %s, 40, 30, 20, '1', '1')",
                         kpi_rows)

            # Boolean rules. Each names two services, by host name and service
            # description -- the only thing in a BAM configuration that has to
            # be resolved from names, and the reason the whole service table is
            # read today.
            boolean_rows = []
            boolean_kpi_rows = []
            boolean_id = 1
            for ba in range(1, bas + 1):
                for _ in range(boolexps_per_ba):
                    first = picked * 7919 % services + 1
                    picked += 1
                    second = picked * 7919 % services + 1
                    picked += 1
                    host_a = (first - 1) // services_by_host + 1
                    host_b = (second - 1) // services_by_host + 1
                    expression = (
                        f"{{host_{host_a} service_{first}}} {{IS}} {{CRITICAL}} "
                        f"{{OR}} "
                        f"{{host_{host_b} service_{second}}} {{IS}} {{CRITICAL}}")
                    boolean_rows.append((boolean_id, f"bool-{boolean_id}",
                                         expression))
                    boolean_kpi_rows.append((boolean_id, ba))
                    boolean_id += 1
            if boolean_rows:
                _insert_many(cursor,
                             "INSERT INTO mod_bam_boolean (boolean_id, name, "
                             "expression, bool_state, activate) "
                             "VALUES (%s, %s, %s, 1, 1)",
                             boolean_rows)
                _insert_many(cursor,
                             "INSERT INTO mod_bam_kpi (boolean_id, id_ba, "
                             "drop_warning, drop_critical, drop_unknown, "
                             "config_type, activate) "
                             "VALUES (%s, %s, 50, 100, 75, '1', '1')",
                             boolean_kpi_rows)

            # Meta-service KPIs. Each needs its meta_<id> virtual service, on
            # the BAM host like the ba_<id> ones: reader_v2 resolves the ids of
            # a meta KPI by looking that description up, and refuses the whole
            # configuration when one is missing.
            if metas:
                _insert_many(cursor,
                             "INSERT INTO meta_service (meta_id, meta_name, "
                             "meta_activate) VALUES (%s, %s, '1')",
                             [(m, f"meta-{m}") for m in range(1, metas + 1)])
                meta_service_rows = []
                meta_relations = []
                meta_kpi_rows = []
                for m in range(1, metas + 1):
                    vsid = services + bas + m
                    meta_service_rows.append((vsid, f"meta_{m}", f"meta-{m}"))
                    meta_relations.append((bam_host_id, vsid))
                    # Spread over the BAs, so that one BA is not the only one
                    # carrying every meta KPI.
                    meta_kpi_rows.append((m, (m - 1) % bas + 1))
                _insert_many(cursor,
                             "INSERT INTO service (service_id, "
                             "service_description, display_name, "
                             "service_register, service_activate) "
                             "VALUES (%s, %s, %s, '2', '1')",
                             meta_service_rows)
                _insert_many(cursor,
                             "INSERT INTO host_service_relation (host_host_id, "
                             "service_service_id) VALUES (%s, %s)",
                             meta_relations)
                _insert_many(cursor,
                             "INSERT INTO mod_bam_kpi (meta_id, id_ba, "
                             "kpi_type, drop_warning, drop_critical, "
                             "drop_unknown, config_type, activate) "
                             "VALUES (%s, %s, '2', 40, 30, 20, '1', '1')",
                             meta_kpi_rows)
        connection.commit()
        dirty = _settle(connection)
        # Printed and not returned: the returned dict becomes the run's
        # parameters, which is what pairs two campaigns in the store, and a
        # value that moves from run to run would stop them pairing.
        print(f"settled with {dirty} dirty pages left")

    return {"hosts": hosts, "services": services, "bas": bas,
            "service_kpis": bas * service_kpis_per_ba,
            "boolexps": bas * boolexps_per_ba,
            "metas": metas}


if __name__ == "__main__":
    # Populating by hand, without robot, to prepare or inspect a run.
    if len(sys.argv) < 4:
        print("usage: bam_config_gen.py <hosts> <services_by_host> <bas> "
              "[service_kpis_per_ba] [boolexps_per_ba]")
        sys.exit(1)
    print(ctn_bam_bench_populate(*[int(a) for a in sys.argv[1:]]))
