#!/usr/bin/python3
#
# Copyright 2025 Centreon
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
# This script is a little tcp server working on port 5669. It can simulate
# a cbd instance. It is useful to test the validity of BBDO packets sent by
# centengine.

# confcmp is a tool to compare a cfg file that configures Engine and a table of the centreon_storage database,
# to check if they are in sync.
# It can be used to check if the configuration of Engine is well stored in the centreon_storage database.
from robot.api import logger
import pymysql
import time

DB_HOST = "localhost"
DB_USER = "centreon"
DB_PASS = "centreon"
DB_NAME_STORAGE = "centreon_storage"


class ConfComparator:
    """
    A class to compare a poller cfg file and what Broker wrote in the storage database.
    """

    def __init__(self, poller_id: int, cfg_file: str, table: str = "", query: str = "", obj_name: str = "", check_length: bool = True):
        """
        Constructor of the class.

        Args:
            self: The instance to build.
            poller_id: A poller id.
            cfg_file: The cfg file representing the configuration (for example /tmp/var/lib/centreon/config/1/services.cfg)
            table: The table to get information on (for example: services)
            query: This argument is optional, if given, it represents the query to execute to get the data. Otherwise, the query is made from the given table (but is necessarily simpler).
            obj_name: This is optional, we know that for the table services, the object is service. But we don't know for the table resources what are we interested about. So this argument erases the ambiguity.
        """
        self.poller_id = poller_id
        self.cfg_file = cfg_file
        self.table = table
        self.query = query
        self.check_length = check_length

        # Correspondence between table name and object that is defined in the cfg file
        obj_dict = {
            "hosts": "host",
            "services": "service",
            "severities": "severity",
            "tags": "tag",
        }
        if obj_name == "":
            logger.console(f"Getting obj_dict[{table}]")
            self.obj_name = obj_dict[table]
        else:
            logger.console(f"Getting obj_name as {obj_name}")
            self.obj_name = obj_name

    def get_data(self):
        """
        Returns an array with the fields to compare. They will be compared in that specific order. Each item has its name in the table, in the cfg file and also its type that can be "str", "int" or "float".
        """

        if self.table == "hosts":
            return [
                {"db_field": "name", "cfg_field": "host_name", "type": "str"},
                {"db_field": "alias", "cfg_field": "alias", "type": "str"},
                {"db_field": "address", "cfg_field": "address", "type": "str"},
                {"db_field": "check_command",
                 "cfg_field": "check_command", "type": "str"},
                {"db_field": "check_period",
                 "cfg_field": "check_period", "type": "str"},
                {"db_field": "host_id", "cfg_field": "_HOST_ID", "type": "int"},
            ]
        elif self.table == "services":
            return [
                {"db_field": "service_id", "cfg_field": "_SERVICE_ID", "type": "int"},
                {"db_field": "description",
                 "cfg_field": "service_description", "type": "str"},
                {"db_field": "host_name", "cfg_field": "host_name", "type": "str"},
                {"db_field": "check_command",
                 "cfg_field": "check_command", "type": "str"},
                {"db_field": "check_period",
                 "cfg_field": "check_period", "type": "str"},
                {"db_field": "max_check_attempts",
                 "cfg_field": "max_check_attempts", "type": "int"},
                {"db_field": "check_interval",
                 "cfg_field": "check_interval", "type": "float"},
                {"db_field": "retry_interval",
                 "cfg_field": "retry_interval", "type": "float"},
                {"db_field": "retry_interval",
                 "cfg_field": "retry_interval", "type": "float"},
            ]
        elif self.table == "resources" and self.obj_name == "host":
            return [
                {"db_field": "name", "cfg_field": "host_name", "type": "str"},
                {"db_field": "alias", "cfg_field": "alias", "type": "str"},
                {"db_field": "address", "cfg_field": "address", "type": "str"},
                {"db_field": "id", "cfg_field": "_HOST_ID", "type": "int"},
            ]
        elif self.table == "resources" and self.obj_name == "service":
            return [
                {"db_field": "id", "cfg_field": "_SERVICE_ID", "type": "int"},
                {"db_field": "name",
                 "cfg_field": "service_description", "type": "str"},
                {"db_field": "parent_name",
                 "cfg_field": "host_name", "type": "str"},
                {"db_field": "active_checks_enabled",
                 "cfg_field": "active_checks_enabled", "type": "int"},
                {"db_field": "passive_checks_enabled",
                 "cfg_field": "passive_checks_enabled", "type": "int"},
                {"db_field": "max_check_attempts",
                 "cfg_field": "max_check_attempts", "type": "int"},
            ]
        elif self.table == "severities":
            return [
                {"db_field": "id", "cfg_field": "id", "type": "int"},
                {"db_field": "type", "cfg_field": "type",
                 "type": "enum{service=>0,host=>1}"},
                {"db_field": "name", "cfg_field": "severity_name", "type": "str"},
                {"db_field": "level", "cfg_field": "level", "type": "int"},
                {"db_field": "icon_id", "cfg_field": "icon_id", "type": "int"},
            ]
        elif self.table == "tags":
            return [
                {"db_field": "id", "cfg_field": "id", "type": "int"},
                {"db_field": "type", "cfg_field": "type",
                 "type": "enum{servicegroup=>0,hostgroup=>1,servicecategory=>2,hostcategory=>3}"},
                {"db_field": "name", "cfg_field": "tag_name", "type": "str"},
            ]
        else:
            raise TypeError(f"get_data doesn't work with table '{self.table}'")

    def convert(self, obj, from_cfg: bool):
        """
        Make the conversion from a dictionary obtained from the cfg file or the database to an array where items are list with fields in the same order than what we have in the get_data() method.

        Args:
            self: the instance object.
            obj: the dictionary to convert.
            from_cfg: a boolean telling if the obj has been obtained from a cfg file (True) or from the DB (False).

        Returns: A list of lists.
        """
        data = self.get_data()
        if from_cfg:
            keys = [d["cfg_field"] for d in data]
        else:
            keys = [d["db_field"] for d in data]

        retval = [str(obj[k]) for k in keys]
        return retval

    def build_file_content(self):
        """
        Build the configuration from the cfg file content as a list of lists.

        Returns: A list of list.
        """
        fields = [d["cfg_field"] for d in self.get_data()]
        types = [d["type"] for d in self.get_data()]
        retval = []
        with open(self.cfg_file, 'r') as f:
            content = f.readlines()
        obj = {}
        currently_building = False
        for line in content:
            line = line.strip()
            if line.startswith(f"define {self.obj_name}"):
                if currently_building:
                    # An object has already been started and we start a new one,
                    # so we have to save the previous one.
                    row = self.convert(obj, True)
                    retval.append(row)
                    obj = {}

                # Now, we can start the new one
                currently_building = True
            else:
                for f, t in zip(fields, types):
                    if line.startswith(f):
                        if t == "str":
                            obj[f] = line.split()[1]
                        elif t == "int":
                            obj[f] = int(line.split()[1])
                        elif t == "float":
                            obj[f] = float(line.split()[1])
                        elif t.startswith("enum{"):
                            items = t[5:-1]
                            items = items.split(",")
                            value = line.split()[1]
                            for ff in items:
                                if ff.startswith(f"{value}=>"):
                                    obj[f] = int(ff.split("=>")[1])
                                    break
                        else:
                            raise TypeError(
                                "Types in data are among str, int or float")
        if currently_building:
            # An object has already been started and we start a new one,
            # so we have to save the previous one.
            row = self.convert(obj, True)
            retval.append(row)
        return retval

    def build_db_content(self):
        """
        Build the configuration from the database as a list of lists.

        Returns: A list of lists.
        """
        retval = []
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     autocommit=True,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                if self.query == "":
                    data = [cf["db_field"] for cf in self.get_data()]
                    data_str = ", ".join(data)
                    sql = f"SELECT {data_str} FROM {self.table} WHERE enabled=1 AND instance_id={self.poller_id}"
                    logger.console(sql)
                else:
                    sql = self.query
                cursor.execute(sql)
                result = cursor.fetchall()
                for r in result:
                    row = self.convert(r, False)
                    retval.append(row)
        return retval

    def compare(self, timeout: int = 0):
        """
        The main function in ConfComparator, it compares the two contents and
        returns if they are equivalent or not. Sometimes, the db content can
        have more items than the file content because we can not always remove
        them. So we then only check that the file content is a subset of the Db
        content.

        Args:
            self: The instance object.
            timeout: Timeout in seconds to wait before giving up. 0 means no timeout.
        Returns: True if they match, False otherwise.
        """
        file_content = self.build_file_content()
        file_content.sort()

        limit = time.time() + timeout + 1
        while time.time() < limit:
            db_content = self.build_db_content()
            db_content.sort()

            if self.check_length and len(file_content) != len(db_content):
                logger.console(
                    f"file content has a length of {len(file_content)} whereas db content length is {len(db_content)}")
            elif len(file_content) == len(db_content):
                if file_content != db_content:
                    logger.console("file content and db content don't match")
                    for idx, (f, d) in enumerate(zip(file_content, db_content)):
                        if f != d:
                            logger.console(
                                f"Difference at index {idx}: file has {f} whereas db has {d}")
                else:
                    return True
            else:
                i, j = 0, 0
                retval = True
                while i < len(file_content) and j < len(db_content):
                    if file_content[i] == db_content[j]:
                        i += 1
                        j += 1
                    elif file_content[i] > db_content[j]:
                        j += 1
                    else:
                        logger.console(
                            f"file content has {file_content[i]} that is not in db content")
                        retval = False
                        break
                if retval:
                    return True
            time.sleep(1)
        return False
