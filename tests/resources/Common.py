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
# This script is a little tcp server working on port 5669. It can simulate
# a cbd instance. It is useful to test the validity of BBDO packets sent by
# centengine.

import jwt
from robot.api import logger
from subprocess import getoutput, Popen, DEVNULL
import re
import os
from pwd import getpwnam
from google.protobuf.json_format import MessageToDict, MessageToJson
import time
import json
import psutil
import random
import shutil
import string
from dateutil import parser
from datetime import datetime, timedelta
from pathlib import Path
import pymysql.cursors
from robot.libraries.BuiltIn import BuiltIn, RobotNotRunningError
from concurrent import futures
import grpc
import grpc_stream_pb2_grpc
import state_pb2
from confcmp import ConfComparator
import Engine


def import_robot_resources():
    global DB_NAME_STORAGE, VAR_ROOT, ETC_ROOT, DB_NAME_CONF, DB_USER, DB_PASS, DB_HOST, DB_PORT
    try:
        BuiltIn().import_resource('db_variables.resource')
        DB_NAME_STORAGE = BuiltIn().get_variable_value("${DBName}")
        DB_NAME_CONF = BuiltIn().get_variable_value("${DBNameConf}")
        DB_USER = BuiltIn().get_variable_value("${DBUser}")
        DB_PASS = BuiltIn().get_variable_value("${DBPass}")
        DB_HOST = BuiltIn().get_variable_value("${DBHost}")
        DB_PORT = BuiltIn().get_variable_value("${DBPort}")
        VAR_ROOT = BuiltIn().get_variable_value("${VarRoot}")
        ETC_ROOT = BuiltIn().get_variable_value("${EtcRoot}")
    except RobotNotRunningError:
        pass  # Robot Framework is not running, skip resource import.


DB_NAME_STORAGE = ""
DB_NAME_CONF = ""
DB_USER = ""
DB_PASS = ""
DB_HOST = ""
DB_PORT = ""
VAR_ROOT = ""
ETC_ROOT = ""

BBDO2 = True

import_robot_resources()
TIMEOUT = 30


def ctn_in_bbdo2():
    """ Check if we are in bbdo2 mode
    """
    global BBDO2
    return BBDO2


def ctn_set_bbdo2(value: bool):
    """ Set the bbdo2 mode

    Args:
        value (bool): The value to set
    """
    global BBDO2
    BBDO2 = value


def ctn_parse_tests_params():
    params = os.environ.get("TESTS_PARAMS")
    if params is not None and len(params) > 4:
        return json.loads(params)
    else:
        return {}


TESTS_PARAMS = ctn_parse_tests_params()


def ctn_is_using_direct_grpc():
    default_bbdo_version = TESTS_PARAMS.get("default_bbdo_version")
    default_transport = TESTS_PARAMS.get("default_transport")
    return default_bbdo_version is not None and default_transport == "grpc" and default_bbdo_version >= "3.1.0"


def ctn_check_connection(port: int, pid1: int, pid2: int):
    limit = time.time() + TIMEOUT
    r = re.compile(
        r"^ESTAB.*127\.0\.0\.1\]*:{}\s|^ESTAB.*\[::1\]*:{}\s".format(port, port))
    p = re.compile(
        r"127\.0\.0\.1\]*:(\d+)\s+.*127\.0\.0\.1\]*:(\d+)\s+.*,pid=(\d+)")
    p_v6 = re.compile(
        r"::1\]*:(\d+)\s+.*::1\]*:(\d+)\s+.*,pid=(\d+)")
    while time.time() < limit:
        out = getoutput("ss -plant")
        lst = out.split('\n')
        estab_port = list(filter(r.match, lst))
        if len(estab_port) >= 2:
            ok = [False, False]
            for line in estab_port:
                m = p.search(line)
                if m is not None:
                    if pid1 == int(m.group(3)):
                        ok[0] = True
                    if pid2 == int(m.group(3)):
                        ok[1] = True
                m = p_v6.search(line)
                if m is not None:
                    if pid1 == int(m.group(3)):
                        ok[0] = True
                    if pid2 == int(m.group(3)):
                        ok[1] = True
            if ok[0] and ok[1]:
                return True
        time.sleep(1)
    return False


def ctn_wait_for_connections(port: int, nb: int, timeout: int = 60):
    """!  wait until nb connection are established on localhost and port
    @param port connection port
    @param nb number of connections expected
    @param timeout  timeout in seconds
    @return True if nb connections are established
    """
    limit = time.time() + timeout
    r = re.compile(
        fr"^ESTAB.*127\.0\.0\.1:{port}\s|^ESTAB.*\[::ffff:127\.0\.0\.1\]:{port}\s|^ESTAB.*\[::1\]*:{port}\s")

    while time.time() < limit:
        out = getoutput("ss -plant")
        lst = out.split('\n')
        estab_port = list(filter(r.match, lst))
        if len(estab_port) >= nb:
            return True
        logger.console(f"Currently {estab_port} connections")
        time.sleep(2)
    return False


def ctn_wait_for_listen_on_range(port1: int, port2: int, prog: str, timeout: int = 30):
    """Wait that an instance of the given program listens on each port in the
       given range. On success, the function returns True, if the timeout is
       reached with some missing instances, it returns False.

    Args:
        port1: The first port
        port2: The second port (all the ports p such that port1 <= p <p port2
               will be tested.
        prog: The name of the program that should be listening.
        timeout: A timeout in seconds.
    Returns:
        A boolean True on success.
    """
    port1 = int(port1)
    port2 = int(port2)
    rng = range(port1, port2 + 1)
    limit = time.time() + timeout
    r = re.compile(rf"^LISTEN [0-9]+\s+[0-9]+\s+\[::1\]:([0-9]+)\s+.*{prog}")
    size = port2 - port1 + 1

    def ok(line):
        m = r.match(line)
        if m:
            if int(m.group(1)) in rng:
                return True
        return False

    while time.time() < limit:
        out = getoutput("ss -plant")
        lst = out.split('\n')
        listen_port = list(filter(ok, lst))
        if len(listen_port) >= size:
            return True
    return False


def ctn_wait_for_port_free(port: int, timeout: int = 30):
    """Wait until no process is listening on the given port.

    Args:
        port: The TCP port to check.
        timeout: Timeout in seconds.
    Returns:
        True if the port is free before the timeout, False otherwise.
    """
    port = int(port)
    limit = time.time() + timeout
    r = re.compile(rf"[:\[]{port}\s")
    while time.time() < limit:
        out = getoutput("ss -tlnp")
        if not any(r.search(line) for line in out.split('\n')):
            return True
        time.sleep(0.5)
    return False


def ctn_get_date(d: str, agent_format: bool = False):
    """Generates a date from a string. This string can be just a timestamp or a date in iso format

    Args:
        d (str): the date as a string

    Returns:
        datetime: The date once converted.
    """
    try:
        ts = int(d)
        retval = datetime.fromtimestamp(int(ts))
    except ValueError:
        if (not agent_format):
            retval = parser.parse(d[:-6])
        else:
            retval = parser.parse(d)
    return retval


def ctn_extract_date_from_log(line: str, agent_format: bool = False):
    p = re.compile(r"\[([^\]]*)\]")
    m = p.match(line)
    if m is None:
        return None
    try:
        return ctn_get_date(m.group(1), agent_format)
    except parser.ParserError:
        logger.console(f"Unable to parse the date from the line {line}")
        return None


def ctn_get_round_current_date():
    """
    Returns the current date round to the nearest lower second as a timestamp.
    """
    return int(time.time())


def ctn_find_regex_in_log_with_timeout(log: str, date, content, timeout: int):

    limit = time.time() + timeout
    c = ""
    while time.time() < limit:
        ok, c = ctn_find_in_log(log, date, content, regex=True)
        if ok:
            return True, c
        time.sleep(5)
    logger.console(f"Unable to find regex '{c}' from {date} during {timeout}s")
    return False, c


def ctn_find_in_log_with_timeout(log: str, date, content, timeout: int, **kwargs):
    limit = time.time() + timeout
    c = ""
    kwargs['regex'] = False

    while time.time() <= limit:
        ok, c = ctn_find_in_log(log, date, content, **kwargs)
        if ok:
            return True
        time.sleep(5)
    logger.console(f"Unable to find '{c}' from {date} during {timeout}s")
    return False


def ctn_find_in_log_with_timeout_with_line(log: str, date, content, timeout: int):
    """! search a pattern in log from date param
    @param log: path of the log file
    @param date: date from which it begins search
    @param content: array of pattern to search
    @param timeout: time out in second
    @return  True/False, array of lines found for each pattern
    """
    limit = time.time() + timeout
    c = ""
    while time.time() < limit:
        ok, c = ctn_find_in_log(log, date, content, regex=False)
        if ok:
            return ok, c
        time.sleep(5)
    logger.console(f"Unable to find '{c}' from {date} during {timeout}s")
    return False, None


def ctn_find_in_log(log: str, date, content, **kwargs):
    """Find content in log file from the given date

    Args:
        log (str): The log file
        date (_type_): A date as a string
        content (_type_): An array of strings we want to find in the log.

    Returns:
        boolean,str: The boolean is True on success, and the string contains the first string not found in logs otherwise.
    """
    verbose = True
    regex = False
    agent_format = False
    one_of = False
    if 'verbose' in kwargs:
        verbose = 'verbose' == 'True'
    if 'regex' in kwargs:
        regex = bool(kwargs['regex'])
    if 'agent_format' in kwargs:
        agent_format = bool(kwargs['agent_format'])
    if 'one_of' in kwargs:
        one_of = bool(kwargs['one_of'])

    res = []

    try:
        with open(log, "r") as f:
            lines = f.readlines()
        idx = ctn_find_line_from(lines, date, agent_format)

        if one_of:
            found = False
            for i in range(idx, len(lines)):
                line = lines[i]
                for c in content:
                    if regex:
                        match = re.search(c, line)
                    else:
                        match = c in line
                    if match:
                        if verbose:
                            logger.console(f"\"{c}\" found at line {i} from {idx}")
                        found = True
                        res.append(line)
                        return True, res
        else:
            for c in content:
                found = False
                for i in range(idx, len(lines)):
                    line = lines[i]
                    if regex:
                        match = re.search(c, line)
                    else:
                        match = c in line
                    if match:
                        if verbose:
                            logger.console(f"\"{c}\" found at line {i} from {idx}")
                        found = True
                        res.append(line)
                        break
                if not found:
                    return False, c

        return True, res
    except IOError:
        logger.console("The file '{}' does not exist".format(log))
        return False, content[0]


def ctn_get_fqdn_hostname():
    """Return the fqdn host name of the computer.

    Returns:
        str: the host name
    """
    retval = getoutput("hostname --fqdn")
    retval = retval.rstrip()
    return retval


