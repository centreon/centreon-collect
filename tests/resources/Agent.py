#!/usr/bin/python3
#
# Copyright 2023-2025 Centreon
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

from os import makedirs, environ
import time
from robot.libraries.BuiltIn import BuiltIn,RobotNotRunningError
from socket import gethostname
import Common
import json
from robot.api import logger

def import_robot_resources():
    global VAR_ROOT, ETC_ROOT, CONF_DIR
    try:
        BuiltIn().import_resource('db_variables.resource')
        ETC_ROOT = BuiltIn().get_variable_value("${EtcRoot}")
        VAR_ROOT = BuiltIn().get_variable_value("${VarRoot}")
        CONF_DIR = ETC_ROOT + "/centreon-engine"
    except RobotNotRunningError:
        # Handle this case if Robot Framework is not running
        print("Robot Framework is not running. Skipping resource import.")


ETC_ROOT = ""
VAR_ROOT = ""
CONF_DIR = ""

import_robot_resources()

def ctn_used_address():
    """
    ctn_used_address

    Get USED_ADDRESS env variable content
    """
    return environ.get('USED_ADDRESS', '127.0.0.1')


def ctn_host_hostname():
    """
    ctn_host_hostname

    Get HOST_HOSTNAME env variable content
    """
    return environ.get('HOST_HOSTNAME', Common.ctn_get_hostname())


agent_config = """
{
    "log_level":"trace",
    "endpoint":"localhost:4317",
    "host":"host_1",
    "log_type":"file",
    "log_file":"/tmp/var/log/centreon-engine/centreon-agent.log" """


agent_encrypted_config = f"""
{{
    "log_level":"trace",
    "endpoint":"{ctn_host_hostname()}:4318",
    "host":"host_1",
    "log_type":"file",
    "log_file":"{VAR_ROOT}/log/centreon-engine/centreon-agent.log" """


reversed_agent_config=f"""
{{
    "log_level":"trace",
    "endpoint":"{ctn_host_hostname()}:4320",
    "host":"host_1",
    "log_type":"file",
    "log_file":"{VAR_ROOT}/log/centreon-engine/centreon-agent.log" """

reversed_agent_encrypted_config=f"""
{{
    "log_level":"trace",
    "endpoint":"{ctn_host_hostname()}:4321",
    "host":"host_1",
    "log_type":"file",
    "log_file":"{VAR_ROOT}/log/centreon-engine/centreon-agent.log" """




def ctn_config_centreon_agent(key_path:str = None, cert_path:str = None, ca_path:str = None,token:str = None):
    """ctn_config_centreon_agent
    Creates a default centreon agent config listening on  0.0.0.0:4317 (no encryption) or 0.0.0.0:4318 (encryption) 
    Args:
        key_path: path of the private key file
        cert_path: path of public certificate file
        ca_path: path of the authority certificate file
    """
    #in case of wsl, agent is executed in windows host
    if environ.get("RUN_ENV","") == "WSL":
        return

    makedirs(CONF_DIR, mode=0o777, exist_ok=True)
    with open(f"{CONF_DIR}/centagent.json", "w") as ff:
        if cert_path is not None or ca_path is not None:
            ff.write(agent_encrypted_config)
        else:
            ff.write(agent_config)
        if key_path is not None or  cert_path is not None or ca_path is not None:
            ff.write(",\n  \"encryption\":true")
        if key_path is not None:
            ff.write(f",\n  \"private_key\":\"{key_path}\"")
        if cert_path is not None:
            ff.write(f",\n  \"public_cert\":\"{cert_path}\"")
        if ca_path is not None:
            ff.write(f",\n  \"ca_certificate\":\"{ca_path}\"")
        if token is not None:
            ff.write(f",\n  \"token\":\"{token}\"")

        ff.write("\n}\n")



def ctn_config_reverse_centreon_agent(key_path:str = None, cert_path:str = None, ca_path:str = None,trustred_token:list = None):
    """ctn_config_centreon_agent
    Creates a default reversed centreon agent config listening on  0.0.0.0:4320 (no encryption) or 0.0.0.0:4321 (encryption)
    Args:
        key_path: path of the private key file
        cert_path: path of public certificate file
        ca_path: path of the authority certificate file
    """
    #in case of wsl, agent is executed in windows host
    if environ.get("RUN_ENV","") == "WSL":
        return

    makedirs(CONF_DIR, mode=0o777, exist_ok=True)
    with open(f"{CONF_DIR}/centagent.json", "w") as ff:
        if cert_path is not None or ca_path is not None:
            ff.write(reversed_agent_encrypted_config)
        else:
            ff.write(reversed_agent_config)
        ff.write(",\n  \"reversed_grpc_streaming\":true")
        if key_path is not None or  cert_path is not None or ca_path is not None:
            ff.write(",\n  \"encryption\":true")
        if key_path is not None:
            ff.write(f",\n  \"private_key\":\"{key_path}\"")
        if cert_path is not None:
            ff.write(f",\n  \"public_cert\":\"{cert_path}\"")
        if ca_path is not None:
            ff.write(f",\n  \"ca_certificate\":\"{ca_path}\"")
        if trustred_token is not None:
            ff.write(f",\n  \"trusted_tokens\":[")
            for index, value in enumerate(trustred_token):
                if index > 0:
                    ff.write(",")
                ff.write(f"\n \"{value}\"")
            ff.write(f"]")
        ff.write("\n}\n")


