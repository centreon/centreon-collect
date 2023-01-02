from robot.api import logger
from subprocess import getoutput
import re
import json
import time
from dateutil import parser
from datetime import datetime


##
# @brief Given two files with md5 as rows, this function checks the first list
# contains the same md5 as the second list. There are exceptions in these files
# that's why we need a function to make this test.
#
# @param str The first file name
# @param str The second file name
#
# @return A boolean True on success
def files_contain_same_json(file_e: str, file_b: str):
    new_inst = { "_type": 4294901762, "category": 65535, "element": 2, "broker_id":1, "broker_name":"", "enabled":True, "poller_id":1, "poller_name":"Central"}

    f1 = open(file_e)
    content1 = f1.readlines()

    f2 = open(file_b)
    content2 = f2.readlines()

    idx1 = 0
    idx2 = 0

    r = re.compile(r"^[^{]* (\{.*\})$")
    while idx1 < len(content1) and idx2 < len(content2):
        m1 = r.match(content1[idx1])
        if m1 is not None:
            c1 = m1.group(1)
        else:
            logger.console("content at line {} of '{}' is not JSON: {}".format(idx1, file_e, content1[idx1]))
            idx1 += 1
            continue
        m2 = r.match(content2[idx2])
        if m2 is not None:
            c2 = m2.group(1)
        else:
            logger.console("content at line {} of '{}' is not JSON: {}".format(idx2, file_b, content2[idx2]))
            idx2 += 1
            continue

        if c1 == c2:
            idx1 += 1
            idx2 += 1
        else:
            js1 = json.loads(c1)
            js2 = json.loads(c2)
            if js2['_type'] == 4294901762:
                idx2 += 1
                continue
            if js1['_type'] == 4294901762:
                idx1 += 1
                continue

            if len(js1) != len(js2):
                return False
            for k in js1:
                if isinstance(js1[k], float):
                    if abs(js1[k] - js2[k]) > 0.1:
                        logger.console(f"contents are different: <<{c1}>> vs <<{c2}>>")
                        return False
                else:
                    if js1[k] != js2[k]:
                        logger.console(f"contents are different: <<{c1}>> vs <<{c2}>>")
                        return False
            idx1 += 1
            idx2 += 1
    retval = idx1 == len(content1) or idx2 == len(content2)
    if not retval:
        logger.console("not at the end of files idx1 = {}/{} or idx2 = {}/{}".format(idx1, len(content1), idx2, len(content2)))
        return False
    return True

def files_contain_same_md5_1(file_e: str, file_b: str):

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
            print("We have to improve comparaison here")
            return False
    return idx1 == len(content1) and idx2 == len(content2)

def neb_type(t):
    dico = [
        [],
        [
            'neb:none', # = 0,
            'neb:acknowledgement = 1',
            'neb:comment',
            'neb:custom_variable',
            'neb:custom_variable_status',
            'neb:downtime',
            'neb:event_handler',
            'neb:flapping_status',
            'neb:host_check',
            'neb:host_dependency',
            'neb:host_group',
            'neb:host_group_member',
            'neb:host',
            'neb:host_parent',
            'neb:host_status',
            'neb:instance',
            'neb:instance_status',
            'neb:log_entry',
            'neb:module',
            'neb:service_check',
            'neb:service_dependency',
            'neb:service_group',
            'neb:service_group_member',
            'neb:service',
            'neb:service_status',
            'neb:instance_configuration',
            'neb:responsive_instance',
            'neb:pb_service',
            'neb:pb_adaptive_service',
            'neb:pb_service_status',
            'neb:pb_host',
            'neb:pb_adaptive_host',
            'neb:pb_host_status',
            'neb:pb_severity',
            'neb:pb_tag',
        ],
        [
            'bbdo:version_response',
            'bbdo:ack',
            'bbdo:stop',
            'bbdo:rebuild_graphs',
            'bbdo:remove_graphs',
        ],
        [
            'storage:none',
            'storage:metric',
            'storage:rebuild',
            'storage:remove_graph',
            'storage:status',
            'storage:index_mapping',
            'storage:metric_mapping',
            'storage:rebuild_message',
            'storage:remove_graph_message',
        ],
        [],
        [],
        [
            'bam:ba_status',
            'bam:kpi_status',
            'bam:meta_service_status',
            'bam:ba_event',
            'bam:kpi_event',
            'bam:ba_duration_event',
            'bam:dimension_ba_event',
            'bam:dimension_kpi_event',
            'bam:dimension_ba_bv_relation_event',
            'bam:dimension_bv_event',
            'bam:dimension_truncate_table_signal',
            'bam:rebuild',
            'bam:dimension_timeperiod',
            'bam:dimension_ba_timeperiod_relation',
            'bam:dimension_timeperiod_exception',
            'bam:dimension_timeperiod_exclusion',
            'bam:inherited_downtime',
        ],
    ]
    cat = ((t >> 16) & 0xffff)
    elem = t & 0xffff
    return dico[cat][elem]


