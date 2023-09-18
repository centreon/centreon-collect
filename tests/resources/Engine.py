import Common
import grpc
import math
from google.protobuf import empty_pb2
from google.protobuf.timestamp_pb2 import Timestamp
import engine_pb2
import engine_pb2_grpc
from array import array
from os import makedirs, chmod
from os.path import exists, dirname
from xml.etree.ElementTree import Comment
from robot.api import logger
from robot.libraries.BuiltIn import BuiltIn
import db_conf
import random
import shutil
import sys
import time
import re
import stat
import string


sys.path.append('.')


SCRIPT_DIR: str = dirname(__file__) + "/engine-scripts/"
VAR_ROOT = BuiltIn().get_variable_value("${VarRoot}")
ETC_ROOT = BuiltIn().get_variable_value("${EtcRoot}")
CONF_DIR = ETC_ROOT + "/centreon-engine"
ENGINE_HOME = VAR_ROOT + "/lib/centreon-engine"
TIMEOUT = 30


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
        self.anomaly_detection_internal_id = 1
        self.build_configs(hosts, srv_by_host)
        makedirs(ETC_ROOT, mode=0o777, exist_ok=True)
        makedirs(VAR_ROOT, mode=0o777, exist_ok=True)
        makedirs(CONF_DIR, mode=0o777, exist_ok=True)
        makedirs(ENGINE_HOME, mode=0o777, exist_ok=True)
        makedirs(ETC_ROOT + "/centreon-broker", mode=0o777, exist_ok=True)
        makedirs(VAR_ROOT + "/log/centreon-engine/", mode=0o777, exist_ok=True)
        makedirs(VAR_ROOT + "/log/centreon-broker/", mode=0o777, exist_ok=True)

    def create_centengine(self, id: int, debug_level=0):
        grpc_port = id + 50001
        return ("cfg_file={2}/config{0}/hosts.cfg\n"
                "cfg_file={2}/config{0}/services.cfg\n"
                "cfg_file={2}/config{0}/commands.cfg\n"
                "cfg_file={2}/config{0}/contactgroups.cfg\n"
                "#cfg_file={2}/config{0}/contacts.cfg\n"
                "cfg_file={2}/config{0}/hostgroups.cfg\n"
                "cfg_file={2}/config{0}/timeperiods.cfg\n"
                "cfg_file={2}/config{0}/escalations.cfg\n"
                "cfg_file={2}/config{0}/dependencies.cfg\n"
                "cfg_file={2}/config{0}/connectors.cfg\n"
                "#cfg_file={2}/config{0}/meta_commands.cfg\n"
                "#cfg_file={2}/config{0}/meta_timeperiod.cfg\n"
                "#cfg_file={2}/config{0}/meta_host.cfg\n"
                "#cfg_file={2}/config{0}/meta_services.cfg\n"
                "broker_module=/usr/lib64/centreon-engine/externalcmd.so\n"
                "broker_module=/usr/lib64/nagios/cbmod.so {4}/centreon-broker/central-module{0}.json\n"
                "interval_length=60\n"
                "use_timezone=:Europe/Paris\n"
                "resource_file={2}/config{0}/resource.cfg\n"
                "log_file={3}/log/centreon-engine/config{0}/centengine.log\n"
                "status_file={3}/log/centreon-engine/config{0}/status.dat\n"
                "command_check_interval=1s\n"
                "command_file={3}/lib/centreon-engine/config{0}/rw/centengine.cmd\n"
                "state_retention_file={3}/log/centreon-engine/config{0}/retention.dat\n"
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
                "debug_file={3}/log/centreon-engine/config{0}/centengine.debug\n"
                "debug_level={1}\n"
                "debug_verbosity=2\n"
                "log_pid=1\n"
                "macros_filter=KEY80,KEY81,KEY82,KEY83,KEY84\n"
                "enable_macros_filter=0\n"
                "rpc_port={5}\n"
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
                "log_file_line=1\n"
                "log_v2_logger=file\n"
                "log_level_functions=trace\n"
                "log_level_config=info\n"
                "log_level_events=info\n"
                "log_level_checks=info\n"
                "log_level_notifications=info\n"
                "log_level_eventbroker=info\n"
                "log_level_external_command=trace\n"
                "log_level_commands=info\n"
                "log_level_downtimes=trace\n"
                "log_level_comments=info\n"
                "log_level_macros=info\n"
                "log_level_process=info\n"
                "log_level_runtime=info\n"
                "log_flush_period=0\n"
                "soft_state_dependencies=0\n"
                "obsess_over_services=0\n"
                "process_performance_data=0\n"
                "check_for_orphaned_services=0\n"
                "check_for_orphaned_hosts=0\n"
                "check_service_freshness=1\n"
                "enable_flap_detection=0\n").format(id, debug_level, CONF_DIR, VAR_ROOT, ETC_ROOT, grpc_port)

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
    _KEY_SERV{0}_{1}                VAL_SERV{1}
}}
""".format(
            host_id, service_id, self.service_cmd[service_id])
        return retval


    def create_anomaly_detection(self, host_id: int, dependent_service_id: int, metric_name: string, sensitivity: float = 0.0):
        self.last_service_id += 1
        service_id = self.last_service_id
        retval = """define anomalydetection {{
    host_id {0}
    host_name host_{0}
    internal_id {4}
    service_id {1}
    service_description      anomaly_{1}
    dependent_service_id {2}
    metric_name {3}
    sensitivity {5}
    status_change 1
    thresholds_file /tmp/anomaly_threshold.json
}} """.format(host_id, service_id, dependent_service_id, metric_name, self.anomaly_detection_internal_id, sensitivity)
        self.anomaly_detection_internal_id += 1
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
    def create_contact_group(id, mbs):
        retval = """define contactgroup {{
    contactgroup_name              contactgroup_{0}
    alias                          contactgroup_{0}
    members                        {1}
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
    def create_escalations_file(poller: int, name: int, SG: str, contactgroup: str):
        config_file = f"{CONF_DIR}/config{poller}/escalations.cfg"
        with open(config_file, "a+") as ff:
            content = """define serviceescalation {{
    ;escalation_name                esc{0}
    escalation_period              24x7
    escalation_options             w,c,r
    servicegroup_name              {1}
    contact_groups                 {2}
    }}
    """.format(name, SG, contactgroup)
            ff.write(content)

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
            f = open(config_dir + "/centengine.cfg", "w")
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
    command_line                    {0}/check.pl 0
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
    connector_line                 /usr/lib64/centreon-connector/centreon_connector_ssh --debug --log-file=/tmp/var/log/centreon-engine/config0/connector_ssh.log 
}
""")
            f.close()
            f = open(config_dir + "/resource.cfg", "w")
            f.write("""$USER1$=/usr/lib64/nagios/plugins
$CENTREONPLUGINS$=/usr/lib/centreon/plugins""")
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
define timeperiod {
    name                           none
    timeperiod_name                none
    alias                          Never
}
define timeperiod {
    name                           nonworkhours
    timeperiod_name                nonworkhours
    alias                          Non-Work Hours
    sunday                         00:00-24:00
    monday                         00:00-09:00,17:00-24:00
    tuesday                        00:00-09:00,17:00-24:00
    wednesday                      00:00-09:00,17:00-24:00
    thursday                       00:00-09:00,17:00-24:00
    friday                         00:00-09:00,17:00-24:00
    saturday                       00:00-24:00
}
""")
            f.close()
            f = open(config_dir + "/hostgroups.cfg", "w")
            f.close()
            f = open(config_dir + "/contacts.cfg", "w")
            f.write("""define contact {
    contact_name                   John_Doe
    alias                          admin
    email                          admin@admin.tld
    host_notification_period       24x7
    service_notification_period    24x7
    host_notification_options      d,u,r,f,s
    service_notification_options   w,c,r
    register                       1
    host_notifications_enabled     1
    service_notifications_enabled  1
}
define contact {
    contact_name                   U1
    alias                          U1
    email                          U1@gmail.com
    host_notification_period       24x7
    service_notification_period    24x7
    host_notification_options      d,u,r,f,s
    service_notification_options   w,u,c,r,f,s
    register                       1
    host_notifications_enabled     1
    service_notifications_enabled  1
    service_notification_commands              command_notif
    host_notification_commands              command_notif
}
define contact {
    contact_name                   U2
    alias                          U2
    email                          U2@gmail.com
    host_notification_period       24x7
    service_notification_period    24x7
    host_notification_options      d,u,r,f,s
    service_notification_options   w,u,c,r,f,s
    register                       1
    host_notifications_enabled     1
    service_notifications_enabled  1
    service_notification_commands              command_notif
    host_notification_commands              command_notif    
}
define contact {
    contact_name                   U3
    alias                          U3
    email                          U3@gmail.com
    host_notification_period       24x7
    service_notification_period    24x7
    host_notification_options      d,u,r,f,s
    service_notification_options   w,u,c,r,f,s
    register                       1
    host_notifications_enabled     1
    service_notifications_enabled  1
    service_notification_commands              command_notif
    host_notification_commands              command_notif
}
define contact {
    contact_name                   U4
    alias                          U4
    email                          U4@gmail.com
    host_notification_period       24x7
    service_notification_period    24x7
    host_notification_options      d,u,r,f,s
    service_notification_options   w,u,c,r,f,s
    register                       1
    host_notifications_enabled     1
    service_notifications_enabled  1
    service_notification_commands              command_notif
    host_notification_commands              command_notif
}
            """)
            f.close()
            with open(config_dir + "/dependencies.cfg", "w") as f:
                f.write("#dependencies.cfg\n")

            with open(config_dir + "/contactgroups.cfg", "w") as f:
                f.write("#contactgroups.cfg\n")

            f = open(config_dir + "/escalations.cfg", "w")
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
        lines_to_prep = ["cfg_file=" + ETC_ROOT + "/centreon-engine/config0/centreon-bam-command.cfg\n", "cfg_file=" + ETC_ROOT + "/centreon-engine/config0/centreon-bam-timeperiod.cfg\n",
                         "cfg_file=" + ETC_ROOT + "/centreon-engine/config0/centreon-bam-host.cfg\n", "cfg_file=" + ETC_ROOT + "/centreon-engine/config0/centreon-bam-services.cfg\n"]
        f = open(config_dir + "/centengine.cfg", "w")
        f.writelines(lines_to_prep)
        f.writelines(lines)
        f.close()

    def centengine_conf_add_anomaly(self):
        config_dir = "{}/config0".format(CONF_DIR)
        f = open(config_dir + "/centengine.cfg", "r")
        lines = f.readlines()
        f.close
        f = open(config_dir + "/centengine.cfg", "w")
        f.writelines("cfg_file=" + config_dir +
                     "/anomaly_detection.cfg\n")
        f.writelines(lines)
        f.close()


