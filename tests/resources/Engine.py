from os import makedirs, chmod
from os.path import exists, dirname
from robot.api import logger
import db_conf
import random
import shutil
import sys
import time
import re
import stat

import grpc


CONF_DIR = "/etc/centreon-engine"
ENGINE_HOME = "/var/lib/centreon-engine"
SCRIPT_DIR: str = dirname(__file__) + "/engine-scripts/"


class EngineInstance:
    def __init__(self, count: int, hosts: int = 50, srv_by_host: int = 20):
        self.last_service_id = 0
        self.hosts = []
        self.services = []
        self.last_host_id = 0
        self.last_host_group_id = 0
        self.commands_count = 50
        self.instances = count
        self.service_cmd = {}
        self.build_configs(hosts, srv_by_host)

    def create_centengine(self, id: int, debug_level=0):
        return ("cfg_file={2}/config{0}/hosts.cfg\n"
                "cfg_file={2}/config{0}/services.cfg\n"
                "cfg_file={2}/config{0}/commands.cfg\n"
                "#cfg_file={2}/config{0}/contactgroups.cfg\n"
                "#cfg_file={2}/config{0}/contacts.cfg\n"
                "cfg_file={2}/config{0}/hostgroups.cfg\n"
                "cfg_file={2}/config{0}/timeperiods.cfg\n"
                "#cfg_file={2}/config{0}/escalations.cfg\n"
                "#cfg_file={2}/config{0}/dependencies.cfg\n"
                "cfg_file={2}/config{0}/connectors.cfg\n"
                "#cfg_file={2}/config{0}/meta_commands.cfg\n"
                "#cfg_file={2}/config{0}/meta_timeperiod.cfg\n"
                "#cfg_file={2}/config{0}/meta_host.cfg\n"
                "#cfg_file={2}/config{0}/meta_services.cfg\n"
                "broker_module=/usr/lib64/centreon-engine/externalcmd.so\n"
                "broker_module=/usr/lib64/nagios/cbmod.so /etc/centreon-broker/central-module{0}.json\n"
                "interval_length=60\n"
                "use_timezone=:Europe/Paris\n"
                "resource_file={2}/config{0}/resource.cfg\n"
                "log_file=/var/log/centreon-engine/config{0}/centengine.log\n"
                "status_file=/var/log/centreon-engine/config{0}/status.dat\n"
                "command_check_interval=1s\n"
                "command_file=/var/lib/centreon-engine/config{0}/rw/centengine.cmd\n"
                "state_retention_file=/var/log/centreon-engine/config{0}/retention.dat\n"
                "retention_update_interval=60\n"
                "sleep_time=0.2\n"
                "service_inter_check_delay_method=s\n"
                "service_interleave_factor=s\n"
                "max_concurrent_checks=400\n"
                "max_service_check_spread=5\n"
                "check_result_reaper_frequency=5\n"
                "low_service_flap_threshold=25.0\n"
                "high_service_flap_threshold=50.0\n"
                "low_host_flap_threshold=25.0\n"
                "high_host_flap_threshold=50.0\n"
                "service_check_timeout=10\n"
                "host_check_timeout=12\n"
                "event_handler_timeout=30\n"
                "notification_timeout=30\n"
                "ocsp_timeout=5\n"
                "ochp_timeout=5\n"
                "perfdata_timeout=5\n"
                "date_format=euro\n"
                "illegal_object_name_chars=~!$%^&*\"|'<>?,()=\n"
                "illegal_macro_output_chars=`~$^&\"|'<>\n"
                "admin_email=titus@bidibule.com\n"
                "admin_pager=admin\n"
                "event_broker_options=-1\n"
                "cached_host_check_horizon=60\n"
                "debug_file=/var/log/centreon-engine/config{0}/centengine.debug\n"
                "debug_level={1}\n"
                "debug_verbosity=2\n"
                "log_pid=1\n"
                "macros_filter=KEY80,KEY81,KEY82,KEY83,KEY84\n"
                "enable_macros_filter=0\n"
                "rpc_port=50001\n"
                "postpone_notification_to_timeperiod=0\n"
                "instance_heartbeat_interval=30\n"
                "enable_notifications=1\n"
                "execute_service_checks=1\n"
                "accept_passive_service_checks=1\n"
                "enable_event_handlers=1\n"
                "check_external_commands=1\n"
                "use_retained_program_state=1\n"
                "use_retained_scheduling_info=1\n"
                "use_syslog=0\n"
                "log_notifications=1\n"
                "log_service_retries=1\n"
                "log_host_retries=1\n"
                "log_event_handlers=1\n"
                "log_external_commands=1\n"
                "log_v2_enabled=1\n"
                "log_legacy_enabled=0\n"
                "log_v2_logger=file\n"
                "log_level_functions=trace\n"
                "log_level_config=info\n"
                "log_level_events=info\n"
                "log_level_checks=info\n"
                "log_level_notifications=info\n"
                "log_level_eventbroker=info\n"
                "log_level_external_command=trace\n"
                "log_level_commands=info\n"
                "log_level_downtimes=info\n"
                "log_level_comments=info\n"
                "log_level_macros=info\n"
                "log_level_process=info\n"
                "log_level_runtime=info\n"
                "soft_state_dependencies=0\n"
                "obsess_over_services=0\n"
                "process_performance_data=0\n"
                "check_for_orphaned_services=0\n"
                "check_for_orphaned_hosts=0\n"
                "check_service_freshness=1\n"
                "enable_flap_detection=0\n").format(id, debug_level, CONF_DIR)

    def create_host(self):
        self.last_host_id += 1
        hid = self.last_host_id
        a = hid % 255
        q = hid // 255
        b = q % 255
        q //= 255
        c = q % 255
        q //= 255
        d = q % 255

        retval = {
            "config": "define host {{\n" "    host_name                      host_{0}\n    alias                          "
                      "host_{0}\n    address                        {1}.{2}.{3}.{4}\n    check_command                "
                      "  checkh{0}\n    check_period                   24x7\n    register                       1\n    "
                      "_KEY{0}                      VAL{0}\n    _SNMPCOMMUNITY                 public\n    "
                      "_SNMPVERSION                   2c\n    _HOST_ID                       {0}\n}}\n".format(
                          hid, a, b, c, d),
            "hid": hid}
        return retval

    def create_service(self, host_id: int, cmd_ids: int):
        self.last_service_id += 1
        service_id = self.last_service_id
        command_id = random.randint(cmd_ids[0], cmd_ids[1])
        self.service_cmd[service_id] = "command_{}".format(command_id)

        retval = """define service {{
    host_name                       host_{0}
    service_description             service_{1}
    _SERVICE_ID                     {1}
    check_command                   {2}
    check_period                    24x7
    max_check_attempts              3
    check_interval                  5
    retry_interval                  5
    register                        1
    active_checks_enabled           1
    passive_checks_enabled          1
}}
""".format(
            host_id, service_id, self.service_cmd[service_id])
        return retval

    def create_bam_timeperiod(self):
        retval = """define timeperiod {
  timeperiod_name                centreon-bam-timeperiod
  alias                          centreon-bam-timeperiod
  sunday                         00:00-24:00
  monday                         00:00-24:00
  tuesday                        00:00-24:00
  wednesday                      00:00-24:00
  thursday                       00:00-24:00
  friday                         00:00-24:00
  saturday                       00:00-24:00
}
"""
        config_dir = "{}/config0".format(CONF_DIR)
        ff = open(config_dir + "/centreon-bam-timeperiod.cfg", "a+")
        ff.write(retval)
        ff.close()

    def create_bam_command(self):
        retval = """define command {
  command_name                   centreon-bam-check
  command_line                   $CENTREONPLUGINS$/check_centreon_bam -i $ARG1$
                }

define command {
  command_name                   centreon-bam-host-alive
  command_line                   /usr/lib64/nagios/plugins//check_ping -H $HOSTADDRESS$ -w 3000.0,80% -c 5000.0,100% -p 1
}
"""
        config_dir = "{}/config0".format(CONF_DIR)
        ff = open(config_dir + "/centreon-bam-command.cfg", "a+")
        ff.write(retval)
        ff.close()

    def create_bam_host(self):
        self.last_host_id += 1
        print(self.last_host_id)
        host_id = self.last_host_id
        retval = """define host {{
  host_name                      _Module_BAM_1
  alias                          Centreon BAM Module
  address                        127.0.0.1
  check_command                  centreon-bam-host-alive
  max_check_attempts             3
  check_interval                 1
  active_checks_enabled          0
  passive_checks_enabled         0
  check_period                   centreon-bam-timeperiod
  notification_period            centreon-bam-timeperiod
  notification_options           d
  _HOST_ID                       {}
  register                       1
}}
""".format(host_id)
        config_dir = "{}/config0".format(CONF_DIR)
        ff = open(config_dir + "/centreon-bam-host.cfg", "a+")
        ff.write(retval)
        ff.close()
        return host_id

    def create_bam_service(self, name: str, display_name: str, host_name: str, check_command: str):
        self.last_service_id += 1
        service_id = self.last_service_id
        retval = """define service {{
    service_description             {1}
    display_name                    {2}
    host_name                       {0}
    check_command                   {3}
    max_check_attempts              1
    normal_check_interval           5
    retry_check_interval            1
    active_checks_enabled           1
    passive_checks_enabled          1
    check_period                    centreon-bam-timeperiod
    notification_period             24x7
    notifications_enabled           0
    event_handler_enabled           0
    _SERVICE_ID                     {4}
    register                        1
}}
""".format(host_name, name, display_name, check_command, service_id)
        config_dir = "{}/config0".format(CONF_DIR)
        ff = open(config_dir + "/centreon-bam-services.cfg", "a+")
        ff.write(retval)
        ff.close()
        return service_id

    @staticmethod
    def create_command(cmd):
        retval: str
        if cmd % 2 == 0:
            retval = """define command {{
    command_name                    command_{1}
    command_line                    {0}/check.pl {1}
    connector                       Perl Connector
}}
""".format(ENGINE_HOME, cmd)
        else:
            retval = """define command {{
    command_name                    command_{1}
    command_line                    {0}/check.pl {1}
}}
""".format(ENGINE_HOME, cmd)
        return retval

    @staticmethod
    def create_host_group(id, mbs):
        retval = """define hostgroup {{
    hostgroup_id                    {0}
    hostgroup_name                  hostgroup_{0}
    alias                           hostgroup_{0}
    members                         {1}
}}
""".format(id, ",".join(mbs))
        logger.console(retval)
        return retval

    @staticmethod
    def create_service_group(id, mbs):
        retval = """define servicegroup {{
    servicegroup_id                    {0}
    servicegroup_name                  servicegroup_{0}
    alias                           servicegroup_{0}
    members                         {1}
}}
""".format(id, ",".join(mbs))
        logger.console(retval)
        return retval

    @staticmethod
    def create_severities(poller: int, nb: int, offset: int):
        config_file = "{}/config{}/severities.cfg".format(CONF_DIR, poller)
        ff = open(config_file, "w+")
        content = ""
        typ = ["service", "host"]
        for i in range(nb):
            level = i % 5 + 1
            content += """define severity {{
    id                     {0}
    name                   severity{3}
    level                  {1}
    icon_id                {2}
    type                   {4}
}}
""".format(i + 1, level, 6 - level, i + offset, typ[i % 2])
        ff.write(content)
        ff.close()

    @staticmethod
    def create_template_file(poller: int, typ: str, what: str, ids):
        config_file = "{}/config{}/{}Templates.cfg".format(
            CONF_DIR, poller, typ)
        ff = open(config_file, "w+")
        content = ""
        idx = 1
        for i in ids:
            content += """define {} {{
name                   {}_template_{}
{}               {}
register               0
active_checks_enabled  1
passive_checks_enabled 1
}}
""".format(typ, typ, idx, what, i)
            idx += 1
        ff.write(content)
        ff.close()

    @staticmethod
    def create_tags(poller: int, nb: int, offset: int):
        tt = ["servicegroup", "hostgroup", "servicecategory", "hostcategory"]

        config_file = "{}/config{}/tags.cfg".format(CONF_DIR, poller)
        ff = open(config_file, "w+")
        content = ""
        tid = 0
        for i in range(nb):
            if i % 4 == 0:
                tid += 1
            typ = tt[i % 4]
            content += """define tag {{
    id                     {0}
    name                   tag{2}
    type                   {1}
}}
""".format(tid, typ, i + offset)
        ff.write(content)
        ff.close()

    def build_configs(self, hosts: int, services_by_host: int, debug_level=0):
        if exists(CONF_DIR):
            shutil.rmtree(CONF_DIR)
        r = 0
        if hosts % self.instances > 0:
            r = 1
        v = int(hosts / self.instances) + r
        last = hosts - (self.instances - 1) * v
        for inst in range(self.instances):
            if v < hosts:
                nb_hosts = v
                hosts -= v
            else:
                nb_hosts = hosts
                hosts = 0

            config_dir = "{}/config{}".format(CONF_DIR, inst)
            makedirs(config_dir)
            f = open(config_dir + "/centengine.cfg", "w+")
            bb = self.create_centengine(inst, debug_level=debug_level)
            f.write(bb)
            f.close()

            f = open(config_dir + "/hosts.cfg", "w")
            ff = open(config_dir + "/services.cfg", "w")
            for i in range(1, nb_hosts + 1):
                h = self.create_host()
                f.write(h["config"])
                self.hosts.append("host_{}".format(h["hid"]))
                for j in range(1, services_by_host + 1):
                    ff.write(self.create_service(h["hid"],
                                                 (inst * self.commands_count + 1, (inst + 1) * self.commands_count)))
                    self.services.append("service_{}".format(h["hid"]))
            ff.close()
            f.close()

            f = open(config_dir + "/commands.cfg", "w")
            for i in range(inst * self.commands_count + 1, (inst + 1) * self.commands_count + 1):
                f.write(self.create_command(i))
            for i in range(self.last_host_id):
                f.write("""define command {{
    command_name                    checkh{1}
    command_line                    {0}/check.pl 0 {1}
}}
""".format(ENGINE_HOME, i + 1))
            f.write("""define command {{
    command_name                    notif
    command_line                    {0}/notif.pl
}}
define command {{
    command_name                    test-notif
    command_line                    {0}/notif.pl
}}
""".format(ENGINE_HOME))
            f.close()
            f = open(config_dir + "/connectors.cfg", "w")
            f.write("""define connector {
    connector_name                 Perl Connector
    connector_line                 /usr/lib64/centreon-connector/centreon_connector_perl
}

define connector {
    connector_name                 SSH Connector
    connector_line                 /usr/lib64/centreon-connector/centreon_connector_ssh
}
""")
            f.close()
            f = open(config_dir + "/resource.cfg", "w")
            f.close()
            f = open(config_dir + "/timeperiods.cfg", "w")
            f.write("""define timeperiod {
    name                           24x7
    timeperiod_name                24x7
    alias                          24_Hours_A_Day,_7_Days_A_Week
    sunday                         00:00-24:00
    monday                         00:00-24:00
    tuesday                        00:00-24:00
    wednesday                      00:00-24:00
    thursday                       00:00-24:00
    friday                         00:00-24:00
    saturday                       00:00-24:00
}
define timeperiod {
    name                           24x6
    timeperiod_name                24x6
    alias                          24_Hours_A_Day,_7_Days_A_Week
    sunday                         00:00-24:00
    monday                         00:00-24:00
    tuesday                        00:00-24:00
    wednesday                      00:00-24:00
    thursday                       00:00-24:00
    friday                         00:00-24:00
    saturday                       00:00-24:00
}
""")
            f.close()
            f = open(config_dir + "/hostgroups.cfg", "w")
            f.close()
            if not exists(ENGINE_HOME):
                makedirs(ENGINE_HOME)
            for file in ["check.pl", "notif.pl"]:
                shutil.copyfile("{0}/{1}".format(SCRIPT_DIR, file),
                                "{0}/{1}".format(ENGINE_HOME, file))
                chmod("{0}/{1}".format(ENGINE_HOME, file),
                      stat.S_IRWXU | stat.S_IRGRP | stat.S_IXGRP)
            if not exists(ENGINE_HOME + "/config{}/rw".format(inst)):
                makedirs(ENGINE_HOME + "/config{}/rw".format(inst))

    def centengine_conf_add_bam(self):
        config_dir = "{}/config0".format(CONF_DIR)
        f = open(config_dir + "/centengine.cfg", "r")
        lines = f.readlines()
        f.close
        lines_to_prep = ["cfg_file=/etc/centreon-engine/config0/centreon-bam-command.cfg\n", "cfg_file=/etc/centreon-engine/config0/centreon-bam-timeperiod.cfg\n",
                         "cfg_file=/etc/centreon-engine/config0/centreon-bam-host.cfg\n", "cfg_file=/etc/centreon-engine/config0/centreon-bam-services.cfg\n"]
        f = open(config_dir + "/centengine.cfg", "w")
        f.writelines(lines_to_prep)
        f.writelines(lines)
        f.close()


