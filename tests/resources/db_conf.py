#!/usr/bin/python3
import pymysql.cursors
from robot.libraries.BuiltIn import BuiltIn,RobotNotRunningError



def import_robot_resources():
    global DB_NAME_STORAGE, DB_NAME_CONF, DB_USER_ROOT, DB_PASS_ROOT, DB_USER, DB_PASS, DB_HOST, DB_PORT, VAR_ROOT, ETC_ROOT, CONF_DIR, ENGINE_HOME
    try:
        BuiltIn().import_resource('db_variables.resource')
        DB_NAME_STORAGE = BuiltIn().get_variable_value("${DBName}")
        DB_NAME_CONF = BuiltIn().get_variable_value("${DBNameConf}")
        DB_USER_ROOT = BuiltIn().get_variable_value("${DBUserRoot}")
        DB_PASS_ROOT = BuiltIn().get_variable_value("${DBPassRoot}")
        DB_USER = BuiltIn().get_variable_value("${DBUser}")
        DB_PASS = BuiltIn().get_variable_value("${DBPass}")
        DB_HOST = BuiltIn().get_variable_value("${DBHost}")
        DB_PORT = BuiltIn().get_variable_value("${DBPort}")
        VAR_ROOT = BuiltIn().get_variable_value("${VarRoot}")
        ETC_ROOT = BuiltIn().get_variable_value("${EtcRoot}")

        CONF_DIR = ETC_ROOT + "/centreon-engine"
        ENGINE_HOME = VAR_ROOT + "/lib/centreon-engine"
    except RobotNotRunningError:
        # Handle this case if Robot Framework is not running
        print("Robot Framework is not running. Skipping resource import.")

DB_NAME_STORAGE = ""
DB_NAME_CONF = ""
DB_USER_ROOT = ""
DB_PASS_ROOT = ""
DB_USER = ""
DB_PASS = ""
DB_HOST = ""
DB_PORT = ""
VAR_ROOT = ""
ETC_ROOT = ""

CONF_DIR = ""
ENGINE_HOME = ""

import_robot_resources()