##
# @brief Given two files generated by a stream connector, this function checks
# that no events except special ones are sent / replayed twice.
#
# @param str The first file name
# @param str The second file name
#
# @return A boolean True on success
def check_multiplicity_when_broker_restarted(file1: str, file2: str):
    f1 = open(file1)
    content1 = f1.readlines()
    f2 = open(file2)
    content2 = f2.readlines()

    r = re.compile(r".* INFO: ([0-9]+)\s+([0-9a-f]+)\s(.*)$")

    def create_md5_list(content):
        lst = dict()
        typ = dict()
        for l in content:
            m = r.match(l)
            if m:
                type, md5, js = int(m.group(1)), m.group(2), m.group(3)
                if type != 65544 and type != 4294901762 and type != 196613:
                    if md5 in lst:
                        lst[md5] += 1
                    else:
                        lst[md5] = 1
                        typ[md5] = type
        return lst, typ

    lst1, typ1 = create_md5_list(content1)
    lst2, typ2 = create_md5_list(content2)

    res1 = set(lst1.values())
    res2 = set(lst2.values())
    if len(res1) != 1 or len(res2) != 1:
        for k in lst1:
            if lst1[k] != 1:
                logger.console("In lst1: Bad {} {} with type {}".format(k, lst1[k], neb_type(typ1[k])))
        for k in lst2:
            if lst2[k] != 1:
                logger.console("In lst2: Bad {} {} with type {}".format(k, lst2[k], neb_type(typ2[k])))
    return len(res1) == 1 and len(res2) == 1

##
# @brief Given two files generated by a stream connector, this function checks
# that no events except special ones are sent / replayed twice.
#
# @param str The first file name
# @param str The second file name
#
# @return A boolean True on success
def check_multiplicity_when_engine_restarted(file1: str, file2: str):
    f1 = open(file1)
    content1 = f1.readlines()
    f2 = open(file2)
    content2 = f2.readlines()

    r = re.compile(r".* INFO: ([0-9]+)\s+([0-9a-f]+)\s(.*)$")

    def create_md5_list(content):
        lst = dict()
        typ = dict()
        for l in content:
            m = r.match(l)
            if m:
                type, md5, js = int(m.group(1)), m.group(2), m.group(3)
                """
                 Are removed:
                   * instance configurations
                   * modules
                   * host checks (they can be done several times
                """
                if type != 65544 and type != 4294901762 and type != 65554 and type != 65561 and type != 65548 and type != 65559 and type != 65551:
                    if md5 in lst:
                        lst[md5] += 1
                    else:
                        lst[md5] = 1
                        typ[md5] = type
        return lst, typ

    lst1, typ1 = create_md5_list(content1)
    lst2, typ2 = create_md5_list(content2)

    res1 = set(lst1.values())
    res2 = set(lst2.values())
    if len(res1) != 1 or len(res2) != 1:
        for k in lst1:
            if lst1[k] != 1:
                logger.console("In lst1: Bad {} {} with type {}".format(k, lst1[k], neb_type(typ1[k])))
        for k in lst2:
            if lst2[k] != 1:
                logger.console("In lst2: Bad {} {} with type {}".format(k, lst2[k], neb_type(typ2[k])))
    return len(res1) == 1 and len(res2) == 1

#print(files_contain_same_json("/tmp/lua-engine.log", "/tmp/lua.log"))
#print(check_multiplicity_when_engine_restarted("/tmp/lua-engine.log", "/tmp/lua.log"))
