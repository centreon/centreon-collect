from robot.api import logger
from subprocess import getoutput
import re
import os
import time
from dateutil import parser
from datetime import datetime
import pymysql.cursors

TIMEOUT = 30


def check_connection(port: int, pid1: int, pid2: int):
    limit = time.time() + TIMEOUT
    r = re.compile(r"^ESTAB.*127\.0\.0\.1:{}\s".format(port))
    while time.time() < limit:
        out = getoutput("ss -plant")
        lst = out.split('\n')
        estab_port = list(filter(r.match, lst))
        if len(estab_port) >= 2:
            ok = [False, False]
            p = re.compile(r"127\.0\.0\.1:(\d+)\s+127\.0\.0\.1:(\d+)\s+.*,pid=(\d+)")
            for l in estab_port:
                m = p.search(l)
                if m:
                    if pid1 == int(m.group(3)):
                        ok[0] = True
                    if pid2 == int(m.group(3)):
                        ok[1] = True
            if ok[0] and ok[1]:
                return True
        time.sleep(1)

    return False


def get_date(d: str):
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
    logger.console("Unable to find '{}' from {} during {}s".format(c, date, timeout))
    return False


def find_in_log(log: str, date, content):
    my_date = parser.parse(date)
    try:
        f = open(log, "r")
        lines = f.readlines()
        p = re.compile(r"\[([^\]]*)\]")

        # Let's find my_date
        start = 0
        end = len(lines) - 1
        idx = start
        while end - start > 1:
            idx = (start + end) // 2
            m = p.match(lines[idx])
            if not m:
                logger.console("Unable to parse the date ({} <= {} <= {}): <<{}>>".format(start, idx, end, lines[idx]))
            idx_d = get_date(m.group(1))
            if my_date <= idx_d:
                end = idx
            elif my_date > idx_d:
                start = idx

        for c in content:
            found = False
            for i in range(idx, len(lines)):
                line = lines[i]
                if c in line:
                    logger.console("\"{}\" found at line {} from {}".format(c, i, idx))
                    found = True
                    break
            if not found:
                return False, c

        return True, ""
    except IOError:
        logger.console("The file '{}' does not exist".format(log))
        return False, content[0]


def get_hostname():
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


def start_mysql():
    getoutput("systemctl start mysql")


def stop_mysql():
    getoutput("systemctl stop mysql")


def kill_broker():
    getoutput("kill -SIGKILL $(ps aux | grep '/usr/sbin/cbwd' | grep -v grep | awk '{print $2}')")
    getoutput("kill -SIGKILL $(ps aux | grep '/usr/sbin/cbd' | grep -v grep | awk '{print $2}')")


def kill_engine():
    getoutput("kill -SIGKILL $(ps aux | grep '/usr/sbin/centengine' | grep -v grep | awk '{print $2}')")


def clear_retention():
    getoutput("find /var -name '*.cache.*' -delete")
    getoutput("find /tmp -name 'lua*' -delete")
    getoutput("find /tmp -name 'central-*' -delete")
    getoutput("find /var -name '*.memory.*' -delete")
    getoutput("find /var -name '*.queue.*' -delete")
    getoutput("find /var -name '*.unprocessed*' -delete")


def clear_cache():
    getoutput("find /var -name '*.cache.*' -delete")

def engine_log_table_duplicate(result: list):
    dup = True
    for i in result:
        if (i[0] % 2) != 0:
            dup = False
    return dup

def engine_log_file_duplicate(log: str, date):
    my_date = parser.parse(date)
    try:
        f = open(log, "r")
        lines = f.readlines()
        count_true = 0
        count_false = 0
        q = re.compile(r"\[([^\]]*)\] \[([^\]]*)\] \[([^\]]*)\] \[([^\]]*)\]")
        for i in range(0, len(lines)):
            m = q.search(lines[i])
            if not m:
                count_false += 1
            else:
                count_true += 1
        if count_false != count_true:
            return False
        else:
            return True
    except IOError:
        logger.console("The file '{}' does not exist".format(log))
        return False

def check_reschedule(log: str, date, content: str):
    my_date = parser.parse(date)
    try:
        f = open(log, "r")
        lines = f.readlines()
        p = re.compile(r"\[([^\]]*)\]")

        # Let's find my_date
        start = 0
        end = len(lines) - 1
        idx = start
        while end - start > 1:
            idx = (start + end) // 2
            m = p.match(lines[idx])
            if not m or m is None:
                logger.console("Unable to parse the date ({} <= {} <= {}): <<{}>>".format(start, idx, end, lines[idx]))
            idx_d = get_date(m.group(1))
            if my_date <= idx_d:
                end = idx
            elif my_date > idx_d:
                start = idx
        retry_check = False
        normal_check = False
        for i in range(idx, len(lines)):
            line = lines[i]
            if content in line:
                logger.console("\"{}\" found at line {} from {}".format(content, i, idx))
                row = line.split()
                delta = int(datetime.strptime(row[19], "%Y-%m-%dT%H:%M:%S").timestamp()) - int(datetime.strptime(row[14], "%Y-%m-%dT%H:%M:%S").timestamp())
                if delta == 60:
                    retry_check = True
                elif delta == 300:
                    normal_check = True
        return retry_check , normal_check
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
        if m:
            if int(m.group(1)) != status:
                lines[i] = "{}=>{}\n".format(cmd, status)
            done = True
            break

    if not done:
        lines.append("{}=>{}\n".format(cmd, status))
    f = open("/tmp/states", "w")
    f.writelines(lines)
    f.close()


