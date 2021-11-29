from robot.api import logger
from subprocess import getoutput
import re
import time
from dateutil import parser
from datetime import datetime


def compare_md5(file_e: str, file_b: str):

    getoutput("awk '{{print $8}}' {0} > {0}.md5".format(file_e))
    getoutput("awk '{{print $8}}' {0} > {0}.md5".format(file_b))

    f1 = open("{}.md5".format(file_e))
    content1 = f1.readlines()

    f2 = open("{}.md5".format(file_b))
    content2 = f2.readlines()

    idx1 = 0
    idx2 = 0

    while idx1 < len(content1) and idx2 < len(content2):
        if content1[idx1] == "test1.lua\n":
            idx1 += 1

        if content2[idx2] == "test.lua\n":
            idx2 += 1
            if content2[idx2] == "055b1a6348a16305474b60de439a0efd\n":
                idx2 += 1
            else:
                return False

        if content1[idx1] == content2[idx2]:
            idx1 += 1
            idx2 += 1
        else:
            return False
    if idx1 == len(content1) and idx2 == len(content2):
        return True
    return False

def check_multiplicity(file1: str, file2: str):
    getoutput("sort {0}.md5 | uniq -c > /tmp/sort-e.md5".format(file1))
    getoutput("sort {0}.md5 | uniq -c > /tmp/sort-b.md5".format(file2))
    f1 = open("/tmp/sort-e.md5")
    content1 = f1.readlines()
    f2 = open("/tmp/sort-b.md5")
    content2 = f2.readlines()

    r = re.compile(r"^\s*([0-9]+)\s+.*$")
    ff1 = []
    ff2 = []
    for l in content1:
        m = r.match(l)
        if m and m.group(1) != '1':
            ff1.append(l)
    for l in content2:
        m = r.match(l)
        if m and m.group(1) != '1':
            ff2.append(l)

    for l in ff1:
        logger.console(l)
    logger.console("#####")
    for l in ff2:
        logger.console(l)

#check_multiplicity("/home/david/mnt/tmp/lua-engine.log", "/home/david/mnt/tmp/lua.log")
