from robot.api import logger
from subprocess import getoutput
import re
import time
from dateutil import parser
from datetime import datetime

TIMEOUT = 30

def check_connection(port: int, pid1: int, pid2: int):
  limit = time.time() + TIMEOUT
  while time.time() < limit:
    out = getoutput("ss -plant")
    lst = out.split('\n')
    estab = list(filter(lambda l: l.startswith("ESTAB"), lst))
    estab_port = list(filter(lambda l: "127.0.0.1:" + str(port) + " " in l, estab))
    if len(estab_port) > 0:
      break
    time.sleep(1)

  ok = [False, False]
  p = re.compile(r"127\.0\.0\.1:(\d+)\s+127\.0\.0\.1:(\d+)\s+.*,pid=(\d+)")
  for l in estab_port:
    m = p.search(l)
    if m:
      if int(m.group(1)) == port or int(m.group(2)) == port:
        if pid1 == int(m.group(3)):
          ok[0] = True
        if pid2 == int(m.group(3)):
          ok[1] = True
  return ok[0] and ok[1]

def get_date(d: str):
  try:
    ts = int(d)
    retval = datetime.fromtimestamp(int(ts))
  except ValueError:
    retval = parser.parse(d[:-6])
  return retval

def find_in_log(log: str, date, content):
  my_date = parser.parse(date)
  f = open(log, "r")
  lines = f.readlines()
  p = re.compile("\[([^\]]*)\]")

  # Let's find my_date
  start = 0
  end = len(lines) - 1
  while end - start > 1:
    idx = (start + end) // 2
    m = p.match(lines[idx])
    idx_d = get_date(m.group(1))
    if my_date <= idx_d:
      end = idx
    elif my_date > idx_d:
      start = idx

  for c in content:
    found = False
    for i in range(idx, len(lines)):
      if i == 4822:
        print("attention")
      line = lines[i]
      if c in line:
        found = True
        break
    if not found:
      logger.console("Unable to find from {} (line {}): {}".format(date, idx, c))
      return False

  return True


#now = "2021-10-22 16:36:59.519"
#print(find_in_log("/var/log/centreon-broker/central-broker-master.log", now, ["extension 'COMPRESSION' is set to 'yes' in the configuration but cannot be activated because of peer configuration'", "we have extensions 'COMPRESSION' and peer has ''"]))
#logger.console(check_connection(5669, 16088, 16219))
#print(find_in_log("/var/log/centreon-broker/central-broker-master.log", 18, ["mysql_connection", "sql stream instanciation"]))
#print(find_in_log("/var/log/centreon-engine/config0/centengine.log", 1634888409, ["initialized successfully", "No output returned from host check"]))
#print(find_in_log("/var/log/centreon-engine/config0/centengine.log", 1634888409, ["initialized successfully", "externalcmd.so"]))
#print(find_in_log("/var/log/centreon-broker/central-broker-master.log", 18, ["mysql_connection", "sql stream instanciation"]))
