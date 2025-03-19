#!/usr/bin/env python3
"""
 * Copyright 2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com


The goal of this script is to bench cma and nsclient++ by creating many services.
You can create 3 types of checks
- cpu (native)
- memory (native)
- echo (cmd.exe /C echo)

"""

import argparse

parser = argparse.ArgumentParser(
    prog="create_bench_service.py", description='add services to centreon-engine in order to stress cma or nsclient.')
parser.add_argument('--agent', '-a', type=str, default="cma",
                    help='type of agent: cma or nsclient or nsclient_connector or nrpe or nrpe_connector.')
parser.add_argument('--echo', '-e', type=int, default=0,
                    help='number of echo service (cmd.exe /C echo).')
parser.add_argument('--cpu', '-c', type=int, default=0,
                    help='number of native cpu check.')
parser.add_argument('--memory', '-m', type=int, default=0,
                    help='number of native memory check.')
parser.add_argument('--plugin_storage', '-p',  type=int, default=0, help='number of centreon_plugins.exe storage check.')

args = parser.parse_args()

def create_common_services_central():
    """
    Create common services for central server (poller not cma)
    """

    with open("/etc/centreon-engine/services.cfg", "w") as ff:
        ff.write("""
define service {
    host_name                      Centreon-central 
    service_description            proc-sshd 
    register                       1 
    use                            App-Monitoring-Centreon-Process-sshd-custom 
    _SERVICE_ID                    101 
}

define service {
    host_name                      Centreon-central 
    service_description            proc-httpd 
    register                       1 
    use                            App-Monitoring-Centreon-Process-httpd-custom 
    _SERVICE_ID                    102 
}

define service {
    host_name                      Centreon-central 
    service_description            proc-crond 
    register                       1 
    use                            App-Monitoring-Centreon-Process-crond-custom 
    _SERVICE_ID                    103 
}

define service {
    host_name                      Centreon-central 
    service_description            proc-centengine 
    register                       1 
    use                            App-Monitoring-Centreon-Process-centengine-custom 
    _SERVICE_ID                    104 
}

define service {
    host_name                      Centreon-central 
    service_description            proc-gorgone 
    register                       1 
    use                            App-Monitoring-Centreon-Process-centcore-custom 
    _CRITICAL                      1: 
    _PROCESSNAME                   gorgone-.* 
    _SERVICE_ID                    105 
}

define service {
    host_name                      Centreon-central 
    service_description            proc-broker-sql 
    register                       1 
    use                            App-Monitoring-Centreon-Process-broker-sql-custom 
    _PROCESSARGS                   /etc/centreon-broker/central-broker.json 
    _SERVICE_ID                    106 
}

define service {
    host_name                      Centreon-central 
    service_description            proc-broker-rrd 
    register                       1 
    use                            App-Monitoring-Centreon-Process-broker-rrd-custom 
    _PROCESSARGS                   /etc/centreon-broker/central-rrd.json 
    _SERVICE_ID                    107 
}

define service {
    host_name                      Centreon-central 
    service_description            Ping 
    register                       1 
    use                            Base-Ping-LAN-custom 
    _SERVICE_ID                    108 
}

define service {
    host_name                      Centreon-central 
    service_description            Swap 
    register                       1 
    use                            OS-Linux-Swap-SNMP-custom 
    _SERVICE_ID                    109 
}

define service {
    host_name                      Centreon-central 
    service_description            Memory 
    register                       1 
    use                            OS-Linux-Memory-SNMP-custom 
    _EXTRAOPTIONS                  '--redhat' 
    _SERVICE_ID                    110 
}

define service {
    host_name                      Centreon-central 
    service_description            Load 
    register                       1 
    use                            OS-Linux-Load-SNMP-custom 
    _SERVICE_ID                    111 
}

define service {
    host_name                      Centreon-central 
    service_description            Cpu 
    register                       1 
    use                            OS-Linux-Cpu-SNMP-custom 
    _SERVICE_ID                    112 
}


""")
        
def create_common_services():
    with open("/etc/centreon-engine/services.cfg", "w") as ff:
        ff.write("""

define service {
    host_name                      Poller
    service_description            Cpu 
    register                       1 
    use                            OS-Linux-Cpu-SNMP-custom 
    _SERVICE_ID                    200 
}


""")