engine = None

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
    if engine is None:
        return 0
    else:
        return engine.instances


##
# @brief Function to change a value in the centengine.cfg for the config idx.
#
# @param idx index of the configuration (from 0)
# @param key the key to change the value.
# @param value the new value to set to the key variable.
#
def engine_config_set_value(idx: int, key: str, value: str, force: bool = False):
    filename = ETC_ROOT + \
        "/centreon-engine/config{}/centengine.cfg".format(idx)
    f = open(filename, "r")
    lines = f.readlines()
    f.close()

    replaced = False
    for i in range(len(lines)):
        if lines[i].startswith(key + "="):
            lines[i] = "{}={}\n".format(key, value)
            replaced = True

    if not replaced and force:
        lines.append("{}={}\n".format(key, value))

    f = open(filename, "w")
    f.writelines(lines)
    f.close()

##
# @brief Function to add a value in the centengine.cfg for the config idx.
#
# @param idx index of the configuration (from 0)
# @param key the key to change the value.
# @param value the new value to set to the key variable.
#


def engine_config_add_value(idx: int, key: str, value: str):
    filename = ETC_ROOT + \
        "/centreon-engine/config{}/centengine.cfg".format(idx)
    f = open(filename, "a")
    f.write(f"{key}={value}")
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
    filename = ETC_ROOT + "/centreon-engine/config{}/services.cfg".format(idx)
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

