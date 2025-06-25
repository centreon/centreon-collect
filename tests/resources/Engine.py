#!/usr/bin/python3
#
# Copyright 2023-2024 Centreon
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
# This script is a little tcp server working on port 5669. It can simulate
# a cbd instance. It is useful to test the validity of BBDO packets sent by
# centengine.
import Common
import grpc
import math
from google.protobuf import empty_pb2
from google.protobuf.timestamp_pb2 import Timestamp
from google.protobuf.json_format import MessageToDict
import engine_pb2
import engine_pb2_grpc
import opentelemetry.proto.collector.metrics.v1.metrics_service_pb2
import opentelemetry.proto.collector.metrics.v1.metrics_service_pb2_grpc
import opentelemetry.proto.metrics.v1.metrics_pb2
from array import array
from dateutil import parser
import datetime
from os import makedirs, chmod
from os.path import exists, dirname
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
import json


sys.path.append('.')


SCRIPT_DIR: str = dirname(__file__) + "/engine-scripts/"
VAR_ROOT = BuiltIn().get_variable_value("${VarRoot}")
ETC_ROOT = BuiltIn().get_variable_value("${EtcRoot}")
CONF_DIR = ETC_ROOT + "/centreon-engine"
ENGINE_HOME = f"{VAR_ROOT}/lib/centreon-engine"
TIMEOUT = 30


class EngineInstance:
    def __init__(self, count: int, hosts: int = 50, srv_by_host: int = 20, sh_command: bool = False):
        self.last_service_id = 0
        self.hosts = []
        self.services = []
        self.service_by_host = srv_by_host
        self.last_host_id = 0
        self.last_host_group_id = 0
        self.commands_count = 50
        self.instances = count
        self.service_cmd = {}
        self.anomaly_detection_internal_id = 1
        self.build_configs(hosts, srv_by_host, 0, sh_command)
        makedirs(ETC_ROOT, mode=0o777, exist_ok=True)
        makedirs(VAR_ROOT, mode=0o777, exist_ok=True)
        makedirs(CONF_DIR, mode=0o777, exist_ok=True)
        makedirs(ENGINE_HOME, mode=0o777, exist_ok=True)
        makedirs(ETC_ROOT + "/centreon-broker", mode=0o777, exist_ok=True)
        makedirs(f"{VAR_ROOT}/log/centreon-engine/", mode=0o777, exist_ok=True)
        makedirs(f"{VAR_ROOT}/log/centreon-broker/", mode=0o777, exist_ok=True)

    def _create_centengine(self, id: int, debug_level=0):
        """
        _create_centengine

        Create the centengine.cfg file for the instance id

        Args:
            id (int): id
            debug_level (int, optional): Defaults to 0.
        """
        grpc_port = id + 50001
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
                "log_level_otl=trace\n"
                "log_flush_period=0\n"
                "soft_state_dependencies=0\n"
                "obsess_over_services=0\n"
                "process_performance_data=0\n"
                "check_for_orphaned_services=0\n"
                "check_for_orphaned_hosts=0\n"
                "check_service_freshness=1\n"
                "enable_flap_detection=0\n").format(id, debug_level, CONF_DIR, VAR_ROOT, ETC_ROOT, grpc_port)

    def _create_host(self):
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
            "config": f"define host {{\n" f"    host_name                      host_{hid}\n    alias                          "
            f"host_{hid}\n    address                        {a}.{b}.{c}.{d}\n    check_command                "
            f"  checkh{hid}\n    check_period                   24x7\n    register                       1\n    "
            f"_KEY{hid}                      VAL{hid}\n    _SNMPCOMMUNITY                 public\n    "
            f"_SNMPVERSION                   2c\n    _HOST_ID                       {hid}\n}}\n",
            "hid": hid}
        return retval

    def _create_service(self, host_id: int, cmd_ids: tuple[int, int]):
        self.last_service_id += 1
        service_id = self.last_service_id
        command_id = random.randint(cmd_ids[0], cmd_ids[1])
        self.service_cmd[service_id] = f"command_{command_id}"

        retval = f"""define service {{
    host_name                       host_{host_id}
    service_description             service_{service_id}
    _SERVICE_ID                     {service_id}
    check_command                   {self.service_cmd[service_id]}
    check_period                    24x7
    max_check_attempts              3
    check_interval                  5
    retry_interval                  5
    register                        1
    active_checks_enabled           1
    passive_checks_enabled          1
    _KEY_SERV{host_id}_{service_id}                VAL_SERV{service_id}
}}
"""
        return retval, service_id

    def _create_service_with_sh_command(self, host_id: int, service_index_in_host: int):
        """
            Create a service that uses command_{host_id}_{service_index_in_host}
            if service_id is a multiple of 10, we set _KO macro and check.sh will return 1
        """
        self.last_service_id += 1
        service_id = self.last_service_id
        cmd = f"command_{host_id}_{service_index_in_host}"
        self.service_cmd[service_id] = cmd

        retval = f"""define service {{
    host_name                       host_{host_id}
    service_description             service_{service_id}
    _SERVICE_ID                     {service_id}
    check_command                   {cmd}
    check_period                    24x7
    max_check_attempts              3
    check_interval                  1
    retry_interval                  1
    register                        1
    active_checks_enabled           1
    passive_checks_enabled          1
    _VAR1                           VAL_SERV_{host_id}_{service_id}
    _VAR2                           VAL_HOST_{host_id}
"""
        # for all service_id multiple of 10, check.sh exit with 1
        if service_id % 10 == 0:
            retval += "    _KO                             KO\n"

        retval += "}\n"
        return retval, service_id

    def ctn_create_anomaly_detection(self, host_id: int, dependent_service_id: int, metric_name: string, sensitivity: float = 0.0):
        """
            Create an anomaly detection service.
            Example:
            | `Create Anomaly Detection` | 1 | 2 | cpu | 0.0 |
        """
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
        """create_bam_timeperiod

        Create the timeperiod for the BAM module

        Create Bam Timeperiod
        """
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
        with open(f"{config_dir}/centreon-bam-timeperiod.cfg", "a+") as ff:
            ff.write(retval)

    def create_bam_command(self):
        """create_bam_command _summary_

        Create the command for the BAM module

        Create Bam Command
        """
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
        with open(f"{config_dir}/centreon-bam-command.cfg", "a+") as ff:
            ff.write(retval)

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
        with open(f"{config_dir}/centreon-bam-host.cfg", "a+") as ff:
            ff.write(retval)
        return host_id

    def create_bam_service(self, name: str, display_name: str, host_name: str, check_command: str):
        """create_bam_service _Create Bam Service_

        Create a service for the BAM module

        Args:
            name (str): service_name
            display_name (str): display_name
            host_name (str): host_name
            check_command (str): check_command
        """
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
        with open(f"{config_dir}/centreon-bam-services.cfg", "a+") as ff:
            ff.write(retval)
        return service_id

    @staticmethod
    def create_command(cmd):
        retval: str
        if cmd % 2 == 0:
            retval = f"""define command {{
    command_name                    command_{cmd}
    command_line                    {ENGINE_HOME}/check.pl --id {cmd}
    connector                       Perl Connector
}}
"""
        else:
            retval = f"""define command {{
    command_name                    command_{cmd}
    command_line                    {ENGINE_HOME}/check.pl --id {cmd}
}}
"""
        return retval

    @staticmethod
    def create_sh_command(host_id: int, service_id: int):
        return f"""define command {{
    command_name                    command_{host_id}_{service_id}
    command_line                    {ENGINE_HOME}/check.sh {host_id} {service_id}
}}
"""

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
        config_file = f"{CONF_DIR}/config{poller}/severities.cfg"
        with open(config_file, "w+") as ff:
            content = ""
            typ = ["service", "host"]
            for i in range(nb):
                level = i % 5 + 1
                content += f"""define severity {{
    id                     {i + 1}
    severity_name          severity{i + offset}
    level                  {level}
    icon_id                {6 - level}
    type                   {typ[i % 2]}
}}
"""
            ff.write(content)

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
    def create_dependencies_file(poller: int, dependenthost: str, host: str, dependentservice: str, service: str):
        config_file = f"{CONF_DIR}/config{poller}/dependencies.cfg"
        with open(config_file, "a+") as ff:
            content = """define servicedependency {{
    ;dependency_name               HD_test
    execution_failure_criteria     n 
    notification_failure_criteria  c 
    inherits_parent                1 
    dependent_host_name            {0} 
    host_name                      {1} 
    dependent_service_description  {2} 
    service_description            {3} 

    }}
    """.format(dependenthost, host, dependentservice, service)
            ff.write(content)

    @staticmethod
    def create_dependenciesgrp_file(poller: int, dependentservicegroup: str, servicegroup: str):
        config_file = f"{CONF_DIR}/config{poller}/dependencies.cfg"
        with open(config_file, "a+") as ff:
            content = """define servicedependency {{
    ;dependency_name               MSD_test 
    execution_failure_criteria     n 
    notification_failure_criteria  c 
    inherits_parent                1 
    dependent_servicegroup_name    {0} 
    servicegroup_name              {1} 

    }}
    """.format(dependentservicegroup, servicegroup)
            ff.write(content)

    @staticmethod
    def create_dependencieshst_file(poller: int, dependenthost: str, host: str):
        config_file = f"{CONF_DIR}/config{poller}/dependencies.cfg"
        with open(config_file, "a+") as ff:
            content = """define hostdependency {{
    ;dependency_name               HD_test2 
    execution_failure_criteria     n 
    notification_failure_criteria  d 
    inherits_parent                1 
    dependent_host_name            {0} 
    host_name                      {1} 

    }}
    """.format(dependenthost, host)
            ff.write(content)

    @staticmethod
    def create_dependencieshstgrp_file(poller: int, dependenthostgrp: str, hostgrp: str):
        config_file = f"{CONF_DIR}/config{poller}/dependencies.cfg"
        with open(config_file, "a+") as ff:
            content = """define hostdependency {{
    ;dependency_name               HD_test2 
    execution_failure_criteria     n 
    notification_failure_criteria  d 
    inherits_parent                1 
    dependent_hostgroup_name       {0} 
    hostgroup_name                 {1} 

    }}
    """.format(dependenthostgrp, hostgrp)
            ff.write(content)

    @staticmethod
    def create_template_file(poller: int, typ: str, what: str, ids):
        config_file = f"{CONF_DIR}/config{poller}/{typ}Templates.cfg"
        with open(config_file, "w+") as ff:
            content = ""
            idx = 1
            for i in ids:
                content += f"""define {typ} {{
name                   {typ}_template_{idx}
{what}               {i}
register               0
active_checks_enabled  1
passive_checks_enabled 1
}}
"""
                idx += 1
            ff.write(content)

    @staticmethod
    def create_tags(poller: int, nb: int, offset: int, tag_type: str):
        tt = ["servicegroup", "hostgroup", "servicecategory", "hostcategory"]

        config_file = f"{CONF_DIR}/config{poller}/tags.cfg"
        with open(config_file, "w+") as ff:
            content = ""
            tid = 0
            for i in range(nb):
                if len(tag_type) > 0:
                    typ = tag_type
                    tid += 1
                else:
                    if i % 4 == 0:
                        tid += 1
                    typ = tt[i % 4]
                content += f"""define tag {{
    id                     {tid}
    tag_name               tag{i + offset}
    type                   {typ}
}}
"""
            ff.write(content)

    def build_configs(self, hosts: int, services_by_host: int, debug_level=0, sh_command: bool = False):
        if exists(CONF_DIR):
            shutil.rmtree(CONF_DIR)
        r = 0
        if hosts % self.instances > 0:
            r = 1
        v = int(hosts / self.instances) + r
        for inst in range(self.instances):
            if v < hosts:
                nb_hosts = v
                hosts -= v
            else:
                nb_hosts = hosts
                hosts = 0

            config_dir = "{}/config{}".format(CONF_DIR, inst)
            makedirs(config_dir)
            with open(f"{config_dir}/centengine.cfg", "w") as f:
                bb = self._create_centengine(inst, debug_level=debug_level)
                f.write(bb)

            with open(f"{config_dir}/hosts.cfg", "w") as f:
                with open(f"{config_dir}/services.cfg", "w") as ff:
                    for i in range(1, nb_hosts + 1):
                        h = self._create_host()
                        f.write(h["config"])
                        self.hosts.append("host_{}".format(h["hid"]))
                        for j in range(1, services_by_host + 1):
                            if (sh_command):
                                svc = self._create_service_with_sh_command(
                                    h["hid"], j)
                            else:
                                svc = self._create_service(
                                    h["hid"], (inst * self.commands_count + 1, (inst + 1) * self.commands_count))
                            ff.write(svc[0])
                            self.services.append(f"service_{svc[1]}")

            with open(f"{config_dir}/commands.cfg", "w") as f:
                if (sh_command):
                    for host_id in range(1, nb_hosts + 1):
                        for service_id in range(1, services_by_host + 1):
                            f.write(self.create_sh_command(
                                host_id, service_id))
                else:
                    for i in range(inst * self.commands_count + 1, (inst + 1) * self.commands_count + 1):
                        f.write(self.create_command(i))
                for i in range(self.last_host_id):
                    f.write("""define command {{
    command_name                    checkh{1}
    command_line                    {0}/check.pl --id 0
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
            with open(f"{config_dir}/connectors.cfg", "w") as f:
                f.write("""define connector {
    connector_name                 Perl Connector
    connector_line                 /usr/lib64/centreon-connector/centreon_connector_perl --debug --log-file=/tmp/var/log/centreon-engine/config0/connector_perl.log
}

define connector {
    connector_name                 SSH Connector
    connector_line                 /usr/lib64/centreon-connector/centreon_connector_ssh --debug --log-file=/tmp/var/log/centreon-engine/config0/connector_ssh.log
}
""")
            with open(f"{config_dir}/resource.cfg", "w") as f:
                f.write("""$USER1$=/usr/lib64/nagios/plugins
$CENTREONPLUGINS$=/usr/lib/centreon/plugins""")
            with open(f"{config_dir}/timeperiods.cfg", "w") as f:
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
    name                           workhours
    timeperiod_name                workhours
    alias                          Work Hours
    sunday                         09:00-12:00,14:00-18:00
    monday                         09:00-12:00,14:00-18:00
    tuesday                        09:00-12:00,14:00-18:00
    wednesday                      09:00-12:00,14:00-18:00
    thursday                       09:00-12:00,14:00-18:00
    friday                         09:00-12:00,14:00-18:00
    saturday                       09:00-12:00,14:00-18:00
}
""")
            with open(f"{config_dir}/hostgroups.cfg", "w") as f:
                pass
            with open(f"{config_dir}/contacts.cfg", "w") as f:
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
            with open(f"{config_dir}/dependencies.cfg", "w") as f:
                f.write("#dependencies.cfg\n")

            with open(f"{config_dir}/contactgroups.cfg", "w") as f:
                f.write("#contactgroups.cfg\n")

            with open(f"{config_dir}/escalations.cfg", "w") as f:
                pass

            if not exists(ENGINE_HOME):
                makedirs(ENGINE_HOME)
            for file in ["check.pl", "check.sh", "notif.pl"]:
                shutil.copyfile(f"{SCRIPT_DIR}/{file}",
                                f"{ENGINE_HOME}/{file}")
                chmod(f"{ENGINE_HOME}/{file}", stat.S_IRWXU |
                      stat.S_IRGRP | stat.S_IXGRP)
            if not exists(f"{ENGINE_HOME}/config{inst}/rw"):
                makedirs(f"{ENGINE_HOME}/config{inst}/rw")

    def centengine_conf_add_bam(self):
        """
        centengine_conf_add_bam _entengine Conf Add Bam_

        Add the bam configuration to the centengine.cfg file
        """
        config_dir = "{}/config0".format(CONF_DIR)
        with open(f"{config_dir}/centengine.cfg", "r") as f:
            lines = f.readlines()
        lines_to_prep = ["cfg_file=" + ETC_ROOT + "/centreon-engine/config0/centreon-bam-command.cfg\n", "cfg_file=" + ETC_ROOT + "/centreon-engine/config0/centreon-bam-timeperiod.cfg\n",
                         "cfg_file=" + ETC_ROOT + "/centreon-engine/config0/centreon-bam-host.cfg\n", "cfg_file=" + ETC_ROOT + "/centreon-engine/config0/centreon-bam-services.cfg\n"]
        with open(f"{config_dir}/centengine.cfg", "w") as f:
            f.writelines(lines_to_prep)
            f.writelines(lines)

    def centengine_conf_add_anomaly(self):
        config_dir = f"{CONF_DIR}/config0"
        with open(f"{config_dir}/centengine.cfg", "r") as f:
            lines = f.readlines()
        with open(f"{config_dir}/centengine.cfg", "w") as f:
            f.writelines(f"cfg_file={config_dir}/anomaly_detection.cfg\n")
            f.writelines(lines)