##
# @brief Configure all the necessary files for num instances of centengine.
#
# @param num: How many engine configurations to start
#
def config_engine(num: int, hosts: int = 50, srv_by_host: int = 20):
    global engine
    engine = EngineInstance(num, hosts, srv_by_host)


##
# @brief Accessor to the number of centengine configurations
#
def get_engines_count():
    return engine.instances


##
# @brief Function to change a value in the centengine.cfg for the config idx.
#
# @param idx index of the configuration (from 0)
# @param key the key to change the value.
# @param value the new value to set to the key variable.
#
def engine_config_set_value(idx: int, key: str, value: str, force: bool = False):
    filename = "/etc/centreon-engine/config{}/centengine.cfg".format(idx)
    f = open(filename, "r")
    lines = f.readlines()
    f.close()

    if force:
        lines.append("{}={}\n".format(key, value))
    else:
        for i in range(len(lines)):
            if lines[i].startswith(key + "="):
                lines[i] = "{}={}\n".format(key, value)

    f = open(filename, "w")
    f.writelines(lines)
    f.close()


##
# @brief Function to change a value in the services.cfg for the config idx.
#
# @param idx index of the configuration (from 0)
# @param desc service description of the service to modify.
# @param key the key to change the value.
# @param value the new value to set to the key variable.
#
def engine_config_set_value_in_services(idx: int, desc: str, key: str, value: str):
    filename = "/etc/centreon-engine/config{}/services.cfg".format(idx)
    f = open(filename, "r")
    lines = f.readlines()
    f.close()

    r = re.compile(r"^\s*service_description\s+" + desc + "\s*$")
    for i in range(len(lines)):
        if r.match(lines[i]):
            lines.insert(i + 1, "    {}              {}\n".format(key, value))

    f = open(filename, "w")
    f.writelines(lines)
    f.close()


