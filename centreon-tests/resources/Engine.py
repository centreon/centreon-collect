import random
from robot.api import logger
import shutil
from os import makedirs
import sys
from os.path import exists, dirname

CONF_DIR = "/etc/centreon-engine"
ENGINE_HOME = "/var/lib/centreon-engine"
SCRIPT_DIR: str = dirname(__file__) + "/engine-scripts/"

class EngineInstance:
    def __init__(self, count: int):
        self.last_service_id = 0
        self.hosts = []
        self.last_host_id = 0
        self.last_host_group_id = 0
        self.commands_count = 50
        self.instances = count
        self.build_configs(50, 20)

    def create_centengine(self, id: int, debug_level=0):
        return ("#cfg_file={2}/config{0}/hostTemplates.cfg\n"
                "cfg_file={2}/config{0}/hosts.cfg\n"
                "#cfg_file={2}/config{0}/serviceTemplates.cfg\n"
                "cfg_file={2}/config{0}/services.cfg\n"
                "cfg_file={2}/config{0}/commands.cfg\n"
                "#cfg_file={2}/config{0}/contactgroups.cfg\n"
                "#cfg_file={2}/config{0}/contacts.cfg\n"
                "cfg_file={2}/config{0}/hostgroups.cfg\n"
                "#cfg_file={2}/config{0}/servicegroups.cfg\n"
                "cfg_file={2}/config{0}/timeperiods.cfg\n"
                "#cfg_file={2}/config{0}/escalations.cfg\n"
                "#cfg_file={2}/config{0}/dependencies.cfg\n"
                "cfg_file={2}/config{0}/connectors.cfg\n"
                "#cfg_file={2}/config{0}/meta_commands.cfg\n"
                "#cfg_file={2}/config{0}/meta_timeperiod.cfg\n"
                "#cfg_file={2}/config{0}/meta_host.cfg\n"
                "#cfg_file={2}/config{0}/meta_services.cfg\n"
                "broker_module=/usr/lib64/centreon-engine/externalcmd.so\n"
                "broker_module=/usr/lib64/nagios/cbmod.so /etc/centreon-broker/central-module.json\n"
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
                "grpc_port=50001\n"
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
                "log_legacy_enabled=1\n"
                "log_v2_logger=file\n"
                "log_level_functions=info\n"
                "log_level_config=info\n"
                "log_level_events=info\n"
                "log_level_checks=info\n"
                "log_level_notifications=info\n"
                "log_level_eventbroker=info\n"
                "log_level_external_command=info\n"
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
            "config": "define host {{\n" "host_name                      host_{0}\n    alias                          "
                      "host_{0}\n    address                        {1}.{2}.{3}.{4}\n    check_command                "
                      "  checkh{0}\n    check_period                   24x7\n    register                       1\n    "
                      "_KEY{0}                      VAL{0}\n    _SNMPCOMMUNITY                 public\n    "
                      "_SNMPVERSION                   2c\n    _HOST_ID                       {0}\n}}\n".format(
                hid, a, b, c, d),
            "hid": hid}
        return retval

    def create_service(self, host_id: int, cmd_ids):
        self.last_service_id += 1
        service_id = self.last_service_id

        command_id = random.randint(cmd_ids[0], cmd_ids[1])
        retval = """define service {{
    host_name                       host_{0}
    service_description             service_{1}
    _SERVICE_ID                     {1}
    check_command                   command_{2}
    max_check_attempts              3
    check_interval                  5
    retry_interval                  5
    register                        1
    active_checks_enabled           1
    passive_checks_enabled          1
}}
""".format(
            host_id, service_id, command_id)
        return retval

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

    def create_host_group(self, mbs):
        self.last_host_group_id += 1
        hid = self.last_host_group_id

        retval = """define hostgroup {{
    hostgroup_id                    {0}
    hostgroup_name                  hostgroup_{0}
    alias                           hostgroup_{0}
    members                         {1}
}}
""".format(hid, ",".join(mbs))
        logger.console(retval)
        return retval
        
    def build_configs(self, hosts: int, services_by_host: int, debug_level=0):
        if exists(CONF_DIR):
          shutil.rmtree(CONF_DIR)
        v = int(hosts / self.instances)
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
""")
            f.close()
            f = open(config_dir + "/hostgroups.cfg", "w")
            f.close()
            if not exists(ENGINE_HOME):
                makedirs(ENGINE_HOME)
            for file in ["check.pl", "notif.pl"]:
                shutil.copyfile("{0}/{1}".format(SCRIPT_DIR, file), "{0}/{1}".format(ENGINE_HOME, file))

def config_engine(num: int):
  global engine
  engine = EngineInstance(num)

def get_engines_count():
  return engine.instances

def engine_config_set_value(idx: int, key: str, value: str):
  filename = "/etc/centreon-engine/config{}/centengine.cfg".format(idx)
  f = open(filename, "r")
  lines = f.readlines()
  f.close()

  for i in range(len(lines)):
    if lines[i].startswith(key + "="):
      lines[i] = "{}={}\n".format(key, value)

  f = open(filename, "w")
  f.writelines(lines)
  f.close()

def add_host_group(index: int, members: list):
    mbs = []
    for m in members:
        if m in engine.hosts:
            mbs.append(m)

    f = open("/etc/centreon-engine/config{}/hostgroups.cfg".format(index), "a+")
    logger.console(mbs)
    f.write(engine.create_host_group(mbs))
    f.close()

def engine_log_duplicate(result: list):
    dup = True
    for i in result:
        if (i[0] % 2) != 0:
            dup = False
    return dup
