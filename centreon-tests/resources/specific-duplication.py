from robot.api import logger
from subprocess import getoutput
import re
import time
from dateutil import parser
from datetime import datetime


def compare_md5(file_e: str, file_b: str):

    logger.console("file1 = " + file_e)
    logger.console("file2 = " + file_b)
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
    getoutput("sort {}.md5 | uniq -c > sort-e.md5".format(file1))
    getoutput("sort {}.md5 | uniq -c > sort-e.md5".format(file2))