def engine_config_replace_value_in_services(idx: int, desc: str, key: str, value: str):
    """! Function to update a value in the services.cfg for the config idx.
    @param idx index of the configuration (from 0)
    @param desc service description of the service to modify.
    @param key the key to change the value.
    @param value the new value to set to the key variable.
    """

    filename = f"{ETC_ROOT}/centreon-engine/config{idx}/services.cfg"
    with open(filename, "r") as f:
        lines = f.readlines()
    r = re.compile(r"^\s*service_description\s+" + desc + "\s*$")
    rkey = re.compile(r"^\s*" + key + "\s+[\w\.]+\s*$")
    for i in range(len(lines)):
        if r.match(lines[i]):
            while i < len(lines) and lines[i] != "}":
                if rkey.match(lines[i]):
                    lines[i] = f"    {key}                 {value}\n"
                    break
                i += 1

    with open(filename, "w") as f:
        f.writelines(lines)

##
# @brief Function to change a value in the hosts.cfg for the config idx.
#
# @param idx index of the configuration (from 0)
# @param desc host name of the host to modify.
# @param key the key to change the value.
# @param value the new value to set to the key variable.
#


def engine_config_set_value_in_hosts(idx: int, desc: str, key: str, value: str):
    filename = ETC_ROOT + "/centreon-engine/config{}/hosts.cfg".format(idx)
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


##
# @brief Function to change a value in the hosts.cfg for the config idx.
#
# @param idx index of the configuration (from 0)
# @param desc host name of the host to modify.
# @param key the key to change the value.
# @param value the new value to set to the key variable.
#
def engine_config_replace_value_in_hosts(idx: int, desc: str, key: str, value: str):
    filename = ETC_ROOT + "/centreon-engine/config{}/hosts.cfg".format(idx)
    f = open(filename, "r")
    lines = f.readlines()
    f.close()

    r = re.compile(r"^\s*host_name\s+" + desc + "\s*$")
    rkey = re.compile(r"^\s*"+key+"\s+[\w\.]+\s*$")
    for i in range(len(lines)):
        if r.match(lines[i]):
            while i < len(lines) and lines[i] != "}":
                if rkey.match(lines[i]):
                    lines[i] = "    {}              {}\n".format(key, value)
                    break
                i += 1

    f = open(filename, "w")
    f.writelines(lines)
    f.close()


##
# @brief Function to change a value in the commands.cfg for the config idx.
#
# @param idx index of the configuration (from 0)
# @param command_index  index of the command (may be a regex)
# @param new_command
#
def engine_config_change_command(idx: int, command_index: str, new_command: str):
    f = open(f"{CONF_DIR}/config{idx}/commands.cfg", "r")
    lines = f.readlines()
    f.close
    new_lines = []
    r = re.compile(f"^\\s+command_name\\s+command_{command_index}$")
    found = 0
    for line in lines:
        if found == 1:
            found = 0
            new_lines.append(
                f"    command_line                    {new_command}\n")
        else:
            new_lines.append(line)
        if r.match(line) is not None:
            found = 1
    f = open(f"{CONF_DIR}/config0/commands.cfg", "w")
    f.writelines(new_lines)
    f.close


##
# @brief Function to add a new command in the commands.cfg for the config idx
#
# @param idx index of the configuration (from 0)
# @param command_name
# @param new_command
#
def engine_config_add_command(idx: int, command_name: str, new_command: str, connector: str = None):
    f = open(f"{CONF_DIR}/config{idx}/commands.cfg", "a")
    if connector is None:
        f.write("""define command {{
    command_name                   {} 
    command_line                   {}
}}
    """.format(command_name, new_command))
    else:
        f.write("""define command {{
    command_name                   {} 
    command_line                   {}
    connector                      {}
}}
    """.format(command_name, new_command, connector))
    f.close()


#
# @param idx index of the configuration (from 0)
# @param desc contact_name
# @param key the key to change the value.
# @param value the new value to set to the key variable.
#
def engine_config_set_value_in_contacts(idx: int, desc: str, key: str, value: str):
    filename = f"{ETC_ROOT}/centreon-engine/config{idx}/contacts.cfg"
    f = open(filename, "r")
    lines = f.readlines()
    f.close()

    r = re.compile(r"^\s*contact_name\s+" + desc + "\s*$")
    for i in range(len(lines)):
        if r.match(lines[i]):
            lines.insert(i + 1, f"    {key}              {value}\n")
            break

    f = open(filename, "w")
    f.writelines(lines)
    f.close()


def engine_config_set_value_in_escalations(idx: int, desc: str, key: str, value: str):
    with open(f"{ETC_ROOT}/centreon-engine/config{idx}/escalations.cfg", "r") as ff:
        lines = ff.readlines()
    r = re.compile(r"^\s*;escalation_name\s+" + desc + "\s*$")
    for i in range(len(lines)):
        m = r.match(lines[i])
        if m is not None:
            lines.insert(i + 1, f"    {key}                     {value}\n")
    with open(f"{ETC_ROOT}/centreon-engine/config{idx}/escalations.cfg", "w") as ff:
        ff.writelines(lines)

def engine_config_remove_service_host(idx: int, host: str):
    filename = ETC_ROOT + "/centreon-engine/config{}/services.cfg".format(idx)
    f = open(filename, "r")
    lines = f.readlines()
    f.close()
    host_name = re.compile(r"^\s*host_name\s+" + host + "\s*$")
    serv_begin = re.compile(r"^define service {$")
    serv_end = re.compile(r"^}$")
    serv_begin_idx = 0
    while True:
        if (serv_begin_idx >= len(lines)):
            break
        if (serv_begin.match(lines[serv_begin_idx])):
            for serv_line_idx in range(serv_begin_idx, len(lines)):
                if (host_name.match(lines[serv_line_idx])):
                    for end_serv_line in range(serv_line_idx, len(lines)):
                        if serv_end.match(lines[end_serv_line]):
                            del lines[serv_begin_idx:end_serv_line + 1]
                            break
                    break
                elif serv_end.match(lines[serv_line_idx]):
                    serv_begin_idx = serv_line_idx
                    break
        else:
            serv_begin_idx = serv_begin_idx + 1

    f = open(filename, "w")
    f.writelines(lines)
    f.close()