def ctn_create_key_and_certificate(host: str, key: str, cert: str):
    if len(key) > 0:
        os.makedirs(os.path.dirname(key), mode=0o777, exist_ok=True)
    if len(cert) > 0:
        os.makedirs(os.path.dirname(cert), mode=0o777, exist_ok=True)
    if len(key) > 0:
        getoutput(
            f"openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 -keyout {key} -out {cert} -subj '/CN={host}'")
    else:
        getoutput(
            f"openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 -out {cert} -subj '/CN={host}'")


def ctn_create_certificate(host: str, cert: str):
    ctn_create_key_and_certificate(host, "", cert)


def ctn_run_env():
    """
    ctn_run_env

    Get RUN_ENV env variable content
    """
    with open('/proc/1/environ', 'r') as f:
        d = f.read()
        logger.console(d)
        arr = d.split("\x00")
        logger.console(arr)
        arr = [l for l in arr if 'container' in l]
        if len(arr) == 0:
            return os.environ.get('RUN_ENV', '')
        else:
            ctn = arr[0].split('=')[1]
            return ctn

def ctn_get_workspace_win():
    """
    ctn_get_workspace_win

    Get WINDOWS_PROJECT_PATH env variable content
    """
    return os.environ.get('WINDOWS_PROJECT_PATH', '')


def ctn_start_mysql():
    if not ctn_run_env():
        if os.path.exists("/usr/bin/systemctl"):
            logger.console("Starting Mariadb with systemd")
            getoutput("systemctl start mysql")
            logger.console("Mariadb started with systemd")
        elif os.path.exists("/usr/sbin/service"):
            if os.path.exists("/usr/libexec/mysqldtoto"):
                logger.console("Starting Mysql with service")
                getoutput("service mysql start")
                logger.console("Mysql started with service")
            elif os.path.exists("/usr/sbin/mariadbd"):
                logger.console("Starting Mariadb with service")
                getoutput("service mariadb start")
                logger.console("Mariadb started with service")
            else:
                logger.console("Cannot detect which database is used, trying to start mariadb with service")
                getoutput("service mariadb start")
        else:
            logger.console("Unable to start the database")
    else:
        if os.path.exists("/usr/libexec/mysqldtoto"):
            logger.console("Starting mysqld directly")
            Popen(["/usr/libexec/mysqldtoto",
                   "--user=root"], stdout=DEVNULL, stderr=DEVNULL)
            logger.console("mysqld directly started")
        elif os.path.exists("/run/mysqld"):
            logger.console("Starting Mariadb directly")
            Popen(["mariadbd", "--socket=/run/mysqld/mysqld.sock",
                   "--user=root"], stdout=DEVNULL, stderr=DEVNULL)
            logger.console("Mariadb directly started")
        else:
            logger.console("Starting Mariadb directly")
            Popen(["mariadbd", "--socket=/var/lib/mysql/mysql.sock",
                   "--user=root"], stdout=DEVNULL, stderr=DEVNULL)
            logger.console("Mariadb directly started")


def is_mariadb_running():
    for proc in psutil.process_iter():
        if ('mariadbd' in proc.name()):
            return True
    return False

def is_mysqld_running():
    for proc in psutil.process_iter():
        if ('mysqldtoto' in proc.name()):
            return True
    return False

def ctn_stop_mysql():
    logger.console("Stopping Mariadb/Mysql")
    if not ctn_run_env():
        logger.console("Stopping Mariadb/Mysql with service or systemd")
        if os.path.exists("/usr/bin/systemctl"):
            logger.console("Stopping Mariadb with systemd")
            getoutput("systemctl stop mysql")
            logger.console("Mariadb stopped with systemd")
        elif os.path.exists("/usr/sbin/service"):
            logger.console("Stopping Mariadb with service")
            # Is it mariadb or mysql ?
            if is_mysqld_running():
                getoutput("service mysql stop")
                logger.console("Mysql stopped with service")
            elif is_mariadb_running():
                getoutput("service mariadb stop")
                logger.console("Mariadb stopped with service")
        else:
            logger.console("Unable to stop the database")
    else:
        logger.console("Stopping Mariadb/Mysql directly")
        if os.path.exists("/usr/libexec/mysqldtoto"):
            logger.console("Stopping directly mysqld")
            for proc in psutil.process_iter():
                if ('mysqldtoto' in proc.name()):
                    logger.console(
                        f"process '{proc.name()}' containing mysqld found: stopping it")
                    proc.terminate()
                    try:
                        logger.console("Waiting for 30s mysqld to stop")
                        proc.wait(30)
                    except psutil.TimeoutExpired:
                        logger.console("mysqld don't want to stop => kill")
                        proc.kill()

            for proc in psutil.process_iter():
                if ('mysqldtoto' in proc.name()):
                    logger.console(f"process '{proc.name()}' still alive")
                    logger.console("mysqld don't want to stop => kill")
                    proc.kill()

            logger.console("mysqld directly stopped")
        else:
            logger.console("Stopping directly MariaDB")
            for proc in psutil.process_iter():
                if ('mariadbd' in proc.name()):
                    logger.console(
                        f"process '{proc.name()}' containing mariadbd found: stopping it")
                    proc.terminate()
                    try:
                        logger.console("Waiting for 30s mariadbd to stop")
                        proc.wait(30)
                    except psutil.TimeoutExpired:
                        logger.console("mariadb don't want to stop => kill")
                        proc.kill()

            for proc in psutil.process_iter():
                if ('mariadbd' in proc.name()):
                    logger.console(f"process '{proc.name()}' still alive")
                    logger.console("mariadb don't want to stop => kill")
                    proc.kill()

            logger.console("Mariadb directly stopped")


def ctn_stop_rrdcached():
    getoutput("pkill -9 -f '/usr/bin/rrdcached'")


def ctn_kill_broker():
    getoutput("pkill -SIGKILL -f '/usr/sbin/cbwd'")
    getoutput("pkill -SIGKILL -f '/usr/sbin/cbd'")


def ctn_kill_engine():
    getoutput("pkill -SIGKILL -f '/usr/sbin/centengine'")
    Engine.ctn_clear_instances()


def ctn_clear_retention():
    getoutput(f"find {VAR_ROOT} -name '*.cache*' -delete")
    getoutput("find /tmp -name 'lua*' -delete")
    getoutput(f"find {VAR_ROOT} -name '*.memory.*' -delete")
    getoutput(f"find {VAR_ROOT} -name '*.queue.*' -delete")
    getoutput(f"find {VAR_ROOT} -name '*.unprocessed*' -delete")
    getoutput(f"find {VAR_ROOT} -name 'retention.dat' -delete")


def ctn_clear_cache():
    getoutput(f"find {VAR_ROOT} -name '*.cache.*' -delete")


def ctn_clear_logs():
    shutil.rmtree(f"{VAR_ROOT}/log/centreon-engine", ignore_errors=True)
    shutil.rmtree(f"{VAR_ROOT}/log/centreon-broker", ignore_errors=True)
    os.makedirs(f"{VAR_ROOT}/log/centreon-engine")
    os.makedirs(f"{VAR_ROOT}/log/centreon-broker")


def ctn_engine_log_table_duplicate(result: list):
    dup = True
    for i in result:
        if (i[0] % 2) != 0:
            dup = False
    return dup


def ctn_check_engine_logs_are_duplicated(log: str, date):
    try:
        with open(log, "r") as f:
            lines = f.readlines()

        idx = ctn_find_line_from(lines, date)
        logs_old = []
        logs_new = []
        old_log = re.compile(r"\[[^\]]*\] \[[^\]]*\] ([^\[].*)")
        new_log = re.compile(
            r"\[[^\]]*\] \[[^\]]*\] \[.*\] \[[0-9]+\] (.*)")
        for line in lines[idx:]:
            mo = old_log.match(line)
            mn = new_log.match(line)
            if mo is not None:
                if mo.group(1) in logs_new:
                    logs_new.remove(mo.group(1))
                else:
                    logs_old.append(mo.group(1))
            else:
                mn = new_log.match(line)
                if mn is not None:
                    if mn.group(1) in logs_old:
                        logs_old.remove(mn.group(1))
                    else:
                        logs_new.append(mn.group(1))
        if len(logs_old) <= 1:
            # It is possible to miss one log because of the initial split of the
            # file.
            return True
        else:
            logger.console(
                "{} old logs are not duplicated".format(len(logs_old)))
            for line in logs_old:
                logger.console(line)
            # We don't care about new logs not duplicated, in a future, we won't have any old logs
    except IOError:
        logger.console("The file '{}' does not exist".format(log))
        return False


def ctn_find_line_from(lines, date, agent_format: bool = False):
    try:
        my_date = parser.parse(date)
    except (parser.ParserError, TypeError):
        my_date = datetime.fromtimestamp(date)

    # Let's find my_date
    start = 0
    end = len(lines) - 1
    idx = start
    while end > start:
        idx = (start + end) // 2
        idx_d = ctn_extract_date_from_log(lines[idx], agent_format)
        while idx_d is None:
            logger.console("Unable to parse the date ({} <= {} <= {}): <<{}>>".format(
                start, idx, end, lines[idx]))
            idx -= 1
            if idx >= 0:
                idx_d = ctn_extract_date_from_log(lines[idx], agent_format)
            else:
                logger.console("We are at the first line and no date found")
                return 0
        if my_date <= idx_d and end != idx:
            end = idx
        elif my_date > idx_d and start != idx:
            start = idx
        else:
            break
    return idx


def ctn_check_reschedule(log: str, date, content: str, retry: bool):
    try:
        with open(log, "r") as f:
            lines = f.readlines()

        idx = ctn_find_line_from(lines, date)

        r = re.compile(r".* last check at (.*) and next check at (.*)$")
        target = 60 if retry else 300
        for i in range(idx, len(lines)):
            line = lines[i]
            if content in line:
                logger.console(
                    "\"{}\" found at line {} from {}".format(content, i, idx))
                m = r.match(line)
                if m:
                    delta = int(datetime.strptime(m[2], "%Y-%m-%dT%H:%M:%S").timestamp()) - int(
                        datetime.strptime(m[1], "%Y-%m-%dT%H:%M:%S").timestamp())
                    # delta is near target
                    if abs(delta - target) < 2:
                        return True
        logger.console(
            f"loop finished without finding a line '{content}' with a duration of {target}s")
        return False
    except IOError:
        logger.console("The file '{}' does not exist".format(log))
        return False


def ctn_check_reschedule_with_timeout(log: str, date, content: str, retry: bool, timeout: int):
    limit = time.time() + timeout
    while time.time() < limit:
        v = ctn_check_reschedule(log, date, content, retry)
        if v:
            return True
        time.sleep(5)
    return False


