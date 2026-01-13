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

# utilities used in stream connector tests

import json
import re


def ctn_get_host_cache_info(log_file_path: str, host_id: int):
    """
    Find last line like host 1 : { "host_id" : "1","name" : "host_1","notes_url" : "","hostgroups" : .....

    Args:
        log_file_path (str): log file path
        host_id:
    Returns:
        Data of the json string as a python object 
    """
    with open(log_file_path) as f:
        lines = f.readlines()

    search_pattern = re.compile(
        rf"^.*INFO:\s+host {host_id}\s+:\s+({{.*}})$")

    last = {}
    for line in lines:
        m = search_pattern.search(line)
        if m is not None:
            last = json.loads(m.group(1))
    return last


def ctn_get_hostgroup_cache_info(log_file_path: str, hostgroup_id: int):
    """
    Find last line like hostgroup 1 : { "name" : "hostgroup_1","id" : "1"}

    Args:
        log_file_path (str): log file path
        hostgroup_id:
    Returns:
        Data of the json string as a python object 
    """
    with open(log_file_path) as f:
        lines = f.readlines()

    search_pattern = re.compile(
        rf"^.*INFO:\s+hostgroup {hostgroup_id}\s+:\s+({{.*}})$")

    last = {}
    for line in lines:
        m = search_pattern.search(line)
        if m is not None:
            last = json.loads(m.group(1))
    return last
