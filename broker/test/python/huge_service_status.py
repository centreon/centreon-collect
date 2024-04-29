#!/usr/bin/python3


import pymysql
import time

host_serv=[]

with pymysql.connect(host='127.0.0.1',
                                 user='centreon',
                                 password='centreon',
                                 database='centreon_storage',
                                 charset='utf8mb4',
                                 cursorclass=pymysql.cursors.DictCursor) as db_connection:
    with db_connection.cursor() as cursor:
        sql = "select hosts.name, services.description FROM hosts join services on hosts.host_id = services.host_id WHERE hosts.enabled=1 and services.enabled=1"
        cursor.execute(sql)
        result = cursor.fetchall()
        for r in result:
            host_serv.append((r['name'], r['description']))


with open(f"/tmp/var/lib/centreon-engine/config0/rw/centengine.cmd", "w") as f:
    index = 0
    while True:
        now = int(time.time())
        if index > len(host_serv):
            index = 0
        (hst, svc) = host_serv[index]
        cmd = f"[{now}] SCHEDULE_FORCED_SVC_CHECK;{hst};{svc};{now}\n"
        #cmd = f"[{now}] PROCESS_SERVICE_CHECK_RESULT;{hst};{svc};0;output ok\n"
        f.write(cmd)