def ctn_clear_commands_status():
    if os.path.exists("/tmp/states"):
        os.remove("/tmp/states")


def ctn_set_command_status(cmd, status):
    if os.environ.get("RUN_ENV", "") == "WSL":
        state_path = "states"
    else:
        state_path = "/tmp/states"

    if os.path.exists(state_path):
        with open(state_path, 'r') as f:
            lines = f.readlines()
    else:
        lines = []
    p = re.compile("{}=>(.*)$".format(cmd))
    done = False
    for i in range(len(lines)):
        m = p.match(lines[i])
        if m is not None:
            if int(m.group(1)) != status:
                lines[i] = "{}=>{}\n".format(cmd, status)
            done = True
            break

    if not done:
        lines.append("{}=>{}\n".format(cmd, status))
    with open(state_path, "w") as f:
        f.writelines(lines)


def ctn_truncate_resource_host_service():
    connection = pymysql.connect(host=DB_HOST,
                                 user=DB_USER,
                                 password=DB_PASS,
                                 autocommit=True,
                                 database=DB_NAME_STORAGE,
                                 charset='utf8mb4',
                                 cursorclass=pymysql.cursors.DictCursor)

    with connection:
        with connection.cursor() as cursor:
            cursor.execute("DELETE FROM resources_tags")
            cursor.execute("DELETE FROM resources")
            cursor.execute("DELETE FROM hosts")
            cursor.execute("DELETE FROM services")


def ctn_check_service_resource_status_with_timeout(hostname: str, service_desc: str, status: int, timeout: int, state_type: str = "SOFT"):
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     autocommit=True,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    f"SELECT r.status,r.status_confirmed FROM resources r LEFT JOIN services s ON r.id=s.service_id AND r.parent_id=s.host_id LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='{hostname}' AND s.description='{service_desc}'")
                result = cursor.fetchall()
                if len(result) > 0:
                    logger.console(f"result: {result}")
                if len(result) > 0 and result[0]['status'] is not None and int(result[0]['status']) == int(status):
                    logger.console(
                        f"status={result[0]['status']} and status_confirmed={result[0]['status_confirmed']}")
                    if state_type == 'ANY':
                        return True
                    elif state_type == 'HARD' and int(result[0]['status_confirmed']) == 1:
                        return True
                    elif state_type == 'SOFT' and int(result[0]['status_confirmed']) == 0:
                        return True
        time.sleep(1)
    return False


def ctn_check_service_resource_status_with_timeout_rt(hostname: str, service_desc: str, status: int, timeout: int, state_type: str = "SOFT"):
    """
    brief : same as ctn_check_service_resource_status_with_timeout but with additional return

    Check the status of a service resource within a specified timeout period.

    This function connects to a MySQL database and queries the status of a service resource
    associated with a given hostname and service description. It repeatedly checks the status
    until the specified timeout period is reached. The function can check for different state types
    (SOFT, HARD, or ANY).

    Args:
        hostname (str): The name of the host.
        service_desc (str): The description of the service.
        status (int): The desired status to check for.
        timeout (int): The timeout period in seconds.
        state_type (str, optional): The type of state to check for. Defaults to "SOFT". 
                                    Can be "SOFT", "HARD", or "ANY".

    Returns:
        tuple: A tuple containing a boolean indicating if the desired status was found and the output message.
               (True, output) if the desired status is found within the timeout period.
               (False, "") if the desired status is not found within the timeout period.
    """
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     autocommit=True,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    f"SELECT r.status,r.status_confirmed,r.output FROM resources r LEFT JOIN services s ON r.id=s.service_id AND r.parent_id=s.host_id LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='{hostname}' AND s.description='{service_desc}'")
                result = cursor.fetchall()
                if len(result) > 0:
                    logger.console(f"result: {result}")
                if len(result) > 0 and result[0]['status'] is not None and int(result[0]['status']) == int(status):
                    logger.console(
                        f"status={result[0]['status']} and status_confirmed={result[0]['status_confirmed']}")
                    if state_type == 'ANY':
                        return True, result[0]['output']
                    elif state_type == 'HARD' and int(result[0]['status_confirmed']) == 1:
                        return True, result[0]['output']
                    elif state_type == 'SOFT' and int(result[0]['status_confirmed']) == 0:
                        return True, result[0]['output']
        time.sleep(1)
    return False, ""


def ctn_check_acknowledgement_with_timeout(hostname: str, service_desc: str, entry_time: int, status: int, timeout: int, state_type: str = "SOFT"):
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     autocommit=True,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                logger.console(
                    f"SELECT a.acknowledgement_id, a.state, a.type, a.deletion_time FROM acknowledgements a LEFT JOIN services s ON a.host_id=s.host_id AND a.service_id=s.service_id LEFT join hosts h ON s.host_id=h.host_id WHERE s.description='{service_desc}' AND h.name='{hostname}' AND entry_time >= {entry_time} ORDER BY entry_time DESC")
                cursor.execute(
                    f"SELECT a.acknowledgement_id, a.state, a.type, a.deletion_time FROM acknowledgements a LEFT JOIN services s ON a.host_id=s.host_id AND a.service_id=s.service_id LEFT join hosts h ON s.host_id=h.host_id WHERE s.description='{service_desc}' AND h.name='{hostname}' AND entry_time >= {entry_time} ORDER BY entry_time DESC")
                result = cursor.fetchall()
                if len(result) > 0 and result[0]['state'] is not None and int(result[0]['state']) == int(status) and result[0]['deletion_time'] is None:
                    logger.console(
                        f"status={result[0]['state']} and state_type={result[0]['type']}")
                    if state_type == 'HARD' and int(result[0]['type']) == 1:
                        return int(result[0]['acknowledgement_id'])
                    elif state_type != 'HARD':
                        return int(result[0]['acknowledgement_id'])
        time.sleep(1)
    return 0


def ctn_check_acknowledgement_is_deleted_with_timeout(ack_id: int, timeout: int, which='COMMENTS'):
    """
    Check if an acknowledgement is deleted in comments, acknowledgements or both

    Args:
        ack_id (int): The acknowledgement id
        timeout (int): The timeout in seconds
        which (str): The table to check. It can be 'comments', 'acknowledgements' or 'BOTH'
    """
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     autocommit=True,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    f"SELECT c.deletion_time, a.entry_time, a.deletion_time FROM comments c LEFT JOIN acknowledgements a ON c.host_id=a.host_id AND c.service_id=a.service_id AND c.entry_time=a.entry_time WHERE c.entry_type=4 AND a.acknowledgement_id={ack_id}")
                result = cursor.fetchall()
                if len(result) > 0 and result[0]['deletion_time'] is not None and int(result[0]['deletion_time']) >= int(result[0]['entry_time']):
                    if which == 'BOTH':
                        if result[0]['a.deletion_time']:
                            return True
                        logger.console(
                            f"Acknowledgement {ack_id} is only deleted in comments")
                    else:
                        logger.console(
                            f"Acknowledgement {ack_id} is deleted at {result[0]['deletion_time']}")
                        return True
        time.sleep(1)
    return False


def ctn_check_service_status_with_timeout(hostname: str, service_desc: str, status: int, timeout: int, state_type: str = "SOFT"):
    """
    ctn_check_service_status_with_timeout
    Check the status of a service within a specified timeout period.

    Args:
        hostname (str): The name of the host.
        service_desc (str): The description of the service.
        status (int): The expected status to check for.
        timeout (int): The timeout period in seconds.
        state_type (str, optional): The type of state to check for. Defaults to "SOFT".
                                    Can be "SOFT" or "HARD".
    Returns:
        bool: True if the expected status is found within the timeout period, False otherwise.
    """
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     autocommit=True,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    f"SELECT s.state, s.state_type FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description=\"{service_desc}\" AND h.name=\"{hostname}\" AND s.enabled=1 AND h.enabled=1")
                result = cursor.fetchall()
                logger.console(f"{result}")
                if len(result) > 0 and result[0]['state'] is not None and int(result[0]['state']) == int(status):
                    if state_type == 'HARD' and int(result[0]['state_type']) == 1:
                        return True
                    elif state_type != 'HARD' and int(result[0]['state_type']) == 0:
                        return True
        time.sleep(1)
    return False


def ctn_check_service_status_with_timeout_rt(hostname: str, service_desc: str, status: int, timeout: int, state_type: str = "SOFT"):
    """
    ctn_check_service_status_with_timeout_rt

    same as ctn_check_service_status_with_timeout but returns a pair boolean, output

    Args:
        host_name:
        service_desc:
        status: expected service state
        state_type: HARD, SOFT or ANY
        timeout: time to wait expected check in seconds
    """

    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     autocommit=True,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    f"SELECT s.state, s.state_type, s.output FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description=\"{service_desc}\" AND h.name=\"{hostname}\"")
                result = cursor.fetchall()
                if len(result) > 0 and result[0]['state'] is not None and int(result[0]['state']) == int(status):
                    logger.console(
                        f"status={result[0]['state']} and state_type={result[0]['state_type']} and output={result[0]['output']}")
                    if state_type == 'HARD' and int(result[0]['state_type']) == 1:
                        return True, result[0]['output']
                    elif state_type != 'SOFT' and int(result[0]['state_type']) == 0:
                        return True, result[0]['output']
                    else:
                        return True, result[0]['output']
        time.sleep(1)
    return False, ""


def ctn_check_service_status_enabled(hostname: str, service_desc: str, timeout: int):
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     autocommit=True,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    f"SELECT s.service_id, s.enabled FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description=\"{service_desc}\" AND h.name=\"{hostname}\"")
                result = cursor.fetchall()
                if len(result) > 0 and result[0]['enabled'] is not None:
                    logger.console(
                        f"enabled {service_desc}is enabled")
                else:
                    logger.console(
                        f"enabled {service_desc}is disabled")
                return False
        time.sleep(1)
    return False


def ctn_check_severity_with_timeout(name: str, level, icon_id, timeout: int):
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    "SELECT level, icon_id FROM severities WHERE name='{}'".format(name))
                result = cursor.fetchall()
                if len(result) > 0:
                    if int(result[0]['level']) == int(level) and int(result[0]['icon_id']) == int(icon_id):
                        return True
        time.sleep(1)
    return False


def ctn_check_tag_with_timeout(name: str, typ, timeout: int):
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    "SELECT type FROM tags WHERE name='{}'".format(name))
                result = cursor.fetchall()
                if len(result) > 0:
                    if int(result[0]['type']) == int(typ):
                        return True
        time.sleep(1)
    return False


