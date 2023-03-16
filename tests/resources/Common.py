from robot.api import logger
from subprocess import getoutput, Popen, DEVNULL
import re
import os
import time
from dateutil import parser
from datetime import datetime
import pymysql.cursors
from robot.libraries.BuiltIn import BuiltIn


TIMEOUT = 30

BuiltIn().import_resource('db_variables.robot')
DB_NAME_STORAGE = BuiltIn().get_variable_value("${DBName}")
DB_NAME_CONF = BuiltIn().get_variable_value("${DBNameConf}")
DB_USER = BuiltIn().get_variable_value("${DBUser}")
DB_PASS = BuiltIn().get_variable_value("${DBPass}")
DB_HOST = BuiltIn().get_variable_value("${DBHost}")
DB_PORT = BuiltIn().get_variable_value("${DBPort}")
VAR_ROOT = BuiltIn().get_variable_value("${VarRoot}")


def check_connection(port: int, pid1: int, pid2: int):
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
            for l in estab_port:
                m = p.search(l)
                if m is not None:
                    if pid1 == int(m.group(3)):
                        ok[0] = True
                    if pid2 == int(m.group(3)):
                        ok[1] = True
                m = p_v6.search(l)
                if m is not None:
                    if pid1 == int(m.group(3)):
                        ok[0] = True
                    if pid2 == int(m.group(3)):
                        ok[1] = True
            if ok[0] and ok[1]:
                return True
        time.sleep(1)
    return False


def get_date(d: str):
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
        retval = parser.parse(d[:-6])
    return retval


def find_in_log_with_timeout(log: str, date, content, timeout: int):
    limit = time.time() + timeout
    c = ""
    while time.time() < limit:
        ok, c = find_in_log(log, date, content)
        if ok:
            return True
        time.sleep(5)
    logger.console(
        "Unable to find '{}' from {} during {}s".format(c, date, timeout))
    return False


def find_in_log(log: str, date, content):
    """Find content in log file from the given date

    Args:
        log (str): The log file
        date (_type_): A date as a string
        content (_type_): An array of strings we want to find in the log.

    Returns:
        boolean,str: The boolean is True on success, and the string contains the first string not found in logs otherwise.
    """
    try:
        f = open(log, "r")
        lines = f.readlines()
        f.close()
        idx = find_line_from(lines, date)

        for c in content:
            found = False
            for i in range(idx, len(lines)):
                line = lines[i]
                if c in line:
                    logger.console(
                        "\"{}\" found at line {} from {}".format(c, i, idx))
                    found = True
                    break
            if not found:
                return False, c

        return True, ""
    except IOError:
        logger.console("The file '{}' does not exist".format(log))
        return False, content[0]


def get_hostname():
    """Return the fqdn host name of the computer.

    Returns:
        str: the host name
    """
    retval = getoutput("hostname --fqdn")
    retval = retval.rstrip()
    return retval


def create_key_and_certificate(host: str, key: str, cert: str):
    if len(key) > 0:
        retval = getoutput(
            "openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 -keyout {} -out {} -subj '/CN={}'".format(key,
                                                                                                                cert,
                                                                                                                host))
    else:
        retval = getoutput(
            "openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 -out {} -subj '/CN={}'".format(key, cert, host))


def create_certificate(host: str, cert: str):
    create_key_and_certificate(host, "", cert)


def run_env():
    return getoutput("echo $RUN_ENV | awk '{print $1}'")


def start_mysql():
    if not run_env():
        logger.console("Starting Mariadb with systemd")
        getoutput("systemctl start mysql")
        logger.console("Mariadb started with systemd")
    else:
        logger.console("Starting Mariadb directly")
        Popen(["mariadbd", "--user=root"], stdout=DEVNULL, stderr=DEVNULL)
        logger.console("Mariadb directly started")


