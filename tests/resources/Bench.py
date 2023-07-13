import re
import time
import json
import os
import psutil
import boto3
from git import Repo
from unqlite import UnQLite
import grpc
from google.protobuf import duration_pb2 as google_dot_protobuf_dot_duration__pb2
from cpuinfo import get_cpu_info
from robot.api import logger
from dateutil import parser as date_parser


class delta_process_stat:
    """! This class calc difference between two GetProcessStats results
    """

    def __init__(self, process_stat1, process_stat2):
        """! constructor
        @param process_stat1 last GetProcessStats result
        @param process_stat2 previous GetProcessStats result
        """
        self.query_read_bytes = process_stat1.query_read_bytes - \
            process_stat2.query_read_bytes
        self.query_write_bytes = process_stat1.query_write_bytes - \
            process_stat2.query_write_bytes
        self.real_read_bytes = process_stat1.real_read_bytes - process_stat2.real_read_bytes
        self.real_write_bytes = process_stat1.real_write_bytes - \
            process_stat2.real_write_bytes
        self.user_time = self.sub_duration(
            process_stat1.user_time, process_stat2.user_time)
        self.kernel_time = self.sub_duration(
            process_stat1.kernel_time, process_stat2.kernel_time)

    def sub_duration(self, duration_left, duration_right):
        """! sub 2 protobuf TimeStamps
        @return a protobuf Duration
        """

        ret = google_dot_protobuf_dot_duration__pb2.Duration()
        ret.FromMilliseconds(duration_left.ToMilliseconds() -
                             duration_right.ToMilliseconds())
        a = ret.seconds
        b = ret.nanos
        return ret


def diff_process_stat(process_stat1, process_stat2):
    return delta_process_stat(process_stat1, process_stat2)


def add_value_to_delta_process_stat(delta, key, value):
    """! add an attribute to an object"""
    setattr(delta, key, value)


def get_last_bench_result(log: str, id: int, name_to_find: str):
    """! extract last bench trace in broker log file
    @log  path of the log file
    @param id id field of the bench event
    @param name_to_find a string to find in the bench result like a muxer name
    @return  a json object that contains all bench time points
    """
    last_bench_str = ""
    p = re.compile(
        r".+bench\s+\w+\s+content:'(.\"id\":{}.+{}.+)'".format(id, name_to_find.replace(" ", "\s")))

    try:
        f = open(log, "r")
        lines = f.readlines()
        f.close()

        for line in lines:
            extract = p.match(line)
            if extract is not None:
                last_bench_str = extract.group(1)

        if len(last_bench_str) == 0:
            logger.console("The file '{}' does not contain bench".format(log))
            return None
        return json.loads(last_bench_str)
    except IOError:
        logger.console("The file '{}' does not exist".format(log))
        return None


def get_last_bench_result_with_timeout(log: str, id: int, name_to_find: str, timeout: int):
    """! extract last bench trace in broker log file
    @log  path of the log file
    @param id id field of the bench event
    @param name_to_find a string to find in the bench result like a muxer name
    @param timeout  time out in seconds
    @return  a json object that contains all bench time points
    """
    limit = time.time() + timeout
    while time.time() < limit:
        json_obj = get_last_bench_result(log, id, name_to_find)
        if json_obj is not None:
            return json_obj
        time.sleep(5)
    logger.console("The file '{}' does not contain bench".format(log))
    return None


def calc_bench_delay(bench_event_result, bench_muxer_begin: str, bench_muxer_begin_function: str, bench_muxer_end: str, bench_muxer_end_function: str):
    """! calc a duration between two time points of a bench event json object
    @param bench_event_result bench result json object
    @param bench_muxer_begin name of the muxer owned to the first time point
    @param bench_muxer_begin_function name of the muxer function owned to the first time point
    @param bench_muxer_end name of the muxer owned to the last time point
    @param bench_muxer_end_function name of the muxer function owned to the last time point
    """
    for point in bench_event_result['points']:
        if point['name'] == bench_muxer_begin and point['function'] == bench_muxer_begin_function:
            time_begin = date_parser.parse(point['time'])
        if point['name'] == bench_muxer_end and point['function'] == bench_muxer_end_function:
            time_end = date_parser.parse(point['time'])
    return time_end - time_begin