def ctn_check_severities_count(value: int, timeout: int):
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute("SELECT count(*) FROM severities")
                result = cursor.fetchall()
                if len(result) > 0:
                    if int(result[0]['count(*)']) == int(value):
                        return True
        time.sleep(1)
    return False


def ctn_check_tags_count(value: int, timeout: int):
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute("SELECT count(*) FROM tags")
                result = cursor.fetchall()
                if len(result) > 0:
                    if int(result[0]['count(*)']) == int(value):
                        return True
        time.sleep(1)
    return False


def ctn_check_ba_status_with_timeout(ba_name: str, status: int, timeout: int = TIMEOUT):
    """ check in the database if the BA has the expected status.

    Args:
        ba_name: The name of the BA
        status: The expected status
        timeout: The timeout in seconds

    Returns:
        True if the status is the expected one, False otherwise.
    """
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_CONF,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)
        with connection:
            with connection.cursor() as cursor:
                logger.console(
                    f"SELECT current_status from mod_bam WHERE name='{ba_name}'")
                cursor.execute(
                    f"SELECT current_status FROM mod_bam WHERE name='{ba_name}'")
                result = cursor.fetchall()
                logger.console(f"ba: {result[0]}")
                if len(result) > 0 and result[0]['current_status'] is not None and int(result[0]['current_status']) == int(status):
                    return True
        time.sleep(1)
    return False


def ctn_check_ba_output_with_timeout(ba_name: str, expected_output: str, timeout: int):
    """ check if the expected is written in mod_bam.comment column
    @param ba_name   name of the ba
    @param expected_output  output that we should find in comment column
    @param timeout  timeout in second
    """
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_CONF,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)
        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    f"SELECT * FROM mod_bam WHERE name='{ba_name}'")
                result = cursor.fetchall()
                logger.console(f"ba: {result[0]}")
                if result[0]['current_status'] is not None and result[0]['comment'] == expected_output:
                    return True
        time.sleep(5)
    return False


def ctn_check_downtimes_with_timeout(nb: int, timeout: int):
    """ check if the expected number of downtimes is present in the database.

    Args:
        nb: Expected number of downtimes
        timeout: timeout in seconds
    """
    limit = time.time() + timeout
    result = []
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    "SELECT count(*) FROM downtimes WHERE deletion_time IS NULL")
                result = cursor.fetchall()
                logger.console(f"result: {result}")
                if len(result) > 0 and result[0]['count(*)'] is not None:
                    if result[0]['count(*)'] == int(nb):
                        return True
                    else:
                        logger.console(
                            f"We should have {nb} downtimes but we have {result[0]['count(*)']}")
                        cursor.execute(
                            "SELECT * FROM downtimes WHERE deletion_time IS NULL")
                        result = cursor.fetchall()
        time.sleep(2)
    logger.console(f"Wrong result: {result}")
    return False


# def check_host_downtime_with_timeout(hostname: str, enabled, timeout: int):
#    limit = time.time() + timeout
#    while time.time() < limit:
#        connection = pymysql.connect(host=DB_HOST,
#                                     user=DB_USER,
#                                     password=DB_PASS,
#                                     database=DB_NAME_STORAGE,
#                                     charset='utf8mb4',
#                                     cursorclass=pymysql.cursors.DictCursor)
#
#        with connection:
#            with connection.cursor() as cursor:
#                if enabled != '0':
#                    cursor.execute(
#                        f"SELECT h.scheduled_downtime_depth FROM downtimes d INNER JOIN hosts h ON d.host_id=h.host_id AND d.service_id=0 WHERE d.deletion_time is null AND h.name='{hostname}'")
#                    result = cursor.fetchall()
#                    if len(result) == int(enabled) and not result[0]['scheduled_downtime_depth'] is None and result[0]['scheduled_downtime_depth'] == int(enabled):
#                        return True
#                    if (len(result) > 0):
#                        logger.console(
#                            f"{len(result)} downtimes for host {hostname} scheduled_downtime_depth={result[0]['scheduled_downtime_depth']}")
#                    else:
#                        logger.console(
#                            f"{len(result)} downtimes for host {hostname} scheduled_downtime_depth=None")
#                else:
#                    cursor.execute(
#                        f"SELECT h.scheduled_downtime_depth, d.deletion_time, d.downtime_id FROM hosts h LEFT JOIN downtimes d ON h.host_id = d.host_id AND d.service_id=0 WHERE h.name='{hostname}'")
#                    result = cursor.fetchall()
#                    if len(result) > 0 and not result[0]['scheduled_downtime_depth'] is None and result[0]['scheduled_downtime_depth'] == 0 and (result[0]['downtime_id'] is None or not result[0]['deletion_time'] is None):
#                        return True
#        time.sleep(1)
#    return False


def ctn_check_service_downtime_with_timeout(hostname: str, service_desc: str, enabled: int, timeout: int):
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                first = True
                if enabled != 0:
                    if first:
                        logger.console(
                            f"SELECT s.scheduled_downtime_depth FROM downtimes d INNER JOIN hosts h ON d.host_id=h.host_id INNER JOIN services s ON d.service_id=s.service_id WHERE d.deletion_time is null AND s.description='{service_desc}' AND h.name='{hostname}'")
                        first = False
                    cursor.execute(
                        f"SELECT s.scheduled_downtime_depth FROM downtimes d INNER JOIN hosts h ON d.host_id=h.host_id INNER JOIN services s ON d.service_id=s.service_id WHERE d.deletion_time is null AND s.description='{service_desc}' AND h.name='{hostname}'")
                    result = cursor.fetchall()
                    if len(result) > 0:
                        logger.console(
                            f"scheduled_downtime_depth: {result[0]['scheduled_downtime_depth']}")
                    if len(result) == enabled and result[0]['scheduled_downtime_depth'] is not None and result[0]['scheduled_downtime_depth'] == enabled:
                        return True
                    if (len(result) > 0):
                        logger.console("{} downtimes for serv {} scheduled_downtime_depth={}".format(
                            len(result), service_desc, result[0]['scheduled_downtime_depth']))
                    else:
                        logger.console("{} downtimes for serv {} scheduled_downtime_depth=None".format(
                            len(result), service_desc))
                else:
                    cursor.execute("SELECT s.scheduled_downtime_depth, d.deletion_time, d.downtime_id FROM services s INNER JOIN hosts h on s.host_id = h.host_id LEFT JOIN downtimes d ON s.host_id = d.host_id AND s.service_id = d.service_id WHERE s.description='{}' AND h.name='{}'".format(
                        service_desc, hostname))
                    result = cursor.fetchall()
                    if len(result) > 0 and result[0]['scheduled_downtime_depth'] is not None and result[0]['scheduled_downtime_depth'] == 0 and (result[0]['downtime_id'] is None or result[0]['deletion_time'] is not None):
                        return True
        time.sleep(2)
    return False


def ctn_check_service_check_with_timeout(hostname: str, service_desc: str,  timeout: int, command_line: str):
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     autocommit=True,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    f"SELECT s.command_line FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description=\"{service_desc}\" AND h.name=\"{hostname}\"")
                result = cursor.fetchall()
                if len(result) > 0:
                    logger.console(
                        f"command_line={result[0]['command_line']}")
                    if result[0]['command_line'] is not None and result[0]['command_line'] == command_line:
                        return True
        time.sleep(1)
    return False


def ctn_check_service_check_status_with_timeout(hostname: str, service_desc: str,  timeout: int,  min_last_check: int, state: int, output: str):
    """
    check_service_check_status_with_timeout

    check if service checks infos have been updated

    Args:
        host_name:
        service_desc:
        timeout: time to wait expected check in seconds
        min_last_check: time point after last_check will be accepted
        state: expected service state
        output: expected output
    """
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     autocommit=True,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    f"SELECT s.last_check, s.state, s.output FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description=\"{service_desc}\" AND h.name=\"{hostname}\"")
                result = cursor.fetchall()
                if len(result) > 0:
                    logger.console(
                        f"last_check={result[0]['last_check']} state={result[0]['state']} output={result[0]['output']} ")
                    if result[0]['last_check'] is not None and result[0]['last_check'] >= min_last_check and output in result[0]['output'] and result[0]['state'] == state:
                        return True
        time.sleep(1)
    return False


def ctn_check_service_output_resource_status_with_timeout(hostname: str, service_desc: str, timeout: int, min_last_check: int, status: int, status_type: str,  output: str):
    """
    ctn_check_service_output_resource_status_with_timeout

    check if resource checks infos of an host have been updated

    Args:
        hostname:
        service_desc:
        timeout: time to wait expected check in seconds
        min_last_check: time point after last_check will be accepted
        status: expected host state
        status_type: HARD or SOFT
        output: expected output
    """

    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     autocommit=True,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    f"SELECT r.status, r.status_confirmed, r.output FROM resources r LEFT JOIN services s ON r.id=s.service_id AND r.parent_id=s.host_id JOIN hosts h ON s.host_id=h.host_id WHERE h.name='{hostname}' AND s.description='{service_desc}' AND r.last_check >= {min_last_check}")
                result = cursor.fetchall()
                if len(result) > 0:
                    logger.console(f"result: {result}")
                if len(result) > 0 and result[0]['status'] is not None and int(result[0]['status']) == int(status):
                    logger.console(
                        f"status={result[0]['status']} and status_confirmed={result[0]['status_confirmed']} and output=\"{result[0]['output']}\"")
                    if status_type == 'HARD' and int(result[0]['status_confirmed']) == 1 and output in result[0]['output']:
                        return True
                    elif status_type == 'SOFT' and int(result[0]['status_confirmed']) == 0 and output in result[0]['output']:
                        return True
        time.sleep(1)
    return False


def ctn_check_host_check_with_timeout(hostname: str, start: int, timeout: int):
    """
    ctn_check_host_check_with_timeout

    Checks that the last_check is after the start timestamp.

    Args:
        hostname: the host concerned by the check
        start: a timestamp that should be approximatively the date of the check.
        timeout: A timeout.

    Returns: True on success.
    """
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     autocommit=True,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    f"SELECT last_check FROM resources WHERE name='{hostname}'")
                result = cursor.fetchall()
                if len(result) > 0 and len(result[0]) > 0 and result[0]['last_check'] is not None:
                    logger.console(
                        f"last_check={result[0]['last_check']} ")
                    last_check = int(result[0]['last_check'])
                    if last_check > start:
                        return True
        time.sleep(1)
    return False