def stop_mysql():
    if not run_env():
        logger.console("Stopping Mariadb with systemd")
        getoutput("systemctl stop mysql")
        logger.console("Mariadb stopped with systemd")
    else:
        logger.console("Stopping directly MariaDB")
        getoutput("kill -SIGTERM $(pidof mariadbd)")
        logger.console("Mariadb directly stopped")


def stop_rrdcached():
    getoutput(
        "kill -9 $(ps ax | grep '.usr.bin.rrdcached' | grep -v grep | awk '{print $1}')")


def kill_broker():
    getoutput(
        "kill -SIGKILL $(ps ax | grep '/usr/sbin/cbwd' | grep -v grep | awk '{print $1}')")
    getoutput(
        "kill -SIGKILL $(ps ax | grep '/usr/sbin/cbd' | grep -v grep | awk '{print $1}')")


def kill_engine():
    getoutput(
        "kill -SIGKILL $(ps ax | grep '/usr/sbin/centengine' | grep -v grep | awk '{print $1}')")


def clear_retention():
    getoutput("find " + VAR_ROOT + " -name '*.cache.*' -delete")
    getoutput("find /tmp -name 'lua*' -delete")
    getoutput("find " + VAR_ROOT + " -name '*.memory.*' -delete")
    getoutput("find " + VAR_ROOT + " -name '*.queue.*' -delete")
    getoutput("find " + VAR_ROOT + " -name '*.unprocessed*' -delete")
    getoutput("find " + VAR_ROOT + " -name 'retention.dat' -delete")


def clear_cache():
    getoutput("find " + VAR_ROOT + " -name '*.cache.*' -delete")


def engine_log_table_duplicate(result: list):
    dup = True
    for i in result:
        if (i[0] % 2) != 0:
            dup = False
    return dup


def check_engine_logs_are_duplicated(log: str, date):
    try:
        f = open(log, "r")
        lines = f.readlines()
        f.close()
        idx = find_line_from(lines, date)
        count_true = 0
        count_false = 0
        logs_old = []
        logs_new = []
        old_log = re.compile(r"\[[^\]]*\] \[[^\]]*\] ([^\[].*)")
        new_log = re.compile(
            r"\[[^\]]*\] \[[^\]]*\] \[[^\]]*\] \[[^\]]*\] (.*)")
        for l in lines[idx:]:
            mo = old_log.match(l)
            mn = new_log.match(l)
            if mo is not None:
                if mo.group(1) in logs_new:
                    logs_new.remove(mo.group(1))
                else:
                    logs_old.append(mo.group(1))
            else:
                mn = new_log.match(l)
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
            for l in logs_old:
                logger.console(l)
            # We don't care about new logs not duplicated, in a future, we won't have any old logs
    except IOError:
        logger.console("The file '{}' does not exist".format(log))
        return False


def find_line_from(lines, date):
    my_date = parser.parse(date)
    p = re.compile(r"\[([^\]]*)\]")

    # Let's find my_date
    start = 0
    end = len(lines) - 1
    idx = start
    while end > start:
        idx = (start + end) // 2
        m = p.match(lines[idx])
        while m is None:
            logger.console("Unable to parse the date ({} <= {} <= {}): <<{}>>".format(
                start, idx, end, lines[idx]))
            idx -= 1
            if idx >= 0:
                m = p.match(lines[idx])
            else:
                logger.console("We are at the first line and no date found")

        idx_d = get_date(m.group(1))
        if my_date <= idx_d and end != idx:
            end = idx
        elif my_date > idx_d and start != idx:
            start = idx
        else:
            break
    return idx


def check_reschedule(log: str, date, content: str):
    try:
        f = open(log, "r")
        lines = f.readlines()
        f.close()
        idx = find_line_from(lines, date)

        retry_check = False
        normal_check = False
        for i in range(idx, len(lines)):
            line = lines[i]
            if content in line:
                logger.console(
                    "\"{}\" found at line {} from {}".format(content, i, idx))
                row = line.split()
                delta = int(datetime.strptime(row[19], "%Y-%m-%dT%H:%M:%S").timestamp()) - int(
                    datetime.strptime(row[14], "%Y-%m-%dT%H:%M:%S").timestamp())
                if delta == 60:
                    retry_check = True
                elif delta == 300:
                    normal_check = True
        return retry_check, normal_check
    except IOError:
        logger.console("The file '{}' does not exist".format(log))
        return False