engine = None


def ctn_config_engine(num: int, hosts: int = 50, srv_by_host: int = 20, sh_command: bool = False):
    """
    Configure all the necessary files for num instances of centengine.

    Args:
        num (int): How many engine configurations to start
        hosts (int, optional): Defaults to 50.
        srv_by_host (int, optional): Defaults to 20.
        sh_command: if True, services will use check.sh instead of check.pl, services will have some extra macros
    """
    global engine
    engine = EngineInstance(num, hosts, srv_by_host, sh_command)


def ctn_get_engines_count():
    """
    Return the number of centengine configurations.

    Returns:
        The number of running centengine instances
    """
    if engine is None:
        return 0
    else:
        return engine.instances


def ctn_engine_config_set_value(idx: int, key: str, value: str, force: bool = False):
    """
    Set a value in the centengine.cfg

    Args:
        idx (int): Index of the Engine configuration (from 0)
        key (str): the key whose value needs to change.
        value (str): the new value to set.
        force (bool, optional): Defaults to False. If the key doesn't exist in the configuration, and force is set to
        true, the key will be added to the file.
    """
    filename = f"{ETC_ROOT}/centreon-engine/config{idx}/centengine.cfg"
    with open(filename, "r") as f:
        lines = f.readlines()

    replaced = False
    for i in range(len(lines)):
        if lines[i].startswith(key + "="):
            lines[i] = "{}={}\n".format(key, value)
            replaced = True

    if not replaced and force:
        lines.append("{}={}\n".format(key, value))

    with open(filename, "w") as f:
        f.writelines(lines)


def ctn_engine_config_add_value(idx: int, key: str, value: str):
    """
    ctn_engine_config_add_value _Engine Config Add Value_

    Run a command to add a value in the centengine.cfg for the config idx.

    Args:
        idx (int): idx index of the configuration (from 0)
        key (str): the key to change the value.
        value (str): the new value to set to the key variable.
    """
    filename = f"{ETC_ROOT}/centreon-engine/config{idx}/centengine.cfg"
    with open(filename, "a") as f:
        f.write(f"{key}={value}")


def ctn_engine_config_set_value_in_services(idx: int, desc: str, key: str, value: str):
    """
    Set a parameter in the services.cfg.

    Args:
        idx (int): Index of the centengine configuration (from 0).
        desc (str): Service description of the service to modify.
        key (str): The key whose value needs to change.
        value (str): The new value to set.
    """
    filename = f"{ETC_ROOT}/centreon-engine/config{idx}/services.cfg"
    with open(filename, "r") as f:
        lines = f.readlines()

    r = re.compile(rf"^\s*service_description\s+{desc}\s*$")
    for i in range(len(lines)):
        if r.match(lines[i]):
            lines.insert(i + 1, "    {}              {}\n".format(key, value))

    with open(filename, "w") as f:
        f.writelines(lines)


def ctn_engine_config_replace_value_in_services(idx: int, desc: str, key: str, value: str):
    """
    Changes the value of a parameter in the services.cfg file for the centengine number idx.

    Args:
        idx (int): Index of the configuration (from 0)
        desc (str): Service description of the service to modify.
        key (str): Name of the parameter to change.
        value (str): New value to set.
    """
    filename = f"{ETC_ROOT}/centreon-engine/config{idx}/services.cfg"
    with open(filename, "r") as f:
        lines = f.readlines()
    r = re.compile(rf"^\s*service_description\s+{desc}\s*$")
    rkey = re.compile(rf"^\s*{key}\s+[\w\.]+\s*$")
    for i in range(len(lines)):
        if r.match(lines[i]):
            i -= 1
            while i < len(lines) and lines[i] != "}":
                if rkey.match(lines[i]):
                    lines[i] = f"    {key}                 {value}\n"
                    break
                i += 1

    with open(filename, "w") as f:
        f.writelines(lines)


def ctn_engine_config_set_value_in_hosts(idx: int, desc: str, key: str, value: str, file: str = 'hosts.cfg'):
    """
    Set a parameter in the hosts.cfg for the Engine configuration idx.

    Args:
        idx (int): Index of the Engine configuration (from 0)
        desc (str): host name of the host to modify.
        key (str): the parameter whose value has to change.
        value (str): the value to set.
        file (str): The file to modify, default value 'hosts.cfg'
    """
    filename = f"{ETC_ROOT}/centreon-engine/config{idx}/{file}"
    with open(filename, "r") as f:
        lines = f.readlines()

    r = re.compile(rf"^\s*host_name\s+{desc}\s*$")
    rbis = re.compile(rf"^\s*name\s+{desc}\s*$")
    found = False
    for i in range(len(lines)):
        if r.match(lines[i]):
            lines.insert(i + 1, f"    {key}              {value}\n")
            found = True
            break

    if not found:
        for i in range(len(lines)):
            if rbis.match(lines[i]):
                lines.insert(i + 1, f"    {key}              {value}\n")
                found = True
                break
    with open(filename, "w") as f:
        f.writelines(lines)


def ctn_engine_config_delete_value_in_hosts(idx: int, desc: str, key: str, file: str = 'hosts.cfg'):
    """
    Delete a parameter in the hosts.cfg for the Engine configuration idx.

    Args:
        idx (int): Index of the Engine configuration (from 0)
        desc (str): host name of the host to modify.
        key (str): the parameter that will be deleted.
        file (str): The file to modify, default value 'hosts.cfg'
    """

    filename = f"{ETC_ROOT}/centreon-engine/config{idx}/{file}"
    with open(filename, "r") as f:
        lines = f.readlines()

    r = re.compile(rf"^\s*host_name\s+{desc}\s*$")
    rbis = re.compile(rf"^\s*name\s+{desc}\s*$")
    found = False
    for i in range(len(lines)):
        if r.match(lines[i]):
            print("here" + lines[i])
            for j in range(i + 1, len(lines)):
                if '}' in lines[j]:
                    break
                if key in lines[j]:
                    del lines[j]
                    found = True
                    break
            break

    if not found:
        for i in range(len(lines)):
            if rbis.match(lines[i]):
                for j in range(i + 1, len(lines)):
                    if '}' in lines[j]:
                        break
                    if key in lines[j]:
                        del lines[j]
                        found = True
                        break
                break
    with open(filename, "w") as f:
        f.writelines(lines)


def ctn_engine_config_replace_value_in_hosts(idx: int, desc: str, key: str, value: str, file: str = 'hosts.cfg'):
    """
    Change a parameter in the hosts.cfg file of the Engine config idx.

    Args:
        idx (int): index of the configuration (from 0).
        desc (str): host name of the host to modify.
        key (str): the parameter whose value has to change.
        value (str): the new value to set.
        file (str): The file to modify, default value 'hosts.cfg'.
    """
    filename = f"{ETC_ROOT}/centreon-engine/config{idx}/{file}"
    with open(filename, "r") as f:
        lines = f.readlines()

    r = re.compile(rf"^\s*host_name\s+{desc}\s*$")
    rbis = re.compile(rf"^\s*name\s+{desc}\s*$")
    rkey = re.compile(rf"^\s*{key}\s+[\w\.]+\s*$")
    found = False
    for i in range(len(lines)):
        if r.match(lines[i]):
            while i < len(lines) and lines[i] != "}":
                if rkey.match(lines[i]):
                    lines[i] = f"    {key}              {value}\n"
                    found = True
                    break
                i += 1
        if found:
            break

    if not found:
        for i in range(len(lines)):
            if rbis.match(lines[i]):
                while i < len(lines) and lines[i] != "}":
                    if rkey.match(lines[i]):
                        lines[i] = f"    {key}              {value}\n"
                        found = True
                        break
                    i += 1
            if found:
                break

    with open(filename, "w") as f:
        f.writelines(lines)


def ctn_engine_config_change_command(idx: int, command_index: str, new_command: str):
    """
    Changes the command line of command whose index is command_index in the Engine config idx.

    Args:
        idx (int): Index of the configuration (from 0)
        command_index (str): Index of the command (may be a regex)
        new_command (str): The new command line.
    """
    with open(f"{CONF_DIR}/config{idx}/commands.cfg", "r") as f:
        lines = f.readlines()
    new_lines = []
    r = re.compile(rf"^\s+command_name\s+command_{command_index}$")
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
    with open(f"{CONF_DIR}/config0/commands.cfg", "w") as f:
        f.writelines(new_lines)


def ctn_engine_config_add_command(idx: int, command_name: str, new_command: str, connector: str = ""):
    """
    Add a new command in the commands.cfg for the Engine config idx.

    Args:
        idx (int): Index of the Engine configuration (from 0)
        command_name (str): Command name
        new_command (str): Command line
        connector (str, optional): Defaults to None.
    """
    with open(f"{CONF_DIR}/config{idx}/commands.cfg", "a") as f:
        if connector == "":
            f.write(f"""define command {{
        command_name                   {command_name}
        command_line                   {new_command}
    }}
        """)
        else:
            f.write(f"""define command {{
        command_name                   {command_name}
        command_line                   {new_command}
        connector                      {connector}
    }}
        """)


def ctn_engine_config_set_value_in_contacts(idx: int, desc: str, key: str, value: str):
    """
    Modify a parameter in the contacts.cfg for the Engine config idx.

    Args:
        idx (int): Index of the configuration (from 0)
        desc (str): Contact name
        key (str): The parameter whose value must change.
        value (str): The new value to set.
    """
    filename = f"{ETC_ROOT}/centreon-engine/config{idx}/contacts.cfg"
    with open(filename, "r") as f:
        lines = f.readlines()

    r = re.compile(rf"^\s*contact_name\s+{desc}\s*$")
    for i in range(len(lines)):
        if r.match(lines[i]):
            lines.insert(i + 1, f"    {key}              {value}\n")
            break

    with open(filename, "w") as f:
        f.writelines(lines)


def ctn_engine_config_set_value_in_escalations(idx: int, desc: str, key: str, value: str):
    """
    Replace a value in the escalations.cfg for the config idx

    Args:
        idx (int): Index of the Engine configuration (from 0)
        desc (str): Escalation name
        key (str): the parameter whose value must change.
        value (str): the new value to set.
    """
    with open(f"{ETC_ROOT}/centreon-engine/config{idx}/escalations.cfg", "r") as ff:
        lines = ff.readlines()
    r = re.compile(rf"^\s*;escalation_name\s+{desc}\s*$")
    for i in range(len(lines)):
        m = r.match(lines[i])
        if m is not None:
            lines.insert(i + 1, f"    {key}                     {value}\n")
    with open(f"{ETC_ROOT}/centreon-engine/config{idx}/escalations.cfg", "w") as ff:
        ff.writelines(lines)


def ctn_engine_config_set_value_in_dependencies(idx: int, desc: str, key: str, value: str):
    """
    Set a value in the dependencies.cfg for the config idx

    Args:
        idx (int): Index of the Engine configuration (from 0)
        desc (str): dependency name
        key (str): the parameter whose value must change.
        value (str): the new value to set.
    """
    with open(f"{ETC_ROOT}/centreon-engine/config{idx}/dependencies.cfg", "r") as ff:
        lines = ff.readlines()
    r = re.compile(rf"^\s*;;dependency_name\s+{desc}\s*$")
    for i in range(len(lines)):
        m = r.match(lines[i])
        if m is not None:
            lines.insert(i + 1, f"    {key}                     {value}\n")
    with open(f"{ETC_ROOT}/centreon-engine/config{idx}/dependencies.cfg", "w") as ff:
        ff.writelines(lines)


def ctn_engine_config_remove_service_host(idx: int, host: str):
    """
    Remove all the services of a host from the services.cfg file.

    Args:
        idx (int): index of the configuration (from 0)
        host (str): Host name
    """
    filename = f"{ETC_ROOT}/centreon-engine/config{idx}/services.cfg"
    with open(filename, "r") as f:
        lines = f.readlines()
    host_name = re.compile(rf"^\s*host_name\s+{host}\s*$")
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

    with open(filename, "w") as f:
        f.writelines(lines)


def ctn_engine_config_remove_host(idx: int, host: str):
    """
    Remove a host from the hosts.cfg configuration file.

    Args:
        idx (int): Index of the configuration (from 0)
        host (str): name of the host wanted to be removed
    """
    filename = f"{ETC_ROOT}/centreon-engine/config{idx}/hosts.cfg"
    with open(filename, "r") as f:
        lines = f.readlines()

    host_name = re.compile(rf"^\s*host_name\s+{host}\s*$")
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

    with open(filename, "w") as f:
        f.writelines(lines)


def ctn_engine_config_rename_host(idx: int, old_host_name: str, new_host_name: str):
    """
    Rename a host from the hosts.cfg configuration file.

    Args:
        idx (int): Index of the configuration (from 0)
        old_host_name (str): name of the host wanted to be renamed
        new_host_name (str): new name of the host
    """
    filename = f"{ETC_ROOT}/centreon-engine/config{idx}/hosts.cfg"
    with open(filename, "r") as f:
        lines = f.readlines()

    host_name = re.compile(r"^\s*host_name\s+" + old_host_name + "\s*$")

    for i in range(len(lines)):
        if host_name.match(lines[i]):
            lines[i] = f"    host_name\t{new_host_name}\n"
            break

    with open(filename, "w") as f:
        f.writelines(lines)