def store_result_in_unqlite(file_path: str, test_name: str,  broker_or_engine: str, resources_consumed: delta_process_stat, bench_event_result, bench_muxer_begin: str, bench_muxer_begin_function: str, bench_muxer_end: str, bench_muxer_end_function: str):
    """! store a bench result and process stat difference in an unqlite file
    it also stores git information
    @param file_path  path of the unqlite file
    @param test_name name of the state
    @param broker_or_engine a string equal to engine or broker to identify stats owning
    @param resources_consumed a delta_process_stat object
    @param bench_event_result a json bench result
    @param bench_muxer_begin name of the muxer owned to the first time point
    @param bench_muxer_begin_function name of the muxer function owned to the first time point
    @param bench_muxer_end name of the muxer owned to the last time point
    @param bench_muxer_end_function name of the muxer function owned to the last time point
    """

    row = vars(resources_consumed).copy()
    row['user_time'] = row['user_time'].seconds + \
        row['user_time'].nanos / 1000000000.0
    row['kernel_time'] = row['kernel_time'].seconds + \
        row['kernel_time'].nanos / 1000000000.0
    info = get_cpu_info()
    row['cpu'] = info["brand_raw"]
    row['nb_core'] = info["count"]
    row['memory_size'] = psutil.virtual_memory().total
    row['event_propagation_delay'] = calc_bench_delay(
        bench_event_result, bench_muxer_begin, bench_muxer_begin_function, bench_muxer_end, bench_muxer_end_function).total_seconds()

    # rev_name is like <commit sha> <branch>~*
    rev_name_dev_branch = re.compile(
        r'[0-9a-f]+\s(dev[\w\.]+).*')
    remote_rev_name_dev_branch = re.compile(
        r'[0-9a-f]+\sremotes/origin/(dev[\w\.]+).*')
    # git branch and commit
    try:
        repo = Repo(os.getcwd())
    except:
        # if we launch from tests directory => open ..directory
        repo = Repo(os.getcwd() + "/..")
    commit = repo.head.commit
    origin_found = False
    while True:
        m = rev_name_dev_branch.match(commit.name_rev)
        if m is not None:
            row['origin'] = m.group(1)
            origin_found = True
            break
        m = remote_rev_name_dev_branch.match(commit.name_rev)
        if m is not None:
            row['origin'] = m.group(1)
            origin_found = True
            break

        parents = commit.parents
        if len(parents) == 0:
            break
        commit = parents[0]

    if not origin_found:
        logger.console(
            f"origin not found for commit {repo.head.commit} your branch needs rebase?")
        return False
    if repo.is_dirty():
        row['commit'] = f'uncommited files last_commit:{repo.head.commit.hexsha}'
    else:
        row['commit'] = repo.head.commit.hexsha
    row['t'] = time.time()
    db = UnQLite(file_path)
    benchs = db.collection(
        f'collectbench_{test_name}_{broker_or_engine}_{bench_event_result["id"]}')
    benchs.create()
    benchs.store(row)
    test = benchs.all()
    db.close()
    return True


def upload_database_to_s3(file_path: str):
    """! upload a file to s3
    @param file_path path of the file on the disk
    """
    aws_access_key_id = os.getenv('AWS_ACCESS_KEY_ID')
    aws_secret_access_key = os.getenv('AWS_SECRET_ACCESS_KEY')
    bucket = os.getenv('AWS_BUCKET')
    if aws_access_key_id is None or aws_secret_access_key is None:
        logger.console(
            "AWS_ACCESS_KEY_ID OR AWS_SECRET_ACCESS_KEY or AWS_BUCKET not set")
        return False
    try:
        s3_resource = boto3.resource('s3')
        s3_resource.Object(bucket, os.path.basename(
            file_path)).upload_file(file_path)
        return True
    except Exception as e:
        logger.console(f"upload to s3  exception:{e}")
        return False


def download_database_from_s3(file_path: str):
    """! upload a file to s3
    @param file_path path of the file on the disk
    @bucket s3 bucket
    """
    aws_access_key_id = os.getenv('AWS_ACCESS_KEY_ID')
    aws_secret_access_key = os.getenv('AWS_SECRET_ACCESS_KEY')
    bucket = os.getenv('AWS_BUCKET')
    if aws_access_key_id is None or aws_secret_access_key is None:
        logger.console(
            "AWS_ACCESS_KEY_ID OR AWS_SECRET_ACCESS_KEY or AWS_BUCKET not set")
        return False
    try:
        s3_resource = boto3.resource('s3')
        s3_resource.Object(bucket, os.path.basename(
            file_path)).download_file(file_path)
        return True
    except Exception as e:
        logger.console(f"upload to s3  exception:{e}")
        return False