##
# @brief Function to change a value in the hosts.cfg for the config idx.
#
# @param idx index of the configuration (from 0)
# @param desc host name of the host to modify.
# @param key the key to change the value.
# @param value the new value to set to the key variable.
#
def engine_config_set_value_in_hosts(idx: int, desc: str, key: str, value: str):
    filename = "/etc/centreon-engine/config{}/hosts.cfg".format(idx)
    f = open(filename, "r")
    lines = f.readlines()
    f.close()

    r = re.compile(r"^\s*host_name\s+" + desc + "\s*$")
    for i in range(len(lines)):
        if r.match(lines[i]):
            lines.insert(i + 1, "    {}              {}\n".format(key, value))

    f = open(filename, "w")
    f.writelines(lines)
    f.close()


def add_host_group(index: int, id_host_group: int, members: list):
    mbs = [l for l in members if l in engine.hosts]
    f = open("/etc/centreon-engine/config{}/hostgroups.cfg".format(index), "a+")
    logger.console(mbs)
    f.write(engine.create_host_group(id_host_group, mbs))
    f.close()


def rename_host_group(index: int, id_host_group: int, name: str, members: list):
    mbs = [l for l in members if l in engine.hosts]
    f = open("/etc/centreon-engine/config{}/hostgroups.cfg".format(index), "w")
    logger.console(mbs)
    f.write("""define hostgroup {{
    hostgroup_id                    {0}
    hostgroup_name                  hostgroup_{1}
    alias                           hostgroup_{1}
    members                         {2}
}}
""".format(id_host_group, name, ",".join(mbs)))
    f.close()