def create_cma_services(nb_cpu_check: int, nb_memory_check: int, nb_echo_check: int, nb_plugin_storage: int):
    """
    Create services for cma agent
    param nb_cpu_check: number of cpu check
    param nb_memory_check: number of memory check
    param nb_echo_check: number of echo check
    """

    create_common_services()

    total_services = nb_cpu_check + nb_memory_check + nb_echo_check + nb_plugin_storage + 1
    echo_period = 0
    if nb_echo_check > 0:
        echo_period = total_services // nb_echo_check
    memory_period = 0
    if nb_memory_check > 0:
        memory_period = total_services // nb_memory_check
    cpu_period = 0
    if nb_cpu_check > 0:
        cpu_period = total_services // nb_cpu_check
    nb_plugin_storage_period = 0
    if nb_plugin_storage > 0:
        nb_plugin_storage_period = total_services // nb_plugin_storage
    with open("/etc/centreon-engine/services.cfg", "a") as ff:

        service_index = 10000

        #we try to spread all types of check over check period
        for i in range(total_services):
            if echo_period > 0 and i % echo_period == 0 :
                ff.write(f"""
define service {{
    host_name               host_windows
    service_description     bench_cma_echo_{service_index}
    use                     generic-passive-service
    register                1
    check_command           CMA_echo
    _SERVICE_ID             {service_index}
}}

""")
                service_index += 1

            if memory_period > 0 and i % memory_period == 0 :
                ff.write(f"""
define service {{
    host_name               host_windows
    service_description     bench_cma_memory_{service_index}
    use                     generic-passive-service
    register                1
    check_command           centreon_native_memory
    _SERVICE_ID             {service_index}
}}

""")
                service_index += 1

            if cpu_period > 0 and i % cpu_period == 0 :
                ff.write(f"""
define service {{
    host_name               host_windows
    service_description     bench_cma_cpu_{service_index}
    use                     generic-passive-service
    register                1
    check_command           centreon_native_cpu
    _SERVICE_ID             {service_index}
}}

""")
                service_index += 1

            if nb_plugin_storage_period > 0 and i % nb_plugin_storage_period == 0:
                ff.write(f"""
define service {{
    host_name               host_windows
    service_description     bench_cma_plugins_{service_index}
    use                     generic-passive-service
    register                1
    check_command           CMA_centreon_windows_plugins_cpu
    _SERVICE_ID             {service_index}
}}

""")
                service_index += 1

        ff.write(f"""
define service {{
    host_name               host_windows
    service_description     cma_load
    use                     generic-passive-service
    register                1
    check_command           process_load!centagent
    _SERVICE_ID             {service_index}
}}

""")
        

def create_nsclient_services(nb_cpu_check: int, nb_memory_check: int, nb_echo_check: int):
    """
    Create services for nsclient agent
    param nb_cpu_check: number of cpu check
    param nb_memory_check: number of memory check
    param nb_echo_check: number of echo check
    """
    create_common_services()

    total_services = nb_cpu_check + nb_memory_check + nb_echo_check + 1
    echo_period = 0
    if nb_echo_check > 0:
        echo_period = total_services // nb_echo_check
    memory_period = 0
    if nb_memory_check > 0:
        memory_period = total_services // nb_memory_check
    cpu_period = 0
    if nb_cpu_check > 0:
        cpu_period = total_services // nb_cpu_check
    with open("/etc/centreon-engine/services.cfg", "a") as ff:

        service_index = 10000

        #we try to spread all types of check over check period
        for i in range(total_services):
            if echo_period > 0 and i % echo_period == 0 :
                ff.write(f"""
define service {{
    host_name               host_windows
    service_description     bench_nsclient_echo_{service_index}
    use                     generic-active-service
    register                1
    check_command           OS-Windows-NSClient05-Restapi-Echo
    _SERVICE_ID             {service_index}
}}

""")
                service_index += 1

            if memory_period > 0 and i % memory_period == 0 :
                ff.write(f"""
define service {{
    host_name               host_windows
    service_description     bench_nsclient_memory_{service_index}
    use                     generic-active-service
    register                1
    check_command           OS-Windows-NSClient05-Restapi-Memory
    _SERVICE_ID             {service_index}
}}

""")
                service_index += 1

            if cpu_period > 0 and i % cpu_period == 0 :
                ff.write(f"""
define service {{
    host_name               host_windows
    service_description     bench_nsclient_cpu_{service_index}
    use                     generic-active-service
    register                1
    check_command           OS-Windows-NSClient05-Restapi-Cpu
    _SERVICE_ID             {service_index}
}}

""")
                service_index += 1
        

        ff.write(f"""
define service {{
    host_name               host_windows
    service_description     nsclient_load
    use                     generic-active-service
    register                1
    check_command           nsclient_process_load
    _SERVICE_ID             {service_index}
}}

""")
        

