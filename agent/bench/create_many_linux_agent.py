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


This script is used to create a lot of agent on linux hosts in order to stress centengine

"""

import argparse
import subprocess
from fabric import Connection

parser = argparse.ArgumentParser(
    prog="create_bench_service.py", description='add services to centreon-engine in order to stress cma or nsclient.')
parser.add_argument('--poller', '-p', type=str, 
                    help='ip of the poller where agents will connect to.')
parser.add_argument('--nb_agent', '-n', type=int, default=100,
                    help='number of agent by host to configure and start.')
parser.add_argument('--key_file', '-k', type=str, help='path of the centreon-virtu.pem file.')
parser.add_argument('--ip_file', '-i', type=str, help='path of the file that contains ips of agent hosts.')
parser.add_argument('--install', action='store_true', help='port of the poller where agents will connect to.')
parser.add_argument('--start', action='store_true', help='start agents.')
parser.add_argument('--kill', action='store_true', help='kill agents.')
parser.add_argument('--reverse', action='store_true', help='engine connect to agents.')
parser.add_argument('--nb_cpu_service', type=int,  default=20, help='nb cpu chek per agent.')

args = parser.parse_args()

def create_engine_services():
    print(f"------------------------------------- Create engine service ----------------------------------")

    with open("/tmp/services.cfg", "w") as ff:
        service_index = 100000
        host_index = 1
        with open(args.ip_file, "r") as f:
            for line in f:
                stripped = line.strip()
                if len(stripped) > 0:
                    for agent_index in range(args.nb_agent):
# not yet available on current version of linux cma
#                         ff.write(f"""
# define service {{
#     host_name               cma_host_{host_index}
#     service_description     cma_host_{host_index}_health
#     use                     generic-passive-service
#     register                1
#     check_command           agent-health
#     _SERVICE_ID             {service_index}
# }}
# """)
#                         service_index += 1

                        for cpu_index in range(args.nb_cpu_service):
                            ff.write(f"""
define service {{
    host_name               cma_host_{host_index}
    service_description     bench_cma_cpu_{service_index}
    use                     generic-passive-service
    register                1
    check_command           centreon_native_cpu
    _SERVICE_ID             {service_index}
}}

""")
                            service_index += 1
                        host_index += 1


def create_engine_hosts():
    print(f"------------------------------------- Create engine hosts ----------------------------------")
    with open("/tmp/hosts.cfg", "w") as ff:
        host_index = 1
        with open(args.ip_file, "r") as f:
            for line in f:
                stripped = line.strip()
                if len(stripped) > 0:
                    for agent_index in range(args.nb_agent):
                        ff.write(f"""
define host {{
    host_name                      cma_host_{host_index} 
    address                        {stripped} 
    check_command                  centreon_native_cpu 
    register                       1 
    use                            generic-passive-host 
    _HOST_ID                       {host_index} 
}}

""")
                        host_index += 1


def create_engine_otl_server_json():
    print(f"------------------------------------- Create engine otl server json ----------------------------------")
    with open("/tmp/otl_server.json", "w") as ff:
        ff.write("""
{
    "max_length_grpc_log": 0,
    "otel_server": {
        "host": "0.0.0.0",
        "port": 4317,
	    "max_message_length": 50
    },
    "centreon_agent": {
        "max_concurrent_checks": 100000,
	    "check_interval": 60,
        "export_period": 15""")
        if args.reverse:
            ff.write(""",
        "reverse_connections":[        
""")
            host_index = 1
            with open(args.ip_file, "r") as f:
                first_agent = True
                for line in f:
                    stripped = line.strip()
                    if len(stripped) > 0:
                        for agent_index in range(args.nb_agent):
                            if not first_agent:
                                ff.write(",")
                            first_agent = False
                            ff.write(f"""
            {{
                "host": "{stripped}",
                "port": {4317 + agent_index},
                "max_concurrent_checks": 100000
            }}""")            
                            
            ff.write("""
        ]        
""")
        ff.write("""
    }
}

""")


def upload_hosts_services_otel_json_file_and_restart_engine(host_ip:str):
    """
    Upload hosts, services and otl_server.json files on poller and restart centengine
    """
    print(f"------------------------------------- Upload service file and restart engine on {host_ip} ----------------------------------")
    c= Connection(host_ip, user='ec2-user',connect_kwargs={'key_filename':args.key_file})
    result = c.put("/tmp/services.cfg", "/tmp/services.cfg")
    print(result)
    result = c.put("/tmp/hosts.cfg", "/tmp/hosts.cfg")
    print(result)
    result = c.put("/tmp/otl_server.json", "/tmp/otl_server.json")
    print(result)
    result = c.run("sudo chown centreon-engine: /tmp/services.cfg /tmp/hosts.cfg /tmp/otl_server.json")
    print(result)
    result = c.run("sudo mv /tmp/hosts.cfg /etc/centreon-engine/hosts.cfg")
    print(result)
    result = c.run("sudo mv /tmp/services.cfg /etc/centreon-engine/services.cfg")
    print(result)
    result = c.run("sudo mv /tmp/otl_server.json /etc/centreon-engine/otl_server.json")
    print(result)
    result = c.run("sudo systemctl restart centengine")
    print(result)