def engine_config_remove_host(idx: int, host: str):
    filename = ETC_ROOT + "/centreon-engine/config{}/services.cfg".format(idx)
    f = open(filename, "r")
    lines = f.readlines()
    f.close()

    host_name = re.compile(r"^\s*host_name\s+" + host + "\s*$")
    host_begin = re.compile(r"^define host {$")
    host_end = re.compile(r"^}$")
    host_begin_idx = 0
    while True:
        if (host_begin_idx >= len(lines)):
            break
        if (host_begin.match(lines[host_begin_idx])):
            for host_line_idx in range(host_begin_idx, len(lines)):
                if (host_name.match(lines[host_line_idx])):
                    for end_serv_line in range(host_line_idx, len(lines)):
                        if host_end.match(lines[end_serv_line]):
                            del lines[host_begin_idx:end_serv_line + 1]
                            break
                    break
                elif host_end.match(lines[host_line_idx]):
                    host_begin_idx = host_line_idx
                    break
        else:
            host_begin_idx = host_begin_idx + 1

    f = open(filename, "w")
    f.writelines(lines)
    f.close()


def add_host_group(index: int, id_host_group: int, members: list):
    mbs = [l for l in members if l in engine.hosts]
    f = open(ETC_ROOT + "/centreon-engine/config{}/hostgroups.cfg".format(index), "a+")
    logger.console(mbs)
    f.write(engine.create_host_group(id_host_group, mbs))
    f.close()


def rename_host_group(index: int, id_host_group: int, name: str, members: list):
    mbs = [l for l in members if l in engine.hosts]
    f = open(ETC_ROOT + "/centreon-engine/config{}/hostgroups.cfg".format(index), "w")
    logger.console(mbs)
    f.write("""define hostgroup {{
    hostgroup_id                    {0}
    hostgroup_name                  hostgroup_{1}
    alias                           hostgroup_{1}
    members                         {2}
}}
""".format(id_host_group, name, ",".join(mbs)))
    f.close()


def rename_service(index: int, hst: str, svc: str, new_svc: str):
    f = open(f"{ETC_ROOT}/centreon-engine/config{index}/services.cfg", "r")
    ll = f.readlines()
    f.close()
    rs_start = re.compile(r"^\s*define service {")
    rs_end = re.compile(r"^\s*}")
    rs_hst = re.compile(r"^\s*host_name\s+([a-z_0-9]+)")
    rs_svc = re.compile(r"^\s*service_description\s+([a-z_0-9]+)")
    inside = False
    my_hst = None
    my_svc = None
    l_svc = None

    for i in range(len(ll)):
        l = ll[i]
        if inside:
            if rs_end.match(l):
                inside = False
                if svc == my_svc and hst == my_hst:
                    ll[l_svc] = f"    service_description\t{new_svc}\n"
                svc, hst, l_svc = None, None, None
                continue
            m = rs_hst.search(l)
            if m:
                my_hst = m.group(1)
            else:
                m = rs_svc.search(l)
                if m:
                    my_svc = m.group(1)
                    l_svc = i

        else:
            if rs_start.match(l):
                inside = True

    f = open(f"{ETC_ROOT}/centreon-engine/config{index}/services.cfg", "w")
    f.writelines(ll)
    f.close()


def add_service_group(index: int, id_service_group: int, members: list):
    f = open(
        ETC_ROOT + "/centreon-engine/config{}/servicegroups.cfg".format(index), "a+")
    logger.console(members)
    f.write(engine.create_service_group(id_service_group, members))
    f.close()

def add_contact_group(index: int, id_contact_group: int, members: list):
    with open(f"{ETC_ROOT}/centreon-engine/config{index}/contactgroups.cfg", "a+") as f:
        logger.console(members)
        f.write(engine.create_contact_group(id_contact_group, members))

def create_service(index: int, host_id: int, cmd_id: int):
    f = open(ETC_ROOT + "/centreon-engine/config{}/services.cfg".format(index), "a+")
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


def create_anomaly_detection(index: int, host_id: int, dependent_service_id: int, metric_name: string, sensitivity: float = 0.0):
    f = open(
        ETC_ROOT + "/centreon-engine/config{}/anomaly_detection.cfg".format(index), "a+")
    to_append = engine.create_anomaly_detection(
        host_id, dependent_service_id, metric_name, sensitivity)
    lst = to_append.split('\n')
    good = [l for l in lst if "service_id" in l][0]
    m = re.search(r"service_id\s+([^\s]*)$", good)
    if m is not None:
        retval = int(m.group(1))
    else:
        raise Exception(
            "Impossible to get the service id from '{}'".format(good))
        m = 0
    f.write(to_append)
    f.close()
    engine.centengine_conf_add_anomaly()
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
    return dbconf.create_ba_with_services(name, typ, svc, dt_policy)


def create_ba(name: str, typ: str, critical_impact: int, warning_impact: int, dt_policy="inherit"):
    global dbconf
    return dbconf.create_ba(name, typ, critical_impact, warning_impact, dt_policy)


def add_boolean_kpi(id_ba: int, expression: str, impact_if: bool, critical_impact: int):
    return dbconf.add_boolean_kpi(id_ba, expression, impact_if, critical_impact)


def update_boolean_rule(boolean_id: int, expression: str):
    dbconf.update_boolean_rule(boolean_id, expression)


def add_ba_kpi(id_ba_src: int, id_ba_dest: int, critical_impact: int, warning_impact: int, unknown_impact: int):
    dbconf.add_ba_kpi(id_ba_src, id_ba_dest, critical_impact,
                      warning_impact, unknown_impact)


def add_service_kpi(host: str, serv: str, id_ba: int, critical_impact: int, warning_impact: int, unknown_impact: int):
    global dbconf
    dbconf.add_service_kpi(
        host, serv, id_ba, critical_impact, warning_impact, unknown_impact)


def get_command_id(service: int):
    global engine
    global dbconf
    cmd_name = engine.service_cmd[service]
    return dbconf.command[cmd_name]


def get_command_service_param(service: int):
    global engine
    return engine.service_cmd[service][8:]