def create_nsclient_services_connector(nb_cpu_check: int, nb_memory_check: int, nb_echo_check: int, nb_plugin_storage: int):
    """
    create nsclient services as create_nsclient_services does but uses connector to reduce cpu footprint on poller side
    param nb_cpu_check: number of cpu check
    param nb_memory_check: number of memory check
    param nb_echo_check: number of echo check    
    """
    create_common_services()

    total_services = nb_cpu_check + nb_memory_check + nb_echo_check + nb_plugin_storage + 1
    echo_period = 0
    if nb_echo_check > 0:
        echo_period = total_services // nb_echo_check
    memory_period = 0
    if nb_memory_check > 0:
        memory_period = total_services // nb_memory_check
    cpu_period = 0
    if nb_cpu_check > 0:
        cpu_period = total_services // nb_cpu_check
    plugin_storage_period = 0
    if nb_plugin_storage > 0:
        plugin_storage_period = total_services // nb_plugin_storage
    with open("/etc/centreon-engine/services.cfg", "a") as ff:

        service_index = 10000

        #we try to spread all types of check over check period
        for i in range(total_services):
            if echo_period > 0 and i % echo_period == 0 :
                ff.write(f"""
define service {{
    host_name               host_windows
    service_description     bench_nsclient_echo_{service_index}
    use                     generic-active-service
    register                1
    check_command           OS-Windows-NSClient05-Restapi-Echo-Connector
    _SERVICE_ID             {service_index}
}}

""")
                service_index += 1

            if memory_period > 0 and i % memory_period == 0 :
                ff.write(f"""
define service {{
    host_name               host_windows
    service_description     bench_nsclient_memory_{service_index}
    use                     generic-active-service
    register                1
    check_command           OS-Windows-NSClient05-Restapi-Memory-Connector
    _SERVICE_ID             {service_index}
}}

""")
                service_index += 1

            if cpu_period > 0 and i % cpu_period == 0 :
                ff.write(f"""
define service {{
    host_name               host_windows
    service_description     bench_nsclient_cpu_{service_index}
    use                     generic-active-service
    register                1
    check_command           OS-Windows-NSClient05-Restapi-Cpu-Connector
    _SERVICE_ID             {service_index}
}}

""")
                service_index += 1

            if plugin_storage_period > 0 and i % plugin_storage_period == 0:
                ff.write(f"""
define service {{
    host_name               host_windows
    service_description     bench_nsclient_plugins_{service_index}
    use                     generic-active-service
    register                1
    check_command           OS-Windows-NSClient05-Restapi-Plugins-Storage-Connector
    _SERVICE_ID             {service_index}
}}

""")
                service_index += 1

        ff.write(f"""
define service {{
    host_name               host_windows
    service_description     nsclient_conn_load
    use                     generic-active-service
    register                1
    check_command           nsclient_process_load-Connector
    _SERVICE_ID             {service_index}
}}

""")
        

