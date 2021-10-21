from robot.api import logger
from subprocess import getoutput
import re
import time

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

#logger.console(check_connection(5669, 16088, 16219))