def change_normal_svc_check_interval(use_grpc: int, hst: str, svc: str, check_interval: int):
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.ChangeServiceObjectIntVar(engine_pb2.ChangeObjectInt(
                host_name=hst, service_desc=svc, mode=engine_pb2.ChangeObjectInt.Mode.NORMAL_CHECK_INTERVAL, dval=check_interval))
    else:
        now = int(time.time())
        cmd = "[{}] CHANGE_NORMAL_SVC_CHECK_INTERVAL;{};{};{}\n".format(
            now, hst, svc, check_interval)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def change_normal_host_check_interval(use_grpc: int, hst: str, check_interval: int):
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.ChangeHostObjectIntVar(engine_pb2.ChangeObjectInt(
                host_name=hst, mode=engine_pb2.ChangeObjectInt.Mode.NORMAL_CHECK_INTERVAL, dval=check_interval))
    else:
        now = int(time.time())
        cmd = "[{}] CHANGE_NORMAL_HOST_CHECK_INTERVAL;{};{}\n".format(
            now, hst, check_interval)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def change_retry_svc_check_interval(use_grpc: int, hst: str, svc: str, retry_interval: int):
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.ChangeServiceObjectIntVar(engine_pb2.ChangeObjectInt(
                host_name=hst, service_desc=svc, mode=engine_pb2.ChangeObjectInt.Mode.RETRY_CHECK_INTERVAL, dval=retry_interval))
    else:
        now = int(time.time())
        cmd = "[{}] CHANGE_RETRY_SVC_CHECK_INTERVAL;{};{};{}\n".format(
            now, hst, svc, retry_interval)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def change_retry_host_check_interval(use_grpc: int, hst: str, retry_interval: int):
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.ChangeHostObjectIntVar(engine_pb2.ChangeObjectInt(
                host_name=hst, mode=engine_pb2.ChangeObjectInt.Mode.RETRY_CHECK_INTERVAL, dval=retry_interval))
    else:
        now = int(time.time())
        cmd = "[{}] CHANGE_RETRY_HOST_CHECK_INTERVAL;{};{}\n".format(
            now, hst, retry_interval)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def change_max_svc_check_attempts(use_grpc: int, hst: str, svc: str, max_check_attempts: int):
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.ChangeServiceObjectIntVar(engine_pb2.ChangeObjectInt(
                host_name=hst, service_desc=svc, mode=engine_pb2.ChangeObjectInt.Mode.MAX_ATTEMPTS, intval=max_check_attempts))
    else:
        now = int(time.time())
        cmd = "[{}] CHANGE_MAX_SVC_CHECK_ATTEMPTS;{};{};{}\n".format(
            now, hst, svc, max_check_attempts)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def change_max_host_check_attempts(use_grpc: int, hst: str, max_check_attempts: int):
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.ChangeHostObjectIntVar(engine_pb2.ChangeObjectInt(
                host_name=hst, mode=engine_pb2.ChangeObjectInt.Mode.MAX_ATTEMPTS, intval=max_check_attempts))
    else:
        now = int(time.time())
        cmd = "[{}] CHANGE_MAX_HOST_CHECK_ATTEMPTS;{};{}\n".format(
            now, hst, max_check_attempts)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def change_host_check_timeperiod(use_grpc: int, hst: str, check_timeperiod: str):
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.ChangeHostObjectCharVar(engine_pb2.ChangeObjectChar(
                host_name=hst, mode=engine_pb2.ChangeObjectChar.Mode.CHANGE_CHECK_TIMEPERIOD, charval=check_timeperiod))
    else:
        now = int(time.time())
        cmd = "[{}] CHANGE_HOST_CHECK_TIMEPERIOD;{};{}\n".format(
            now, hst, check_timeperiod)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def change_host_notification_timeperiod(use_grpc: int, hst: str, notification_timeperiod: str):
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.ChangeHostObjectCharVar(engine_pb2.ChangeObjectChar(
                host_name=hst, mode=engine_pb2.ChangeObjectChar.Mode.CHANGE_NOTIFICATION_TIMEPERIOD, charval=notification_timeperiod))
    else:
        now = int(time.time())
        cmd = "[{}] CHANGE_HOST_NOTIFICATION_TIMEPERIOD;{};{}\n".format(
            now, hst, notification_timeperiod)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def change_svc_check_timeperiod(use_grpc: int, hst: str, svc: str, check_timeperiod: str):
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.ChangeServiceObjectCharVar(engine_pb2.ChangeObjectChar(
                host_name=hst, service_desc=svc,  mode=engine_pb2.ChangeObjectChar.Mode.CHANGE_CHECK_TIMEPERIOD, charval=check_timeperiod))
    else:
        now = int(time.time())
        cmd = "[{}] CHANGE_SVC_CHECK_TIMEPERIOD;{};{};{}\n".format(
            now, hst, svc, check_timeperiod)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def change_svc_notification_timeperiod(use_grpc: int, hst: str, svc: str, notification_timeperiod: str):
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.ChangeServiceObjectCharVar(engine_pb2.ChangeObjectChar(
                host_name=hst, service_desc=svc,  mode=engine_pb2.ChangeObjectChar.Mode.CHANGE_NOTIFICATION_TIMEPERIOD, charval=notification_timeperiod))
    else:
        now = int(time.time())
        cmd = "[{}] CHANGE_SVC_NOTIFICATION_TIMEPERIOD;{};{};{}\n".format(
            now, hst, svc, notification_timeperiod)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def disable_host_and_child_notifications(use_grpc: int, hst: str):
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.DisableHostAndChildNotifications(
                engine_pb2.HostIdentifier(name=hst))
    else:
        now = int(time.time())
        cmd = "[{}] DISABLE_HOST_AND_CHILD_NOTIFICATIONS;{}\n".format(
            now, hst)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def enable_host_and_child_notifications(use_grpc: int, hst: str):
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.EnableHostAndChildNotifications(
                engine_pb2.HostIdentifier(name=hst))
    else:
        now = int(time.time())
        cmd = "[{}] ENABLE_HOST_AND_CHILD_NOTIFICATIONS;{}\n".format(
            now, hst)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def disable_host_check(use_grpc: int, hst: str):
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] DISABLE_HOST_CHECK;{}\n".format(
            now, hst)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def enable_host_check(use_grpc: int, hst: str):
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] ENABLE_HOST_CHECK;{}\n".format(
            now, hst)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def disable_host_event_handler(use_grpc: int, hst: str):
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] DISABLE_HOST_EVENT_HANDLER;{}\n".format(
            now, hst)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def enable_host_event_handler(use_grpc: int, hst: str):
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] ENABLE_HOST_EVENT_HANDLER;{}\n".format(
            now, hst)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def disable_host_flap_detection(use_grpc: int, hst: str):
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] DISABLE_HOST_FLAP_DETECTION;{}\n".format(
            now, hst)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def enable_host_flap_detection(use_grpc: int, hst: str):
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] ENABLE_HOST_FLAP_DETECTION;{}\n".format(
            now, hst)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def disable_host_notifications(use_grpc: int, hst: str):
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.DisableHostNotifications(
                engine_pb2.HostIdentifier(name=hst))
    else:
        now = int(time.time())
        cmd = "[{}] DISABLE_HOST_NOTIFICATIONS;{}\n".format(
            now, hst)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def enable_host_notifications(use_grpc: int, hst: str):
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.EnableHostNotifications(
                engine_pb2.HostIdentifier(name=hst))
    else:
        now = int(time.time())
        cmd = "[{}] ENABLE_HOST_NOTIFICATIONS;{}\n".format(
            now, hst)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def update_ano_sensitivity(use_grpc: int, hst: str, serv: str, sensitivity: float):
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.ChangeAnomalyDetectionSensitivity(engine_pb2.ChangeServiceNumber(serv=engine_pb2.ServiceIdentifier(
                names=engine_pb2.NameIdentifier(host_name=hst, service_name=serv)), dval=sensitivity))
    else:
        now = int(time.time())
        cmd = "[{}] CHANGE_ANOMALYDETECTION_SENSITIVITY;{};{};{}\n".format(
            now, hst, serv, sensitivity)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def disable_host_svc_checks(use_grpc: int, hst: str):
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] DISABLE_HOST_SVC_CHECKS;{}\n".format(
            now, hst)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def enable_host_svc_checks(use_grpc: int, hst: str):
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] ENABLE_HOST_SVC_CHECKS;{}\n".format(
            now, hst)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def disable_host_svc_notifications(use_grpc: int, hst: str):
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] DISABLE_HOST_SVC_NOTIFICATIONS;{}\n".format(
            now, hst)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def enable_host_svc_notifications(use_grpc: int, hst: str):
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] ENABLE_HOST_SVC_NOTIFICATIONS;{}\n".format(
            now, hst)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def disable_passive_host_checks(use_grpc: int, hst: str):
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] DISABLE_PASSIVE_HOST_CHECKS;{}\n".format(
            now, hst)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def enable_passive_host_checks(use_grpc: int, hst: str):
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] ENABLE_PASSIVE_HOST_CHECKS;{}\n".format(
            now, hst)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def disable_passive_svc_checks(use_grpc: int, hst: str, svc: str):
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] DISABLE_PASSIVE_SVC_CHECKS;{};{}\n".format(
            now, hst, svc)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def enable_passive_svc_checks(use_grpc: int, hst: str, svc: str):
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] ENABLE_PASSIVE_SVC_CHECKS;{};{}\n".format(
            now, hst, svc)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def start_obsessing_over_host(use_grpc: int, hst: str):
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] START_OBSESSING_OVER_HOST;{}\n".format(
            now, hst)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def stop_obsessing_over_host(use_grpc: int, hst: str):
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] STOP_OBSESSING_OVER_HOST;{}\n".format(
            now, hst)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def start_obsessing_over_svc(use_grpc: int, hst: str, svc: str):
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] START_OBSESSING_OVER_SVC;{};{}\n".format(
            now, hst, svc)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def stop_obsessing_over_svc(use_grpc: int, hst: str, svc: str):
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] STOP_OBSESSING_OVER_SVC;{};{}\n".format(
            now, hst, svc)
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()