def ctn_check_host_check_status_with_timeout(hostname: str, timeout: int, min_last_check: int, state: int, output: str):
    """
    ctn_check_host_check_status_with_timeout

    check if host checks infos have been updated

    Args:
        hostname:
        timeout: time to wait expected check in seconds
        min_last_check: time point after last_check will be accepted
        state: expected host state
        output: expected output
    """
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     autocommit=True,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    f"SELECT last_check, state, output FROM hosts WHERE name='{hostname}'")
                result = cursor.fetchall()
                if len(result) > 0:
                    logger.console(
                        f"last_check={result[0]['last_check']} state={result[0]['state']} output={result[0]['output']} ")
                    if result[0]['last_check'] is not None and result[0]['last_check'] >= min_last_check and output in result[0]['output'] and result[0]['state'] == state:
                        return True
                    else:
                        logger.console(
                            f"last_check: {result[0]['last_check']} - min_last_check: {min_last_check} - expected output: {output} - output: {result[0]['output']} - expected state: {state} - state: {result[0]['state']}")
        time.sleep(1)
    return False


def ctn_check_host_output_resource_status_with_timeout(hostname: str, timeout: int, min_last_check: int, status: int, status_type: str,  output: str):
    """
    ctn_check_host_output_resource_status_with_timeout

    check if resource checks infos of an host have been updated

    Args:
        hostname:
        timeout: time to wait expected check in seconds
        min_last_check: time point after last_check will be accepted
        status: expected host state
        status_type: HARD or SOFT
        output: expected output
    """

    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     autocommit=True,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    f"SELECT r.status, r.status_confirmed, r.output FROM resources r JOIN hosts h ON r.id=h.host_id WHERE h.name='{hostname}' AND r.parent_id=0 AND r.last_check >= {min_last_check}")
                result = cursor.fetchall()
                if len(result) > 0:
                    logger.console(f"result: {result}")
                if len(result) > 0 and result[0]['status'] is not None and int(result[0]['status']) == int(status):
                    logger.console(
                        f"status={result[0]['status']} and status_confirmed={result[0]['status_confirmed']}")
                    if status_type == 'HARD' and int(result[0]['status_confirmed']) == 1 and output in result[0]['output']:
                        return True
                    elif status_type == 'SOFT' and int(result[0]['status_confirmed']) == 0 and output in result[0]['output']:
                        return True
        time.sleep(1)
    return False


def ctn_show_downtimes():
    connection = pymysql.connect(host=DB_HOST,
                                 user=DB_USER,
                                 password=DB_PASS,
                                 database=DB_NAME_STORAGE,
                                 charset='utf8mb4',
                                 cursorclass=pymysql.cursors.DictCursor)

    with connection:
        with connection.cursor() as cursor:
            cursor.execute(
                "select * FROM downtimes WHERE deletion_time is null")
            result = cursor.fetchall()

    for r in result:
        logger.console(f" >> {r}")


def ctn_delete_service_downtime(hst: str, svc: str):
    now = int(time.time())
    did = 0
    while time.time() < now + TIMEOUT:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    f"select d.internal_id from downtimes d inner join hosts h on d.host_id=h.host_id inner join services s on d.service_id=s.service_id where d.deletion_time is null and s.scheduled_downtime_depth<>'0' and s.description='{svc}' and h.name='{hst}' LIMIT 1")
                result = cursor.fetchall()
                if len(result) > 0:
                    did = int(result[0]['internal_id'])
                    break
        time.sleep(1)

    if did != 0:
        logger.console(f"delete downtime internal_id={did}")
        with open(f"{VAR_ROOT}/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
            f.write(f"[{now}] DEL_SVC_DOWNTIME;{did}\n")


def ctn_number_of_downtimes_is(nb: int, timeout: int = TIMEOUT):
    """
    Check if the number of downtimes is the expected one.
    Args:
        nb: The expected number of downtimes
        timeout: The timeout in seconds

    Returns:
        True if the number of downtimes is the expected one, False otherwise.
    """
    limit = time.time() + timeout
    retval = False
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    "SELECT count(*) FROM downtimes d INNER JOIN hosts h ON d.host_id=h.host_id INNER JOIN services s ON d.service_id=s.service_id WHERE d.deletion_time is null AND s.enabled='1' AND s.scheduled_downtime_depth>0")
                result = cursor.fetchall()
                logger.console(f"count(*) = {result[0]['count(*)']}")
                if int(result[0]['count(*)']) == int(nb):
                    retval = True
                    break
        time.sleep(1)

    connection = pymysql.connect(host=DB_HOST,
                                 user=DB_USER,
                                 password=DB_PASS,
                                 database=DB_NAME_STORAGE,
                                 charset='utf8mb4',
                                 cursorclass=pymysql.cursors.DictCursor)

    with connection:
        with connection.cursor() as cursor:
            cursor.execute(
                "SELECT * FROM downtimes d INNER JOIN hosts h ON d.host_id=h.host_id INNER JOIN services s ON d.service_id=s.service_id WHERE d.deletion_time is null AND s.enabled='1' AND s.scheduled_downtime_depth>0")
            result = cursor.fetchall()
            if retval:
                logger.console("We get the expected number of downtimes")
            else:
                logger.console("Not the expected number of downtimes")
            logger.console(json.dumps(result, indent=4, sort_keys=True))
    return retval


def ctn_clear_db(table: str):
    connection = pymysql.connect(host=DB_HOST,
                                 user=DB_USER,
                                 password=DB_PASS,
                                 database=DB_NAME_STORAGE,
                                 charset='utf8mb4',
                                 cursorclass=pymysql.cursors.DictCursor)

    with connection:
        with connection.cursor() as cursor:
            cursor.execute("DELETE FROM {}".format(table))
        connection.commit()


def ctn_clear_db_conf(table: str):
    connection = pymysql.connect(host=DB_HOST,
                                 user=DB_USER,
                                 password=DB_PASS,
                                 database=DB_NAME_CONF,
                                 charset='utf8mb4',
                                 cursorclass=pymysql.cursors.DictCursor)

    with connection:
        with connection.cursor() as cursor:
            cursor.execute(f"DELETE FROM {table}")
        connection.commit()


def ctn_check_service_severity_with_timeout(host_id: int, service_id: int, severity_id, timeout: int):
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute("select sv.id from resources r left join severities sv ON r.severity_id=sv.severity_id where r.parent_id = {} and r.id={}".format(
                    host_id, service_id))
                result = cursor.fetchall()
                if len(result) > 0:
                    if severity_id == 'None':
                        if result[0]['id'] is None:
                            return True
                    elif result[0]['id'] is not None and int(result[0]['id']) == int(severity_id):
                        return True
        time.sleep(1)
    return False


def ctn_check_host_severity_with_timeout(host_id: int, severity_id, timeout: int = TIMEOUT):
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     autocommit=True,
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    "select sv.id from resources r left join severities sv ON r.severity_id=sv.severity_id where r.parent_id = 0 and r.id={}".format(host_id))
                result = cursor.fetchall()
                if len(result) > 0:
                    logger.console(result)
                    if severity_id == 'None':
                        if result[0]['id'] is None:
                            return True
                    elif result[0]['id'] is not None and int(result[0]['id']) == int(severity_id):
                        return True
        time.sleep(1)
    return False


def ctn_check_resources_tags_with_timeout(parent_id: int, mid: int, typ: str, tag_ids: list, timeout: int, enabled: bool = True):
    """
    check if the tags of a resource are the same as the expected ones
    Args:
        parent_id: id of the parent of the resource 0 for hosts
        mid: id of the resource
        typ: type of the resource (servicegroup, hostgroup or servicecategory)
        tag_ids: expected tags ids
        timeout: timeout in seconds
        enabled: if True, check if the tags are enabled, otherwise check if they are different
        Returns: 
           - enabled = True: True if the tags resource (parent_id, mid) is attached to all tags in tag_ids
           - enabled = False: True if resource (parent_id, mid) is attached to none tag of tag_ids

    """
    if typ == 'servicegroup':
        t = 0
    elif typ == 'hostgroup':
        t = 1
    elif typ == 'servicecategory':
        t = 2
    else:
        t = 3
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                logger.console(
                    f"select t.id from resources r inner join resources_tags rt on r.resource_id=rt.resource_id inner join tags t on rt.tag_id=t.tag_id WHERE r.id={mid} and r.parent_id={parent_id} and t.type={t} and r.enabled=1")
                cursor.execute(
                    f"select t.id from resources r inner join resources_tags rt on r.resource_id=rt.resource_id inner join tags t on rt.tag_id=t.tag_id WHERE r.id={mid} and r.parent_id={parent_id} and t.type={t} and r.enabled=1")
                result = cursor.fetchall()
                logger.console(result)
                if not enabled:
                    if len(result) == 0:
                        return True
                    else:
                        found_in_tags_ids = False
                        for r in result:
                            if r['id'] in tag_ids:
                                logger.console(
                                    "id {} is in tag ids".format(r['id']))
                                found_in_tags_ids = True
                                break
                        if not found_in_tags_ids:
                            return True
                elif len(result) > 0:
                    if len(result) == len(tag_ids):
                        equals = True
                        for r in result:
                            if r['id'] not in tag_ids:
                                logger.console(
                                    "id {} is not in tag ids".format(r['id']))
                                equals = False
                                break
                        if equals:
                            return True
                    else:
                        logger.console(
                            f"Result and tag_ids should have the same size, moreover 'id' in result should be values of tag_ids, result size = {len(result)} and tag_ids size = {len(tag_ids)} - their content are result: {result} and tag_ids: {tag_ids}")
                else:
                    logger.console("result")
                    logger.console(result)
        time.sleep(1)
    return False


def ctn_check_host_tags_with_timeout(host_id: int, tag_id: int, timeout: int):
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    "SELECT t.id FROM resources_tags rt, tags t WHERE rt.tag_id = t.tag_id and resource_id={} and t.id={}".format(host_id, tag_id))
                result = cursor.fetchall()
                if len(result) > 0:
                    if int(result[0]['id']) == tag_id:
                        return True
        time.sleep(1)
    return False


def ctn_check_number_of_resources_monitored_by_poller_is(poller: int, value: int, timeout: int):
    """
    Check the number of resources monitored by a poller.

    Args:
        poller: The poller id
        value: The expected number of resources
        timeout: The timeout in seconds

    Returns:
        True if the number of resources monitored by the poller is equal to the expected value,
        False otherwise.
    """
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    f"SELECT count(*) FROM resources WHERE poller_id={poller} AND enabled=1")
                result = cursor.fetchall()
                logger.console(
                    f"SELECT count(*) FROM resources WHERE poller_id={poller} AND enabled=1 => {result[0]} <-> {value}")
                if len(result) > 0:
                    if int(result[0]['count(*)']) == value:
                        return True
        time.sleep(1)
    return False


