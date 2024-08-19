#
# Copyright 2024 Centreon
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

from robot.api import logger
import re
import time
from dateutil import parser
from datetime import datetime, timedelta
import requests


def ctn_get_api_log_with_timeout(token: str, node_path='', host='http://127.0.0.1:8085', timeout=15):
    """! Query gorgone log API until the response contains a log with code 2 (success) or 1 (failure)
        @param token: token to search in the API
        @param node_path: part of the API URL defining if we use the local gorgone or another one, ex node/2/
        @param timeout: timeout in seconds
        @param host: gorgone API URL with the port
        @return True(when output of the command is found)/False(on failure or timeout),
                and a json object containing the incriminated log for failure or success.
        """
    limit_date = time.time() + timeout
    api_json = []
    while time.time() < limit_date:
        time.sleep(1)
        uri = host + "/api/" + node_path + "log/" + token
        response = requests.get(uri)
        (status, output) = parse_json_response(response)
        if status == '':
            continue
        return status, output

    return False, api_json["data"]


def parse_json_response(response):
    api_json = response.json()
    # http code should either be 200 for success or 404 for no log found if we are too early.
    # as the time of writing, status code is always 200 because webapp autodiscovery module always expect a 200.
    if response.status_code != 200 and response.status_code != 404:
        return False, api_json

    if 'error' in api_json and api_json['error'] == "no_log":
        return '', ''
    for log_detail in api_json["data"]:
        if log_detail["code"] == 2:
            return False, log_detail
        if log_detail["code"] == 100:
            return True, log_detail


# these function search log in the gorgone log file
def ctn_find_in_log_with_timeout(log: str, content, timeout=20, date=-1, regex=False):
    """! search a pattern in log from date param
        @param log: path of the log file
        @param date: date from witch it begins search, you might want to use robot Get Current Date function
        @param content: array of pattern to search
        @param timeout: time out in second
        @param regex: search use regex, default to false
        @return  True/False, array of lines found for each pattern
        """
    if date == -1:
        date = datetime.now().timestamp() - 1
    limit = time.time() + timeout
    c = ""
    while time.time() < limit:
        ok, c = ctn_find_in_log(log, date, content, regex)
        if ok:
            return True, c
        time.sleep(5)
    logger.console(f"Unable to find '{c}' from {date} during {timeout}s")
    return False


def ctn_find_in_log(log: str, date, content, regex=False):
    """Find content in log file from the given date

    Args:
        log (str): The log file
        date (_type_): A date as a string
        content (_type_): An array of strings we want to find in the log.

    Returns:
        boolean,str: The boolean is True on success, and the string contains the first string not found in logs otherwise.
    """
    logger.info(f"regex={regex}")
    res = []

    try:
        f = open(log, "r", encoding="latin1")
        lines = f.readlines()
        f.close()
        idx = ctn_find_line_from(lines, date)

        for c in content:
            found = False
            for i in range(idx, len(lines)):
                line = lines[i]
                if regex:
                    match = re.search(c, line)
                else:
                    match = c in line
                if match:
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


def ctn_extract_date_from_log(line: str):
    p = re.compile(r"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})")
    m = p.match(line)
    if m is None:
        return None
    try:
        return parser.parse((m.group(1)))
    except parser.ParserError:
        logger.console(f"Unable to parse the date from the line {line}")
        return None


def ctn_find_line_from(lines, date):
    try:
        my_date = parser.parse(date)
    except:
        my_date = datetime.fromtimestamp(date)

    # Let's find my_date
    start = 0
    end = len(lines) - 1
    idx = start
    while end > start:
        idx = (start + end) // 2
        idx_d = ctn_extract_date_from_log(lines[idx])
        while idx_d is None:
            logger.console("Unable to parse the date ({} <= {} <= {}): <<{}>>".format(
                start, idx, end, lines[idx]))
            idx -= 1
            if idx >= 0:
                idx_d = ctn_extract_date_from_log(lines[idx])
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