def service_ext_commands(hst: str, svc: str, state: int, output: str):
    now = int(time.time())
    cmd = "[{}] PROCESS_SERVICE_CHECK_RESULT;{};{};{};{}\n".format(
        now, hst, svc, state, output)
    f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def process_host_check_result(hst: str, state: int, output: str):
    now = int(time.time())
    cmd = "[{}] PROCESS_HOST_CHECK_RESULT;{};{};{}\n".format(
        now, hst, state, output)
    f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def schedule_service_downtime(hst: str, svc: str, duration: int):
    now = int(time.time())
    cmd = "[{2}] SCHEDULE_SVC_DOWNTIME;{0};{1};{2};{3};0;0;{4};admin;Downtime set by admin\n".format(
        hst, svc, now, now+duration, duration)
    f = open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def schedule_service_fixed_downtime(hst: str, svc: str, duration: int):
    now = int(time.time())
    cmd = "[{2}] SCHEDULE_SVC_DOWNTIME;{0};{1};{2};{3};1;0;{4};admin;Downtime set by admin\n".format(
        hst, svc, now, now + duration, duration)
    f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def schedule_host_fixed_downtime(poller: int, hst: str, duration: int):
    now = int(time.time())
    cmd1 = "[{1}] SCHEDULE_HOST_DOWNTIME;{0};{1};{2};1;0;;admin;Downtime set by admin\n".format(
        hst, now, now + duration)
    cmd2 = "[{1}] SCHEDULE_HOST_SVC_DOWNTIME;{0};{1};{2};1;0;;admin;Downtime set by admin\n".format(
        hst, now, now + duration)
    f = open(
        VAR_ROOT + "/lib/centreon-engine/config{}/rw/centengine.cmd".format(poller), "w")
    f.write(cmd1)
    f.write(cmd2)
    f.close()


def schedule_host_downtime(poller: int, hst: str, duration: int):
    now = int(time.time())
    cmd1 = "[{1}] SCHEDULE_HOST_DOWNTIME;{0};{1};{2};1;0;{3};admin;Downtime set by admin\n".format(
        hst, now, now + duration, duration)
    cmd2 = "[{1}] SCHEDULE_HOST_SVC_DOWNTIME;{0};{1};{2};1;0;{3};admin;Downtime set by admin\n".format(
        hst, now, now + duration, duration)
    f = open(
        VAR_ROOT + "/lib/centreon-engine/config{}/rw/centengine.cmd".format(poller), "w")
    f.write(cmd1)
    f.write(cmd2)
    f.close()