def ctn_check_number_of_downtimes(expected: int, start, timeout: int):
    limit = time.time() + timeout
    try:
        d = parser.parse(start)
    except (parser.ParserError, TypeError):
        d = datetime.fromtimestamp(start)
    d = d.timestamp()
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)
        with connection:
            with connection.cursor() as cursor:
                logger.console(
                    f"SELECT count(*) FROM downtimes WHERE start_time >= {d} AND deletion_time IS NULL")
                cursor.execute(
                    f"SELECT count(*) FROM downtimes WHERE start_time >= {d} AND deletion_time IS NULL")
                result = cursor.fetchall()
                if len(result) > 0:
                    logger.console(
                        "{}/{} active downtimes".format(result[0]['count(*)'], expected))
                    if int(result[0]['count(*)']) == expected:
                        return True
        time.sleep(1)
    return False


def ctn_check_number_of_relations_between_hostgroup_and_hosts(hostgroup: int, value: int, timeout: int):
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    "SELECT count(*) FROM hosts_hostgroups WHERE hostgroup_id={}".format(hostgroup))
                result = cursor.fetchall()
                if len(result) > 0:
                    logger.console(
                        f"SELECT count(*) FROM hosts_hostgroups WHERE hostgroup_id={hostgroup} => {result[0]}")
                    if int(result[0]['count(*)']) == value:
                        return True
        time.sleep(1)
    return False


def ctn_check_number_of_relations_between_servicegroup_and_services(servicegroup: int, value: int, timeout: int, service_group_name: str = None):
    limit = time.time() + timeout
    request = f"SELECT count(*) from servicegroups s join services_servicegroups sg on s.servicegroup_id = sg.servicegroup_id  WHERE s.servicegroup_id={servicegroup}"

    if service_group_name is not None:
        request += f" AND name='{service_group_name}'"

    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(request)
                result = cursor.fetchall()
                if len(result) > 0:
                    if int(result[0]['count(*)']) == value:
                        return True
        time.sleep(1)
    return False


def ctn_check_field_db_value(request: str, value, timeout: int):
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4')

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(request)
                result = cursor.fetchall()
                if len(result) > 0:
                    if result[0][0] == value:
                        return True
                    else:
                        logger.console(
                            f"result[0][0]={result[0][0]} expected={value}")
        time.sleep(1)
    return False


def ctn_check_host_status(host: str, value: int, typ: int, in_resources: bool, timeout: int = TIMEOUT):
    """
    Check the status of a host in the database

    Args:
        host: the name of the host
        value: the expected status value
        typ: 0 for SOFT, 1 for HARD
        in_resources: True to check in resources table, False to check in hosts table
        timeout: timeout in seconds (default: TIMEOUT)
    Returns:
        True if the host has the expected status, False otherwise
    """
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                key = ''
                confirmed = ''
                if in_resources:
                    cursor.execute(
                        f"SELECT status, status_confirmed FROM resources WHERE parent_id=0 AND name='{host}'")
                    key = 'status'
                    confirmed = 'status_confirmed'
                else:
                    cursor.execute(
                        f"SELECT state, state_type FROM hosts WHERE name='{host}'")
                    key = 'state'
                    confirmed = 'state_type'
                result = cursor.fetchall()
                logger.console(f"{result}")
                if len(result) > 0:
                    if int(result[0][key]) == value and int(result[0][confirmed]) == typ:
                        return True
        time.sleep(1)
    return False


def ctn_find_internal_id(date, exists=True, timeout: int = TIMEOUT):
    my_date = datetime.timestamp(parser.parse(date))
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     autocommit=True,
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                logger.console(
                    "select internal_id from comments where entry_time >= {} and deletion_time is null".format(my_date))
                cursor.execute(
                    "select internal_id from comments where entry_time >= {} and deletion_time is null".format(my_date))
                result = cursor.fetchall()
                if len(result) > 0 and exists:
                    return result[0]['internal_id']
                elif len(result) == 0:
                    logger.console("Query to find the internal_id failed")
                    if not exists:
                        return True
        time.sleep(1)
    return False


def ctn_create_bad_queue(filename: str):
    with open(f"{VAR_ROOT}/lib/centreon-broker/{filename}", 'wb') as f:
        buffer = bytearray(10000)
        buffer[0] = 0
        buffer[1] = 0
        buffer[2] = 0
        buffer[3] = 0
        buffer[4] = 0
        buffer[5] = 0
        buffer[6] = 8
        buffer[7] = 0
        t = 0
        for i in range(8, 10000):
            buffer[i] = t
            t += 1
            if t > 100:
                t = 0
        f.write(buffer)


def ctn_grep(file_path: str, pattern: str):
    with open(file_path, "r") as file:
        for line in file:
            if re.search(pattern, line):
                return line.strip()
    return ""


def ctn_check_types_in_resources(lst: list):
    connection = pymysql.connect(host=DB_HOST,
                                 user=DB_USER,
                                 password=DB_PASS,
                                 database=DB_NAME_STORAGE,
                                 charset='utf8mb4',
                                 autocommit=True,
                                 cursorclass=pymysql.cursors.DictCursor)

    with connection:
        with connection.cursor() as cursor:
            logger.console("select distinct type from resources")
            cursor.execute("select distinct type from resources")
            result = cursor.fetchall()
            if len(result) > 0:
                for t in lst:
                    found = False
                    for r in result:
                        v = r['type']
                        if int(v) == int(t):
                            found = True
                            break
                    if not found:
                        logger.console(
                            f"Value {t} not found in result of query 'select distinct type from resources'")
                        return False
                return True
    return False


def ctn_get_collect_version():
    with open("../CMakeLists.txt", "r") as f:
        lines = f.readlines()

    filtered = filter(lambda line: line.startswith("set(COLLECT_"), lines)

    rmaj = re.compile(r"set\(COLLECT_MAJOR\s*([0-9]+)")
    rmin = re.compile(r"set\(COLLECT_MINOR\s*([0-9]+)")
    rpatch = re.compile(r"set\(COLLECT_PATCH\s*([0-9]+)")
    for line in filtered:
        m1 = rmaj.match(line)
        m2 = rmin.match(line)
        m3 = rpatch.match(line)
        if m1:
            maj = m1.group(1)
        if m2:
            mini = m2.group(1)
        if m3:
            patch = m3.group(1)
    return f"{maj}.{mini}.{patch}"


def ctn_wait_until_file_modified(path: str, date: str, timeout: int = TIMEOUT):
    """! wait until file is modified
    @param path  path of the file
    @param date  minimal of modified time
    @param path  timeout timeout in seconds
    @return True if file has been modified since date
    """
    try:
        my_date = parser.parse(date).timestamp()
    except (parser.ParserError, TypeError):
        my_date = datetime.fromtimestamp(date).timestamp()
    limit = time.time() + timeout
    while time.time() < limit:
        try:
            stat_result = os.stat(path)
            if stat_result.st_mtime > my_date:
                return True
            time.sleep(5)
        except OSError:
            time.sleep(5)

    logger.console(f"{path} not modified since {date}")
    return False


def ctn_set_user_id_from_name(user_name: str):
    """! modify user id
    @param user_name  user name as centreon-engine
    """
    user_id = getpwnam(user_name).pw_uid
    os.setuid(user_id)


def ctn_get_uid():
    return os.getuid()


def ctn_set_uid(user_id: int):
    os.setuid(user_id)


def ctn_has_file_permissions(path: str, permission: int):
    """! test if file has permission passed in parameter
    it does a AND with permission parameter
    @param path path of the file
    @permission mask to test file permission
    @return True if the file has the requested permissions
    """
    stat_res = os.stat(path)
    if stat_res is None:
        logger.console(f"fail to get permission of {path}")
        return False
    masked = stat_res.st_mode & permission
    return masked == permission


def ctn_compare_dot_files(file1: str, file2: str):
    """
    Compare two dot files file1 and file2 after removing the pointer addresses
    that clearly are not the same.

    Args:
        file1: The first file to compare.
        file2: The second file to compare.

    Returns: True if they have the same content, False otherwise.
    """

    with open(file1, "r") as f1:
        content1 = f1.readlines()
    with open(file2, "r") as f2:
        content2 = f2.readlines()
    r = re.compile(r"(.*) 0x[0-9a-f]+")

    def replace_ptr(line):
        m = r.match(line)
        if m:
            return m.group(1)
        else:
            return line

    content1 = list(map(replace_ptr, content1))
    content2 = list(map(replace_ptr, content2))

    if len(content1) != len(content2):
        return False
    for i in range(len(content1)):
        if content1[i] != content2[i]:
            logger.console(
                f"Files are different at line {i + 1}: first => << {content1[i].strip()} >> and second => << {content2[i].strip()} >>")
            return False
    return True


def ctn_create_bbdo_grpc_server(port: int, ):
    """
    Start a BBDO streaming grpc server.
    It answers nothing and simulates proxy behavior when cbd is down
    Args:
        port: port to listen
    Returns: grpc server
    """

    class service_implementation(grpc_stream_pb2_grpc.centreon_bbdoServicer):
        """
        bbdo grpc service that does nothing
        """

        def exchange(self, request_iterator, context):
            time.sleep(0.01)
            for request in request_iterator:
                logger.console(request)
            context.abort(grpc.StatusCode.UNAVAILABLE, "unavailable")

    private_key = open('/tmp/server_1234.key', 'rb').read()
    certificate_chain = open('/tmp/server_1234.crt', 'rb').read()
    ca_cert = open('/tmp/ca_1234.crt', 'rb').read()

    server = grpc.server(futures.ThreadPoolExecutor(max_workers=2))
    grpc_stream_pb2_grpc.add_centreon_bbdoServicer_to_server(
        service_implementation(), server)
    creds = grpc.ssl_server_credentials([(private_key, certificate_chain)],
                                        root_certificates=ca_cert)

    server.add_secure_port("0.0.0.0:5669", creds)
    server.start()
    return server


def create_random_string(length: int):
    """
    create_random_string

    create a string with random char
    Args:
        length output string length
    Returns: a string
    """
    letters = string.ascii_lowercase
    return ''.join(random.choice(letters) for i in range(length))


def ctn_create_random_dictionary(nb_entries: int):
    """
    create_random_dictionary

    create a dictionary with random keys and random string values

    Args:
        nb_entries  dictionary size
    Returns: a dictionary
    """
    dict_ret = {}
    for ii in range(nb_entries):
        dict_ret[create_random_string(10)] = create_random_string(10)

    return dict_ret


def ctn_protobuf_to_json(protobuf_obj):
    """
    protobuf_to_json

    Convert a protobuf object to json
    it replaces uppercase letters in keys by _<lower>
    """
    converted = MessageToJson(protobuf_obj)
    return json.loads(converted)


