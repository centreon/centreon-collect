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

from os import makedirs
from robot.libraries.BuiltIn import BuiltIn

ETC_ROOT = BuiltIn().get_variable_value("${EtcRoot}")
CONF_DIR = ETC_ROOT + "/centreon-engine"


agent_config="""
{
    "log_level":"trace",
    "endpoint":"localhost:4317",
    "host":"host_1",
    "log_type":"file",
    "log_file":"/tmp/var/log/centreon-engine/centreon-agent.log" """


def ctn_config_centreon_agent(key_path:str = None, cert_path:str = None, ca_path:str = None):
    """ctn_config_centreon_agent
    Creates a default centreon agent config without encryption nor reverse connection
    """
    makedirs(CONF_DIR, mode=0o777, exist_ok=True)
    with open(f"{CONF_DIR}/centagent.json", "w") as ff:
        ff.write(agent_config)
        if key_path is not None or  cert_path is not None or ca_path is not None:
            ff.write(",\n  \"encryption\":true")
        if key_path is not None:
            ff.write(f",\n  \"private_key\":\"{key_path}\"")
        if cert_path is not None:
            ff.write(f",\n  \"public_cert\":\"{cert_path}\"")
        if ca_path is not None:
            ff.write(f",\n  \"ca_certificate\":\"{ca_path}\"")
        ff.write("\n}\n")



def ctn_config_reverse_centreon_agent(key_path:str = None, cert_path:str = None, ca_path:str = None):
    """ctn_config_centreon_agent
    Creates a default reversed centreon agent config without encryption listening on 0.0.0.0:4317
    """
    makedirs(CONF_DIR, mode=0o777, exist_ok=True)
    with open(f"{CONF_DIR}/centagent.json", "w") as ff:
        ff.write(agent_config)
        ff.write(",\n  \"reverse_connection\":true")
        if key_path is not None or  cert_path is not None or ca_path is not None:
            ff.write(",\n  \"encryption\":true")
        if key_path is not None:
            ff.write(f",\n  \"private_key\":\"{key_path}\"")
        if cert_path is not None:
            ff.write(f",\n  \"public_cert\":\"{cert_path}\"")
        if ca_path is not None:
            ff.write(f",\n  \"ca_certificate\":\"{ca_path}\"")
        ff.write("\n}\n")