def install_monitoring_agent(host_ip:str):
    """
    Install monitoring agent on host
    It copies the install_linux_agent.sh script on the host and run it
    """
    print(f"------------------------------------ Install monitoring agent on {host_ip} ----------------------------------")
    c= Connection(host_ip, user='ec2-user',connect_kwargs={'key_filename':args.key_file})
    result = c.put("install_linux_agent.sh", "/tmp/install_linux_agent.sh")
    print(result)
    result = c.run("sudo /tmp/install_linux_agent.sh")
    print(result)


def create_agent_config_file(host_ip:str, first_agent_index:int):
    """
    We run many agents on host. 
    We create a config file for each agent
    We upload them on the host
    """
    print(f"------------------------------------- Create agent config file on {host_ip} ----------------------------------")
    c= Connection(host_ip, user='ec2-user',connect_kwargs={'key_filename':args.key_file})
    reverse_str = "false"
    if args.reverse:
        reverse_str = "true"
    
    endpoint = f"{args.poller}:4317"

    for i in range(args.nb_agent):
        if args.reverse:
            endpoint = f"{host_ip}:{4317 + i}"
        with open(f"/tmp/agent{i}.cfg", "w") as f:
            f.write(f"""
{{
    "log_file":"/tmp/centagent_{i}.log",
    "log_level":"info",
    "log_type":"file",
    "log_max_file_size":10,
    "log_max_files":3,
    "endpoint":"{endpoint}",
    "encryption":false,
    "public_cert":"",
    "private_key":"",
    "ca_certificate":"",
    "ca_name":"",
    "host":"cma_host_{first_agent_index + i}",
    "reversed_grpc_streaming":{reverse_str}
}}""")
    subprocess.run("tar -cf /tmp/agent.tar /tmp/agent*.cfg", shell=True)
    result = c.put("/tmp/agent.tar", "/tmp/agent.tar")
    print(result)
    result = c.run("tar -xf /tmp/agent.tar -C /")
    print(result)

def create_agent_sh_start_file():
    """
    Create a script that start all agents
    """
    print(f"------------------------------------- Create agent start file ----------------------------------")
    with open("start_agent.sh", "w") as f:
        f.write(f"""#!/bin/bash

killall -9 centagent

sleep 5

/bin/rm /tmp/centagent*.log
  
for i in `seq 0 {args.nb_agent - 1}`
do
    /usr/bin/centagent /tmp/agent$i.cfg 1>/dev/null 2>&1 </dev/null & 
done
""")

def start_agent(host_ip:str):
    """
    Start all agents on host
    """
    print(f"------------------------------------- Start agent on {host_ip} ----------------------------------")
    c= Connection(host_ip, user='ec2-user',connect_kwargs={'key_filename':args.key_file})
    result = c.put("start_agent.sh", "/tmp/start_agent.sh")
    print(result)
    result = c.run("sh /tmp/start_agent.sh")
    print(result)

def kill_agent(host_ip:str):
    """
    kill agents on host
    """
    print(f"------------------------------------- Kill agent on {host_ip} ----------------------------------")
    c= Connection(host_ip, user='ec2-user',connect_kwargs={'key_filename':args.key_file})
    result = c.run("killall -9 centagent")
    print(result)

if args.install:
    with open(args.ip_file, "r") as f:
        for line in f:
            stripped = line.strip()
            if len(stripped) > 0:
                install_monitoring_agent(line.strip())


if args.kill:
    with open(args.ip_file, "r") as f:
        for line in f:
            stripped = line.strip()
            if len(stripped) > 0:
                kill_agent(line.strip())
    exit(0)

with open(args.ip_file, "r") as f:
    first_agent_index = 1
    for line in f:
        stripped = line.strip()
        if len(stripped) > 0:
            create_agent_config_file(stripped, first_agent_index)
            first_agent_index += args.nb_agent

if args.start:
    create_agent_sh_start_file()
    create_engine_services()
    create_engine_hosts()
    create_engine_otl_server_json()
    upload_hosts_services_otel_json_file_and_restart_engine(args.poller)
    with open(args.ip_file, "r") as f:
        for line in f:
            stripped = line.strip()
            if len(stripped) > 0:
                start_agent(stripped)