def check_service_status_with_timeout(hostname: str, service_desc: str, status: int, timeout: int):
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host='localhost',
                                 user='centreon',
                                 password='centreon',
                                 database='centreon_storage',
                                 charset='utf8mb4',
                                 cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute("SELECT s.state FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE s.description=\"{}\" AND h.name=\"{}\"".format(service_desc, hostname))
                result = cursor.fetchall()
                if result[0]['state'] and int(result[0]['state']) == status:
                    return True
        time.sleep(5)
    return False

def check_severity_with_timeout(name: str, level, icon_id, timeout: int):
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host='localhost',
                                 user='centreon',
                                 password='centreon',
                                 database='centreon_storage',
                                 charset='utf8mb4',
                                 cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute("SELECT level, icon_id FROM severities WHERE name='{}'".format(name))
                result = cursor.fetchall()
                if len(result) > 0:
                    if int(result[0]['level']) == int(level) and int(result[0]['icon_id']) == int(icon_id):
                        return True
        time.sleep(1)
    return False

def check_tag_with_timeout(name: str, typ, timeout: int):
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host='localhost',
                                 user='centreon',
                                 password='centreon',
                                 database='centreon_storage',
                                 charset='utf8mb4',
                                 cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute("SELECT type FROM tags WHERE name='{}'".format(name))
                result = cursor.fetchall()
                if len(result) > 0:
                    if int(result[0]['type']) == int(typ):
                        return True
        time.sleep(1)
    return False

def check_severities_count(value: int, timeout: int):
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host='localhost',
                                 user='centreon',
                                 password='centreon',
                                 database='centreon_storage',
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
        connection = pymysql.connect(host='localhost',
                                 user='centreon',
                                 password='centreon',
                                 database='centreon_storage',
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
        connection = pymysql.connect(host='localhost',
                                 user='centreon',
                                 password='centreon',
                                 database='centreon',
                                 charset='utf8mb4',
                                 cursorclass=pymysql.cursors.DictCursor)
        with connection:
            with connection.cursor() as cursor:
                cursor.execute("SELECT current_status FROM mod_bam WHERE name='{}'".format(ba_name))
                result = cursor.fetchall()
                if result[0]['current_status'] and int(result[0]['current_status']) == status:
                    return True
        time.sleep(5)
    return False

def check_service_downtime_with_timeout(hostname: str, service_desc: str, enabled, timeout: int):
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host='localhost',
                                 user='centreon',
                                 password='centreon',
                                 database='centreon_storage',
                                 charset='utf8mb4',
                                 cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute("SELECT s.scheduled_downtime_depth from services s LEFT JOIN hosts h ON s.host_id=h.host_id wHERE s.description='{}' AND h.name='{}'".format(service_desc, hostname))
                result = cursor.fetchall()
                if not result[0]['scheduled_downtime_depth'] is None and result[0]['scheduled_downtime_depth'] == int(enabled):
                    return True
        time.sleep(5)
    return False

def delete_service_downtime(hst: str, svc: str):
    now = int(time.time())
    connection = pymysql.connect(host='localhost',
                             user='centreon',
                             password='centreon',
                             database='centreon_storage',
                             charset='utf8mb4',
                             cursorclass=pymysql.cursors.DictCursor)

    with connection:
        with connection.cursor() as cursor:
            cursor.execute("select d.internal_id from downtimes d inner join hosts h on d.host_id=h.host_id inner join services s on d.service_id=s.service_id where d.cancelled='0' and s.scheduled_downtime_depth='1' and s.description='{}' and h.name='{}'".format(svc, hst))
            result = cursor.fetchall()
            did = int(result[0]['internal_id'])

    cmd = "[{}] DEL_SVC_DOWNTIME;{}".format(now, did)
    f = open("/var/lib/centreon-engine/config0/rw/centengine.cmd", "w")
    f.write(cmd)
    f.close()

def number_of_downtimes_is(nb: int):
    connection = pymysql.connect(host='localhost',
                             user='centreon',
                             password='centreon',
                             database='centreon_storage',
                             charset='utf8mb4',
                             cursorclass=pymysql.cursors.DictCursor)

    with connection:
        with connection.cursor() as cursor:
            cursor.execute("SELECT count(*) from services WHERE scheduled_downtime_depth='1'")
            result = cursor.fetchall()
            return int(result[0]['count(*)']) == int(nb)

def clear_db(table: str):
    connection = pymysql.connect(host='localhost',
                             user='centreon',
                             password='centreon',
                             database='centreon_storage',
                             charset='utf8mb4',
                             cursorclass=pymysql.cursors.DictCursor)

    with connection:
        with connection.cursor() as cursor:
            cursor.execute("DELETE FROM {}".format(table))
        connection.commit()

def check_service_severity_with_timeout(host_id: int, service_id: int, severity_id, timeout: int):
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host='localhost',
                                 user='centreon',
                                 password='centreon',
                                 database='centreon_storage',
                                 charset='utf8mb4',
                                 cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute("select sv.id from resources r left join severities sv ON r.severity_id=sv.severity_id where r.parent_id = {} and r.id={}".format(host_id, service_id))
                result = cursor.fetchall()
                logger.console(result)
                if len(result) > 0:
                    if severity_id == 'None':
                        if result[0]['id'] is None:
                            return True
                    elif not result[0]['id'] is None and int(result[0]['id']) == int(severity_id):
                        return True
        time.sleep(1)
    return False
