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
"""Generation of Engine configurations of arbitrary size, outside of robot.

The objects written here are deliberately the same as the ones
``tests/resources/Engine.py`` generates -- same directives, same macros, same
proportion of commands going through the Perl connector -- so that a figure
measured by the standalone benchmark and one measured by a robot test describe
the same work. What differs is what surrounds them: everything lands in one
throw-away directory, nothing is written under /tmp/etc or /tmp/var, and no
broker module is referenced. That is what makes this generator usable without a
container, a database or a daemon.

Contacts are generated but left out of centengine.cfg, exactly as the robot
template does: hosts and services reference no contact, so resolve() is happy
without them, and a benchmark that suddenly started resolving notification
contacts would no longer be measuring the same thing.

One deliberate departure from that template: hosts and services carry a
notification_period. Without it resolve() emits "Notifier has no notification
time period defined!" once per object, and at fifty thousand services the
benchmark would be measuring spdlog rather than the configuration path.
"""

import os
from typing import NamedTuple


class GeneratedConfig(NamedTuple):
    """Where a generated configuration lives, and how big it is."""

    directory: str
    main_file: str
    hosts: int
    services: int


_TIMEPERIODS = """define timeperiod {
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
"""

_CONNECTORS = """define connector {
    connector_name                 Perl Connector
    connector_line                 /usr/lib64/centreon-connector/centreon_connector_perl
}

define connector {
    connector_name                 SSH Connector
    connector_line                 /usr/lib64/centreon-connector/centreon_connector_ssh
}
"""


def _host(host_id: int) -> str:
    """Render one host definition.

    Args:
        host_id (int): the host id, which also seeds its address.

    Returns:
        The host block, as text.
    """
    # Same address spreading as the robot generator, so that two hosts never
    # share an address and the object count is the only thing that varies.
    a = host_id % 255
    q = host_id // 255
    b = q % 255
    q //= 255
    c = q % 255
    q //= 255
    d = q % 255
    return (f"define host {{\n"
            f"    host_name                      host_{host_id}\n"
            f"    alias                          host_{host_id}\n"
            f"    address                        {a}.{b}.{c}.{d}\n"
            f"    check_command                  checkh{host_id}\n"
            f"    check_period                   24x7\n"
            f"    notification_period            24x7\n"
            f"    register                       1\n"
            f"    _KEY{host_id}                      VAL{host_id}\n"
            f"    _SNMPCOMMUNITY                 public\n"
            f"    _SNMPVERSION                   2c\n"
            f"    _HOST_ID                       {host_id}\n"
            f"}}\n")


def _service(host_id: int, service_id: int, command: str) -> str:
    """Render one service definition.

    Args:
        host_id (int): the host it belongs to.
        service_id (int): its own id.
        command (str): the check command it references.

    Returns:
        The service block, as text.
    """
    return (f"define service {{\n"
            f"    host_name                       host_{host_id}\n"
            f"    service_description             service_{service_id}\n"
            f"    _SERVICE_ID                     {service_id}\n"
            f"    check_command                   {command}\n"
            f"    check_period                    24x7\n"
            f"    notification_period             24x7\n"
            f"    max_check_attempts              3\n"
            f"    check_interval                  5\n"
            f"    retry_interval                  5\n"
            f"    register                        1\n"
            f"    active_checks_enabled           1\n"
            f"    passive_checks_enabled          1\n"
            f"    _KEY_SERV{host_id}_{service_id}                VAL_SERV{service_id}\n"
            f"}}\n")


def _main_config(directory: str, engine_home: str, timezone: str) -> str:
    """Render centengine.cfg.

    Args:
        directory (str): where the object files sit; also where the log, the
            status file and the retention file are written, so that a run leaves
            nothing outside its own directory.
        engine_home (str): directory holding the check plugins referenced by the
            generated commands. Nothing is ever executed by --verify-config, but
            a plausible path keeps the configuration readable.
        timezone (str): value of use_timezone, tzdata prefixed with a colon.

    Returns:
        The content of centengine.cfg.
    """
    # No broker_module and no broker_module_cfg_file: the configuration path
    # being measured does not load them, and referencing them would tie the
    # benchmark to an installed cbmod.
    return "\n".join([
        f"cfg_file={directory}/hosts.cfg",
        f"cfg_file={directory}/services.cfg",
        f"cfg_file={directory}/commands.cfg",
        f"cfg_file={directory}/hostgroups.cfg",
        f"cfg_file={directory}/servicegroups.cfg",
        f"cfg_file={directory}/timeperiods.cfg",
        f"cfg_file={directory}/connectors.cfg",
        f"#cfg_file={directory}/contacts.cfg",
        "interval_length=60",
        f"use_timezone={timezone}",
        f"resource_file={directory}/resource.cfg",
        f"log_file={directory}/centengine.log",
        f"status_file={directory}/status.dat",
        f"state_retention_file={directory}/retention.dat",
        "command_check_interval=1s",
        f"command_file={directory}/centengine.cmd",
        "retention_update_interval=60",
        "sleep_time=0.2",
        "service_inter_check_delay_method=s",
        "service_interleave_factor=s",
        "max_concurrent_checks=400",
        "max_service_check_spread=5",
        "check_result_reaper_frequency=5",
        "low_service_flap_threshold=25.0",
        "high_service_flap_threshold=50.0",
        "low_host_flap_threshold=25.0",
        "high_host_flap_threshold=50.0",
        "service_check_timeout=10",
        "host_check_timeout=12",
        "event_handler_timeout=30",
        "notification_timeout=30",
        "ocsp_timeout=5",
        "ochp_timeout=5",
        "perfdata_timeout=5",
        "date_format=euro",
        "illegal_object_name_chars=~!$%^&*\"|'<>?,()=",
        "illegal_macro_output_chars=`~$^&\"|'<>",
        "admin_email=titus@bidibule.com",
        "admin_pager=admin",
        "event_broker_options=-1",
        "cached_host_check_horizon=60",
        f"debug_file={directory}/centengine.debug",
        "debug_level=0",
        "debug_verbosity=2",
        "log_pid=1",
        "enable_macros_filter=0",
        "instance_heartbeat_interval=30",
        "enable_notifications=1",
        "execute_service_checks=1",
        "accept_passive_service_checks=1",
        "enable_event_handlers=1",
        "check_external_commands=1",
        "use_retained_program_state=1",
        "use_retained_scheduling_info=1",
        "use_syslog=0",
        "log_notifications=1",
        "log_service_retries=1",
        "log_host_retries=1",
        "log_event_handlers=1",
        "log_external_commands=1",
        "log_v2_enabled=1",
        "log_legacy_enabled=0",
        "log_file_line=1",
        "log_v2_logger=file",
        # The generated configuration is meant to be loaded, not watched: the
        # trace levels of the robot template would write hundreds of megabytes
        # and the benchmark would be measuring spdlog.
        "log_level_functions=error",
        "log_level_config=info",
        "log_level_events=error",
        "log_level_checks=error",
        "log_level_notifications=error",
        "log_level_eventbroker=error",
        "log_level_external_command=error",
        "log_level_commands=error",
        "log_level_downtimes=error",
        "log_level_comments=error",
        "log_level_macros=error",
        "log_level_process=info",
        "log_level_runtime=error",
        f"engine_home={engine_home}",
        "",
    ])