def ctn_engine_config_set_host_value(idx: int, host: str, key: str, value: str):
    """
    set a value of a host in the hosts.cfg configuration file.

    Args:
        idx (int): Index of the configuration (from 0)
        host (str): name of the host
        key (str): the parameter whose value must change.
        value (str): the new value to set.
    """
    filename = f"{ETC_ROOT}/centreon-engine/config{idx}/hosts.cfg"
    with open(filename, "r") as f:
        lines = f.readlines()

    key_name = re.compile(r"^\s*" + key)
    host_name = re.compile(r"^\s*host_name\s+" + host + "\s*$")
    host_end = re.compile(r"^}$")
    host_begin_idx = 0
    replaced = False
    while not replaced:
        if (host_begin_idx >= len(lines)):
            break
        if (host_name.match(lines[host_begin_idx])):
            for host_line_idx in range(host_begin_idx, len(lines)):
                if (key_name.match(lines[host_line_idx])):
                    lines[host_line_idx] = f"    {key}              {value}\n"
                    replaced = True
                    break
                elif host_end.match(lines[host_line_idx]):
                    host_begin_idx = host_line_idx
                    break
        else:
            host_begin_idx = host_begin_idx + 1

    with open(filename, "w") as f:
        f.writelines(lines)


def ctn_add_host_group(index: int, id_host_group: int, members: list):
    """
    Add a host group on the engine instance index

    Args:
        index (int): index of the configuration (from 0)
        id_host_group (int): ID of the new host group to add.
        members (list): A list of host names.
    """
    mbs = [line for line in members if line in engine.hosts]
    with open(f"{ETC_ROOT}/centreon-engine/config{index}/hostgroups.cfg", "a+") as f:
        logger.console(mbs)
        f.write(engine.create_host_group(id_host_group, mbs))


def ctn_rename_host_group(index: int, id_host_group: int, name: str, members: list):
    """
    Rename a host group on the engine instance index. It also modifies its members.

    Warning:
        This function changes the configuration file but not the internal configuration. It can lead to conflicts.

    Args:
        index (int): index of the configuration (from 0)
        id_host_group (int): Host group ID.
        name (str): host_group_name
        members (list): The new list of host members.
    """
    mbs = [line for line in members if line in engine.hosts]
    mbs_str = ",".join(mbs)
    with open(f"{ETC_ROOT}/centreon-engine/config{index}/hostgroups.cfg", "w") as f:
        logger.console(mbs)
        f.write(f"""define hostgroup {{
    hostgroup_id                    {id_host_group}
    hostgroup_name                  hostgroup_{name}
    alias                           hostgroup_{name}
    members                         {mbs_str}
}}
""")


def ctn_rename_service(index: int, hst: str, svc: str, new_svc: str):
    """
    Rename a service on the engine instance index.

    Args:
        index (int): Index of the configuration(from 0).
        hst (str): The host containing the service.
        svc (str): The description of the service.
        new_svc (str): The new description of the service.
    """
    with open(f"{ETC_ROOT}/centreon-engine/config{index}/services.cfg", "r") as f:
        ll = f.readlines()
    rs_start = re.compile(r"^\s*define service {")
    rs_end = re.compile(r"^\s*}")
    rs_hst = re.compile(r"^\s*host_name\s+([a-z_0-9]+)")
    rs_svc = re.compile(r"^\s*service_description\s+([a-z_0-9]+)")
    inside = False
    my_hst = None
    my_svc = None
    l_svc = None

    for i in range(len(ll)):
        line = ll[i]
        if inside:
            if rs_end.match(line):
                inside = False
                if svc == my_svc and hst == my_hst:
                    ll[l_svc] = f"    service_description\t{new_svc}\n"
                svc, hst, l_svc = None, None, None
                continue
            m = rs_hst.search(line)
            if m:
                my_hst = m.group(1)
            else:
                m = rs_svc.search(line)
                if m:
                    my_svc = m.group(1)
                    l_svc = i

        else:
            if rs_start.match(line):
                inside = True

    with open(f"{ETC_ROOT}/centreon-engine/config{index}/services.cfg", "w") as f:
        f.writelines(ll)


def ctn_add_service_group(index: int, id_service_group: int, members: list):
    """
    Add a service group on the engine instance index.

    Args:
        index (int): index of the configuration (from 0)
        id_service_group (int): ID of the new service group.
        members (list): A list of its members.
    """
    with open(
            ETC_ROOT + "/centreon-engine/config{}/servicegroups.cfg".format(index), "a+") as f:
        logger.console(members)
        f.write(engine.create_service_group(id_service_group, members))


def ctn_rename_service_group(index: int, old_servicegroup_name: str, new_service_group_name: str):
    """
    Rename a service group.

    Args:
        index: index of the poller configuration (from 0)
        old_servicegroup_name: Service group name to look for and to replace.
        new_service_group_name: New name.
    """
    with open(f"{ETC_ROOT}/centreon-engine/config{index}/servicegroups.cfg", "r") as f:
        ll = f.readlines()
    group_name_search = re.compile(
        fr"^\s+servicegroup_name\s+{old_servicegroup_name}$")
    for i in range(len(ll)):
        line = ll[i]
        if group_name_search.match(line):
            ll[i] = f"    servicegroup_name                  {new_service_group_name}\n"
            break
    with open(f"{ETC_ROOT}/centreon-engine/config{index}/servicegroups.cfg", "w") as f:
        f.writelines(ll)


def ctn_add_contact_group(index: int, id_contact_group: int, members: list):
    """
    Add a contact group on the engine instance index.

    Args:
        index (int): Index of the poller configuration (from 0).
        id_contact_group (int): ID of new contactgroup.
        members (list): A list of the members (by name).
    """
    with open(f"{ETC_ROOT}/centreon-engine/config{index}/contactgroups.cfg", "a+") as f:
        logger.console(members)
        f.write(engine.create_contact_group(id_contact_group, members))


def ctn_create_service(index: int, host_id: int, cmd_id: int):
    """
    Create a service on the engine instance index, on the host host_id, with the command cmd_id.

    Args:
        index (int): Index of the poller configuration (from 0).
        host_id (int): The host ID of the new service to create.
        cmd_id (int): The command ID this new service has to use.

    Returns:
        A service ID.

    Example:
    | ${svc_id} | Create Service | 0 | 1 | 1 |
    """
    global engine
    with open(f"{ETC_ROOT}/centreon-engine/config{index}/services.cfg", "a+") as f:
        svc = engine._create_service(host_id, (1, cmd_id))
        retval = svc[1]
        f.write(svc[0])
    return retval


def ctn_create_anomaly_detection(index: int, host_id: int, dependent_service_id: int, metric_name: string, sensitivity: float = 0.0):
    """
    Create an anomaly detection on the engine instance with the given index.

    Args:
        index (int): index of the Engine configuration (from 0)
        host_id (int): ID of the host containing the new anomaly detection.
        dependent_service_id (int): ID of the dependent service linked to the new anomaly detection.
        metric_name (string): The service metric name used for the anomaly detection.
        sensitivity (float, optional): Defaults to 0.0.

    Returns:
        The ID of the new anomaly detection.
    """
    with open(
            f"{ETC_ROOT}/centreon-engine/config{index}/anomaly_detection.cfg", "a+") as f:
        to_append = engine.ctn_create_anomaly_detection(
            host_id, dependent_service_id, metric_name, sensitivity)
        lst = to_append.split('\n')
        good = [line for line in lst if "service_id" in line][0]
        m = re.search(r"service_id\s+([^\s]*)$", good)
        if m is not None:
            retval = int(m.group(1))
        else:
            raise Exception(
                "Impossible to get the service id from '{}'".format(good))
            m = 0
        f.write(to_append)
    engine.centengine_conf_add_anomaly()
    return retval


def ctn_clone_engine_config_to_db():
    """
    Clone all the Engine configurations to the database. In other words, create
    the current configuration in the centreon database.
    """
    global dbconf
    dbconf = db_conf.DbConf(engine)
    dbconf.create_conf_db()


def ctn_add_bam_config_to_engine():
    """
    Add the bam configuration to the Engine.
    """
    global dbconf
    dbconf.init_bam()


def ctn_create_ba_with_services(name: str, typ: str, svc: list, dt_policy="inherit"):
    """
    Create a BA with the given services.

    Args:
        name (str): name of the ba
        typ (str): type of the ba: worst, best, ratio_percent, ratio_number, impact.
        svc (list): services name chosen to create the ba.
        dt_policy (str, optional): Defaults to "inherit": inherit, ignore, ignore_all.

    Returns:
        A tuple(BA ID, virtual service associated to the BA).
    """
    global dbconf
    return dbconf.ctn_create_ba_with_services(name, typ, svc, dt_policy)


def ctn_create_ba(name: str, typ: str, critical_impact: int, warning_impact: int, dt_policy="inherit", activate: int = 1):
    """
    Create a BA.

    Args:
        name (str): the BA name.
        typ (str): The type of the ba (worst,best,impact, ...)
        critical_impact (int): Impact weight in the event of a Critical condition, in real-time monitoring
        warning_impact (int): Impact weight in the event of a Warning condition, in real-time monitoring. Ignored if indicator is a boolean rule
        dt_policy (str, optional): Defaults to "inherit": inherit, ignore, ignore_all
        activate: 1 for enable, 0 for disable

    Returns:
        A tuple(BA ID, virtual service associated to the BA).
    """
    global dbconf
    return dbconf.ctn_create_ba(name, typ, critical_impact, warning_impact, dt_policy, activate)


def ctn_add_relations_ba_timeperiods(id_ba: int, id_time_period: int):
    """
    add a line in mod_bam_relations_ba_timeperiods table

    Args:
        id_ba: 
        id_time_period:
    """

    global dbconf
    return dbconf.ctn_add_relations_ba_timeperiods(id_ba, id_time_period)


def ctn_add_boolean_kpi(id_ba: int, expression: str, impact_if: bool, critical_impact: int):
    """
    Add a boolean KPI to a BA.

    Args:
        id_ba (int): The BA ID.
        expression (str): An expression.
        impact_if (bool): (true/false)
        critical_impact (int): Impact weight in the event of a Critical condition, in real-time monitoring

    Returns:
        The ID of the boolean expression.
    """
    return dbconf.ctn_add_boolean_kpi(id_ba, expression, impact_if, critical_impact)


def ctn_update_boolean_rule(boolean_id: int, expression: str):
    """
    Udpate a boolean rule.

    Args:
        boolean_id (int): The ID of the boolean expression to change.
        expression (str): The new expression.
    """
    dbconf.ctn_update_boolean_rule(boolean_id, expression)


def ctn_add_ba_kpi(id_ba_src: int, id_ba_dest: int, critical_impact: int, warning_impact: int, unknown_impact: int):
    """
    Add a BA KPI.

    Args:
        id_ba_src (int): The ID of the daughter BA.
        id_ba_dest (int): The ID of the mother BA.
        critical_impact (int): Impact weight in the event of a Critical condition, in real-time monitoring
        warning_impact (int): Impact weight in the event of a Warning condition, in real-time monitoring. Ignored if indicator is a boolean rule
        unknown_impact (int): _Impact weight in the event of an Unknown condition, in real-time monitoring. Ignored if indicator is a boolean rule
    """
    dbconf.ctn_add_ba_kpi(id_ba_src, id_ba_dest, critical_impact,
                          warning_impact, unknown_impact)


def ctn_add_service_kpi(host: str, serv: str, id_ba: int, critical_impact: int, warning_impact: int, unknown_impact: int):
    """
    Add a service KPI.

    Args:
        host (str): Host name of the host containing the service.
        serv (str): Service description of the service.
        id_ba (int): ID of the parent BA of the service KPI.
        critical_impact (int): Impact weight in the event of a Critical condition, in real-time monitoring
        warning_impact (int): Impact weight in the event of a Warning condition, in real-time monitoring. Ignored if indicator is a boolean rule
        unknown_impact (int): _Impact weight in the event of an Unknown condition, in real-time monitoring. Ignored if indicator is a boolean rule
    """
    global dbconf
    dbconf.ctn_add_service_kpi(
        host, serv, id_ba, critical_impact, warning_impact, unknown_impact)


def ctn_remove_service_kpi(id_ba: int, host: str, svc: str):
    """
    Remove a service kpi given by hostname/service description from a ba given by its id.

    Args:
        id_ba: The BA ID.
        host: the host name.
        svc: the service description.
    """
    global dbconf
    dbconf.ctn_remove_service_kpi(id_ba, host, svc)


def ctn_get_service_command_id(service: int):
    """
    Get the command ID of the service with the given ID.

    Args:
        service (int): ID of the service.

    Returns:
        The command ID.
    """
    global engine
    return engine.service_cmd[service][8:]