def add_service_group(index: int, id_service_group: int, members: list):
    f = open("/etc/centreon-engine/config{}/servicegroups.cfg".format(index), "a+")
    logger.console(members)
    f.write(engine.create_service_group(id_service_group, members))
    f.close()


def create_service(index: int, host_id: int, cmd_id: int):
    f = open("/etc/centreon-engine/config{}/services.cfg".format(index), "a+")
    svc = engine.create_service(host_id, [1, cmd_id])
    lst = svc.split('\n')
    good = [l for l in lst if "_SERVICE_ID" in l][0]
    m = re.search(r"_SERVICE_ID\s+([^\s]*)$", good)
    if m is not None:
        retval = int(m.group(1))
    else:
        raise Exception(
            "Impossible to get the service id from '{}'".format(good))
        m = 0
    f.write(svc)
    f.close()
    return retval


def engine_log_duplicate(result: list):
    dup = True
    for i in result:
        if (i[0] % 2) != 0:
            dup = False
    return dup


def clone_engine_config_to_db():
    global dbconf
    dbconf = db_conf.DbConf(engine)
    dbconf.create_conf_db()


def add_bam_config_to_engine():
    global dbconf
    dbconf.init_bam()


def create_ba_with_services(name: str, typ: str, svc: list, dt_policy="inherit"):
    global dbconf
    dbconf.create_ba_with_services(name, typ, svc, dt_policy)