def check_reschedule_with_timeout(log: str, date, content: str, timeout: int):
    limit = time.time() + timeout
    c = ""
    while time.time() < limit:
        v1, v2 = check_reschedule(log, date, content)
        if v1 and v2:
            return v1, v2
        time.sleep(5)
    return False


def clear_commands_status():
    if os.path.exists("/tmp/states"):
        os.remove("/tmp/states")


def set_command_status(cmd, status):
    if os.path.exists("/tmp/states"):
        f = open("/tmp/states")
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
    f = open("/tmp/states", "w")
    f.writelines(lines)
    f.close()


def check_service_status_with_timeout(hostname: str, service_desc: str, status: int, timeout: int, state_type: str = "SOFT"):
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
                    f"SELECT s.state, s.state_type FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description=\"{service_desc}\" AND h.name=\"{hostname}\"")
                result = cursor.fetchall()
                if len(result) > 0 and result[0]['state'] is not None and int(result[0]['state']) == int(status):
                    logger.console(
                        f"status={result[0]['state']} and state_type={result[0]['state_type']}")
                    if state_type == 'HARD' and int(result[0]['state_type']) == 1:
                        return True
                    elif state_type != 'HARD':
                        return True
        time.sleep(1)
    return False


def check_severity_with_timeout(name: str, level, icon_id, timeout: int):
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


def check_tag_with_timeout(name: str, typ, timeout: int):
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


def check_severities_count(value: int, timeout: int):
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


def check_tags_count(value: int, timeout: int):
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


def check_ba_status_with_timeout(ba_name: str, status: int, timeout: int):
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
                    "SELECT current_status FROM mod_bam WHERE name='{}'".format(ba_name))
                result = cursor.fetchall()
                if result[0]['current_status'] is not None and int(result[0]['current_status']) == int(status):
                    return True
        time.sleep(5)
    return False


def check_service_downtime_with_timeout(hostname: str, service_desc: str, enabled, timeout: int):
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
                cursor.execute("SELECT s.scheduled_downtime_depth from services s LEFT JOIN hosts h ON s.host_id=h.host_id wHERE s.description='{}' AND h.name='{}'".format(
                    service_desc, hostname))
                result = cursor.fetchall()
                if len(result) > 0 and not result[0]['scheduled_downtime_depth'] is None and result[0]['scheduled_downtime_depth'] == int(enabled):
                    return True
        time.sleep(5)
    return False


def delete_service_downtime(hst: str, svc: str):
    now = int(time.time())
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
            did = int(result[0]['internal_id'])

    cmd = f"[{now}] DEL_SVC_DOWNTIME;{did}"
    f = open(VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()


def number_of_downtimes_is(nb: int, timeout: int = TIMEOUT):
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
                    "SELECT count(*) from services WHERE enabled='1' AND scheduled_downtime_depth='1'")
                result = cursor.fetchall()
                logger.console(f"count(*) = {result[0]['count(*)']}")
                if int(result[0]['count(*)']) == int(nb):
                    return True
        time.sleep(1)
    return False


def clear_db(table: str):
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


def check_service_severity_with_timeout(host_id: int, service_id: int, severity_id, timeout: int):
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
                    elif not result[0]['id'] is None and int(result[0]['id']) == int(severity_id):
                        return True
        time.sleep(1)
    return False


def check_host_severity_with_timeout(host_id: int, severity_id, timeout: int = TIMEOUT):
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
                    if severity_id == 'None':
                        if result[0]['id'] is None:
                            return True
                    elif not result[0]['id'] is None and int(result[0]['id']) == int(severity_id):
                        return True
        time.sleep(1)
    return False