def ctn_compare_string_with_file(string_to_compare: str, file_path: str):
    """
    ctn_compare_string_with_file

    compare a multiline string with a file content
    Args:
        string_to_compare: multiline string to compare
        file_path: path of the file to compare

    Returns: True if contents are identical
    """
    str_lines = string_to_compare.splitlines(keepends=True)
    with open(file_path) as f:
        file_lines = f.readlines()
    if len(str_lines) != len(file_lines):
        return False
    for str_line, file_line in zip(str_lines, file_lines):
        if str_line != file_line:
            return False
    return True


def ctn_check_service_perfdata(host: str, serv: str, timeout: int, precision: float, expected: dict):
    """
    Check if performance data are near as expected.
        host (str): The hostname of the service to check.
        serv (str): The service name to check.
        timeout (int): The timeout value for the check.
        precision (float): The precision required for the performance data comparison.
        expected (dict): A dictionary containing the expected performance data values.
    """
    limit = time.time() + timeout
    query = f"""SELECT sub_query.metric_name, db.value FROM data_bin db JOIN
            (SELECT m.metric_name, MAX(db.ctime) AS last_data, db.id_metric FROM data_bin db
                JOIN metrics m ON db.id_metric = m.metric_id
                JOIN index_data id ON id.id = m.index_id
                WHERE id.host_name='{host}' AND id.service_description='{serv}'
                GROUP BY m.metric_id) sub_query 
            ON db.ctime = sub_query.last_data AND db.id_metric = sub_query.id_metric"""
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)
        with connection:
            with connection.cursor() as cursor:
                cursor.execute(query)
                result = cursor.fetchall()
                if len(result) >= len(expected):
                    for res in result:
                        logger.console(
                            f"metric: {res['metric_name']}, value: {res['value']}")
                        metric = res['metric_name']
                        value = float(res['value'])
                        # as windows agent is not restarted, he can send metrics from previous tests once engine is restarted, so we ignore them
                        if metric not in expected:
                            continue
                        if expected[metric] is not None and abs(value - expected[metric]) > precision:
                            logger.console(
                                f"ERROR unexpected value for {metric}, expected: {expected[metric]}, found: {value}")
                            return False
                    return True
        time.sleep(1)
    logger.console(f"unexpected result: {result}")
    return False


def ctn_check_service_check_interval(host: str, serv: str, timeout: int, expected_interval: int, precision: float):
    """
    Check if performance data of a metric are spaced of expected_interval.

    Args:
        host (str): The hostname of the service to check.
        serv (str): The service name to check.
        timeout (int): The timeout value for the check.
        expected_interval (int): The expected interval between two performance data points.
        precision (float): The precision required for the interval comparison.

    Returns:
        True if the intervals are as expected, False otherwise.
    """

    # we work on last metric in order to not take into account metrics of previous tests
    limit = time.time() + timeout
    query = f"""SELECT  db.ctime, db.id_metric FROM data_bin db JOIN
            (SELECT MAX(db.id_metric) AS id_metric FROM data_bin db
                JOIN metrics m ON db.id_metric = m.metric_id
                JOIN index_data id ON id.id = m.index_id
                WHERE id.host_name='{host}' AND id.service_description='{serv}') sub_query 
            ON db.id_metric = sub_query.id_metric ORDER BY db.ctime"""
    result = ()
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)
        with connection:
            with connection.cursor() as cursor:
                cursor.execute(query)
                result = cursor.fetchall()
                # we don't take first check into account as it may be generated by previous test
                if (len(result) >= 3):
                    for i in range(len(result) - 2):
                        time_diff = result[i + 2]['ctime'] - result[i + 1]['ctime']
                        logger.console(
                            f"serv:{serv}, metric: {result[i + 1]['id_metric']}, ctime:{result[i + 1]['ctime']}, time_diff: {time_diff}")
                        if abs(time_diff - expected_interval) > precision:
                            logger.console(
                                f"ERROR unexpected interval, expected: {expected_interval}, found: {time_diff}")
                            return False
                    return True
        time.sleep(1)
    logger.console(f"unexpected result: {result}")
    return False


def ctn_check_agent_information(total_nb_agent: int, nb_poller: int, timeout: int):
    """
    Check if agent_information table is filled. Collect version is also checked
        total_nb_agent (int): total number of agents
        nb_poller (int): nb poller with at least one agent connected.
        timeout (int): The timeout value for the check.
    """
    collect_version = ctn_get_collect_version()

    collect_major = int(collect_version.split(".")[0])
    collect_minor = int(collect_version.split(".")[1])
    collect_patch = int(collect_version.split(".")[2])

    limit = time.time() + timeout
    query = "SELECT infos FROM agent_information WHERE enabled = 1"
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)
        with connection:
            with connection.cursor() as cursor:
                cursor.execute(query)
                result = cursor.fetchall()
                if len(result) == nb_poller:
                    nb_agent = 0
                    for res in result:
                        logger.console(f"infos: {res['infos']}")
                        agent_infos = json.loads(res['infos'])
                        for by_agent_info in agent_infos:
                            if by_agent_info['agent_major'] != collect_major or by_agent_info['agent_minor'] != collect_minor or by_agent_info['agent_patch'] != collect_patch:
                                logger.console(
                                    f"unexpected version: {by_agent_info['agent_major']}.{by_agent_info['agent_minor']}.{by_agent_info['agent_patch']}")
                                return False
                            nb_agent += by_agent_info['nb_agent']
                    if nb_agent == total_nb_agent:
                        return True
        time.sleep(1)
    logger.console(f"unexpected result: {result}")
    return False


def ctn_get_nb_process(exe: str):
    """
    ctn_get_nb_process

    get the number of process with a specific executable
    Args:
        exe: executable to search
    Returns: number of process
    """

    counter = 0

    for p in psutil.process_iter():
        if exe in p.name() or exe in ' '.join(p.cmdline()):
            counter += 1
    return counter


def ctn_check_service_flapping(host: str, serv: str, timeout: int, precision: float, expected: int):
    """
    Check if performance data are near as expected.
        host (str): The hostname of the service to check.
        serv (str): The service name to check.
        timeout (int): The timeout value for the check.
        precision (float): The precision required for the performance data comparison.
        expected (int): expected flapping value.
    """
    limit = time.time() + timeout

    s_query = f"""SELECT s.flapping, s.percent_state_change FROM services s JOIN hosts h on s.host_id = h.host_id  WHERE h.name='{host}' AND description='{serv}'"""
    r_query = f"""SELECT flapping, percent_state_change FROM resources WHERE parent_name='{host}' AND name='{serv}'"""

    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)
        with connection:
            with connection.cursor() as cursor:
                cursor.execute(s_query)
                result = cursor.fetchall()
                if len(result) == 1 and result[0]['flapping'] == 1 and abs(result[0]['percent_state_change'] - expected) < precision:
                    cursor.execute(r_query)
                    result = cursor.fetchall()
                    if len(result) == 1 and result[0]['flapping'] == 1 and abs(result[0]['percent_state_change'] - expected) < precision:
                        return True
        time.sleep(1)
    logger.console(f"unexpected result: {result}")
    return False


def ctn_check_host_flapping(host: str, timeout: int, precision: float, expected: int):
    """
    Check if performance data are near as expected.
        host (str): The hostname of the service to check.
        timeout (int): The timeout value for the check.
        precision (float): The precision required for the performance data comparison.
        expected (int): expected flapping value.
    """
    limit = time.time() + timeout

    s_query = f"""SELECT flapping, percent_state_change FROM hosts WHERE name='{host}'"""
    r_query = f"""SELECT flapping, percent_state_change FROM resources WHERE name='{host}' AND parent_id=0"""

    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)
        with connection:
            with connection.cursor() as cursor:
                cursor.execute(s_query)
                result = cursor.fetchall()
                if len(result) == 1 and result[0]['flapping'] == 1 and abs(result[0]['percent_state_change'] - expected) < precision:
                    cursor.execute(r_query)
                    result = cursor.fetchall()
                    if len(result) == 1 and result[0]['flapping'] == 1 and abs(result[0]['percent_state_change'] - expected) < precision:
                        return True
        time.sleep(1)
    logger.console(f"unexpected result: {result}")
    return False


def ctn_get_process_limit(pid: int, limit: str):
    """
    ctn_get_process_limit

    Get a limit of a process
    Args:
        pid: process id
        limit: limit to get

    Returns: limit value
    """
    try:
        with open(f"/proc/{pid}/limits") as f:
            for line in f:
                if limit in line:
                    fields = line.split()
                    return int(fields[len(fields) - 3]), int(fields[len(fields) - 2])
    except (OSError, ValueError):
        return -1, -1
    return -1, -1


def ctn_create_jwt_token(exp_s: int, secret: str = "centreon"):
    """
    ctn_create_jwt_token

    create a jwt token
    Returns: jwt token
    """
    value = random.randint(0, 100000)
    payload = {
        "iss": f"centreon{value}",
        "iat": int(datetime.now().timestamp()),
        "exp": int((datetime.now() + timedelta(seconds=exp_s)).timestamp())
    }
    logger.console(payload)
    return jwt.encode(payload, secret, algorithm="HS256")


def ctn_check_state_configurations_are_equal(file1, file2):
    """
    ctn_check_state_configurations_are_equal

    Compare two dump protobuf state configurations files
    Args:
        file1: first file to compare
        file2: second file to compare

    Returns: True if they are equal
    """

    def compare_dicts(d1, d2):
        """
        Compare two dictionaries recursively, considering lists equal if
        they contain the same elements in any order.

        Returns:
            True if the dictionaries are equal, False otherwise.
        """

        if isinstance(d1, dict) and isinstance(d2, dict):
            if d1.keys() != d2.keys():
                return False
            return all(compare_dicts(d1[k], d2[k]) for k in d1)

        elif isinstance(d1, list) and isinstance(d2, list):
            if len(d1) != len(d2):
                return False
            try:
                from collections import Counter
                return Counter(d1) == Counter(d2)
            except TypeError:
                d2_copy = d2[:]
                for item in d1:
                    for i, candidate in enumerate(d2_copy):
                        if compare_dicts(item, candidate):
                            del d2_copy[i]
                            break
                    else:
                        return False
                return not d2_copy

        else:
            return d1 == d2

    with open(file1, "rb") as f1:
        content1 = f1.read()
    with open(file2, "rb") as f2:
        content2 = f2.read()

    pb1 = state_pb2.State()
    pb2 = state_pb2.State()

    pb1.ParseFromString(content1)
    pb2.ParseFromString(content2)

    dico1 = MessageToDict(pb1)
    dico2 = MessageToDict(pb2)

    return compare_dicts(dico1, dico2)