def get_command_id(service: int):
    global engine
    global dbconf
    cmd_name = engine.service_cmd[service]
    return dbconf.command[cmd_name]


def process_service_check_result(hst: str, svc: str, state: int, output: str, config='config0'):
    now = int(time.time())
    cmd = "[{}] PROCESS_SERVICE_CHECK_RESULT;{};{};{};{}\n".format(
        now, hst, svc, state, output)
    f = open(f"/var/lib/centreon-engine/{config}/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def change_normal_svc_check_interval(hst: str, svc: str, check_interval: int):
    now = int(time.time())
    cmd = "[{}] CHANGE_NORMAL_SVC_CHECK_INTERVAL;{};{};{}\n".format(
        now, hst, svc, check_interval)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def change_normal_host_check_interval(hst: str, check_interval: int):
    now = int(time.time())
    cmd = "[{}] CHANGE_NORMAL_HOST_CHECK_INTERVAL;{};{}\n".format(
        now, hst, check_interval)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def change_retry_svc_check_interval(hst: str, svc: str, retry_interval: int):
    now = int(time.time())
    cmd = "[{}] CHANGE_RETRY_SVC_CHECK_INTERVAL;{};{};{}\n".format(
        now, hst, svc, retry_interval)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def change_retry_host_check_interval(hst: str, retry_interval: int):
    now = int(time.time())
    cmd = "[{}] CHANGE_RETRY_HOST_CHECK_INTERVAL;{};{}\n".format(
        now, hst, retry_interval)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def change_max_svc_check_attempts(hst: str, svc: str, max_check_attempts: int):
    now = int(time.time())
    cmd = "[{}] CHANGE_MAX_SVC_CHECK_ATTEMPTS;{};{};{}\n".format(
        now, hst, svc, max_check_attempts)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def change_max_host_check_attempts(hst: str, max_check_attempts: int):
    now = int(time.time())
    cmd = "[{}] CHANGE_MAX_HOST_CHECK_ATTEMPTS;{};{}\n".format(
        now, hst, max_check_attempts)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def change_host_check_command(hst: str, Check_Command: str):
    now = int(time.time())
    cmd = "[{}] CHANGE_HOST_CHECK_COMMAND;{};{}\n".format(
        now, hst, Check_Command)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def change_host_check_timeperiod(hst: str, check_timeperiod: str):
    now = int(time.time())
    cmd = "[{}] CHANGE_HOST_CHECK_TIMEPERIOD;{};{}\n".format(
        now, hst, check_timeperiod)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def change_host_notification_timeperiod(hst: str, notification_timeperiod: str):
    now = int(time.time())
    cmd = "[{}] CHANGE_HOST_NOTIFICATION_TIMEPERIOD;{};{}\n".format(
        now, hst, notification_timeperiod)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def change_svc_check_timeperiod(hst: str, svc: str, check_timeperiod: str):
    now = int(time.time())
    cmd = "[{}] CHANGE_SVC_CHECK_TIMEPERIOD;{};{};{}\n".format(
        now, hst, svc, check_timeperiod)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def change_svc_notification_timeperiod(hst: str, svc: str, notification_timeperiod: str):
    now = int(time.time())
    cmd = "[{}] CHANGE_SVC_NOTIFICATION_TIMEPERIOD;{};{};{}\n".format(
        now, hst, svc, notification_timeperiod)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def disable_host_and_child_notifications(hst: str):
    now = int(time.time())
    cmd = "[{}] DISABLE_HOST_AND_CHILD_NOTIFICATIONS;{}\n".format(
        now, hst)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def enable_host_and_child_notifications(hst: str):
    now = int(time.time())
    cmd = "[{}] ENABLE_HOST_AND_CHILD_NOTIFICATIONS;{}\n".format(
        now, hst)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def disable_host_check(hst: str):
    now = int(time.time())
    cmd = "[{}] DISABLE_HOST_CHECK;{}\n".format(
        now, hst)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def enable_host_check(hst: str):
    now = int(time.time())
    cmd = "[{}] ENABLE_HOST_CHECK;{}\n".format(
        now, hst)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def disable_host_event_handler(hst: str):
    now = int(time.time())
    cmd = "[{}] DISABLE_HOST_EVENT_HANDLER;{}\n".format(
        now, hst)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def enable_host_event_handler(hst: str):
    now = int(time.time())
    cmd = "[{}] ENABLE_HOST_EVENT_HANDLER;{}\n".format(
        now, hst)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def disable_host_flap_detection(hst: str):
    now = int(time.time())
    cmd = "[{}] DISABLE_HOST_FLAP_DETECTION;{}\n".format(
        now, hst)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def enable_host_flap_detection(hst: str):
    now = int(time.time())
    cmd = "[{}] ENABLE_HOST_FLAP_DETECTION;{}\n".format(
        now, hst)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def disable_host_notifications(hst: str):
    now = int(time.time())
    cmd = "[{}] DISABLE_HOST_NOTIFICATIONS;{}\n".format(
        now, hst)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def enable_host_notifications(hst: str):
    now = int(time.time())
    cmd = "[{}] ENABLE_HOST_NOTIFICATIONS;{}\n".format(
        now, hst)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def disable_host_svc_checks(hst: str):
    now = int(time.time())
    cmd = "[{}] DISABLE_HOST_SVC_CHECKS;{}\n".format(
        now, hst)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def enable_host_svc_checks(hst: str):
    now = int(time.time())
    cmd = "[{}] ENABLE_HOST_SVC_CHECKS;{}\n".format(
        now, hst)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def disable_host_svc_notifications(hst: str):
    now = int(time.time())
    cmd = "[{}] DISABLE_HOST_SVC_NOTIFICATIONS;{}\n".format(
        now, hst)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def enable_host_svc_notifications(hst: str):
    now = int(time.time())
    cmd = "[{}] ENABLE_HOST_SVC_NOTIFICATIONS;{}\n".format(
        now, hst)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def disable_passive_host_checks(hst: str):
    now = int(time.time())
    cmd = "[{}] DISABLE_PASSIVE_HOST_CHECKS;{}\n".format(
        now, hst)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def enable_passive_host_checks(hst: str):
    now = int(time.time())
    cmd = "[{}] ENABLE_PASSIVE_HOST_CHECKS;{}\n".format(
        now, hst)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def disable_passive_svc_checks(hst: str, svc: str):
    now = int(time.time())
    cmd = "[{}] DISABLE_PASSIVE_SVC_CHECKS;{};{}\n".format(
        now, hst, svc)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def enable_passive_svc_checks(hst: str, svc: str):
    now = int(time.time())
    cmd = "[{}] ENABLE_PASSIVE_SVC_CHECKS;{};{}\n".format(
        now, hst, svc)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def start_obsessing_over_host(hst: str):
    now = int(time.time())
    cmd = "[{}] START_OBSESSING_OVER_HOST;{}\n".format(
        now, hst)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def stop_obsessing_over_host(hst: str):
    now = int(time.time())
    cmd = "[{}] STOP_OBSESSING_OVER_HOST;{}\n".format(
        now, hst)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def start_obsessing_over_svc(hst: str, svc: str):
    now = int(time.time())
    cmd = "[{}] START_OBSESSING_OVER_SVC;{};{}\n".format(
        now, hst, svc)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def stop_obsessing_over_svc(hst: str, svc: str):
    now = int(time.time())
    cmd = "[{}] STOP_OBSESSING_OVER_SVC;{};{}\n".format(
        now, hst, svc)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def service_ext_commands(hst: str, svc: str, state: int, output: str):
    now = int(time.time())
    cmd = "[{}] PROCESS_SERVICE_CHECK_RESULT;{};{};{};{}\n".format(
        now, hst, svc, state, output)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def process_host_check_result(hst: str, state: int, output: str):
    now = int(time.time())
    cmd = "[{}] PROCESS_HOST_CHECK_RESULT;{};{};{}\n".format(
        now, hst, state, output)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def schedule_service_downtime(hst: str, svc: str, duration: int):
    now = int(time.time())
    cmd = "[{2}] SCHEDULE_SVC_DOWNTIME;{0};{1};{2};{3};1;0;{4};admin;Downtime set by admin".format(
        hst, svc, now, now + duration, duration)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def schedule_host_downtime(poller: int, hst: str, duration: int):
    now = int(time.time())
    cmd1 = "[{1}] SCHEDULE_HOST_DOWNTIME;{0};{1};{2};1;0;{3};admin;Downtime set by admin\n".format(
        hst, now, now + duration, duration)
    cmd2 = "[{1}] SCHEDULE_HOST_SVC_DOWNTIME;{0};{1};{2};1;0;{3};admin;Downtime set by admin\n".format(
        hst, now, now + duration, duration)
    f = open("/var/lib/centreon-engine/config{}/rw/centengine.cmd".format(poller), "w")
    f.write(cmd1)
    f.write(cmd2)
    f.close()


def delete_host_downtimes(poller: int, hst: str):
    now = int(time.time())
    cmd = "[{}] DEL_HOST_DOWNTIME_FULL;{};;;;;;;;\n".format(now, hst)
    f = open("/var/lib/centreon-engine/config{}/rw/centengine.cmd".format(poller), "w")
    f.write(cmd)
    f.close()


def schedule_forced_svc_check(host: str, svc: str, pipe: str = "/var/lib/centreon-engine/rw/centengine.cmd"):
    now = int(time.time())
    f = open(pipe, "w")
    cmd = "[{2}] SCHEDULE_FORCED_SVC_CHECK;{0};{1};{2}".format(host, svc, now)
    f.write(cmd)
    f.close()
    time.sleep(0.05)


def schedule_forced_host_check(host: str, pipe: str = "/var/lib/centreon-engine/rw/centengine.cmd"):
    now = int(time.time())
    f = open(pipe, "w")
    cmd = "[{1}] SCHEDULE_FORCED_HOST_CHECK;{0};{1}".format(host, now)
    f.write(cmd)
    f.close()
    time.sleep(0.05)


def create_severities_file(poller: int, nb: int, offset: int = 1):
    engine.create_severities(poller, nb, offset)


def create_template_file(poller: int, typ: str, what: str, ids: list):
    engine.create_template_file(poller, typ, what, ids)


def create_template_file(poller: int, typ: str, what: str, ids: list):
    engine.create_template_file(poller, typ, what, ids)


def create_tags_file(poller: int, nb: int, offset: int = 1):
    engine.create_tags(poller, nb, offset)


def config_engine_add_cfg_file(poller: int, cfg: str):
    ff = open("{}/config{}/centengine.cfg".format(CONF_DIR, poller), "r")
    lines = ff.readlines()
    ff.close()
    r = re.compile(r"^\s*cfg_file=")
    for i in range(len(lines)):
        if r.match(lines[i]):
            lines.insert(
                i, "cfg_file={}/config{}/{}\n".format(CONF_DIR, poller, cfg))
            break
    ff = open("{}/config{}/centengine.cfg".format(CONF_DIR, poller), "w+")
    ff.writelines(lines)
    ff.close()


def add_severity_to_services(poller: int, severity_id: int, svc_lst):
    ff = open("{}/config{}/services.cfg".format(CONF_DIR, poller), "r")
    lines = ff.readlines()
    ff.close()
    r = re.compile(r"^\s*_SERVICE_ID\s*(\d+)$")
    for i in range(len(lines)):
        m = r.match(lines[i])
        if m is not None and m.group(1) in svc_lst:
            lines.insert(
                i + 1, "    severity_id                     {}\n".format(severity_id))

    ff = open("{}/config{}/services.cfg".format(CONF_DIR, poller), "w")
    ff.writelines(lines)
    ff.close()


def add_severity_to_hosts(poller: int, severity_id: int, svc_lst):
    ff = open("{}/config{}/hosts.cfg".format(CONF_DIR, poller), "r")
    lines = ff.readlines()
    ff.close()
    r = re.compile(r"^\s*_HOST_ID\s*(\d+)$")
    for i in range(len(lines)):
        m = r.match(lines[i])
        if m is not None and m.group(1) in svc_lst:
            lines.insert(
                i + 1, "    severity_id                     {}\n".format(severity_id))

    ff = open("{}/config{}/hosts.cfg".format(CONF_DIR, poller), "w")
    ff.writelines(lines)
    ff.close()


def add_template_to_services(poller: int, tmpl: str, svc_lst):
    ff = open("{}/config{}/services.cfg".format(CONF_DIR, poller), "r")
    lines = ff.readlines()
    ff.close()
    r = re.compile(r"^\s*_SERVICE_ID\s*(\d+)$")
    for i in range(len(lines)):
        m = r.match(lines[i])
        if m is not None and m.group(1) in svc_lst:
            lines.insert(
                i + 1, "    use                     {}\n".format(tmpl))

    ff = open("{}/config{}/services.cfg".format(CONF_DIR, poller), "w")
    ff.writelines(lines)
    ff.close()


def add_tags_to_services(poller: int, type: str, tag_id: str, svc_lst):
    ff = open("{}/config{}/services.cfg".format(CONF_DIR, poller), "r")
    lines = ff.readlines()
    ff.close()
    r = re.compile(r"^\s*_SERVICE_ID\s*(\d+)$")
    for i in range(len(lines)):
        m = r.match(lines[i])
        if m is not None and m.group(1) in svc_lst:
            lines.insert(
                i + 1, "    {}                     {}\n".format(type, tag_id))
    ff = open("{}/config{}/services.cfg".format(CONF_DIR, poller), "w")
    ff.writelines(lines)
    ff.close()


def remove_severities_from_services(poller: int):
    ff = open("{}/config{}/services.cfg".format(CONF_DIR, poller), "r")
    lines = ff.readlines()
    ff.close()
    r = re.compile(r"^\s*severity_id\s*\d+$")
    out = [l for l in lines if not r.match(l)]
    ff = open("{}/config{}/services.cfg".format(CONF_DIR, poller), "w")
    ff.writelines(out)
    ff.close()


def remove_severities_from_hosts(poller: int):
    ff = open("{}/config{}/hosts.cfg".format(CONF_DIR, poller), "r")
    lines = ff.readlines()
    ff.close()
    r = re.compile(r"^\s*severity_id\s*\d+$")
    out = [l for l in lines if not r.match(l)]
    ff = open("{}/config{}/hosts.cfg".format(CONF_DIR, poller), "w")
    ff.writelines(out)
    ff.close()

##
# @brief Function that search a check, retrieve command index and return check result
# then it searchs the string "connector::run: id=1090", and then search "connector::_recv_query_execute: id=1090,"
# and return this line
#
# @param debug_file_path path of the debug log file
# @param str_to_search string after which we will start connector::run search
#


def check_search(debug_file_path: str, str_to_search):
    with open(debug_file_path, 'r') as f:
        lines = f.readlines()
        for first_ind in range(len(lines)):
            find_index = lines[first_ind].find(str_to_search + ' ')
            if (find_index > 0):
                for second_ind in range(first_ind, len(lines)):
                    # search cmd_id
                    m = re.search(
                        r"^\[\d+\]\s+\[\d+\]\s+connector::run:\s+id=(\d+)", lines[second_ind])
                    if m is not None:
                        cmd_id = m.group(1)
                        r_query_execute = r"^\[\d+\]\s+\[\d+\]\s+connector::_recv_query_execute:\s+id=" + \
                            cmd_id + ",\s+(\S[\s\S]+)$"
                        for third_ind in range(second_ind, len(lines)):
                            m = re.match(
                                r_query_execute, lines[third_ind])
                            if m is not None:
                                return m.group(1)
                        return "_recv_query_execute not found" + r_query_execute
                return "connector::run not found"
        return "check_search don t find " + str_to_search


def add_tags_to_hosts(poller: int, type: str, tag_id: str, hst_lst):
    ff = open("{}/config{}/hosts.cfg".format(CONF_DIR, poller), "r")
    lines = ff.readlines()
    ff.close()
    r = re.compile(r"^\s*_HOST_ID\s*(\d+)$")
    for i in range(len(lines)):
        m = r.match(lines[i])
        if m is not None and m.group(1) in hst_lst:
            lines.insert(
                i + 1, "    {}                     {}\n".format(type, tag_id))

    ff = open("{}/config{}/hosts.cfg".format(CONF_DIR, poller), "w")
    ff.writelines(lines)
    ff.close()


def remove_tags_from_services(poller: int, type: str):
    ff = open("{}/config{}/services.cfg".format(CONF_DIR, poller), "r")
    lines = ff.readlines()
    ff.close()
    r = re.compile(r"^\s*" + type + r"\s*[0-9,]+$")
    lines = [l for l in lines if not r.match(l)]
    ff = open("{}/config{}/services.cfg".format(CONF_DIR, poller), "w")
    ff.writelines(lines)
    ff.close()


def remove_tags_from_hosts(poller: int, type: str):
    ff = open("{}/config{}/hosts.cfg".format(CONF_DIR, poller), "r")
    lines = ff.readlines()
    ff.close()
    r = re.compile(r"^\s*" + type + r"\s*[0-9,]+$")
    lines = [l for l in lines if not r.match(l)]
    ff = open("{}/config{}/hosts.cfg".format(CONF_DIR, poller), "w")
    ff.writelines(lines)
    ff.close()


def add_template_to_services(poller: int, tmpl: str, svc_lst):
    ff = open("{}/config{}/services.cfg".format(CONF_DIR, poller), "r")
    lines = ff.readlines()
    ff.close()
    r = re.compile(r"^\s*_SERVICE_ID\s*(\d+)$")
    for i in range(len(lines)):
        m = r.match(lines[i])
        if m is not None and m.group(1) in svc_lst:
            lines.insert(
                i + 1, "    use                     {}\n".format(tmpl))

    ff = open("{}/config{}/services.cfg".format(CONF_DIR, poller), "w")
    ff.writelines(lines)
    ff.close()


def add_template_to_hosts(poller: int, tmpl: str, hst_lst):
    ff = open("{}/config{}/hosts.cfg".format(CONF_DIR, poller), "r")
    lines = ff.readlines()
    ff.close()
    r = re.compile(r"^\s*_HOST_ID\s*(\d+)$")
    for i in range(len(lines)):
        m = r.match(lines[i])
        if m is not None and m.group(1) in hst_lst:
            lines.insert(
                i + 1, "    use                     {}\n".format(tmpl))

    ff = open("{}/config{}/hosts.cfg".format(CONF_DIR, poller), "w")
    ff.writelines(lines)
    ff.close()


def config_engine_remove_cfg_file(poller: int, fic: str):
    ff = open("{}/config{}/centengine.cfg".format(CONF_DIR, poller), "r")
    lines = ff.readlines()
    ff.close()
    r = re.compile(
        r"^\s*cfg_file=/etc/centreon-engine/config{}/{}".format(poller, fic))
    linesearch = [l for l in lines if not r.match(l)]
    ff = open("{}/config{}/centengine.cfg".format(CONF_DIR, poller), "w")
    ff.writelines(linesearch)
    ff.close()