def check_resources_tags_with_timeout(parent_id: int, mid: int, typ: str, tag_ids: list, timeout: int, enabled: bool = True):
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
                logger.console("select t.id from resources r inner join resources_tags rt on r.resource_id=rt.resource_id inner join tags t on rt.tag_id=t.tag_id WHERE r.id={} and r.parent_id={} and t.type={}".format(
                    mid, parent_id, t))
                cursor.execute("select t.id from resources r inner join resources_tags rt on r.resource_id=rt.resource_id inner join tags t on rt.tag_id=t.tag_id WHERE r.id={} and r.parent_id={} and t.type={}".format(
                    mid, parent_id, t))
                result = cursor.fetchall()
                logger.console(result)
                if not enabled:
                    if len(result) == 0:
                        return True
                    else:
                        for r in result:
                            if r['id'] in tag_ids:
                                logger.console(
                                    "id {} is in tag ids".format(r['id']))
                                break
                        return True
                elif enabled and len(result) > 0:
                    if len(result) == len(tag_ids):
                        for r in result:
                            if r['id'] not in tag_ids:
                                logger.console(
                                    "id {} is not in tag ids".format(r['id']))
                                break
                        return True
                    else:
                        logger.console("different sizes: result:{} and tag_ids:{}".format(
                            len(result), len(tag_ids)))
                else:
                    logger.console("result")
                    logger.console(result)
        time.sleep(1)
    return False


def check_host_tags_with_timeout(host_id: int, tag_id: int, timeout: int):
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


def check_number_of_resources_monitored_by_poller_is(poller: int, value: int, timeout: int):
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
                    "SELECT count(*) FROM resources WHERE poller_id={} AND enabled=1".format(poller))
                result = cursor.fetchall()
                if len(result) > 0:
                    if int(result[0]['count(*)']) == value:
                        return True
        time.sleep(1)
    return False


def check_number_of_downtimes(expected: int, start, timeout: int):
    limit = time.time() + timeout
    d = parser.parse(start).timestamp()
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
                    "SELECT count(*) FROM downtimes WHERE start_time >= {} AND deletion_time IS NULL".format(d))
                result = cursor.fetchall()
                if len(result) > 0:
                    logger.console(
                        "{}/{} active downtimes".format(result[0]['count(*)'], expected))
                    if int(result[0]['count(*)']) == expected:
                        return True
        time.sleep(1)
    return False


def check_number_of_relations_between_hostgroup_and_hosts(hostgroup: int, value: int, timeout: int):
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
                    if int(result[0]['count(*)']) == value:
                        return True
        time.sleep(1)
    return False


def check_number_of_relations_between_servicegroup_and_services(servicegroup: int, value: int, timeout: int):
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
                    "SELECT count(*) FROM services_servicegroups WHERE servicegroup_id={}".format(servicegroup))
                result = cursor.fetchall()
                if len(result) > 0:
                    if int(result[0]['count(*)']) == value:
                        return True
        time.sleep(1)
    return False


def check_host_status(host: str, value: int, t: int, in_resources: bool, timeout: int = TIMEOUT):
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
                        "SELECT status, status_confirmed FROM resources WHERE parent_id=0 AND name='{}'".format(host))
                    key = 'status'
                    confirmed = 'status_confirmed'
                else:
                    cursor.execute(
                        "SELECT state, state_type FROM hosts WHERE name='{}'".format(host))
                    key = 'state'
                    confirmed = 'state_type'
                result = cursor.fetchall()
                if len(result) > 0:
                    if int(result[0][key]) == value and int(result[0][confirmed]) == t:
                        return True
                    else:
                        logger.console("Host '{}' has status '{}' with confirmed '{}'".format(
                            host, result[0][key], result[0][confirmed]))
        time.sleep(1)
    return False


def find_internal_id(date, exists=True, timeout: int = TIMEOUT):
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


def check_types_in_resources(lst: list):
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


def grep(file_path: str, pattern: str):
    with open(file_path, "r") as file:
        for line in file:
            if re.search(pattern, line):
                return line.strip()
    return ""