def ctn_notify_broker_of_engine_config_change(idx: int):
    """
    Notify the broker of a change in the engine configuration.

    Args:
        idx (int): The index of the configuration to notify.
    """
    logger.console(
        f"Notify broker of engine config change for poller {idx + 1}")
    lck_file = f"{VAR_ROOT}/lib/centreon/config/{idx + 1}.lck"
    Path(lck_file).touch()


def ctn_hosts_are_identical(poller_id: int, filename: str):
    """ Check if all hosts of a poller have the same configuration as in the configuration file.

    Args:
        poller_id (int): The poller ID to check.
        filename (str): The path to the configuration file.

    Returns: True if all hosts are identical to the configuration file, False otherwise.
    """

    comparator = ConfComparator(poller_id, filename, "hosts")
    return comparator.compare()


def ctn_host_resources_are_identical(poller_id: int, filename: str):
    """ Check if all hosts of a poller (in the resources table) have the same configuration as in the configuration file.

    Args:
        poller_id (int): The poller ID to check.
        filename (str): The path to the configuration file.

    Returns: True if all hosts are identical to the configuration file, False otherwise.
    """

    comparator = ConfComparator(poller_id, filename, "resources",
                                query=f"SELECT name, alias, address, id FROM resources WHERE parent_id=0 AND enabled=1 AND poller_id={poller_id}", obj_name="host")
    return comparator.compare()


def ctn_services_are_identical(poller_id: int, filename: str):
    """ Check if all services of a poller have the same configuration as in the configuration file.

    Args:
        poller_id (int): The poller ID to check.
        filename (str): The path to the configuration file.

    Returns: True if all services are identical to the configuration file, False otherwise.
    """

    comparator = ConfComparator(poller_id, filename, "services",
                                query=f"SELECT s.description, s.check_command, s.check_period, s.max_check_attempts, s.check_interval, s.retry_interval, s.notification_interval, s.notification_period, s.first_notification_delay, s.flap_detection, s.low_flap_threshold, s.high_flap_threshold, s.volatile, s.notes_url, s.action_url, s.icon_image, s.icon_image_alt, h.name AS host_name,s.service_id, h.host_id FROM services s JOIN hosts h ON s.host_id = h.host_id WHERE s.enabled=1 AND h.instance_id={poller_id}")
    return comparator.compare()


def ctn_service_resources_are_identical(poller_id: int, filename: str):
    """ Check if all services (in the resources table) of a poller have the same configuration as in the configuration file.

    Args:
        poller_id (int): The poller ID to check.
        filename (str): The path to the configuration file.

    Returns: True if all services are identical to the configuration file, False otherwise.
    """

    comparator = ConfComparator(poller_id, filename, table="resources",
                                query=f"SELECT name, max_check_attempts, notes_url, action_url, parent_name,id, parent_id, passive_checks_enabled, active_checks_enabled FROM resources WHERE enabled=1 AND poller_id={poller_id} AND parent_id>0", obj_name="service")
    return comparator.compare()


def ctn_severities_are_identical(poller_id: int, filename: str, timeout: int = TIMEOUT):
    """ Check if all severities (in the severities table) of a poller have the same configuration as in the configuration file.

    Args:
        poller_id: The poller ID to check.
        filename: The path to the configuration file.
        timeout: The timeout value for the check (in seconds, default is TIMEOUT).

    Returns: True if all severities are identical to the configuration file, False otherwise.
    """

    comparator = ConfComparator(poller_id, filename, table="severities",
                                query="SELECT id, type, name, level, icon_id FROM severities", check_length=False)
    return comparator.compare(timeout)


def ctn_tags_are_identical(poller_id: int, filename: str, timeout: int = TIMEOUT):
    """ Check if all tags (in the tags table) of a poller have the same configuration as in the configuration file.

    Args:
        poller_id: The poller ID to check.
        filename: The path to the configuration file.
        timeout: The timeout value for the check (in seconds, default is TIMEOUT).

    Returns: True if all tags are identical to the configuration file, False otherwise.
    """

    comparator = ConfComparator(poller_id, filename, table="tags",
                                query="SELECT id, type, name FROM tags", check_length=False)
    return comparator.compare(timeout)


def ctn_check_severity_ids(logfile: str, start):
    """
    Check if severity ids are correct in the log file compared to the database.
    The start date is given to check in the log from that date.
    We don't check all the rows in the severities table since we just want to
    check the last insertion/modification of severities given by the logs.

    Args:
        logfile: The path to the log file.
        start: The start time to check (as a datetime object or a Unix timestamp).

    Returns: True if severity ids are correct, False otherwise.
    """
    with open(logfile, "r") as f:
        lines = f.readlines()

    idx = ctn_find_line_from(lines, start)

    r = re.compile(
        r".*Severity with id (\d+) and type (\d+) has severity_id (\d+)")
    from_logs = {}
    for line in lines[idx:]:
        m = r.match(line)
        if m:
            id = int(m.group(1))
            type = int(m.group(2))
            severity_id = int(m.group(3))
            from_logs[(id, type)] = severity_id

    connection = pymysql.connect(host=DB_HOST,
                                 user=DB_USER,
                                 password=DB_PASS,
                                 autocommit=True,
                                 database=DB_NAME_STORAGE,
                                 charset='utf8mb4',
                                 cursorclass=pymysql.cursors.DictCursor)

    logger.console(f"Severities found in logs: {from_logs}")
    with connection:
        with connection.cursor() as cursor:
            cursor.execute("SELECT id, type, severity_id FROM severities")
            result = cursor.fetchall()

    retval = True
    for r in result:
        id = int(r['id'])
        type = int(r['type'])
        severity_id = int(r['severity_id'])
        if (id, type) not in from_logs:
            logger.console(f"Severity {(id, type)} not found in logs")
            continue

        if from_logs[(id, type)] != severity_id:
            logger.console(
                f"Severity {(id, type)} has severity_id {from_logs[(id, type)]} in logs and {severity_id} in database")
            retval = False
    return retval


def ctn_check_tag_ids(logfile: str, start):
    """
    Check if tag ids are correct in the log file compared to the database.
    The start date is given to check in the log from that date.
    We don't check all the rows in the tags table since we just want to
    check the last insertion/modification of tags given by the logs.

    Args:
        logfile: The path to the log file.
        start: The start time to check (as a datetime object or a Unix timestamp).

    Returns: True if tag ids are correct, False otherwise.
    """
    with open(logfile, "r") as f:
        lines = f.readlines()

    idx = ctn_find_line_from(lines, start)

    r = re.compile(
        r".*Tag with id (\d+) and type (\d+) has tag_id (\d+)")
    from_logs = {}
    for line in lines[idx:]:
        m = r.match(line)
        if m:
            id = int(m.group(1))
            type = int(m.group(2))
            tag_id = int(m.group(3))
            from_logs[(id, type)] = tag_id

    connection = pymysql.connect(host=DB_HOST,
                                 user=DB_USER,
                                 password=DB_PASS,
                                 autocommit=True,
                                 database=DB_NAME_STORAGE,
                                 charset='utf8mb4',
                                 cursorclass=pymysql.cursors.DictCursor)

    logger.console(f"Tags found in logs: {from_logs}")
    with connection:
        with connection.cursor() as cursor:
            cursor.execute("SELECT id, type, tag_id FROM tags")
            result = cursor.fetchall()

    retval = True
    for r in result:
        id = int(r['id'])
        type = int(r['type'])
        tag_id = int(r['tag_id'])
        if (id, type) not in from_logs:
            logger.console(f"Tag {(id, type)} not found in logs")
            continue

        if from_logs[(id, type)] != tag_id:
            logger.console(
                f"Tag {(id, type)} has tag_id {from_logs[(id, type)]} in logs and {tag_id} in database")
            retval = False
    return retval


def ctn_clear_prot_files(broker_only: bool = False):
    """
    Remove .prot files in /tmp directory.
    If broker_only is True, only remove broker prot files (i.e. files named
    <ID>.prot), leaving Engine's state.prot files untouched.
    """
    for file in Path('/tmp').rglob('*.prot'):
        if broker_only and file.name == 'state.prot':
            continue
        file.unlink()

def ctn_check_resource_ids(typ: str, logfile: str):
    """
    Check if resource ids are correct in the log file compared to the database.
    We check all the rows in the resources table.

    Args:
        typ: The type of resource to check (e.g., "host", "service", "AD").
        logfile: The path to the log file.

    Returns: True if resource ids are correct, False otherwise.
    """
    with open(logfile, "r") as f:
        lines = f.readlines()

    types = {
            "host": [1, "Host", "host"],
            "service": [0, "Service", "service"],
            "AD": [4, "Anomaly detection", "service"],
            }

    r_add = re.compile(rf".*{types[typ][1]} resource with id (\d+):(\d+) has resource_id (\d+)")
    r_del = re.compile(rf".*Disabling {types[typ][2]} resource with id (\d+):(\d+)")
    from_logs = {}
    for line in lines:
        m = r_add.match(line)
        if m:
            parent_id = int(m.group(1))
            id = int(m.group(2))
            resource_id = int(m.group(3))
            logger.console(f"New resource from logs {(parent_id, id)}, resource_id={resource_id}")
            from_logs[(parent_id, id)] = resource_id
        else:
            m = r_del.match(line)
            if m:
                parent_id = int(m.group(1))
                id = int(m.group(2))
                if (parent_id, id) in from_logs:
                    logger.console(f"Resource {(parent_id, id)} deleted, removing it from list")
                    del from_logs[(parent_id, id)]

    connection = pymysql.connect(host=DB_HOST,
                                 user=DB_USER,
                                 password=DB_PASS,
                                 autocommit=True,
                                 database=DB_NAME_STORAGE,
                                 charset='utf8mb4',
                                 cursorclass=pymysql.cursors.DictCursor)
    logger.console(f"Resources found in logs: {from_logs}")

    with connection:
        with connection.cursor() as cursor:
            cursor.execute(f"SELECT resource_id, id, parent_id FROM resources WHERE enabled=1 AND type={types[typ][0]}")
            result = cursor.fetchall()

    retval = True
    for r in result:
        logger.console(f"result: {r}")
        id = int(r['id'])
        parent_id = int(r['parent_id'])
        resource_id = int(r['resource_id'])
        if (parent_id, id) not in from_logs:
            logger.console(f"Resource {(parent_id, id)} not found in logs")
            continue

        if from_logs[(parent_id, id)] != resource_id:
            logger.console(
                f"Resource {(id, type)} has resource_id {from_logs[(id, type)]} in logs and {resource_id} in database")
            retval = False
    if (len(from_logs) != len(result)):
        logger.console(f"Number of resources in logs ({len(from_logs)}) is different from number of resources in database ({len(result)})")
        logger.console(f"Resources in logs: {from_logs}")
        logger.console(f"Resources in DB: {result}")
        retval = False
    return retval

