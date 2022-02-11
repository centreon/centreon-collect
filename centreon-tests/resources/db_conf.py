#!/usr/bin/python3
from robot.api import logger
import sys
import pymysql.cursors

CONF_DIR = "/etc/centreon-engine"
ENGINE_HOME = "/var/lib/centreon-engine"

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
        self.clear_db()

    def clear_db(self):
        # Connect to the database
        connection = pymysql.connect(host='localhost',
                                     user='centreon',
                                     password='centreon',
                                     database='centreon',
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
                cursor.execute("ALTER TABLE host_service_relation AUTO_INCREMENT = 1")
                cursor.execute("DELETE FROM on_demand_macro_host")
                cursor.execute("DELETE FROM on_demand_macro_service")
                cursor.execute("DELETE FROM mod_bam")
                cursor.execute("ALTER TABLE mod_bam AUTO_INCREMENT = 1")
                cursor.execute("DELETE FROM mod_bam_kpi")
                cursor.execute("ALTER TABLE mod_bam_kpi AUTO_INCREMENT = 1")
            connection.commit()

    def create_conf_db(self):
        connection = pymysql.connect(host='localhost',
                                     user='centreon',
                                     password='centreon',
                                     database='centreon',
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                # Read a single record
                for i in range(self.instances):
                    cursor.execute("INSERT INTO nagios_server (name) VALUES (\"poller{}\")".format(i))
            connection.commit()

            # hosts
            hosts = self.hosts_count
            r = 0
            if hosts % self.instances > 0:
              r = 1
            v = int(hosts / self.instances) + r
            last = hosts - (self.instances - 1) * v
            with connection.cursor() as cursor:
                # Insertion of HOSTS COMMANDS
                for i in range(1, self.hosts_count + 1):
                    name = "checkh{}".format(i)
                    cursor.execute("INSERT INTO command (command_name,command_line) VALUES (\"{2}\",\"{0}/check.pl 0 {1}\")".format(ENGINE_HOME, i, name))
                    self.command[name] = cursor.lastrowid
                connection.commit()

                # Insertion of SERVICES COMMANDS
                for i in range(1, self.commands_per_poller_count * self.instances + 1):
                    name = "command_{}".format(i)
                    cursor.execute("INSERT INTO command (command_name,command_line) VALUES (\"{2}\",\"{0}/check.pl {1}\")".format(ENGINE_HOME, i, name))
                    self.command[name] = cursor.lastrowid

                # Two specific commands
                cursor.execute("INSERT INTO command (command_name,command_line) VALUES (\"notif\",\"{}/notif.pl\")".format(ENGINE_HOME))
                self.command["notif"] = cursor.lastrowid
                cursor.execute("INSERT INTO command (command_name,command_line) VALUES (\"test-notif\",\"{}/notif.pl\")".format(ENGINE_HOME))
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
                    cursor.execute("INSERT INTO host (host_name,host_alias,host_address,host_register,command_command_id,timeperiod_tp_id,host_snmp_community,host_snmp_version) VALUES (\"host_{0}\",\"host_{0}\",\"{1}.{2}.{3}.{4}\",'1',{5},1,'public','2c')".format(i, ipa, ipb, ipc, ipd, self.command["checkh{}".format(i)]))
                    self.host["host_{}".format(i)] = i
                connection.commit()

                sid = 1
                for hid in range(1, self.hosts_count + 1):
                  for i in range(1, self.services_per_host_count + 1):
                      cursor.execute("INSERT INTO service (service_description,service_id,service_max_check_attempts,service_normal_check_interval,service_retry_check_interval,service_register,command_command_id, service_active_checks_enabled, service_passive_checks_enabled) VALUES (\"service_{0}\",{0},3, 5, 1, '1', {1}, '1', '1')".format(sid, self.command[self.service_cmd[sid]]))
                      self.service["service_{}".format(sid)] = sid
                      cursor.execute("INSERT INTO host_service_relation (host_host_id,service_service_id) VALUES ({},{})".format(hid, sid))
                      sid += 1
                  cursor.execute("INSERT INTO on_demand_macro_host (host_macro_name,host_macro_value,host_host_id) VALUES ('$_HOSTKEY{0}$','VAL{0}',{0})".format(hid))
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
                        cursor.execute("INSERT INTO ns_host_relation VALUES ({},{})".format(inst, hid))
                        hid += 1
                    connection.commit()

    def create_ba_with_services(self, name:str, typ:str, svc:[(str,str)]):
        connection = pymysql.connect(host='localhost',
                                     user='centreon',
                                     password='centreon',
                                     database='centreon',
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            if typ == 'best':
                t = 1
            elif typ == 'worst':
                t = 2
            with connection.cursor() as cursor:
                cursor.execute("INSERT INTO mod_bam (name, state_source) VALUES ('{}',{})".format(name, t))
                id_ba = cursor.lastrowid
                for v in svc:
                    cursor.execute("INSERT INTO mod_bam_kpi (host_id,service_id,id_ba) VALUES ({},{},{})".format(self.host[v[0]], self.service[v[1]],id_ba))
            connection.commit()