def ctn_echo_command(to_echo:str):
    """
    ctn_echo_command
    returned an echo command usable by testing agent OS
    Args:
        to_echo: string to print to stdout
    """
    if environ.get("RUN_ENV","") == "WSL":
        return '"'+ environ.get('PWSH_PATH') + '"' + " C:/Users/Public/echo.ps1 " + to_echo
    else:
        return "/bin/echo " + to_echo


def ctn_check_pl_command(arg:str):
    """
    ctn_check_pl_command
    returned an check.pl command usable by testing agent OS
    Args:
        arg: arguments to pass to check.pl or check.ps1 command
    """
    if environ.get("RUN_ENV","") == "WSL":
        return '"'+ environ.get('PWSH_PATH') + '"' +" C:/Users/Public/check.ps1 " + arg + " " + environ.get("WINDOWS_PROJECT_PATH")
    else:
        return "/tmp/var/lib/centreon-engine/check.pl " + arg 
        
def ctn_get_drive_statistics(drive_name_format:str):
    """
    ctn_get_drive_statistics
    return a dictionary of drive statistics indexed by expected perfdata names
    Args:
        drive_name_format: format of the drive name to search for
    """
    if environ.get("RUN_ENV","") == "WSL":
        drive_dict = {}
        json_test_args = environ.get("JSON_TEST_PARAMS")
        test_args = json.loads(json_test_args)
        for drive in test_args["drive"]:
            if drive['Free'] is not None:
                drive_dict[drive_name_format.format(drive['Name'])] = (100 * drive['Free']) / (drive['Used'] + drive['Free'])
        return drive_dict
    else:
        return None

def ctn_get_uptime():
    """
    ctn_get_uptime
    return a dict with only one element: uptime => uptime value
    """
    if environ.get("RUN_ENV","") == "WSL":
        uptime_dict = {}
        json_test_args = environ.get("JSON_TEST_PARAMS")
        test_args = json.loads(json_test_args)
        if test_args["uptime"] is not None:
            uptime_dict['uptime'] = time.time() - test_args["uptime"]
            return uptime_dict
    return None
     
def ctn_get_memory():
    """
    ctn_get_memory statistics
    return a dict with these elements (expected perfdata):
    - memory.free.bytes
    - memory.usage.bytes
    - memory.usage.percentage
    - swap.free.bytes
    - swap.usage.bytes
    - swap.usage.percentage
    - virtual-memory.free.bytes
    - virtual-memory.usage.bytes
    - virtual-memory.usage.percentage
    """

    if environ.get("RUN_ENV","") == "WSL":
        memory_dict = {'swap.free.bytes': None, 'swap.usage.bytes': None, 'swap.usage.percentage': None }
        json_test_args = environ.get("JSON_TEST_PARAMS")
        test_args = json.loads(json_test_args)
        if test_args["mem_info"] is not None:
            #values of systeminfo are given in Mb
            virtual_free = int(test_args["mem_info"]["virtual_free"].replace(",", "").split()[0]) *1024 *1024
            virtual_max = int(test_args["mem_info"]["virtual_max"].replace(",", "").split()[0])*1024 *1024
            free= int(test_args["mem_info"]["free"].replace(",", "").split()[0])*1024 *1024
            total = int(test_args["mem_info"]["total"].replace(",", "").split()[0])*1024 *1024
            memory_dict['virtual-memory.free.bytes'] = virtual_free
            memory_dict['virtual-memory.usage.bytes'] = virtual_max - virtual_free
            memory_dict['virtual-memory.usage.percentage'] = 100 - (100.0 * virtual_free) / virtual_max

            memory_dict['memory.free.bytes'] = free
            memory_dict['memory.usage.bytes'] = total - free
            memory_dict['memory.usage.percentage'] = 100 - (100.0 * free) / total    
            return memory_dict
    return None
    
def ctn_get_service():
    """
    ctn_get_service statistics
    return a dict with these elements (expected perfdata):
    - services.stopped.count
    - services.starting.count
    - services.stopping.count
    - services.running.count
    - services.continuing.count
    - services.pausing.count
    - services.paused.count
    """

    if environ.get("RUN_ENV","") == "WSL":
        service_dict = {'services.stopped.count': 0, 'services.starting.count': None, 'services.stopping.count': None, 'services.running.count': 0, 
                       'services.continuing.count': None, 'services.pausing.count': None, 'services.paused.count': None }
        json_test_args = environ.get("JSON_TEST_PARAMS")
        test_args = json.loads(json_test_args)
        if test_args["serv_stat"] is not None:
            service_dict["services.stopped.count"] = test_args["serv_stat"]["services.stopped.count"]
            service_dict["services.running.count"] = test_args["serv_stat"]["services.running.count"]
            return service_dict
    return None
    