class DbConf:
    def __init__(self, engine):
        self.last_service_id = 0
        self.hosts = []
        self.command = {}
        self.host = {}
        self.service = {}
        self.last_host_id = 0
        self.last_host_group_id = 0
        self.commands_count = 50
        self.instances = engine.instances
        self.service_cmd = engine.service_cmd
        self.hosts_count = 50
        self.services_per_host_count = 20
        self.commands_per_poller_count = 50
        self.ctn_clear_db()
        self.engine = engine

    def ctn_clear_db(self):
        # Connect to the database
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER_ROOT,
                                     password=DB_PASS_ROOT,
                                     database=DB_NAME_CONF,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                # Read a single record
                cursor.execute("DELETE FROM nagios_server")
                cursor.execute("ALTER TABLE nagios_server AUTO_INCREMENT = 1")
                cursor.execute("DELETE FROM host")
                cursor.execute("ALTER TABLE host AUTO_INCREMENT = 1")
                cursor.execute("DELETE FROM command")
                cursor.execute("ALTER TABLE command AUTO_INCREMENT = 1")
                cursor.execute("DELETE FROM ns_host_relation")
                cursor.execute("DELETE FROM service")
                cursor.execute("ALTER TABLE service AUTO_INCREMENT = 1")
                cursor.execute("DELETE FROM host_service_relation")
                cursor.execute(
                    "ALTER TABLE host_service_relation AUTO_INCREMENT = 1")
                cursor.execute("DELETE FROM on_demand_macro_host")
                cursor.execute("DELETE FROM on_demand_macro_service")
                cursor.execute("DELETE FROM mod_bam")
                cursor.execute("ALTER TABLE mod_bam AUTO_INCREMENT = 1")
                cursor.execute("DELETE FROM mod_bam_kpi")
                cursor.execute("DELETE FROM mod_bam_poller_relations")
                cursor.execute("ALTER TABLE mod_bam_kpi AUTO_INCREMENT = 1")
                cursor.execute("DELETE FROM mod_bam_boolean")
                cursor.execute(
                    "ALTER TABLE mod_bam_boolean AUTO_INCREMENT = 1")
                cursor.execute("DELETE FROM timeperiod")
                cursor.execute("ALTER TABLE timeperiod AUTO_INCREMENT = 1")
                cursor.execute("SET GLOBAL FOREIGN_KEY_CHECKS=0")
            connection.commit()

    def init_bam(self):
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_CONF,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        self.engine.create_bam_timeperiod()
        self.engine.create_bam_command()
        with connection:
            with connection.cursor() as cursor:
                self.module_bam_hid = self.engine.create_bam_host()
                cursor.execute("INSERT INTO host (host_id, host_name, contact_additive_inheritance, cg_additive_inheritance,host_location,host_locked,host_register,host_activate) VALUES ({}, '_Module_BAM_1',0,0,0,0,'2','1')".format(self.module_bam_hid))
            with connection.cursor() as cursor:
                cursor.execute("""INSERT INTO timeperiod (`tp_id`,`tp_name`,`tp_alias`,`tp_sunday`,`tp_monday`,`tp_tuesday`,`tp_wednesday`,`tp_thursday`,`tp_friday`,`tp_saturday`)
VALUES (1,'24x7','24_Hours_A_Day,_7_Days_A_Week','00:00-24:00','00:00-24:00','00:00-24:00','00:00-24:00','00:00-24:00','00:00-24:00','00:00-24:00');""")
            connection.commit()
        self.engine.centengine_conf_add_bam()

    def create_conf_db(self):
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_CONF,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                # Read a single record
                for i in range(self.instances):
                    cursor.execute(
                        "INSERT INTO nagios_server (name) VALUES (\"poller{}\")".format(i))
            connection.commit()

            # hosts
            hosts = self.hosts_count
            r = 0
            if hosts % self.instances > 0:
                r = 1
            v = int(hosts / self.instances) + r
            with connection.cursor() as cursor:
                # Insertion of HOSTS COMMANDS
                for i in range(1, self.hosts_count + 1):
                    name = f"checkh{i}"
                    cursor.execute(
                        f"INSERT INTO command (command_name,command_line) VALUES (\"{name}\",\"{ENGINE_HOME}/check.pl --id 0 --state {i}\")")
                    self.command[name] = cursor.lastrowid
                connection.commit()

                # Insertion of SERVICES COMMANDS
                for i in range(1, self.commands_per_poller_count * self.instances + 1):
                    name = f"command_{i}"
                    cursor.execute(
                        f"INSERT INTO command (command_name,command_line) VALUES (\"{name}\",\"{ENGINE_HOME}/check.pl --id {i}\")")
                    self.command[name] = cursor.lastrowid

                # Two specific commands
                cursor.execute(
                    f"INSERT INTO command (command_name,command_line) VALUES (\"notif\",\"{ENGINE_HOME}/notif.pl\")")
                self.command["notif"] = cursor.lastrowid
                cursor.execute(
                    f"INSERT INTO command (command_name,command_line) VALUES (\"test-notif\",\"{ENGINE_HOME}/notif.pl\")")
                self.command["test-notif"] = cursor.lastrowid
                connection.commit()

                for i in range(1, self.hosts_count + 1):
                    ipa = i % 255
                    q = i // 255
                    ipb = q % 255
                    q //= 255
                    ipc = q % 255
                    q //= 255
                    ipd = q % 255
                    cursor.execute("INSERT INTO host (host_name,host_alias,host_address,host_register,command_command_id,timeperiod_tp_id,host_snmp_community,host_snmp_version) VALUES (\"host_{0}\",\"host_{0}\",\"{1}.{2}.{3}.{4}\",'1',{5},1,'public','2c')".format(
                        i, ipa, ipb, ipc, ipd, self.command["checkh{}".format(i)]))
                    self.host["host_{}".format(i)] = i
                connection.commit()

                sid = 1
                for hid in range(1, self.hosts_count + 1):
                    for i in range(1, self.services_per_host_count + 1):
                        cursor.execute("INSERT INTO service (service_description,service_id,service_max_check_attempts,service_normal_check_interval,service_retry_check_interval,service_register,command_command_id, service_active_checks_enabled, service_passive_checks_enabled) VALUES (\"service_{0}\",{0},3, 5, 1, '1', {1}, '1', '1')".format(
                            sid, self.command[self.service_cmd[sid]]))
                        self.service["service_{}".format(sid)] = sid
                        cursor.execute(
                            "INSERT INTO host_service_relation (host_host_id,service_service_id) VALUES ({},{})".format(hid, sid))
                        sid += 1
                    cursor.execute(
                        "INSERT INTO on_demand_macro_host (host_macro_name,host_macro_value,host_host_id) VALUES ('$_HOSTKEY{0}$','VAL{0}',{0})".format(hid))
                connection.commit()

            hid = 1
            for inst in range(1, self.instances + 1):
                if v < hosts:
                    nb_hosts = v
                    hosts -= v
                else:
                    nb_hosts = hosts
                    hosts = 0

                # We have nb_hosts to build in the poller${inst}
                with connection.cursor() as cursor:
                    for i in range(1, nb_hosts + 1):
                        ipa = i % 255
                        q = i // 255
                        ipb = q % 255
                        q //= 255
                        ipc = q % 255
                        q //= 255
                        ipd = q % 255
                        cursor.execute(
                            "INSERT INTO ns_host_relation VALUES ({},{})".format(inst, hid))
                        hid += 1
                    connection.commit()

    def ctn_create_ba_with_services(self, name: str, typ: str, svc: [(str, str)], dt_policy):
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_CONF,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            if typ == 'best':
                t = 1
            elif typ == 'worst':
                t = 2
            with connection.cursor() as cursor:
                if dt_policy == "inherit":
                    inherit_dt = 1
                elif dt_policy == "ignore":
                    inherit_dt = 2
                else:
                    inherit_dt = 0

                cursor.execute("INSERT INTO mod_bam (name, state_source, activate,id_reporting_period,level_w,level_c,id_notification_period,notifications_enabled,event_handler_enabled, inherit_kpi_downtimes) VALUES ('{}',{},'1',1, 80, 70, 1,'0', '0','{}')".format(name, t, inherit_dt))
                id_ba = cursor.lastrowid
                sid = self.engine.create_bam_service("ba_{}".format(
                    id_ba), name, "_Module_BAM_1", "centreon-bam-check!{}".format(id_ba))
                cursor.execute("INSERT INTO service (service_id, service_description, display_name, service_active_checks_enabled, service_passive_checks_enabled,service_register) VALUES ({0}, \"ba_{1}\",\"{2}\",'2','2','2')".format(
                    sid, id_ba, name))
                cursor.execute("INSERT INTO host_service_relation (host_host_id, service_service_id) VALUES ({},{})".format(
                    self.module_bam_hid, sid))
                cursor.execute(
                    "INSERT INTO mod_bam_poller_relations VALUES ({},1)".format(id_ba))
                for v in svc:
                    cursor.execute("INSERT INTO mod_bam_kpi (host_id,service_id,id_ba,drop_warning_impact_id,drop_critical_impact_id,drop_unknown_impact_id,config_type) VALUES ({},{},{},1,1,1,'0')".format(
                        self.host[v[0]], self.service[v[1]], id_ba))

                connection.commit()
                return (id_ba, sid)

    def ctn_create_ba(self, name: str, typ: str, critical_impact: int, warning_impact: int, dt_policy: str, activate:int = 1):
        """
        Create a BA in the centreon database.

        Args:
            name: name of the BA
            typ: One word among {best, worst, ratio_percent, ratio_number, impact}
            critical_impact: Critical impact level
            warning_impact: Warning impact level
            dt_policy: Downtime policy: inherit, ignore
            activate: 1 for enable, 0 for disable
        """
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_CONF,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            if typ == 'best':
                t = 1
            elif typ == 'worst':
                t = 2
            elif typ == 'ratio_percent':
                t = 3
            elif typ == 'ratio_number':
                t = 4
            elif typ == 'impact':
                t = 0
            if dt_policy == "inherit":
                inherit_dt = 1
            elif dt_policy == "ignore":
                inherit_dt = 2
            else:
                inherit_dt = 0
            with connection.cursor() as cursor:
                cursor.execute(f"INSERT INTO mod_bam (name, state_source, activate,id_reporting_period,level_w,level_c,id_notification_period,notifications_enabled,event_handler_enabled, inherit_kpi_downtimes) VALUES ('{name}',{t},'{activate}',1, {warning_impact}, {critical_impact}, 1,'0', '0','{inherit_dt}')")
                id_ba = cursor.lastrowid
                sid = self.engine.create_bam_service("ba_{}".format(
                    id_ba), name, "_Module_BAM_1", "centreon-bam-check!{}".format(id_ba))
                cursor.execute("INSERT INTO service (service_id, service_description, display_name, service_active_checks_enabled, service_passive_checks_enabled,service_register) VALUES ({0}, \"ba_{1}\",\"{2}\",'2','2','2')".format(
                    sid, id_ba, name))
                cursor.execute("INSERT INTO host_service_relation (host_host_id, service_service_id) VALUES ({},{})".format(
                    self.module_bam_hid, sid))
                cursor.execute(
                    "INSERT INTO mod_bam_poller_relations VALUES ({},1)".format(id_ba))
                connection.commit()
                return (id_ba, sid)

    def ctn_add_service_kpi(self, host: str, serv: str, id_ba: int, critical_impact: int, warning_impact: int, unknown_impact: int):
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_CONF,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute("INSERT INTO mod_bam_kpi (host_id,service_id,id_ba,drop_warning,drop_critical,drop_unknown,config_type) VALUES ({},{},{},{},{},{},'1')".format(
                    self.host[host], self.service[serv], id_ba, warning_impact, critical_impact, unknown_impact))

            connection.commit()

    def ctn_remove_service_kpi(self, id_ba: int, host: str, svc: str):
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_CONF,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(f"DELETE FROM mod_bam_kpi WHERE host_id={self.host[host]} AND service_id={self.service[svc]} AND id_ba={id_ba}")
            connection.commit()

    def ctn_add_boolean_kpi(self, id_ba: int, expression: str, impact_if: bool, critical_impact: int):
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_CONF,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    f"INSERT INTO mod_bam_boolean (name, expression, bool_state, activate) VALUES('bool test','{expression}', {int(impact_if)}, 1)")
                boolean_id = cursor.lastrowid
                cursor.execute(
                    f"INSERT INTO mod_bam_kpi (boolean_id,id_ba,drop_warning,drop_critical,drop_unknown,config_type) VALUES ({boolean_id},{id_ba},50,{critical_impact},75,'1')")

            connection.commit()
            return boolean_id

    def ctn_update_boolean_rule(self, boolean_id: int, expression: str):
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_CONF,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    f"UPDATE mod_bam_boolean SET expression='{expression}' WHERE boolean_id={boolean_id}")

            connection.commit()

    def ctn_add_ba_kpi(self, id_ba_src: int, id_ba_dest: int, critical_impact: int, warning_impact: int, unknown_impact: int):
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_CONF,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute("INSERT INTO mod_bam_kpi (id_indicator_ba,id_ba,drop_warning,drop_critical,drop_unknown,config_type) VALUES ({},{},{},{},{},'1')".format(
                    id_ba_src, id_ba_dest, warning_impact, critical_impact, unknown_impact))

            connection.commit()

    def ctn_add_relations_ba_timeperiods(self, id_ba:int, id_time_period:int):
        """
        Add a line in mod_bam_relations_ba_timeperiods table

        Args:
            id_ba: The ID of the BA.
            id_time_period: The ID of the timeperiod.
        """
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_CONF,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(f"INSERT INTO mod_bam_relations_ba_timeperiods (ba_id, tp_id) VALUES ({id_ba},{id_time_period})")

            connection.commit()