def ctn_change_normal_svc_check_interval(use_grpc: int, hst: str, svc: str, check_interval: int):
    """
    Update the normal check interval for a service.

    Args:
        use_grpc (int): If not zero, the action is made by gRPC, otherwise it is done with a legacy command.
        hst (str): Host name of host containing the service.
        svc (str): Service description.
        check_interval (int): new check interval in seconds.
    """
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.ChangeServiceObjectIntVar(engine_pb2.ChangeObjectInt(
                host_name=hst, service_desc=svc, mode=engine_pb2.ChangeObjectInt.Mode.NORMAL_CHECK_INTERVAL, dval=check_interval))
    else:
        now = int(time.time())
        cmd = "[{}] CHANGE_NORMAL_SVC_CHECK_INTERVAL;{};{};{}\n".format(
            now, hst, svc, check_interval)
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_change_normal_host_check_interval(use_grpc: int, hst: str, check_interval: int):
    """
    Update the normal check interval for a host.

    Args:
        use_grpc (int): if not zero by grpc, otherwise using legacy commands.
        hst (str): host name.
        check_interval (int): new check interval in seconds.
    """
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.ChangeHostObjectIntVar(engine_pb2.ChangeObjectInt(
                host_name=hst, mode=engine_pb2.ChangeObjectInt.Mode.NORMAL_CHECK_INTERVAL, dval=check_interval))
    else:
        now = int(time.time())
        cmd = f"[{now}] CHANGE_NORMAL_HOST_CHECK_INTERVAL;{hst};{check_interval}\n"
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_change_retry_svc_check_interval(use_grpc: int, hst: str, svc: str, retry_interval: int):
    """
    Change the retry check interval of a service.

    Args:
        use_grpc (int): if not zero by grpc, otherwise with legacy commands.
        hst (str): Host name of the service.
        svc (str): Description of the service.
        retry_interval (int): New retry interval in seconds.
    """
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.ChangeServiceObjectIntVar(engine_pb2.ChangeObjectInt(
                host_name=hst, service_desc=svc, mode=engine_pb2.ChangeObjectInt.Mode.RETRY_CHECK_INTERVAL, dval=retry_interval))
    else:
        now = int(time.time())
        cmd = f"[{now}] CHANGE_RETRY_SVC_CHECK_INTERVAL;{hst};{svc};{retry_interval}\n"
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_change_retry_host_check_interval(use_grpc: int, hst: str, retry_interval: int):
    """
    Change the retry check interval for a host.

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): Host name of the concerned host.
        retry_interval (int): New retry interval in seconds.
    """
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.ChangeHostObjectIntVar(engine_pb2.ChangeObjectInt(
                host_name=hst, mode=engine_pb2.ChangeObjectInt.Mode.RETRY_CHECK_INTERVAL, dval=retry_interval))
    else:
        now = int(time.time())
        cmd = f"[{now}] CHANGE_RETRY_HOST_CHECK_INTERVAL;{hst};{retry_interval}\n"
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_change_max_svc_check_attempts(use_grpc: int, hst: str, svc: str, max_check_attempts: int):
    """
    Change the max check attempts for a service.

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): host name of the service.
        svc (str): service description.
        max_check_attempts (int): number of max check attempts wanted.
    """
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.ChangeServiceObjectIntVar(engine_pb2.ChangeObjectInt(
                host_name=hst, service_desc=svc, mode=engine_pb2.ChangeObjectInt.Mode.MAX_ATTEMPTS, intval=max_check_attempts))
    else:
        now = int(time.time())
        cmd = f"[{now}] CHANGE_MAX_SVC_CHECK_ATTEMPTS;{hst};{svc};{max_check_attempts}\n"
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_change_max_host_check_attempts(use_grpc: int, hst: str, max_check_attempts: int):
    """
    Change the max check attempts of a host.

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): Host name.
        max_check_attempts (int): number of max check attempts wanted.
    """
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.ChangeHostObjectIntVar(engine_pb2.ChangeObjectInt(
                host_name=hst, mode=engine_pb2.ChangeObjectInt.Mode.MAX_ATTEMPTS, intval=max_check_attempts))
    else:
        now = int(time.time())
        cmd = "[{}] CHANGE_MAX_HOST_CHECK_ATTEMPTS;{};{}\n".format(
            now, hst, max_check_attempts)
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_change_host_check_timeperiod(use_grpc: int, hst: str, check_timeperiod: str):
    """
    Change the check timeperiod for a host.

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): host name.
        check_timeperiod (str): check time period to set (examples: 24x7, 24x6, workhours..).
    """
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.ChangeHostObjectCharVar(engine_pb2.ChangeObjectChar(
                host_name=hst, mode=engine_pb2.ChangeObjectChar.Mode.CHANGE_CHECK_TIMEPERIOD, charval=check_timeperiod))
    else:
        now = int(time.time())
        cmd = "[{}] CHANGE_HOST_CHECK_TIMEPERIOD;{};{}\n".format(
            now, hst, check_timeperiod)
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_change_host_notification_timeperiod(use_grpc: int, hst: str, notification_timeperiod: str):
    """
    Change the host notification timeperiod for a given host.

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str):  host name of the concerned host.
        notification_timeperiod (str): notification check period (24x7, 24x6, workhours..)
    """
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.ChangeHostObjectCharVar(engine_pb2.ChangeObjectChar(
                host_name=hst, mode=engine_pb2.ChangeObjectChar.Mode.CHANGE_NOTIFICATION_TIMEPERIOD, charval=notification_timeperiod))
    else:
        now = int(time.time())
        cmd = "[{}] CHANGE_HOST_NOTIFICATION_TIMEPERIOD;{};{}\n".format(
            now, hst, notification_timeperiod)
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_change_svc_check_timeperiod(use_grpc: int, hst: str, svc: str, check_timeperiod: str):
    """
    Change the service check timeperiod for a service.

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): host name of the service.
        svc (str): service description of the service.
        check_timeperiod (str): check period (24x7, 24x6, workhours..)
    """
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.ChangeServiceObjectCharVar(engine_pb2.ChangeObjectChar(
                host_name=hst, service_desc=svc,  mode=engine_pb2.ChangeObjectChar.Mode.CHANGE_CHECK_TIMEPERIOD, charval=check_timeperiod))
    else:
        now = int(time.time())
        cmd = "[{}] CHANGE_SVC_CHECK_TIMEPERIOD;{};{};{}\n".format(
            now, hst, svc, check_timeperiod)
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_change_svc_notification_timeperiod(use_grpc: int, hst: str, svc: str, notification_timeperiod: str):
    """
    Change the notification timeperiod for a service.

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): Host name of the service.
        svc (str): Service description of the service.
        notification_timeperiod (str): Notification timeperiod (24x7, 24x6, workhours..)
    """
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.ChangeServiceObjectCharVar(engine_pb2.ChangeObjectChar(
                host_name=hst, service_desc=svc,  mode=engine_pb2.ChangeObjectChar.Mode.CHANGE_NOTIFICATION_TIMEPERIOD, charval=notification_timeperiod))
    else:
        now = int(time.time())
        cmd = "[{}] CHANGE_SVC_NOTIFICATION_TIMEPERIOD;{};{};{}\n".format(
            now, hst, svc, notification_timeperiod)
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_disable_host_and_child_notifications(use_grpc: int, hst: str):
    """
    Disable all the notifications on a host (the host itself and its children).

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): Host name of the concerned host.
    """
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.DisableHostAndChildNotifications(
                engine_pb2.HostIdentifier(name=hst))
    else:
        now = int(time.time())
        cmd = "[{}] DISABLE_HOST_AND_CHILD_NOTIFICATIONS;{}\n".format(
            now, hst)
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_enable_host_and_child_notifications(use_grpc: int, hst: str):
    """
    Enable all the notifications on a host (the host itself and its children).

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): Host name of the concerned host.
    """
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.EnableHostAndChildNotifications(
                engine_pb2.HostIdentifier(name=hst))
    else:
        now = int(time.time())
        cmd = "[{}] ENABLE_HOST_AND_CHILD_NOTIFICATIONS;{}\n".format(
            now, hst)
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_disable_host_check(use_grpc: int, hst: str):
    """
    Disable checks on a given host.

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): host name of the concerned host.
    """
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] DISABLE_HOST_CHECK;{}\n".format(
            now, hst)
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_enable_host_check(use_grpc: int, hst: str):
    """
    Enable checks on a given host.

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): host name of the concerned host.
    """
    if use_grpc == 0:
        now = int(time.time())
        cmd = f"[{now}] ENABLE_HOST_CHECK;{hst}\n"
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_disable_host_event_handler(use_grpc: int, hst: str):
    """
    Disable a host event handler.

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): host name of the concerned host.
    """
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] DISABLE_HOST_EVENT_HANDLER;{}\n".format(
            now, hst)
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_enable_host_event_handler(use_grpc: int, hst: str):
    """
    Enable a host event handler.

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): host name of the concerned host.
    """
    if use_grpc == 0:
        now = int(time.time())
        cmd = f"[{now}] ENABLE_HOST_EVENT_HANDLER;{hst}\n"
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_disable_host_flap_detection(use_grpc: int, hst: str):
    """
    Disable the flap detection on a host.

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): host name of the concerned host.
    """
    if use_grpc == 0:
        now = int(time.time())
        cmd = f"[{now}] DISABLE_HOST_FLAP_DETECTION;{hst}\n"
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_enable_host_flap_detection(use_grpc: int, hst: str):
    """
    Enable the flap detection on a host.

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): host name of the concerned host.
    """
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] ENABLE_HOST_FLAP_DETECTION;{}\n".format(
            now, hst)
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_disable_host_notifications(use_grpc: int, hst: str):
    """
    Disable the notifications on a host.

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): host name of the concerned host.
    """
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.DisableHostNotifications(
                engine_pb2.HostIdentifier(name=hst))
    else:
        now = int(time.time())
        cmd = "[{}] DISABLE_HOST_NOTIFICATIONS;{}\n".format(
            now, hst)
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_enable_host_notifications(use_grpc: int, hst: str):
    """
    Enable notifications on a host.

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): host name of the concerned host.
    """
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.EnableHostNotifications(
                engine_pb2.HostIdentifier(name=hst))
    else:
        now = int(time.time())
        cmd = "[{}] ENABLE_HOST_NOTIFICATIONS;{}\n".format(
            now, hst)
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_update_ano_sensitivity(use_grpc: int, hst: str, serv: str, sensitivity: float):
    """
    Update the anomaly detection sensitivity of an anomalydetection.

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str):  host name of the anomalydetection.
        serv (str): service description of the anomalydetection.
        sensitivity (float): the new sensivity.
    """
    if use_grpc > 0:
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            stub.ChangeAnomalyDetectionSensitivity(engine_pb2.ChangeServiceNumber(serv=engine_pb2.ServiceIdentifier(
                names=engine_pb2.NameIdentifier(host_name=hst, service_name=serv)), dval=sensitivity))
    else:
        now = int(time.time())
        cmd = f"[{now}] CHANGE_ANOMALYDETECTION_SENSITIVITY;{hst};{serv};{sensitivity}\n"
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_disable_host_svc_checks(use_grpc: int, hst: str):
    """
    Disable all the checks on a host (on it and on its services).

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): host name of the concerned host.
    """
    if use_grpc == 0:
        now = int(time.time())
        cmd = f"[{now}] DISABLE_HOST_SVC_CHECKS;{hst}\n"
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_enable_host_svc_checks(use_grpc: int, hst: str):
    """
    Enable all the checks on a host (on it and on its services).

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): host name of the concerned host.
    """
    if use_grpc == 0:
        now = int(time.time())
        cmd = f"[{now}] ENABLE_HOST_SVC_CHECKS;{hst}\n"
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_disable_host_svc_notifications(use_grpc: int, hst: str):
    """
    Disable all the notifications on a host (on it and on its services).

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): host name of the concerned host.
    """
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] DISABLE_HOST_SVC_NOTIFICATIONS;{}\n".format(
            now, hst)
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_enable_host_svc_notifications(use_grpc: int, hst: str):
    """
    Enable all the notifications on a host (on it and on its services).

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): host name of the concerned host.
    """
    if use_grpc == 0:
        now = int(time.time())
        cmd = f"[{now}] ENABLE_HOST_SVC_NOTIFICATIONS;{hst}\n"
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_disable_passive_host_checks(use_grpc: int, hst: str):
    """
    Diable the passive checks on a host.

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): host name of the concerned host.
    """
    if use_grpc == 0:
        now = int(time.time())
        cmd = f"[{now}] DISABLE_PASSIVE_HOST_CHECKS;{hst}\n"
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_enable_passive_host_checks(use_grpc: int, hst: str):
    """
    Enable the passive checks on a host.

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): host name of the concerned host.
    """
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] ENABLE_PASSIVE_HOST_CHECKS;{}\n".format(
            now, hst)
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_disable_passive_svc_checks(use_grpc: int, hst: str, svc: str):
    """
    Disable the passive checks on a service.

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): host name of the concerned service.
        svc (str): service description of the concerned service.
    """
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] DISABLE_PASSIVE_SVC_CHECKS;{};{}\n".format(
            now, hst, svc)
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_enable_passive_svc_checks(use_grpc: int, hst: str, svc: str):
    """
    Enable passive checks on a service.

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): host name of the service.
        svc (str): service description of the service.
    """
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] ENABLE_PASSIVE_SVC_CHECKS;{};{}\n".format(
            now, hst, svc)
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_start_obsessing_over_host(use_grpc: int, hst: str):
    """
    Start obsessing over a host.

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): host name of the host.
    """
    if use_grpc == 0:
        now = int(time.time())
        cmd = f"[{now}] START_OBSESSING_OVER_HOST;{hst}\n"
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_stop_obsessing_over_host(use_grpc: int, hst: str):
    """
    Stop obsessing over a host.

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): host name of the host.
    """
    if use_grpc == 0:
        now = int(time.time())
        cmd = f"[{now}] STOP_OBSESSING_OVER_HOST;{hst}\n"
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_start_obsessing_over_svc(use_grpc: int, hst: str, svc: str):
    """
    Start obsessing over a service.

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): host name of the service.
        svc (str): service description of the service.
    """
    if use_grpc == 0:
        now = int(time.time())
        cmd = "[{}] START_OBSESSING_OVER_SVC;{};{}\n".format(
            now, hst, svc)
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_stop_obsessing_over_svc(use_grpc: int, hst: str, svc: str):
    """
    Stop obsessing over a service.

    Args:
        use_grpc (int): If not zero by gRPC, otherwise with legacy commands.
        hst (str): host name of the service.
        svc (str): service description of the service.
    """
    if use_grpc == 0:
        now = int(time.time())
        cmd = f"[{now}] STOP_OBSESSING_OVER_SVC;{hst};{svc}\n"
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)


def ctn_external_command(func):
    def wrapper(*args):
        now = int(time.time())
        cmd = f"[{now}] {func(*args)}"
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(cmd)

    return wrapper


@ctn_external_command
def ctn_process_host_check_result(hst: str, state: int, output: str):
    """
    Process a host check result.

    Args:
        hst: Host name of the host.
        state: State returned by the check.
        output: Output message of the check.

    Returns:
        0 on success.
    """
    return f"PROCESS_HOST_CHECK_RESULT;{hst};{state};{output}\n"


@ctn_external_command
def ctn_schedule_service_downtime(hst: str, svc: str, duration: int):
    """
    Schedule a downtime on a service.

    Args:
        hst (str): Host name of the service.
        svc (str): Service description of the service.
        duration (int): Expected duration in seconds.

    Returns:
        0 on success.
    """
    now = int(time.time())
    return f"SCHEDULE_SVC_DOWNTIME;{hst};{svc};{now};{now+int(duration)};0;0;{duration};admin;Downtime set by admin\n"


@ctn_external_command
def ctn_schedule_service_fixed_downtime(hst: str, svc: str, duration: int):
    """
    Schedule a fixed downtime on a service.

    Args:
        hst (str): Host name of the service.
        svc (str): Service description of the service.
        duration (int): Expected duration in seconds.

    Returns:
        0 on success.
    """
    now = int(time.time())
    return f"SCHEDULE_SVC_DOWNTIME;{hst};{svc};{now};{now+int(duration)};1;0;{duration};admin;Downtime set by admin\n"


def ctn_schedule_host_fixed_downtime(poller: int, hst: str, duration: int):
    """
    Schedule a fixed downtime on a host.

    Args:
        poller (int): Index of the poller to work with.
        hst (str): host name of the host.
        duration (int): Expected duration of the downtime in seconds.
    """
    now = int(time.time())
    cmd1 = f"[{now}] SCHEDULE_HOST_DOWNTIME;{hst};{now};{now + duration};1;0;;admin;Downtime set by admin\n"
    cmd2 = f"[{now}] SCHEDULE_HOST_SVC_DOWNTIME;{hst};{now};{now + duration};1;0;;admin;Downtime set by admin\n"
    with open(f"{VAR_ROOT}/lib/centreon-engine/config{poller}/rw/centengine.cmd", "w") as f:
        f.write(cmd1)
        f.write(cmd2)


def ctn_schedule_host_downtime(poller: int, hst: str, duration: int):
    """
    Schedule a downtime on a host.

    Args:
        poller (int): Index of the poller to work with.
        hst (str): host name of the host.
        duration (int): Expected duration of the downtime in seconds.
    """
    now = int(time.time())
    cmd1 = "[{1}] SCHEDULE_HOST_DOWNTIME;{0};{1};{2};1;0;{3};admin;Downtime set by admin\n".format(
        hst, now, now + duration, duration)
    cmd2 = "[{1}] SCHEDULE_HOST_SVC_DOWNTIME;{0};{1};{2};1;0;{3};admin;Downtime set by admin\n".format(
        hst, now, now + duration, duration)
    with open(
            f"{VAR_ROOT}/lib/centreon-engine/config{poller}/rw/centengine.cmd", "w") as f:
        f.write(cmd1)
        f.write(cmd2)


def ctn_delete_host_downtimes(poller: int, hst: str):
    """
    Delete the downtimes on a host.

    Args:
        poller (int): Poller ID.
        hst (str): host name of the host.
    """
    now = int(time.time())
    cmd = "[{}] DEL_HOST_DOWNTIME_FULL;{};;;;;;;;\n".format(now, hst)
    with open(
            f"{VAR_ROOT}/lib/centreon-engine/config{poller}/rw/centengine.cmd", "w") as f:
        f.write(cmd)