def delete_host_downtimes(poller: int, hst: str):
    now = int(time.time())
    cmd = "[{}] DEL_HOST_DOWNTIME_FULL;{};;;;;;;;\n".format(now, hst)
    f = open(
        f"{VAR_ROOT}/lib/centreon-engine/config{poller}/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def delete_service_downtime_full(poller: int, hst: str, svc: str):
    now = int(time.time())
    cmd = f"[{now}] DEL_SVC_DOWNTIME_FULL;{hst};{svc};;;;;;;\n"
    f = open(
        f"{VAR_ROOT}/lib/centreon-engine/config{poller}/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def schedule_forced_svc_check(host: str, svc: str, pipe: str = VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd"):
    now = int(time.time())
    f = open(pipe, "w")
    cmd = "[{2}] SCHEDULE_FORCED_SVC_CHECK;{0};{1};{2}\n".format(
        host, svc, now)
    f.write(cmd)
    f.close()
    time.sleep(0.05)


def schedule_forced_host_check(host: str, pipe: str = VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd"):
    now = int(time.time())
    f = open(pipe, "w")
    cmd = "[{1}] SCHEDULE_FORCED_HOST_CHECK;{0};{1}\n".format(host, now)
    f.write(cmd)
    f.close()
    time.sleep(0.05)


def create_severities_file(poller: int, nb: int, offset: int = 1):
    engine.create_severities(poller, nb, offset)


def create_escalations_file(poller: int, name: int, SG: str, contactgroup: str):
    engine.create_escalations_file(poller, name, SG, contactgroup)


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


def set_services_passive(poller: int, srv_regex):
    ff = open("{}/config{}/services.cfg".format(CONF_DIR, poller), "r")
    lines = ff.readlines()
    ff.close()
    r = re.compile(f"^\s*service_description\s*({srv_regex})$")
    rce = re.compile(r"^\s*([a-z]*)_checks_enabled\s*([01])$")
    rc = re.compile(r"^\s*}\s*$")
    desc = ""
    for i in range(len(lines)):
        m = r.match(lines[i])
        if m:
            desc = m.group(1)
        elif len(desc) > 0:
            m = rce.match(lines[i])
            if m:
                if m.group(1) == "active":
                    lines[i] = "    active_checks_enabled           0\n"
                elif m.group(1) == "passive":
                    lines[i] = "    passive_checks_enabled          1\n"
            else:
                m = rc.match(lines[i])
                if m:
                    desc = ""

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


def check_search(debug_file_path: str, str_to_search, timeout=TIMEOUT):
    limit = time.time() + timeout
    while time.time() < limit:
        cmd_executed = False
        with open(debug_file_path, 'r') as f:
            lines = f.readlines()
            for first_ind in range(len(lines)):
                find_index = lines[first_ind].find(str_to_search + ' ')
                if (find_index > 0):
                    cmd_executed = True
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
        time.sleep(1)

    if not cmd_executed:
        return f"_recv_query_execute not found on '{r_query_execute}'"
    else:
        return f"check_search doesn't find '{str_to_search}'"


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
        r"^\s*cfg_file=" + ETC_ROOT + "/centreon-engine/config{}/{}".format(poller, fic))
    linesearch = [l for l in lines if not r.match(l)]
    ff = open("{}/config{}/centengine.cfg".format(CONF_DIR, poller), "w")
    ff.writelines(linesearch)
    ff.close()


def external_command(func):
    def wrapper(*args):
        now = int(time.time())
        cmd = f"[{now}] {func(*args)}"
        f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
        f.write(cmd)
        f.close()

    return wrapper


def process_service_check_result_with_metrics(hst: str, svc: str, state: int, output: str, metrics: int, config='config0'):
    now = int(time.time())
    pd = [output + " | "]
    for m in range(metrics):
        v = math.sin((now + m) / 1000) * 5
        pd.append(f"metric{m}={v}")
    full_output = " ".join(pd)
    process_service_check_result(hst, svc, state, full_output, config)


def process_service_check_result(hst: str, svc: str, state: int, output: str, config='config0', use_grpc=0, nb_check=1):
    if use_grpc > 0:
        port = 50001 + int(config[6:])
        with grpc.insecure_channel(f"127.0.0.1:{port}") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            for i in range(nb_check):
                indexed_output = f"{output}_{i}"
                stub.ProcessServiceCheckResult(engine_pb2.Check(
                    host_name=hst, svc_desc=svc, output=indexed_output, code=state))

    else:
        now = int(time.time())
        with open(f"{VAR_ROOT}/lib/centreon-engine/{config}/rw/centengine.cmd", "w") as f:
            for i in range(nb_check):
                cmd = f"[{now}] PROCESS_SERVICE_CHECK_RESULT;{hst};{svc};{state};{output}_{i}\n"
                logger.console(cmd)
                f.write(cmd)


@external_command
def acknowledge_service_problem(hst, service, typ='NORMAL'):
    if typ == 'NORMAL':
        logger.console('acknowledgement is normal')
        sticky = 1
    elif typ == 'STICKY':
        logger.console('acknowledgement is sticky')
        sticky = 2
    else:
        logger.console('acknowledgement type is none')
        sticky = 0

    return f"ACKNOWLEDGE_SVC_PROBLEM;{hst};{service};{sticky};0;0;admin;Service ({hst},{service}) acknowledged\n"


@external_command
def remove_service_acknowledgement(hst, service):
    return f"REMOVE_SVC_ACKNOWLEDGEMENT;{hst};{service}\n"


@external_command
def send_custom_host_notification(hst, notification_option, author, comment):
    return f"SEND_CUSTOM_HOST_NOTIFICATION;{hst};{notification_option};{author};{comment}\n"


@external_command
def add_svc_comment(host_name, svc_description, persistent, user_name, comment):
    return f"ADD_SVC_COMMENT;{host_name};{svc_description};{persistent};{user_name};{comment}\n"


@external_command
def add_host_comment(host_name, persistent, user_name, comment):
    return f"ADD_HOST_COMMENT;{host_name};{persistent};{user_name};{comment}\n"


@external_command
def del_host_comment(comment_id):
    return f"DEL_HOST_COMMENT;{comment_id}\n"


@external_command
def change_host_check_command(hst: str, Check_Command: str):
    return f"CHANGE_HOST_CHECK_COMMAND;{hst};{Check_Command}\n"


@external_command
def change_custom_host_var_command(hst: str, var_name: str, var_value):
    return "CHANGE_CUSTOM_HOST_VAR;{};{};{}\n".format(hst, var_name, var_value)


@external_command
def change_custom_svc_var_command(hst: str, svc: str, var_name: str, var_value):
    return "CHANGE_CUSTOM_SVC_VAR;{};{};{};{}\n".format(hst, svc, var_name, var_value)


@external_command
def change_global_host_event_handler(var_value: str):
    return "CHANGE_GLOBAL_HOST_EVENT_HANDLER;{}\n".format(var_value)


@external_command
def change_global_svc_event_handler(var_value: str):
    return "CHANGE_GLOBAL_SVC_EVENT_HANDLER;{}\n".format(var_value)


@external_command
def set_svc_notification_number(host_name: string, svc_description: string, value):
    return "SET_SVC_NOTIFICATION_NUMBER;{};{};{}\n".format(host_name, svc_description, value)


def create_anomaly_threshold_file(path: string, host_id: int, service_id: int, metric_name: string, values: array):
    f = open(path, "w")
    f.write("""[
    {{
        "host_id": "{0}",
        "service_id": "{1}",
        "metric_name": "{2}",
        "predict": [
            """.format(host_id, service_id, metric_name))
    sep = ""
    for ts_lower_upper in values:
        f.write(sep)
        sep = ","
        f.write("""
            {{
                "timestamp": {0},
                "lower": {1},
                "upper": {2}
            }}""".format(ts_lower_upper[0], ts_lower_upper[1], ts_lower_upper[2]))
    f.write("""
        ]
    }
]
""")
    f.close()


def create_anomaly_threshold_file_V2(path: string, host_id: int, service_id: int, metric_name: string, sensitivity: float, values: array):
    f = open(path, "w")
    f.write("""[
    {{
        "host_id": "{0}",
        "service_id": "{1}",
        "metric_name": "{2}",
        "sensitivity": {3},
        "predict": [
            """.format(host_id, service_id, metric_name, sensitivity))
    sep = ""
    for ts_fit_lower_upper in values:
        f.write(sep)
        sep = ","
        f.write("""
            {{
                "timestamp": {0},
                "fit": {1},
                "lower_margin": {2},
                "upper_margin": {3}
            }}""".format(ts_fit_lower_upper[0], ts_fit_lower_upper[1], ts_fit_lower_upper[2], ts_fit_lower_upper[3]))
    f.write("""
        ]
    }
]
""")
    f.close()


def grep_retention(poller: int, pattern: str):
    return Common.grep("{}/log/centreon-engine/config{}/retention.dat".format(VAR_ROOT, poller), pattern)


def modify_retention_dat(poller, host, service, key, value):
    if host != "" and host != "":
        # We want a service
        ff = open(
            f"{VAR_ROOT}/log/centreon-engine/config{poller}/retention.dat", "r")
        lines = ff.readlines()
        ff.close()

        r_hst = re.compile(r"^\s*host_name=(.*)$")
        r_svc = re.compile(r"^\s*service_description=(.*)$")
        in_block = False
        hst = ""
        svc = ""
        for i in range(len(lines)):
            l = lines[i]
            if not in_block:
                if l == "service {\n":
                    in_block = True
                    continue
            else:
                if l == "}\n":
                    in_block = False
                    hst = ""
                    svc = ""
                    continue
                m = r_hst.match(l)
                if m:
                    hst = m.group(1)
                    continue
                m = r_svc.match(l)
                if m:
                    svc = m.group(1)
                    continue
                if l.startswith(f"{key}=") and host == hst and svc == service:
                    logger.console(f"key '{key}' found !")
                    lines[i] = f"{key}={value}\n"
                    hst = ""
                    svc = ""

        ff = open(
            f"{VAR_ROOT}/log/centreon-engine/config{poller}/retention.dat", "w")
        ff.writelines(lines)
        ff.close()


def modify_retention_dat_host(poller, host, key, value):
    if host != "" and host != "":
        # We want a host
        ff = open(
            f"{VAR_ROOT}/log/centreon-engine/config{poller}/retention.dat", "r")
        lines = ff.readlines()
        ff.close()

        r_hst = re.compile(r"^\s*host_name=(.*)$")
        in_block = False
        hst = ""
        for i in range(len(lines)):
            l = lines[i]
            if not in_block:
                if l == "host {\n":
                    in_block = True
                    continue
            else:
                if l == "}\n":
                    in_block = False
                    hst = ""
                    continue
                m = r_hst.match(l)
                if m:
                    hst = m.group(1)
                    continue
                if l.startswith(f"{key}=") and host == hst:
                    logger.console(f"key '{key}' found !")
                    lines[i] = f"{key}={value}\n"
                    hst = ""

        ff = open(
            f"{VAR_ROOT}/log/centreon-engine/config{poller}/retention.dat", "w")
        ff.writelines(lines)
        ff.close()


##
# @brief Call the GetGenericStats function by gRPC
# it works with both engine and broker
#
# @param port of the grpc server
#
# @return process__stat__pb2.pb_process_stat
#
def get_engine_process_stat(port, timeout=10):
    limit = time.time() + timeout
    while time.time() < limit:
        time.sleep(1)
        with grpc.insecure_channel("127.0.0.1:{}".format(port)) as channel:
            # same for engine and broker
            stub = engine_pb2_grpc.EngineStub(channel)
            try:
                res = stub.GetProcessStats(empty_pb2.Empty())
                return res
            except:
                logger.console("gRPC server not ready")
    logger.console("unable to get process stats")
    return None


##
# @brief make engine to send a bench event
#
# @param id field of the protobuf Bench message
# @param port of the grpc server
#
def send_bench(id: int, port: int):
    ts = Timestamp()
    ts.GetCurrentTime()
    with grpc.insecure_channel("127.0.0.1:{}".format(port)) as channel:
        stub = engine_pb2_grpc.EngineStub(channel)
        stub.SendBench(engine_pb2.BenchParam(id=id, ts=ts))