def generate(directory: str, hosts: int, services_by_host: int,
             commands: int = 50, engine_home: str = "/usr/share/centreon-engine",
             timezone: str = ":Europe/Paris") -> GeneratedConfig:
    """Write a complete Engine configuration of the requested size.

    Args:
        directory (str): where to write it. Created if missing; its .cfg files
            are overwritten.
        hosts (int): number of hosts.
        services_by_host (int): number of services per host.
        commands (int, optional): size of the pool of service check commands,
            drawn from as the robot generator does. Defaults to 50.
        engine_home (str, optional): directory of the check plugins. Defaults to
            "/usr/share/centreon-engine".
        timezone (str, optional): use_timezone value. Defaults to
            ":Europe/Paris", like the robot template -- which requires tzdata to
            be installed, exactly as the robot tests do.

    Returns:
        A GeneratedConfig describing what was written.
    """
    os.makedirs(directory, mode=0o775, exist_ok=True)

    with open(f"{directory}/centengine.cfg", "w") as f:
        f.write(_main_config(directory, engine_home, timezone))

    service_count = 0
    with open(f"{directory}/hosts.cfg", "w") as fh, \
            open(f"{directory}/services.cfg", "w") as fs:
        for host_id in range(1, hosts + 1):
            fh.write(_host(host_id))
            for _ in range(services_by_host):
                service_count += 1
                # Round-robin over the command pool rather than the random draw
                # of the robot generator: two runs of the same size must produce
                # byte-identical files, or the parse time would carry noise that
                # has nothing to do with the code being measured.
                command_id = (service_count % commands) + 1
                fs.write(_service(host_id, service_count,
                                  f"command_{command_id}"))

    with open(f"{directory}/commands.cfg", "w") as f:
        for i in range(1, commands + 1):
            f.write(f"define command {{\n"
                    f"    command_name                    command_{i}\n"
                    f"    command_line                    {engine_home}/check.pl --id {i}\n")
            # Half of them go through the Perl connector, as in the robot
            # configuration: connector resolution is part of what apply() does.
            if i % 2 == 0:
                f.write("    connector                       Perl Connector\n")
            f.write("}\n")
        for host_id in range(1, hosts + 1):
            f.write(f"define command {{\n"
                    f"    command_name                    checkh{host_id}\n"
                    f"    command_line                    {engine_home}/check.pl --id 0\n"
                    f"}}\n")
        f.write(f"define command {{\n"
                f"    command_name                    command_notif\n"
                f"    command_line                    {engine_home}/notif.pl\n"
                f"}}\n")

    with open(f"{directory}/timeperiods.cfg", "w") as f:
        f.write(_TIMEPERIODS)
    with open(f"{directory}/connectors.cfg", "w") as f:
        f.write(_CONNECTORS)
    with open(f"{directory}/resource.cfg", "w") as f:
        f.write("$USER1$=/usr/lib64/nagios/plugins\n"
                "$CENTREONPLUGINS$=/usr/lib/centreon/plugins\n")
    for empty in ("hostgroups.cfg", "servicegroups.cfg"):
        with open(f"{directory}/{empty}", "w") as f:
            f.write(f"#{empty}\n")
    with open(f"{directory}/contacts.cfg", "w") as f:
        f.write("define contact {\n"
                "    contact_name                   John_Doe\n"
                "    alias                          admin\n"
                "    email                          admin@admin.tld\n"
                "    host_notification_period       24x7\n"
                "    service_notification_period    24x7\n"
                "    host_notification_options      d,u,r,f,s\n"
                "    service_notification_options   w,c,r\n"
                "    register                       1\n"
                "    host_notifications_enabled     1\n"
                "    service_notifications_enabled  1\n"
                "    service_notification_commands  command_notif\n"
                "    host_notification_commands     command_notif\n"
                "}\n")

    return GeneratedConfig(directory=directory,
                           main_file=f"{directory}/centengine.cfg",
                           hosts=hosts, services=service_count)