def ctn_delete_service_downtime_full(poller: int, hst: str, svc: str):
    """
    Delete the downtimes on a service.

    Args:
        poller (int): Poller ID.
        hst (str): host name of the service.
        svc (str):  service description of the service.
    """
    now = int(time.time())
    cmd = f"[{now}] DEL_SVC_DOWNTIME_FULL;{hst};{svc};;;;;;;\n"
    with open(
            f"{VAR_ROOT}/lib/centreon-engine/config{poller}/rw/centengine.cmd", "w") as f:
        f.write(cmd)


def ctn_schedule_forced_service_check(host: str, svc: str, pipe: str = f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd"):
    """
    Schedule a forced check on a service.

    Args:
        host (str): host name of the service.
        svc (str): service description of the service.
        pipe (str, optional): The command file. Defaults to "{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd".
    """
    now = int(time.time())
    with open(pipe, "w") as f:
        cmd = f"[{now}] SCHEDULE_FORCED_SVC_CHECK;{host};{svc};{now}\n"
        f.write(cmd)


def ctn_schedule_forced_host_check(host: str, pipe: str = f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd"):
    """
    Schedule a forced check on a host.

    Args:
        host (str): host name of the host.
        pipe (str, optional): The command file to use. Defaults to "{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd".
    """
    now = int(time.time())
    cmd = f"[{now}] SCHEDULE_FORCED_HOST_CHECK;{host};{now}\n"
    with open(pipe, "w") as f:
        f.write(cmd)


def ctn_create_severities_file(poller: int, nb: int, offset: int = 1):
    """
    Create a severities.cfg file for a given poller.

    Args:
        poller (int): Index of the poller.
        nb (int): number of severities.
        offset (int, optional): Defaults to 1.
    """
    engine.create_severities(poller, nb, offset)


def ctn_create_escalations_file(poller: int, name: int, SG: str, contactgroup: str):
    """
    Create an escalations.cfg file for a given poller.

    Args:
        poller (int): Index of the poller.
        name (int): name of escalations (not used).
        SG (str): name of a service group.
        contactgroup (str): name of a contact group.
    """
    engine.create_escalations_file(poller, name, SG, contactgroup)


def ctn_create_dependencies_file(poller: int, dependenthost: str, host: str, dependentservice: str, service: str):
    """
    Create an dependencies.cfg file for a given poller.

    Args:
        poller (int): Index of the poller.
        dependenthost (str): name of the dependent host that we are gonna test
        host (str): name of the host master
        dependentservice (str): name of the dependent service that we are gonna test
        service (str): name of the service master
    """
    engine.create_dependencies_file(
        poller, dependenthost, host, dependentservice, service)


def ctn_create_dependenciesgrp_file(poller: int, dependentservicegroup: str, servicegroup: str):
    """
    Create an dependenciesgrp.cfg file for a given poller.

    Args:
        poller (int): Index of the poller.
        dependentservicegroup (str): Dependent service group names list defines the group(s) of dependent services
        servicegroup (str): Service group names list defines the group(s) of master services
    """
    engine.create_dependenciesgrp_file(
        poller, dependentservicegroup, servicegroup)


def ctn_create_dependencieshst_file(poller: int, dependenthost: str, host: str):
    """
    Create an dependencies.cfg file for a given poller.

    Args:
        poller (int): Index of the poller.
        dependenthost (str): Dependent Host Name
        host (str): master host name
    """
    engine.create_dependencieshst_file(poller, dependenthost, host)


def ctn_create_dependencieshstgrp_file(poller: int, dependenthostgrp: str, hostgrp: str):
    """
    Create an dependencieshstgrp.cfg file for a given poller.

    Args:
        poller (int): Index of the poller.
        dependenthostgrp (str): Dependent host group name list defines the dependent host group(s)
        hostgrp (str): Host groups name list defines the master host group(s)
    """
    engine.create_dependencieshstgrp_file(poller, dependenthostgrp, hostgrp)


def ctn_create_template_file(poller: int, typ: str, what: str, ids: list):
    """
    Create a template file of the form "{typ}Templates.cfg". This should be as
    generic as possible. In fact, not so generic...

    Args:
        poller (int): poller ID.
        typ (str): service, host, ...
        what (str): A string. It depends on what type of template.
        ids (list): For each integer in this list, a template is defined.
    """
    engine.create_template_file(poller, typ, what, ids)


def ctn_create_tags_file(poller: int, nb: int, offset: int = 1, tag_type: str = ""):
    """
    Create a tags file.

    Args:
        poller (int): poller ID.
        nb (int): number of tags to create.
        offset (int, optional): Defaults to 1.
        tag_type: A string among [servicegroup, hostgroup, servicecategory, hostcategory].
    """
    engine.create_tags(poller, nb, offset, tag_type)


def ctn_engine_config_remove_tag(poller: int, tag_id: int):
    """
    Remove all the tags from tags.cfg with the given tag ID.

    Args:
        poller: Poller index.
        tag_id: ID of the tag to remove.
    """
    filename = f"{CONF_DIR}/config{poller}/tags.cfg"
    with open(filename, "r") as ff:
        lines = ff.readlines()

    tag_name = re.compile(f"^\s*id\s+{tag_id}\s*$")
    tag_begin = re.compile(r"^define tag {$")
    tag_end = re.compile(r"^}$")
    tag_begin_idx = 0
    while tag_begin_idx < len(lines):
        if (tag_begin.match(lines[tag_begin_idx])):
            for tag_line_idx in range(tag_begin_idx, len(lines)):
                if (tag_name.match(lines[tag_line_idx])):
                    for end_tag_line in range(tag_line_idx, len(lines)):
                        if tag_end.match(lines[end_tag_line]):
                            del lines[tag_begin_idx:end_tag_line + 1]
                            break
                    break
                elif tag_end.match(lines[tag_line_idx]):
                    tag_begin_idx = tag_line_idx
                    break
        else:
            tag_begin_idx = tag_begin_idx + 1

    with open(filename, "w") as f:
        f.writelines(lines)


def ctn_config_engine_add_cfg_file(poller: int, cfg: str):
    """
    Add a reference to a cfg file in the centengine.cfg file at index _poller_.

    Args:
        poller (int): Poller ID.
        cfg (str): Configuration file name to add.
    """
    with open("{}/config{}/centengine.cfg".format(CONF_DIR, poller), "r") as ff:
        lines = ff.readlines()
    r = re.compile(r"^\s*cfg_file=")
    for i in range(len(lines)):
        if r.match(lines[i]):
            lines.insert(
                i, "cfg_file={}/config{}/{}\n".format(CONF_DIR, poller, cfg))
            break
    with open("{}/config{}/centengine.cfg".format(CONF_DIR, poller), "w+") as ff:
        ff.writelines(lines)


def ctn_add_severity_to_services(poller: int, severity_id: int, svc_lst):
    """
    Add a severity to services.

    Args:
        poller (int): Index of the poller to work with.
        severity_id (int): The severity ID.
        svc_lst (list): A list of service IDs.
    """
    with open(f"{CONF_DIR}/config{poller}/services.cfg", "r") as ff:
        lines = ff.readlines()
    r = re.compile(r"^\s*_SERVICE_ID\s*(\d+)$")
    for i in range(len(lines)):
        m = r.match(lines[i])
        if m is not None and m.group(1) in svc_lst:
            lines.insert(
                i + 1, f"    severity_id                     {severity_id}\n")

    with open(f"{CONF_DIR}/config{poller}/services.cfg", "w") as ff:
        ff.writelines(lines)


def ctn_set_services_passive(poller: int, srv_regex):
    """
    Set passive a list of services.

    Args:
        poller (int): Index of the poller to work with.
        srv_regex (str): A regexp to match service descriptions.
    """

    with open("{}/config{}/services.cfg".format(CONF_DIR, poller), "r") as ff:
        lines = ff.readlines()
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

    with open("{}/config{}/services.cfg".format(CONF_DIR, poller), "w") as ff:
        ff.writelines(lines)


def ctn_set_hosts_passive(poller: int, host_regex):
    """
    Set passive a list of hosts.

    Args:
        poller (int): Index of the poller to work with.
        srv_regex (str): A regexp to match host name.
    """

    with open("{}/config{}/hosts.cfg".format(CONF_DIR, poller), "r") as ff:
        lines = ff.readlines()
    r = re.compile(f"^\s*host_name\s*({host_regex})$")
    for i in range(len(lines)):
        m = r.match(lines[i])
        if m:
            lines.insert(i+1, "    active_checks_enabled           0\n")
            lines.insert(i+2, "    passive_checks_enabled          1\n")
            i += 2

    with open("{}/config{}/hosts.cfg".format(CONF_DIR, poller), "w") as ff:
        ff.writelines(lines)


def ctn_add_severity_to_hosts(poller: int, severity_id: int, svc_lst):
    """
    Add a severity to a list of hosts given by their ID.

    Args:
        poller (int): Index of the poller to work with.
        severity_id (int): The severity ID.
        svc_lst: A list of host IDs.
    """
    with open("{}/config{}/hosts.cfg".format(CONF_DIR, poller), "r") as ff:
        lines = ff.readlines()
    r = re.compile(r"^\s*_HOST_ID\s*(\d+)$")
    for i in range(len(lines)):
        m = r.match(lines[i])
        if m is not None and m.group(1) in svc_lst:
            lines.insert(
                i + 1, "    severity_id                     {}\n".format(severity_id))

    with open(f"{CONF_DIR}/config{poller}/hosts.cfg", "w") as ff:
        ff.writelines(lines)


def ctn_add_tags_to_services(poller: int, type: str, tag_id: str, svc_lst):
    """
    Add tags to a list of services given by their ID (just service ID).

    Args:
        poller (int): Index of the poller to work with.
        type (str): One string of [group_tags, category_tags].
        tag_id (str): A string with the tag IDs separated by a comma.
        svc_lst: A list of service IDs.
    """

    with open(f"{CONF_DIR}/config{poller}/services.cfg", "r") as ff:
        lines = ff.readlines()
    r = re.compile(r"^\s*_SERVICE_ID\s*(\d+)$")
    for i in range(len(lines)):
        m = r.match(lines[i])
        if m is not None and m.group(1) in svc_lst:
            lines.insert(i + 1, f"    {type}                     {tag_id}\n")
    with open(f"{CONF_DIR}/config{poller}/services.cfg", "w") as ff:
        ff.writelines(lines)


def ctn_remove_severities_from_services(poller: int):
    """
    Remove severities from services on a poller.

    Args:
        poller (int): Index of the poller to work with.
    """
    with open("{}/config{}/services.cfg".format(CONF_DIR, poller), "r") as ff:
        lines = ff.readlines()
    r = re.compile(r"^\s*severity_id\s*\d+$")
    out = [line for line in lines if not r.match(line)]
    with open("{}/config{}/services.cfg".format(CONF_DIR, poller), "w") as ff:
        ff.writelines(out)


def ctn_remove_severities_from_hosts(poller: int):
    """
    Remove severities from hosts on a poller.

    Args:
        poller (int): Index of the poller to work with.
    """
    with open(f"{CONF_DIR}/config{poller}/hosts.cfg", "r") as ff:
        lines = ff.readlines()
    r = re.compile(r"^\s*severity_id\s*\d+$")
    out = [line for line in lines if not r.match(line)]
    with open(f"{CONF_DIR}/config{poller}/hosts.cfg", "w") as ff:
        ff.writelines(out)


def ctn_check_search(debug_file_path: str, str_to_search, timeout=TIMEOUT):
    """
    Search a check, retrieve command index and return check result.
    Then it searchs the string "connector::run: id=\d+",
    and then search "connector::_recv_query_execute: id=\d+,"
    and return this line.

    Args:
        debug_file_path (str): path of the debug log file
        str_to_search (str): string after which we will start connector::run search
        timeout (int, optional): Defaults to TIMEOUT.

    *Example:*

    | ${search_result} | `Check Search` | /var/log/centreon-engine/centengine.debug | connector::run: id=1090 |
    | Should Contain | ${search_result} | connector::_recv_query_execute: id=1090, |

    Returns:
        A string.
    """
    limit = time.time() + timeout
    r_query_execute = "none"
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
                            r"connector::run:\s+id=(\d+)", lines[second_ind])
                        if m is not None:
                            cmd_id = m.group(1)
                            r_query_execute = rf".*\s+connector::_recv_query_execute:\s+id={cmd_id}, .*output=(.*)$"
                            for third_ind in range(second_ind, len(lines)):
                                logger.console(lines[third_ind])
                                m = re.match(r_query_execute, lines[third_ind])
                                if m is not None:
                                    return m.group(1)
        time.sleep(1)

    if not cmd_executed:
        return f"_recv_query_execute not found on '{r_query_execute}'"
    else:
        return f"ctn_check_search doesn't find <<{str_to_search}>>"


def ctn_add_tags_to_hosts(poller: int, type: str, tag_id: str, hst_lst):
    """
    Add tags to a list of hosts.

    Args:
        poller (int): Index of the poller to work with.
        type (str):
        tag_id (str):
        hst_lst (_type_):

    Returns: N/A

    """
    with open("{}/config{}/hosts.cfg".format(CONF_DIR, poller), "r") as ff:
        lines = ff.readlines()
    r = re.compile(r"^\s*_HOST_ID\s*(\d+)$")
    for i in range(len(lines)):
        m = r.match(lines[i])
        if m is not None and m.group(1) in hst_lst:
            lines.insert(
                i + 1, "    {}                     {}\n".format(type, tag_id))

    with open("{}/config{}/hosts.cfg".format(CONF_DIR, poller), "w") as ff:
        ff.writelines(lines)


def ctn_remove_tags_from_services(poller: int, type: str):
    """
    Remove tags from services.

    Args:
        poller (int): Index of the poller to work with.
        type (str): The tag type among group_tags or category_tags.
    """
    with open("{}/config{}/services.cfg".format(CONF_DIR, poller), "r") as ff:
        lines = ff.readlines()
    r = re.compile(r"^\s*" + type + r"\s*[0-9,]+$")
    lines = [line for line in lines if not r.match(line)]
    with open("{}/config{}/services.cfg".format(CONF_DIR, poller), "w") as ff:
        ff.writelines(lines)


def ctn_remove_tags_from_hosts(poller: int, type: str):
    """
    Remove tags from hosts.

    Args:
        poller (int): Index of the poller to work with.
        type (str): The tag type among group_tags or category_tags.
    """
    with open(f"{CONF_DIR}/config{poller}/hosts.cfg", "r") as ff:
        lines = ff.readlines()
    r = re.compile(r"^\s*" + type + r"\s*[0-9,]+$")
    lines = [line for line in lines if not r.match(line)]
    with open(f"{CONF_DIR}/config{poller}/hosts.cfg", "w") as ff:
        ff.writelines(lines)


def ctn_add_parent_to_host(poller: int, host: str, parent_host: str):
    """
    Add a parent host to an host.

    Args:
        poller: index of the Engine configuration (from 0)
        host: child host name.
        parent_host: host name of the parent of the child host.
    """
    with open(f"{CONF_DIR}/config{poller}/hosts.cfg", "r") as ff:
        lines = ff.readlines()
    r = re.compile(rf"^\s*host_name\s+{host}$")
    for i in range(len(lines)):
        if r.match(lines[i]):
            lines.insert(
                i + 1, f"    parents                        {parent_host}\n")
            break

    with open(f"{CONF_DIR}/config{poller}/hosts.cfg", "w") as ff:
        ff.writelines(lines)


def ctn_add_template_to_services(poller: int, tmpl: str, svc_lst):
    """
    Add a service template to services.

    Args:
        poller (int): Index of the poller to work with.
        tmpl (str): The name of the template to add.
        svc_lst (list): A list of service IDs. We don't take care of host IDs here.
    """
    with open("{}/config{}/services.cfg".format(CONF_DIR, poller), "r") as ff:
        lines = ff.readlines()
    r = re.compile(r"^\s*_SERVICE_ID\s*(\d+)$")
    for i in range(len(lines)):
        m = r.match(lines[i])
        if m is not None and m.group(1) in svc_lst:
            lines.insert(
                i + 1, f"    use                     {tmpl}\n")

    with open("{}/config{}/services.cfg".format(CONF_DIR, poller), "w") as ff:
        ff.writelines(lines)


def ctn_add_template_to_hosts(poller: int, tmpl: str, hst_lst):
    """
    Add a host template to hosts, each one given by its ID.

    Args:
        poller (int): Index of the poller to work with.
        tmpl (str): The name of the template to add.
        hst_lst (list): A list of host IDs.
    """
    with open("{}/config{}/hosts.cfg".format(CONF_DIR, poller), "r") as ff:
        lines = ff.readlines()
    r = re.compile(r"^\s*_HOST_ID\s*(\d+)$")
    for i in range(len(lines)):
        m = r.match(lines[i])
        if m is not None and m.group(1) in hst_lst:
            lines.insert(
                i + 1, "    use                     {}\n".format(tmpl))

    with open("{}/config{}/hosts.cfg".format(CONF_DIR, poller), "w") as ff:
        ff.writelines(lines)


def ctn_config_engine_remove_cfg_file(poller: int, fic: str):
    """
    Remove a config file reference from the centengine.cfg.

    Args:
        poller (int): The ID of the Engine configuration.
        fic (str): What file to remove.
    """
    with open("{}/config{}/centengine.cfg".format(CONF_DIR, poller), "r") as ff:
        lines = ff.readlines()
    r = re.compile(
        r"^\s*cfg_file=" + ETC_ROOT + f"/centreon-engine/config{poller}/{fic}")
    linesearch = [line for line in lines if not r.match(line)]
    with open("{}/config{}/centengine.cfg".format(CONF_DIR, poller), "w") as ff:
        ff.writelines(linesearch)


def ctn_process_service_check_result_with_metrics(hst: str, svc: str, state: int, output: str, metrics: int, config='config0', metric_name='metric'):
    """
    Send a service check result with metrics.

    Args:
        hst (str): Host name of the service.
        svc (str): Service description of the service.
        state (int): State of the check to set.
        output (str): An output message for the check.
        metrics (int): The number of metrics that should appear in the result.
        config (str, optional): Defaults to 'config0' (useful in case of several Engine running).
        metric_name (str): The base name of metrics. They will appear followed by an integer (for example metric0, metric1, metric2, ...).

    Returns:
        0 on success.
    """
    now = int(time.time())
    pd = [output + " | "]
    for m in range(metrics):
        v = math.sin((now + m) / 1000) * 5
        pd.append(f"{metric_name}{m}={v}")
        logger.trace(f"{metric_name}{m}={v}")
    full_output = " ".join(pd)
    ctn_process_service_check_result(hst, svc, state, full_output, config)


def ctn_process_service_check_result_with_big_metrics(hst: str, svc: str, state: int, output: str, metrics: int, config='config0', metric_name='metric'):
    """
    Send a service check result with metrics but their values are to big to fit into a float.

    Args:
        hst (str): Host name of the service.
        svc (str): Service description of the service.
        state (int): State of the check to set.
        output (str): An output message for the check.
        metrics (int): The number of metrics that should appear in the result.
        config (str, optional): Defaults to 'config0' (useful in case of several Engine running).
        metric_name (str): The base name of metrics. They will appear followed by an integer (for example metric0, metric1, metric2, ...).

    Returns:
        0 on success.
    """
    now = int(time.time())
    pd = [output + " | "]
    for m in range(metrics):
        mx = 3.40282e+039
        v = mx + abs(math.sin((now + m) / 1000) * 5)
        pd.append(f"{metric_name}{m}={v}")
        logger.trace(f"{metric_name}{m}={v}")
    full_output = " ".join(pd)
    ctn_process_service_check_result(hst, svc, state, full_output, config)


def ctn_process_service_check_result(hst: str, svc: str, state: int, output: str, config='config0', use_grpc=0, nb_check=1):
    """
    Send a service check result.

    Args:
        hst (str): Host name of the service.
        svc (str): Service description of the service.
        state (int): State of the check to set.
        output (str): An output message for the check.
        config (str, optional): Defaults to 'config0' (useful in case of several Engine running).
        use_grpc (int, optional): Defaults to 0 (no).
        nb_check (int, optional): Defaults to 1. If nb_check > 1, the check result is sent nb_check times.

    Returns:
        0 on success.
    """
    if use_grpc > 0:
        ts = Timestamp()
        ts.GetCurrentTime()
        port = 50001 + int(config[6:])
        with grpc.insecure_channel(f"127.0.0.1:{port}") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            if nb_check > 1:
                for i in range(nb_check):
                    indexed_output = f"{output}_{i}"
                    stub.ProcessServiceCheckResult(engine_pb2.Check(
                        host_name=hst, svc_desc=svc, check_time=ts, output=indexed_output, code=state))
            else:
                stub.ProcessServiceCheckResult(engine_pb2.Check(
                    host_name=hst, svc_desc=svc, check_time=ts, output=output, code=state))

    else:
        now = int(time.time())
        with open(f"{VAR_ROOT}/lib/centreon-engine/{config}/rw/centengine.cmd", "w") as f:
            if nb_check == 1:
                cmd = f"[{now}] PROCESS_SERVICE_CHECK_RESULT;{hst};{svc};{state};{output}\n"
                f.write(cmd)
            else:
                for i in range(nb_check):
                    cmd = f"[{now}] PROCESS_SERVICE_CHECK_RESULT;{hst};{svc};{state};{output}_{i}\n"
                    f.write(cmd)


@ctn_external_command
def ctn_acknowledge_service_problem(hst, service, typ='NORMAL'):
    """
    Send an acknowledgement on a service.

    Args:
        hst (str): Host name of the service.
        service (str): Service description.
        typ (str, optional): Defaults to 'NORMAL'. Possible values are 'NORMAL', 'STICKY' or 'NONE'.

    Returns:
        0 on success.
    """
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


@ctn_external_command
def ctn_remove_service_acknowledgement(hst, service):
    """
   Remove a service acknowledgement.

    Args:
        hst (str): Host name of the service.
        service (str): Service description of the service.

    Returns:
        0 on success.
    """
    return f"REMOVE_SVC_ACKNOWLEDGEMENT;{hst};{service}\n"


@ctn_external_command
def ctn_send_custom_host_notification(hst, notification_option, author, comment):
    """
    Send a custom host notification.

    Args:
        hst (str): The host name of the concerned host.
        notification_option (int): The notification option.
        author (str): The name of the author.
        comment (str): A comment.

    Returns:
        0 on success.
    """
    return f"SEND_CUSTOM_HOST_NOTIFICATION;{hst};{notification_option};{author};{comment}\n"


@ctn_external_command
def ctn_add_svc_comment(host_name, svc_description, persistent, user_name, comment):
    """
    Add a service comment.

    Args:
        host_name (str): Host name of the service.
        svc_description (str): Description of the service.
        persistent (int): Is the comment persistent?
        user_name (str): User name of the comment's author.
        comment (str): Content of the comment.

    Returns:
        0 on success.
    """
    return f"ADD_SVC_COMMENT;{host_name};{svc_description};{persistent};{user_name};{comment}\n"


@ctn_external_command
def ctn_add_host_comment(host_name, persistent, user_name, comment):
    """
    Add a host comment.

    Args:
        host_name (str): Host name of the impacted host.
        persistent (int): Is the comment persistent?
        user_name (str): User name of the comment's author.
        comment (str): Content of the comment.

    Returns:
        0 on success.
    """
    return f"ADD_HOST_COMMENT;{host_name};{persistent};{user_name};{comment}\n"


@ctn_external_command
def ctn_del_host_comment(comment_id):
    """
    Delete a host comment.

    Args:
        comment_id (int): Comment ID.

    Returns:
        0 on success.
    """
    return f"DEL_HOST_COMMENT;{comment_id}\n"


@ctn_external_command
def ctn_change_host_check_command(hst: str, Check_Command: str):
    """
    Change a host check command.

    Args:
        hst (str): Host name of the host.
        Check_Command (str): New check command to set.

    Returns:
        0 on success.
    """
    return f"CHANGE_HOST_CHECK_COMMAND;{hst};{Check_Command}\n"


@ctn_external_command
def ctn_change_custom_host_var_command(hst: str, var_name: str, var_value):
    """
    Change the value of a host custom variable.

    Args:
        hst (str): The host name of the impacted host.
        var_name (str): The name of the custom variable.
        var_value (str): The new value to set.

    Returns:
        0 on success.
    """
    return "CHANGE_CUSTOM_HOST_VAR;{};{};{}\n".format(hst, var_name, var_value)


@ctn_external_command
def ctn_change_custom_svc_var_command(hst: str, svc: str, var_name: str, var_value):
    """
    Change a service custom variable.

    Args:
        hst (str): Host name of the service.
        svc (str): Service description of the service.
        var_name (str): Name of the custom variable.
        var_value (str): Value to set.

    Returns:
        0 on success.
    """
    return "CHANGE_CUSTOM_SVC_VAR;{};{};{};{}\n".format(hst, svc, var_name, var_value)


@ctn_external_command
def ctn_change_global_host_event_handler(var_value: str):
    """
    Change the global host event handler.

    Args:
        var_value (str): The new handler to set.

    Returns:
        0 on success.
    """
    return "CHANGE_GLOBAL_HOST_EVENT_HANDLER;{}\n".format(var_value)


@ctn_external_command
def ctn_change_global_svc_event_handler(var_value: str):
    """
    Change the global service event handler.

    Args:
        var_value (str): The new handler to set.

    Returns:
        0 on SUCCESS.
    """
    return "CHANGE_GLOBAL_SVC_EVENT_HANDLER;{}\n".format(var_value)


@ctn_external_command
def ctn_set_svc_notification_number(host_name: string, svc_description: string, value):
    """
    Change the notification number of a service.

    Args:
        host_name (string): Host name of the service.
        svc_description (string): Service description of the service.
        value (int): The notification number to set.

    Returns:
        0 on SUCCESS.
    """
    return "SET_SVC_NOTIFICATION_NUMBER;{};{};{}\n".format(host_name, svc_description, value)


def ctn_create_anomaly_threshold_file(path: string, host_id: int, service_id: int, metric_name: string, values: array):
    """
    Create an anomaly detection threshold file using version 1.

    Args:
        path (string): The path to the file.
        host_id (int): The host ID of the dependent service.
        service_id (int): The service ID of the dependent service.
        metric_name (string): The metric name we are interested by.
        values (array): An array of numbers.

    *Example:*

    | `Create Anomaly Threshold File` | /tmp/anomaly_threshold.json | 1 | 1 | metric_1 | ${values} |
    """
    with open(path, "w") as f:
        f.write(f"""[
    {{
        "host_id": "{host_id}",
        "service_id": "{service_id}",
        "metric_name": "{metric_name}",
        "predict": [
            """)
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


def ctn_create_anomaly_threshold_file_V2(path: string, host_id: int, service_id: int, metric_name: string, sensitivity: float, values: array):
    """
    Create an anomaly threshold file using the version 2.

    Args:
        path (string): The path to the file.
        host_id (int): The host ID of the dependent service.
        service_id (int): The service ID of the dependent service.
        metric_name (string): The metric we are interested by.
        sensitivity (float): The sensitivity.
        values (array): An array of numbers.

    *Example:*

    | `Create Anomaly Threshold File V2` | /tmp/anomaly_threshold.json | 1 | 1 | metric_1 | 0.5 | ${values} |
    """
    with open(path, "w") as f:
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


def ctn_grep_retention(poller: int, pattern: str):
    """
    Check if the retention.dat file of an Engine contains a string.

    Args:
        poller (int): ID of the poller to work with.
        pattern (str): The string to look for.

    Returns:
        An empty string if not found, or the found string.
    """
    return Common.ctn_grep("{}/log/centreon-engine/config{}/retention.dat".format(VAR_ROOT, poller), pattern)


def ctn_config_add_otl_connector(poller: int, connector_name: str, command_line: str):
    """
    ctn_config_add_otl_connector

     add a connector entry to connectors.cfg

    Args:
        poller: poller index
        connector_name: 
        command_line:
    """

    with open(f"{CONF_DIR}/config{poller}/connectors.cfg", "a") as f:
        f.write(f"""
define connector {{
    connector_name                 {connector_name}
    connector_line                 {command_line}
}}
""")


def ctn_modify_retention_dat(poller, host, service, key, value):
    """
    Modify a parameter of a service in the retention.dat file.

    Args:
        poller (int): The ID of the poller.
        host (str): Host name of the concerned service.
        service (str): Description of the service.
        key (str): Parameter name to modify.
        value (str): New value to set.
    """
    if host != "" and host != "":
        # We want a service
        with open(
                f"{VAR_ROOT}/log/centreon-engine/config{poller}/retention.dat", "r") as ff:
            lines = ff.readlines()

        r_hst = re.compile(r"^\s*host_name=(.*)$")
        r_svc = re.compile(r"^\s*service_description=(.*)$")
        in_block = False
        hst = ""
        svc = ""
        for i in range(len(lines)):
            line = lines[i]
            if not in_block:
                if line == "service {\n":
                    in_block = True
                    continue
            else:
                if line == "}\n":
                    in_block = False
                    hst = ""
                    svc = ""
                    continue
                m = r_hst.match(line)
                if m:
                    hst = m.group(1)
                    continue
                m = r_svc.match(line)
                if m:
                    svc = m.group(1)
                    continue
                if line.startswith(f"{key}=") and host == hst and svc == service:
                    logger.console(f"key '{key}' found !")
                    lines[i] = f"{key}={value}\n"
                    hst = ""
                    svc = ""

        with open(
                f"{VAR_ROOT}/log/centreon-engine/config{poller}/retention.dat", "w") as ff:
            ff.writelines(lines)


def ctn_modify_retention_dat_host(poller, host, key, value):
    """
    Modify a parameter in the retention.dat file for a given host.

    Args:
        poller (int): ID of the chosen poller.
        host (str): Host name.
        key (str): The parameter to change.
        value (str): The new value to set.
    """
    if host != "" and host != "":
        # We want a host
        with open(
                f"{VAR_ROOT}/log/centreon-engine/config{poller}/retention.dat", "r") as ff:
            lines = ff.readlines()

        r_hst = re.compile(r"^\s*host_name=(.*)$")
        in_block = False
        hst = ""
        for i in range(len(lines)):
            line = lines[i]
            if not in_block:
                if line == "host {\n":
                    in_block = True
                    continue
            else:
                if line == "}\n":
                    in_block = False
                    hst = ""
                    continue
                m = r_hst.match(line)
                if m:
                    hst = m.group(1)
                    continue
                if line.startswith(f"{key}=") and host == hst:
                    logger.console(f"key '{key}' found !")
                    lines[i] = f"{key}={value}\n"
                    hst = ""

        with open(
                f"{VAR_ROOT}/log/centreon-engine/config{poller}/retention.dat", "w") as ff:
            ff.writelines(lines)


def ctn_get_engine_process_stat(port, timeout=10):
    """
    Call the GetGenericStats function by gRPC it works with both engine and broker.
    We get informations that look like what we could get with top or ps.

    Args:
        port (int): port of the grpc server.
        timeout (int, optional): Defaults to 10.

    Returns:
        A protobuf message with the asked informations.
    """
    limit = time.time() + timeout
    while time.time() < limit:
        time.sleep(1)
        with grpc.insecure_channel(f"127.0.0.1:{port}") as channel:
            # same for engine and broker
            stub = engine_pb2_grpc.EngineStub(channel)
            try:
                res = stub.GetProcessStats(empty_pb2.Empty())
                return res
            except:
                logger.console("gRPC server not ready")
    logger.console("unable to get process stats")
    return None


def ctn_send_bench(id: int, port: int):
    """
    Send a bench event.

    Args:
        id (int): field of the protobuf Bench message.
        port (int): port of the gRPC server.
    """
    ts = Timestamp()
    ts.GetCurrentTime()
    with grpc.insecure_channel(f"127.0.0.1:{port}") as channel:
        stub = engine_pb2_grpc.EngineStub(channel)
        stub.SendBench(engine_pb2.BenchParam(id=id, ts=ts))


def ctn_config_host_command_status(idx: int, cmd_name: str, status: int):
    """
    Set the status of a check command.

    Args:
        idx: ID of the Engine configuration.
        cmd_name: Name of the command we work on.
        status: 0, 1, 2 or 3.
    """
    filename = f"{ETC_ROOT}/centreon-engine/config{idx}/commands.cfg"
    with open(filename, "r") as f:
        lines = f.readlines()

    r = re.compile(rf"^\s*command_name\s+{cmd_name}\s*$")
    for i in range(len(lines)):
        if r.match(lines[i]):
            lines[i +
                  1] = f"    command_line                    {ENGINE_HOME}/check.pl --id 0 --state {status}\n"
            break

    with open(filename, "w") as f:
        f.writelines(lines)


def ctn_add_host_dependency(idx: int, host_name: str, dependent_host_name: str):
    """
    Add a host dependency between a host and a dependent host in the dependencies.cfg file.

    Args:
        idx: Index of the poller (from 0).
        host_name: host name of the host.
        dependent_host_name: host name of the dependent host.
    """
    filename = f"{ETC_ROOT}/centreon-engine/config{idx}/dependencies.cfg"
    with open(filename, "a+") as f:
        f.write(f"""
define hostdependency {{
    execution_failure_criteria     d,p
    notification_failure_criteria  o,u
    dependency_period              24x7
    inherits_parent                1
    dependent_host_name            {dependent_host_name}
    host_name                      {host_name}
}}
""")


def ctn_add_service_dependency(idx: int, host_name: str, dependent_host_name: str, service: str, dependent_service: str):
    """
    Add a service dependency between a service and a dependent service in the dependencies.cfg file.

    Args:
        idx: Index of the poller (from 0).
        host_name: Host name of the host containing the service.
        dependent_host_name: Host name of the host containing the dependent service.
        service: Description of the service.
        dependent_service: Description of the dependent service.
    """
    filename = f"{ETC_ROOT}/centreon-engine/config{idx}/dependencies.cfg"
    with open(filename, "a+") as f:
        f.write(f"""
define servicedependency {{
    execution_failure_criteria     c
    notification_failure_criteria  c
    inherits_parent                1
    dependency_period              24x7
    dependent_host_name            {dependent_host_name}
    host_name                      {host_name}
    dependent_service_description  {dependent_service}
    service_description            {service}
}}
""")


def ctn_get_service_command(host_id: int, service_id: int):
    cmd = engine.service_cmd[service_id]
    if cmd.startswith("command_"):
        logger.console(f"Command id = {int(cmd[8:])}")
        return int(cmd[8:])
    else:
        logger.console(
            f"Unable to find the command id of service ({host_id};{service_id})")
        return None


def ctn_get_engine_log_level(port, log, timeout=TIMEOUT):
    """
    Get the log level of a given logger. The timeout is due to the way we ask
    for this information ; we use gRPC and the server may not be correctly
    started.

    Args:
        port: The gRPC port to use.
        log: The logger name.

    Returns:
        A string with the log level.
    """
    limit = time.time() + timeout
    while time.time() < limit:
        logger.console("Try to call GetLogInfo")
        time.sleep(1)
        with grpc.insecure_channel("127.0.0.1:{}".format(port)) as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            try:
                logs = stub.GetLogInfo(empty_pb2.Empty())
                return logs.level[log]

            except:
                logger.console("gRPC server not ready")


def ctn_create_single_day_time_period(idx: int, time_period_name: str, date, minute_duration: int):
    """
    Create a single day time period with a single time range from date to date + minute_duration
    Args
        idx: poller index
        time_period_name: must be unique
        date: time range start
        minute_duration: time range length in minutes
    """
    try:
        my_date = parser.parse(date)
    except:
        my_date = datetime.fromtimestamp(date)

    filename = f"{ETC_ROOT}/centreon-engine/config{idx}/timeperiods.cfg"

    begin = my_date.time()
    end = my_date + datetime.timedelta(minutes=minute_duration)

    with open(filename, "a+") as f:
        f.write(f"""
define timeperiod {{
    timeperiod_name     {time_period_name}
    alias               {time_period_name}
    {my_date.date().isoformat()}  {begin.strftime("%H:%M")}-{end.time().strftime("%H:%M")}
}}
""")


def ctn_add_otl_server_module(idx: int, otl_server_config_json_content: str, with_default_token: bool = True):
    """!
    add a new broker_module line to centengine.cfg and create otl_server config file
    @param idx index ofthe poller usually 0
    @param otl_server_config_json_content json content of the otl configuration file
    """
    filename = f"{ETC_ROOT}/centreon-engine/config{idx}/centengine.cfg"
    otl_server_config_path = f"{ETC_ROOT}/centreon-engine/config{idx}/otl_server.json"
    # add defaut token :
    token = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJjZW50cmVvbjY2MjQxIiwiaWF0IjoxNzQ0MDk3MDgxLCJleHAiOjkyMjMzNzIwMzV9.QkrT77i211-CvXoXqaBxRMzxajzA3-DK-DGVrbvJWA8"

    with open(filename, "a+") as f:
        f.write(
            f"broker_module=/usr/lib64/centreon-engine/libopentelemetry.so {otl_server_config_path}")

    json_load = json.loads(otl_server_config_json_content)

    add_token_otel = False
    if "otel_server" in json_load:
        add_token_otel = True

    if "centreon_agent" in json_load:
        if "reverse_connections" in json_load["centreon_agent"]:
            for obj in json_load["centreon_agent"]["reverse_connections"]:
                if "encryption" in obj and obj["encryption"] == True:
                    obj["token"] = token

    with open(otl_server_config_path, "w") as f:
        pretty_json = json.dumps(json_load, indent=4)
        f.write(pretty_json)
    if add_token_otel and with_default_token:
        # add token to otl_server.json
        ctn_add_token_otl_server_module(idx, token)


def ctn_add_token_otl_server_module(idx: int, token: str):
    """
    Add a token to the "trusted_tokens" list in the otl_server.json configuration file..
    Args:
        idx (int): The index of the configuration directory (e.g., config1, config2, etc.).
        token (str): The token to be added to the "trusted_tokens" list.
    Returns:
        bool: True if the token was successfully inserted, False otherwise.
    """
    otl_server_config_path = f"{ETC_ROOT}/centreon-engine/config{idx}/otl_server.json"
    token_inserted = False

    if not exists(otl_server_config_path):
        return

    with open(otl_server_config_path, "r") as f:
        data = json.load(f)

    # Check if "trusted_tokens" already exists
    if "otel_server" in data and "encryption" in data["otel_server"]:
        if data["otel_server"]["encryption"] == True:
            if "trusted_tokens" in data["otel_server"]:
                if token not in data["otel_server"]["trusted_tokens"]:
                    data["otel_server"]["trusted_tokens"].append(token)
                    token_inserted = True
            else:
                # Insert trusted_tokens after otel_server
                new_data = {}
                for key, value in data.items():
                    new_data[key] = value
                    if key == "otel_server":
                        new_data[key]["trusted_tokens"] = [token]
                        token_inserted = True
                data = new_data

    with open(otl_server_config_path, "w") as f:
        json.dump(data, f, indent=4)

    return token_inserted


def ctn_del_token_otl_server_module(idx: int, token: str):
    """
    Remove a trusted token from the OpenTelemetry (OTel) server configuration file.
    This function modifies the `otl_server.json` configuration file for a specific
    Centreon Engine instance by removing a specified token from the list of trusted tokens.
    Args:
        idx (int): The index of the Centreon Engine instance whose configuration file
                   should be modified.
        token (str): The token to be removed from the list of trusted tokens.
    Returns:
        None: The function does not return a value. If the configuration file does not
              exist, the function exits without making any changes.
    """
    otl_server_config_path = f"{ETC_ROOT}/centreon-engine/config{idx}/otl_server.json"

    if not exists(otl_server_config_path):
        return

    with open(otl_server_config_path, "r") as f:
        data = json.load(f)

    if "trusted_tokens" in data["otel_server"]:
        if token in data["otel_server"]["trusted_tokens"]:
            data["otel_server"]["trusted_tokens"].remove(token)

    with open(otl_server_config_path, "w") as f:
        json.dump(data, f, indent=4)


def ctn_add_token_agent_otl_server(idx_config: int, idx_agent: int, token: str):
    """
    Add a token in the otl_server.json configuration file.to the fields token
    Args:
        idx (int): The index of the configuration directory (e.g., config1, config2, etc.).
        token (str): The token to be added to the "trusted_tokens" list.
    Returns:
        bool: True if the token was successfully inserted, False otherwise.
    """

    otl_server_config_path = f"{ETC_ROOT}/centreon-engine/config{idx_config}/otl_server.json"
    token_inserted = False

    if not exists(otl_server_config_path):
        return

    with open(otl_server_config_path, "r") as f:
        data = json.load(f)

    # Check if "trusted_tokens" already exists
    if "centreon_agent" in data:
        if "reverse_connections" in data["centreon_agent"]:
            data["centreon_agent"]["reverse_connections"][idx_agent]["token"] = token

    with open(otl_server_config_path, "w") as f:
        json.dump(data, f, indent=4)

    return token_inserted


def ctn_randomword(length):
    letters = string.ascii_lowercase
    return ''.join(random.choice(letters) for i in range(length))


# Example of open telemetry request
# {
#     "resourceMetrics": [
#         {
#             "resource": {
#                 "attributes": [
#                     {
#                         "key": "service.name",
#                         "value": {
#                             "stringValue": "demo_telegraf"
#                         }
#                     }
#                 ]
#             },
#             "scopeMetrics": [
#                 {
#                     "scope": {
#                         "attributes": [
#                             {
#                                 "key": "host",
#                                 "value": {
#                                     "stringValue": "d4854a00b171"
#                                 }
#                             }
#                         ]
#                     },
#                     "metrics": [
#                         {
#                             "name": "swap_used_percent",
#                             "gauge": {
#                                 "dataPoints": [
#                                     {
#                                         "timeUnixNano": "1706864500000000000",
#                                         "asDouble": 99.999809264772921,
#                                         "attributes": [
#                                             {
#                                                 "key": "host",
#                                                 "value": {
#                                                     "stringValue": "d4854a00b171"
#                                                 }
#                                             }
#                                         ]
#                                     }
#                                 ]
#                             }
#                         },
#                         {
#                             "name": "swap_total",
#                             "gauge": {
#                                 "dataPoints": [
#                                     {
#                                         "timeUnixNano": "1706864500000000000",
#                                         "asInt": "2147479552",
#                                         "attributes": [
#                                             {
#                                                 "key": "host",
#                                                 "value": {
#                                                     "stringValue": "d4854a00b171"
#                                                 }
#                                             }
#                                         ]
#                                     }
#                                 ]
#                             }
#                         }
#                     ]
#                 }
#             ]
#         }
#     ]
# }


def ctn_add_data_point_to_metric(metric, attrib: dict, metric_value=None):
    """

    ctn_add_data_point_to_metric

    add a data point to metric
    Args:
        metric: metric
        attrib: key =>values to add in datapoint attributes
        metric_value (optional) value of metric, random if not given

    """
    data_point = metric.gauge.data_points.add()
    data_point.time_unix_nano = int(time.time()) * 1000000000
    if metric_value is not None:
        data_point.as_double = metric_value
    else:
        data_point.as_double = random.random()
    for key, value in attrib.items():
        attr = data_point.attributes.add()
        attr.key = key
        attr.value.string_value = value


def ctn_create_otl_metric(name: str, nb_datapoints: int, attrib: dict, metric_value=None):
    """

    create_otl_metric

    create a Metric
    Args:
        name:  metric name
        nb_datapoints: number of datapoints added to the metric
        attrib: key =>values to add in datapoint attributes
        metric_value (optional) value of metric, random if not given
    Returns:  opentelemetry.proto.metrics.v1.Metric object
    """
    metric = opentelemetry.proto.metrics.v1.metrics_pb2.Metric()
    metric.name = name
    for i in range(nb_datapoints):
        ctn_add_data_point_to_metric(metric, attrib, metric_value)
    return metric


def ctn_create_otl_scope_metrics(scope_attr: dict, metrics: list):
    """
    create_otl_scope_metrics

    create a ScopeMetrics
    Args:
        scope_attr: attributes to add in scope object
        metrics: metrics to add in metrics array
    Returns: opentelemetry.proto.metrics.v1.metrics_pb2.ScopeMetrics object
    """
    scope_metrics = opentelemetry.proto.metrics.v1.metrics_pb2.ScopeMetrics()
    for key, value in scope_attr.items():
        attr = scope_metrics.scope.attributes.add()
        attr.key = key
        attr.value.string_value = value
    for metric in metrics:
        to_fill = scope_metrics.metrics.add()
        to_fill.CopyFrom(metric)
    return scope_metrics


def ctn_create_otl_resource_metrics(resource_attr: dict, scope_metrics: list):
    """
    create_otl_resource_metrics

    create a ResourceMetrics
    Args:
        resource_attr: attributes to add in resource object
        scope_metrics: metrics to add in scopeMetrics array
    Returns: opentelemetry.proto.metrics.v1.metrics_pb2.ResourceMetrics object
    """
    resource_metrics = opentelemetry.proto.metrics.v1.metrics_pb2.ResourceMetrics()
    for key, value in resource_attr.items():
        attr = resource_metrics.resource.attributes.add()
        attr.key = key
        attr.value.string_value = value
    for scope_metric in scope_metrics:
        to_fill = resource_metrics.scope_metrics.add()
        to_fill.CopyFrom(scope_metric)
    return resource_metrics


def ctn_send_otl_to_engine(port: int, resource_metrics: list):
    """
    send_otl_to_engine

    send an otl request to engine otl server

    Args:
        port: port to connect to engine
        resource_metrics: resource_metrics to add to grpc message
    """
    with grpc.insecure_channel(f"127.0.0.1:{port}") as channel:
        # same for engine and broker
        stub = opentelemetry.proto.collector.metrics.v1.metrics_service_pb2_grpc.MetricsServiceStub(
            channel)
        try:
            request = opentelemetry.proto.collector.metrics.v1.metrics_service_pb2.ExportMetricsServiceRequest()
            for res_metric in resource_metrics:
                to_fill = request.resource_metrics.add()
                to_fill.CopyFrom(res_metric)

            return stub.Export(request)
        except:
            logger.console("gRPC server not ready")


def ctn_send_otl_to_engine_secure(target: str, resource_metrics: list, cert: str):
    """
    send_otl_to_engine_secure

    send an otl request to engine otl server using a secure connection

    Args:
        port: port to connect to engine
        resource_metrics: resource_metrics to add to grpc message
        cert: path to the certificate file for secure connection
    """
    with open(cert, 'rb') as f:
        creds = grpc.ssl_channel_credentials(f.read())
    with grpc.secure_channel(target, creds) as channel:
        # same for engine and broker
        stub = opentelemetry.proto.collector.metrics.v1.metrics_service_pb2_grpc.MetricsServiceStub(
            channel)
        try:
            request = opentelemetry.proto.collector.metrics.v1.metrics_service_pb2.ExportMetricsServiceRequest()
            for res_metric in resource_metrics:
                to_fill = request.resource_metrics.add()
                to_fill.CopyFrom(res_metric)

            return stub.Export(request)
        except Exception as e:
            logger.console(f"gRPC server not ready: {e}")


def ctn_get_service_info_grpc(id_h: int, id_s: int):
    """
    Retrieve service information via a gRPC call.

    Args:
        id_h: The identifier of the host to retrieve.
        id_s: The identifier of the service to retrieve.

    Returns:
        A dictionary containing the service informations, if successfully retrieved.
    """
    limit = time.time() + 30
    while time.time() < limit:
        time.sleep(1)
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            identifier = engine_pb2.PairIdsIdentifier(
                host_id=id_h, service_id=id_s)
            request = engine_pb2.ServiceIdentifier(ids=identifier)
            try:
                host = stub.GetService(request)
                host_dict = MessageToDict(
                    host, always_print_fields_with_no_presence=True)
                return host_dict
            except Exception as e:
                logger.console(f"gRPC server not ready {e}")
    return {}


def ctn_get_contact_info_grpc(name: str):
    """
    Retrieve contact information via a gRPC call.

    Args:
        name: The name of the contact to retrieve.

    Returns:
        A dictionary containing the contact information, if successfully retrieved.
    """
    limit = time.time() + 30
    while time.time() < limit:
        time.sleep(1)
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            request = engine_pb2.NameIdentifier(name=name)
            try:
                contact = stub.GetContact(request)
                contact_dict = MessageToDict(
                    contact, always_print_fields_with_no_presence=True)
                return contact_dict
            except Exception as e:
                logger.console(f"gRPC server not ready {e}")
    return {}


def ctn_get_hostgroup_info_grpc(name: str):
    """
    Retrieve host group information via a gRPC call.

    Args:
        name: The name of the host group to retrieve.

    Returns:
        A dictionary containing the host group information, if successfully retrieved.
    """
    limit = time.time() + 30
    while time.time() < limit:
        time.sleep(1)
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            request = engine_pb2.NameIdentifier(name=name)
            try:
                hg = stub.GetHostGroup(request)
                hg_dict = MessageToDict(
                    hg, always_print_fields_with_no_presence=True)
                return hg_dict
            except Exception as e:
                logger.console(f"gRPC server not ready {e}")
    return {}


def ctn_get_servicegroup_info_grpc(name: str):
    """
    Retrieve service group information via a gRPC call.

    Args:
        name: The name of the service group to retrieve.

    Returns:
        A dictionary containing the service group information, if successfully retrieved.
    """
    limit = time.time() + 30
    while time.time() < limit:
        time.sleep(1)
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            request = engine_pb2.NameIdentifier(name=name)
            try:
                sg = stub.GetServiceGroup(request)
                sg_dict = MessageToDict(
                    sg, always_print_fields_with_no_presence=True)
                return sg_dict
            except Exception as e:
                logger.console(f"gRPC server not ready {e}")
    return {}


def ctn_get_contactgroup_info_grpc(name: str):
    """
    Retrieve contact group information via a gRPC call.

    Args:
        name: The name of the contact group to retrieve.

    Returns:
        A dictionary containing the contact group information, if successfully retrieved.
    """
    limit = time.time() + 30
    while time.time() < limit:
        time.sleep(1)
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            request = engine_pb2.NameIdentifier(name=name)
            try:
                cg = stub.GetContactGroup(request)
                cg_dict = MessageToDict(
                    cg, always_print_fields_with_no_presence=True)
                return cg_dict
            except Exception as e:
                logger.console(f"gRPC server not ready {e}")
    return {}


def ctn_get_command_info_grpc(name: str):
    """
    Retrieve command information via a gRPC call.

    Args:
        name: The name of the command to retrieve.

    Returns:
        A dictionary containing the command information, if successfully retrieved.
    """
    limit = time.time() + 30
    while time.time() < limit:
        time.sleep(1)
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            request = engine_pb2.NameIdentifier(name=name)
            try:
                command = stub.GetCommand(request)
                command_dict = MessageToDict(
                    command, always_print_fields_with_no_presence=True)
                return command_dict
            except Exception as e:
                logger.console(f"gRPC server not ready {e}")
    return {}


def ctn_get_connector_info_grpc(name: str):
    """
    Retrieve connector information via a gRPC call.

    Args:
        name: The name of the connector to retrieve.

    Returns:
        A dictionary containing the connector information, if successfully retrieved.
    """
    limit = time.time() + 30
    while time.time() < limit:
        time.sleep(1)
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            request = engine_pb2.NameIdentifier(name=name)
            try:
                connector = stub.GetConnector(request)
                connector_dict = MessageToDict(
                    connector, always_print_fields_with_no_presence=True)
                return connector_dict
            except Exception as e:
                logger.console(f"gRPC server not ready {e}")
    return {}


def ctn_get_service_escalation_info_grpc(host_name: str, service_name: str):
    """
    Retrieve service escalation information via a gRPC call.

    Args:
        name: The name of the service escalation to retrieve.

    Returns:
        A dictionary containing the service escalation information, if successfully retrieved.
    """
    limit = time.time() + 30
    while time.time() < limit:
        time.sleep(1)
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            identifier = engine_pb2.PairNamesIdentifier(
                host_name=host_name, service_name=service_name)
            try:
                ServiceEscalation = stub.GetServiceEscalation(identifier)
                ServiceEscalation_dict = MessageToDict(
                    ServiceEscalation, always_print_fields_with_no_presence=True)
                return ServiceEscalation_dict
            except Exception as e:
                error_details = e.details()
                logger.console(f"gRPC server not ready {e}")
                if error_details == f"could not find serviceescalation with : host '{host_name}',service '{service_name}'":
                    return {}
    return {}


def ctn_get_host_escalation_info_grpc(host_name: str):
    """
    Retrieve host escalation information via a gRPC call.

    Args:
        name: The name of the host escalation to retrieve.

    Returns:
        A dictionary containing the host escalation information, if successfully retrieved.
    """
    limit = time.time() + 30
    while time.time() < limit:
        time.sleep(1)
        with grpc.insecure_channel("127.0.0.1:50001") as channel:
            stub = engine_pb2_grpc.EngineStub(channel)
            identifier = engine_pb2.NameIdentifier(name=host_name)
            try:
                hostEscalation = stub.GetHostEscalation(identifier)
                hostEscalation_dict = MessageToDict(
                    hostEscalation, always_print_fields_with_no_presence=True)
                return hostEscalation_dict
            except Exception as e:
                logger.console(f"gRPC server not ready {e}")
                error_details = e.details()
                if error_details == f"could not find hostescalation '{host_name}'":
                    return {}
    return {}


def ctn_check_key_value_existence(data_list, key, value):
    """
    Check if a specific key-value pair exists in a list of data strings.

    Args:
        data_list: List of strings.
        key: The key to look for.
        value: The value to match.

    Returns:
        True if the key-value pair exists in the data list, otherwise False.
    """
    for item in data_list:
        # Split the string by comma and trim spaces
        properties = [prop.strip() for prop in item.split(',')]
        # Create a dict to store key-value
        item_dict = {}
        for prop in properties:
            k, v = prop.split(':')
            item_dict[k.strip()] = v.strip()
        # Check if key and value exist in the dict
        if item_dict.get('key') == key and item_dict.get('value') == value:
            return True
    return False


def ctn_engine_config_del_block_in_cfg(idx: int, type: str, key: str, file):
    """
    Delete a element in the file given for the Engine configuration idx.

    Args:
        idx (int): Index of the Engine configuration (from 0)
        type (str): The type (host/service/...).
        key (str): The parameter that will be deleted.
        file (str): The file to delete the key from.
    """
    filename = f"{ETC_ROOT}/centreon-engine/config{idx}/{file}"

    with open(filename, "r") as f:
        content = f.read()

    if type == "host":
        pattern = rf"define host \{{\s*host_name\s+{re.escape(key)}\b.*?\}}"
    elif type == "service":
        pattern = rf"define service \{{\s*host_name\s+{re.escape(key)}\b.*?\}}"

    # Use re.sub to remove the matched block
    new_content = re.sub(pattern, '', content, flags=re.DOTALL)
    new_content = re.sub(r'\n\s*\n', '\n', new_content)

    if content != new_content:
        with open(filename, "w") as f:
            f.write(new_content)
    else:
        logger.console(
            f'\n\033[91mFailed : Cannot delete the block  with the type : {type} and the key : {key} in {file}\033[0m')


def ctn_get_host_info_grpc(id: int):
    """
    Retrieve host information via a gRPC call.

    Args:
        id: The identifier of the host to retrieve.

    Returns:
        A dictionary containing the host informations, if successfully retrieved.
    """
    if id is not None:
        limit = time.time() + 30
        while time.time() < limit:
            time.sleep(1)
            with grpc.insecure_channel("127.0.0.1:50001") as channel:
                stub = engine_pb2_grpc.EngineStub(channel)
                request = engine_pb2.HostIdentifier(id=id)
                try:
                    host = stub.GetHost(request)
                    host_dict = MessageToDict(
                        host, always_print_fields_with_no_presence=True)
                    return host_dict
                except Exception as e:
                    logger.console(f"gRPC server not ready {e}")
    return {}


def ctn_engine_command_add_arg(idx: int, command_name: str, arg):
    """
    Add an argument to a command in the commands.cfg file.

    Args:
        idx (int): Index of the Engine configuration (from 0).
        command_name (str): Name of the command to modify or * for all the commands.
        arg (str): Argument to add to the command.
    """
    with open(f"{ETC_ROOT}/centreon-engine/config{idx}/commands.cfg", "r") as f:
        lines = f.readlines()
    # All the commands are updated
    if command_name == '*':
        for i in range(len(lines)):
            if lines[i].find("check.pl") != -1:
                lines[i] = lines[i].rstrip() + f" {arg}\n"
    else:
        found = False
        r = re.compile(rf"^\s*command_name\s+{command_name}\s*$")
        for i in range(len(lines)):
            if not found:
                if r.match(lines[i]):
                    found = True
                    continue
            else:
                if lines[i].find("check.pl") != -1:
                    lines[i] = lines[i].rstrip() + f" {arg}\n"

    with open(f"{ETC_ROOT}/centreon-engine/config{idx}/commands.cfg", "w") as f:
        f.writelines(lines)


def ctn_engine_command_remove_connector(idx: int, command_name: str):
    """
    Remove the connector from a command in the commands.cfg file.

    Args:
        idx (int): Index of the Engine configuration (from 0).
        command_name (str): Name of the command to modify or * for all the commands.
    """
    with open(f"{ETC_ROOT}/centreon-engine/config{idx}/commands.cfg", "r") as f:
        lines = f.readlines()
    # All the commands are updated
    if command_name == '*':
        lines = [line for line in lines if "  connector  " not in line]
    else:
        found = False
        r = re.compile(rf"^\s*command_name\s+{command_name}\s*$")
        for i in range(len(lines)):
            if not found:
                if r.match(lines[i]):
                    found = True
                    continue
            else:
                if lines[i].find("  connector  ") != -1:
                    del lines[i]
                    break

    with open(f"{ETC_ROOT}/centreon-engine/config{idx}/commands.cfg", "w") as f:
        f.writelines(lines)


def ctn_engine_check_sh_command_output():
    """
    Scan the engine log and search service::handle_async_check_result lines
    Check the output of check.sh witch return arguments and NAGIOS__SERVICEVAR1 and NAGIOS__SERVICEVAR2 
    environment variables
    services with an id multiple of 10 have de _KO macro set, so check.sh returns 1 instead of 0

    Returns: number of different services checked
    """

    if not engine:
        return 0
    search_pattern = re.compile(
        r"service::handle_async_check_result\(\) service service_(\d+) res:service_check start_time=(\d+) finish_time=(\d+) timeout=(\d+) ok=(\d+) ret_code=(\d+) output:args (\d+) (\d+) env VAL_SERV_(\d+)_(\d+) VAL_HOST_(\d+)")
    service_checked = {}
    with open(f"{VAR_ROOT}/log/centreon-engine/config0/centengine.log") as f:
        lines = f.readlines()

    for line in lines:
        m = search_pattern.search(line)
        if m is not None:
            service_id = int(m.group(1))
            start_time = m.group(2)
            finish_time = m.group(3)
            timeout = m.group(4)
            ok = m.group(5)
            ret_code = m.group(6)
            host_id = int(m.group(7))
            service_host_index = int(m.group(8))
            env_host_id = int(m.group(9))
            env_service_id = int(m.group(10))
            env_host_id_2 = int(m.group(11))
            if int(finish_time) - int(start_time) > 5:
                logger.console(f"check duration too long: {line}")
                return 0
            if timeout != '0':
                logger.console(f"check timeout: {line}")
                return 0
            if ok != '1':
                logger.console(f"check nok: {line}")
                return 0
            # for all service_id multiple of 10, check.sh exit with 1
            if service_id % 10 != 0:
                if ret_code != '0':
                    logger.console(f"check ret_code no OK: {line}")
                    return 0
            else:
                if ret_code != '1':
                    logger.console(f"check ret_code no CRITICAL: {line}")
                    return 0
            if (host_id - 1)*engine.service_by_host + service_host_index != service_id:
                logger.console(f"bad service id:{line}")
                return 0
            if host_id != env_host_id:
                logger.console(f"bad env host id:{line}")
                return 0
            if service_id != env_service_id:
                logger.console(f"bad env service id:{line}")
                return 0
            if host_id != env_host_id_2:
                logger.console(f"bad env val_host id:{line}")
                return 0
            service_checked[service_id] = 1
    return len(service_checked)