def create_nrpe_services(nb_cpu_check: int, nb_memory_check: int, nb_echo_check: int):
    """
    create nrpe services
    param nb_cpu_check: number of cpu check
    param nb_memory_check: number of memory check
    param nb_echo_check: number of echo check    
    """
    create_common_services()

    total_services = nb_cpu_check + nb_memory_check + nb_echo_check + 1
    echo_period = 0
    if nb_echo_check > 0:
        echo_period = total_services // nb_echo_check
    memory_period = 0
    if nb_memory_check > 0:
        memory_period = total_services // nb_memory_check
    cpu_period = 0
    if nb_cpu_check > 0:
        cpu_period = total_services // nb_cpu_check
    with open("/etc/centreon-engine/services.cfg", "a") as ff:

        service_index = 10000

        #we try to spread all types of check over check period
        for i in range(total_services):
            if echo_period > 0 and i % echo_period == 0 :
                ff.write(f"""
define service {{
    host_name               host_windows
    service_description     bench_nrpe_echo_{service_index}
    use                     generic-active-service
    register                1
    check_command           OS-Windows-NRPE-Echo
    _SERVICE_ID             {service_index}
}}

""")
                service_index += 1

            if memory_period > 0 and i % memory_period == 0 :
                ff.write(f"""
define service {{
    host_name               host_windows
    service_description     bench_nrpe_memory_{service_index}
    use                     generic-active-service
    register                1
    check_command           OS-Windows-NRPE-Memory
    _SERVICE_ID             {service_index}
}}

""")
                service_index += 1

            if cpu_period > 0 and i % cpu_period == 0 :
                ff.write(f"""
define service {{
    host_name               host_windows
    service_description     bench_nrpe_cpu_{service_index}
    use                     generic-active-service
    register                1
    check_command           OS-Windows-NRPE-Cpu
    _SERVICE_ID             {service_index}
}}

""")
                service_index += 1

        ff.write(f"""
define service {{
    host_name               host_windows
    service_description     nsclient_nrpe_load
    use                     generic-active-service
    register                1
    check_command           OS-Windows-NRPE-Load
    _SERVICE_ID             {service_index}
}}

""")
        

def create_nrpe_services_connector(nb_cpu_check: int, nb_memory_check: int, nb_echo_check: int):
    """
    create nrpe services as create_nrpe_services does but uses connector to reduce cpu footprint on poller side
    param nb_cpu_check: number of cpu check
    param nb_memory_check: number of memory check
    param nb_echo_check: number of echo check    
    """
    create_common_services()

    total_services = nb_cpu_check + nb_memory_check + nb_echo_check + 1
    echo_period = 0
    if nb_echo_check > 0:
        echo_period = total_services // nb_echo_check
    memory_period = 0
    if nb_memory_check > 0:
        memory_period = total_services // nb_memory_check
    cpu_period = 0
    if nb_cpu_check > 0:
        cpu_period = total_services // nb_cpu_check
    with open("/etc/centreon-engine/services.cfg", "a") as ff:

        service_index = 10000

        #we try to spread all types of check over check period
        for i in range(total_services):
            if echo_period > 0 and i % echo_period == 0 :
                ff.write(f"""
define service {{
    host_name               host_windows
    service_description     bench_nrpe_echo_{service_index}
    use                     generic-active-service
    register                1
    check_command           OS-Windows-NRPE-Echo-Connector
    _SERVICE_ID             {service_index}
}}

""")
                service_index += 1

            if memory_period > 0 and i % memory_period == 0 :
                ff.write(f"""
define service {{
    host_name               host_windows
    service_description     bench_nrpe_memory_{service_index}
    use                     generic-active-service
    register                1
    check_command           OS-Windows-NRPE-Memory-Connector
    _SERVICE_ID             {service_index}
}}

""")
                service_index += 1

            if cpu_period > 0 and i % cpu_period == 0 :
                ff.write(f"""
define service {{
    host_name               host_windows
    service_description     bench_nrpe_cpu_{service_index}
    use                     generic-active-service
    register                1
    check_command           OS-Windows-NRPE-Cpu-Connector
    _SERVICE_ID             {service_index}
}}

""")
                service_index += 1

        ff.write(f"""
define service {{
    host_name               host_windows
    service_description     nsclient_nrpe_conn_load
    use                     generic-active-service
    register                1
    check_command           OS-Windows-NRPE-Load-Connector
    _SERVICE_ID             {service_index}
}}

""")
        



"""
create_bench_service.py

This script is used to create services in the Centreon monitoring system.
"""

if args.agent == "cma":
    create_cma_services(args.cpu, args.memory, args.echo, args.plugin_storage)
elif args.agent == "nsclient":
    create_nsclient_services(args.cpu, args.memory, args.echo)
elif args.agent == "nsclient_connector":
    create_nsclient_services_connector(args.cpu, args.memory, args.echo, args.plugin_storage)
elif args.agent == "nrpe":
    create_nrpe_services(args.cpu, args.memory, args.echo)
elif args.agent == "nrpe_connector":
    create_nrpe_services_connector(args.cpu, args.memory, args.echo)
