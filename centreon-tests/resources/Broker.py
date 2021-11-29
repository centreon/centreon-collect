import pymysql.cursors
import time
import socket, sys, time
from datetime import datetime
from subprocess import getoutput
import subprocess as subp
from robot.api import logger
import json
import glob
import os.path

TIMEOUT = 30

config = {
"central": """{{
    "centreonBroker": {{
        "broker_id": {0},
        "broker_name": "{1}",
        "poller_id": 1,
        "poller_name": "Central",
        "module_directory": "/usr/share/centreon/lib/centreon-broker",
        "log_timestamp": true,
        "log_thread_id": false,
        "event_queue_max_size": 100000,
        "command_file": "/var/lib/centreon-broker/command.sock",
        "cache_directory": "/var/lib/centreon-broker",
        "log": {{
            "directory": "/var/log/centreon-broker/",
            "filename": "",
            "max_size": 0,
            "loggers": {{
                "core": "trace",
                "config": "error",
                "sql": "error",
                "processing": "error",
                "perfdata": "error",
                "bbdo": "error",
                "tcp": "error",
                "tls": "error",
                "lua": "error",
                "bam": "error"
            }}
        }},
        "input": [
            {{
                "name": "central-broker-master-input",
                "port": "5669",
                "buffering_timeout": "0",
                "retry_interval": "5",
                "protocol": "bbdo",
                "tls": "no",
                "negotiation": "yes",
                "one_peer_retention_mode": "no",
                "compression": "no",
                "type": "ipv4"
            }}
        ],
        "output": [
            {{
                "name": "central-broker-master-sql",
                "db_type": "mysql",
                "retry_interval": "5",
                "buffering_timeout": "0",
                "db_host": "localhost",
                "db_port": "3306",
                "db_user": "centreon",
                "db_password": "centreon",
                "db_name": "centreon_storage",
                "queries_per_transaction": "1000",
                "connections_count": "3",
                "read_timeout": "1",
                "type": "sql"
            }},
            {{
                "name": "centreon-broker-master-rrd",
                "port": "5670",
                "buffering_timeout": "0",
                "host": "localhost",
                "retry_interval": "5",
                "protocol": "bbdo",
                "tls": "no",
                "negotiation": "yes",
                "one_peer_retention_mode": "no",
                "compression": "no",
                "type": "ipv4"
            }},
            {{
                "name": "central-broker-master-perfdata",
                "interval": "60",
                "retry_interval": "5",
                "buffering_timeout": "0",
                "length": "15552000",
                "db_type": "mysql",
                "db_host": "localhost",
                "db_port": "3306",
                "db_user": "centreon",
                "db_password": "centreon",
                "db_name": "centreon_storage",
                "queries_per_transaction": "1000",
                "read_timeout": "1",
                "check_replication": "no",
                "store_in_data_bin": "yes",
                "connections_count": "3",
                "insert_in_index_data": "1",
                "type": "storage"
            }}
        ],
        "stats": [
            {{
                "type": "stats",
                "name": "central-broker-master-stats",
                "json_fifo": "/var/lib/centreon-broker/central-broker-master-stats.json"
            }}
        ],
        "grpc": {{
            "port": 51001
        }}
    }}
}}""",

"module": """{{
    "centreonBroker": {{
        "broker_id": {},
        "broker_name": "{}",
        "poller_id": 1,
        "poller_name": "Central",
        "module_directory": "/usr/share/centreon/lib/centreon-broker",
        "log_timestamp": false,
        "log_thread_id": false,
        "event_queue_max_size": 100000,
        "command_file": "",
        "cache_directory": "/var/lib/centreon-engine",
        "log": {{
            "directory": "/var/log/centreon-broker/",
            "filename": "",
            "max_size": 0,
            "loggers": {{
                "core": "trace",
                "config": "debug",
                "sql": "debug",
                "processing": "debug",
                "perfdata": "debug",
                "bbdo": "debug",
                "tcp": "debug",
                "tls": "debug",
                "lua": "debug",
                "bam": "debug"
            }}
        }},
        "output": [
            {{
                "name": "central-module-master-output",
                "port": "5669",
                "host": "localhost",
                "protocol": "bbdo",
                "tls": "no",
                "negotiation": "yes",
                "buffering_timeout": "0",
                "retry_interval": "60",
                "one_peer_retention_mode": "no",
                "compression": "no",
                "type": "ipv4"
            }}
        ],
        "stats": [
            {{
                "type": "stats",
                "name": "central-module-master-stats",
                "json_fifo": "/var/lib/centreon-engine/central-module-master-stats.json"
            }}
        ],
        "grpc": {{
            "port": 51003
        }}
    }}
}}""",

"rrd": """{{
    "centreonBroker": {{
        "broker_id": {0},
        "broker_name": "{1}",
        "poller_id": 1,
        "poller_name": "Central",
        "module_directory": "/usr/share/centreon/lib/centreon-broker",
        "log_timestamp": true,
        "log_thread_id": false,
        "event_queue_max_size": 100000,
        "command_file": "",
        "cache_directory": "/var/lib/centreon-broker",
        "log": {{
            "directory": "/var/log/centreon-broker/",
            "filename": "",
            "max_size": 0,
            "loggers": {{
                "core": "trace",
                "config": "error",
                "sql": "error",
                "processing": "error",
                "perfdata": "error",
                "bbdo": "info",
                "tcp": "info",
                "tls": "trace",
                "lua": "error",
                "bam": "error"
            }}
        }},
        "input": [
            {{
                "name": "central-rrd-master-input",
                "port": "5670",
                "protocol": "bbdo",
                "tls": "no",
                "retry_interval": "60",
                "negotiation": "yes",
                "buffering_timeout": "0",
                "one_peer_retention_mode": "no",
                "compression": "auto",
                "type": "ipv4"
            }}
        ],
        "output": [
            {{
                "name": "central-rrd-master-output",
                "metrics_path": "/var/lib/centreon/metrics/",
                "status_path": "/var/lib/centreon/status/",
                "write_metrics": "yes",
                "store_in_data_bin": "yes",
                "write_status": "yes",
                "insert_in_index_data": "1",
                "buffering_timeout": "0",
                "retry_interval": "60",
                "type": "rrd"
            }}
        ],
        "stats": [
            {{
                "type": "stats",
                "name": "central-rrd-master-stats",
                "json_fifo": "/var/lib/centreon-broker/central-rrd-master-stats.json"
            }}
        ],
        "grpc": {{ 
            "port": 51002
        }}
    }}
}}""",

"central_map": """{{
    "centreonBroker": {{
        "broker_id": {0},
        "broker_name": "{1}",
        "poller_id": 1,
        "poller_name": "Central",
        "module_directory": "/usr/share/centreon/lib/centreon-broker",
        "log_timestamp": true,
        "log_thread_id": false,
        "event_queue_max_size": 10,
        "command_file": "/var/lib/centreon-broker/command.sock",
        "cache_directory": "/var/lib/centreon-broker",
        "log": {{
            "directory": "/var/log/centreon-broker/",
            "filename": "",
            "max_size": 0,
            "loggers": {{
                "core": "trace",
                "config": "error",
                "sql": "error",
                "processing": "error",
                "perfdata": "error",
                "bbdo": "error",
                "tcp": "error",
                "tls": "error",
                "lua": "error",
                "bam": "error"
            }}
        }},
        "input": [
            {{
                "name": "central-broker-master-input",
                "port": "5669",
                "buffering_timeout": "0",
                "retry_interval": "5",
                "protocol": "bbdo",
                "tls": "no",
                "negotiation": "yes",
                "one_peer_retention_mode": "no",
                "compression": "no",
                "type": "ipv4"
            }}
        ],
        "output": [
            {{
                "name": "central-broker-master-sql",
                "db_type": "mysql",
                "retry_interval": "5",
                "buffering_timeout": "0",
                "db_host": "localhost",
                "db_port": "3306",
                "db_user": "centreon",
                "db_password": "centreon",
                "db_name": "centreon_storage",
                "queries_per_transaction": "1000",
                "connections_count": "3",
                "read_timeout": "1",
                "type": "sql"
            }},
            {{
                "name": "centreon-broker-master-rrd",
                "port": "5670",
                "buffering_timeout": "0",
                "host": "localhost",
                "retry_interval": "5",
                "protocol": "bbdo",
                "tls": "no",
                "negotiation": "yes",
                "one_peer_retention_mode": "no",
                "compression": "no",
                "type": "ipv4"
            }},
            {{
                "name": "centreon-broker-master-map",
                "port": "5671",
                "buffering_timeout": "0",
                "retry_interval": "5",
                "protocol": "bbdo",
                "tls": "no",
                "negotiation": "yes",
                "one_peer_retention_mode": "no",
                "compression": "no",
                "type": "ipv4"
            }},
            {{
                "name": "central-broker-master-perfdata",
                "interval": "60",
                "retry_interval": "5",
                "buffering_timeout": "0",
                "length": "15552000",
                "db_type": "mysql",
                "db_host": "localhost",
                "db_port": "3306",
                "db_user": "centreon",
                "db_password": "centreon",
                "db_name": "centreon_storage",
                "queries_per_transaction": "1000",
                "read_timeout": "1",
                "check_replication": "no",
                "store_in_data_bin": "yes",
                "connections_count": "3",
                "insert_in_index_data": "1",
                "type": "storage"
            }}
        ],
        "stats": [
            {{
                "type": "stats",
                "name": "central-broker-master-stats",
                "json_fifo": "/var/lib/centreon-broker/central-broker-master-stats.json"
            }}
        ],
        "grpc": {{
            "port": 51001
        }}
    }}
}}""",
}

def config_broker(name):
    if name == 'central':
        broker_id = 1
        broker_name = "central-broker-master"
        filename = "central-broker.json"
    elif name == 'central_map':
        broker_id = 1
        broker_name = "central-broker-master"
        filename = "central-broker.json"
    elif name == 'module':
        broker_id = 3
        broker_name = "central-module-master"
        filename = "central-module.json"
    else:
        broker_id = 2
        broker_name = "central-rrd-master"
        filename = "central-rrd.json"

    f = open("/etc/centreon-broker/{}".format(filename), "w")
    f.write(config[name].format(broker_id, broker_name))
    f.close()

def config_broker_sql_output(name, output):
    if name == 'central':
        filename = "central-broker.json"
    elif name == 'module':
        filename = "central-module.json"
    else:
        filename = "central-rrd.json"

    f = open("/etc/centreon-broker/{}".format(filename), "r")
    buf = f.read()
    f.close()
    conf = json.loads(buf)
    output_dict = conf["centreonBroker"]["output"]
    for i, v in enumerate(output_dict):
        if v["type"] == "sql" or v["type"] == "storage" or v["type"] == "unified_sql":
            output_dict.pop(i)
    if output == 'unified_sql':
        output_dict.append({
          "name" : "central-broker-unified-sql",
          "db_type" : "mysql",
          "db_host" : "localhost",
          "db_port" : "3306",
          "db_user" : "centreon",
          "db_password" : "centreon",
          "db_name" : "centreon_storage",
          "interval" : "60",
          "length" : "15552000",
          "queries_per_transaction" : "20000",
          "connections_count" : "4",
          "read_timeout" : "60",
          "buffering_timeout" : "0",
          "retry_interval" : "60",
          "check_replication" : "no",
          "type" : "unified_sql",
          "store_in_data_bin" : "yes",
          "insert_in_index_data" : "1"
        })
    elif output == 'sql/perfdata':
        output_dict.append({
                "name": "central-broker-master-sql",
                "db_type": "mysql",
                "retry_interval": "5",
                "buffering_timeout": "0",
                "db_host": "localhost",
                "db_port": "3306",
                "db_user": "centreon",
                "db_password": "centreon",
                "db_name": "centreon_storage",
                "queries_per_transaction": "1000",
                "connections_count": "3",
                "read_timeout": "1",
                "type": "sql"
        })
        output_dict.append({
                "name": "central-broker-master-perfdata",
                "interval": "60",
                "retry_interval": "5",
                "buffering_timeout": "0",
                "length": "15552000",
                "db_type": "mysql",
                "db_host": "localhost",
                "db_port": "3306",
                "db_user": "centreon",
                "db_password": "centreon",
                "db_name": "centreon_storage",
                "queries_per_transaction": "1000",
                "read_timeout": "1",
                "check_replication": "no",
                "store_in_data_bin": "yes",
                "connections_count": "3",
                "insert_in_index_data": "1",
                "type": "storage"
        })
    f = open("/etc/centreon-broker/{}".format(filename), "w")
    f.write(json.dumps(conf, indent=2))
    f.close()

def broker_config_clear_outputs_except(name, ex : list):
    if name == 'central':
        filename = "central-broker.json"
    elif name == 'module':
        filename = "central-module.json"
    else:
        filename = "central-rrd.json"

    f = open("/etc/centreon-broker/{}".format(filename), "r")
    buf = f.read()
    f.close()
    conf = json.loads(buf)
    output_dict = conf["centreonBroker"]["output"]
    for i, v in enumerate(output_dict):
        if v["type"] not in ex:
            output_dict.pop(i)

    f = open("/etc/centreon-broker/{}".format(filename), "w")
    f.write(json.dumps(conf, indent=2))
    f.close()

def broker_config_add_lua_output(name, output, luafile):
    if name == 'central':
        filename = "central-broker.json"
    elif name == 'module':
        filename = "central-module.json"
    else:
        filename = "central-rrd.json"

    f = open("/etc/centreon-broker/{}".format(filename), "r")
    buf = f.read()
    f.close()
    conf = json.loads(buf)
    output_dict = conf["centreonBroker"]["output"]
    output_dict.append({
      "name": output,
      "path": luafile,
      "type": "lua"
    })
    f = open("/etc/centreon-broker/{}".format(filename), "w")
    f.write(json.dumps(conf, indent=2))
    f.close()

def broker_config_output_set(name, output, key, value):
    if name == 'central':
        filename = "central-broker.json"
    elif name == 'module':
        filename = "central-module.json"
    else:
        filename = "central-rrd.json"
    f = open("/etc/centreon-broker/{}".format(filename), "r")
    buf = f.read()
    f.close()
    conf = json.loads(buf)
    output_dict = [elem for i, elem in enumerate(conf["centreonBroker"]["output"]) if elem["name"] == output][0]
    output_dict[key] = value
    f = open("/etc/centreon-broker/{}".format(filename), "w")
    f.write(json.dumps(conf, indent=2))
    f.close()

def broker_config_output_remove(name, output, key):
    if name == 'central':
        filename = "central-broker.json"
    elif name == 'module':
        filename = "central-module.json"
    else:
        filename = "central-rrd.json"
    f = open("/etc/centreon-broker/{}".format(filename), "r")
    buf = f.read()
    f.close()
    conf = json.loads(buf)
    output_dict = [elem for i, elem in enumerate(conf["centreonBroker"]["output"]) if elem["name"] == output][0]
    if key in output_dict:
      output_dict.pop(key)
    f = open("/etc/centreon-broker/{}".format(filename), "w")
    f.write(json.dumps(conf, indent=2))
    f.close()

def broker_config_input_set(name, inp, key, value):
    if name == 'central':
        filename = "central-broker.json"
    elif name == 'module':
        filename = "central-module.json"
    else:
        filename = "central-rrd.json"
    f = open("/etc/centreon-broker/{}".format(filename), "r")
    buf = f.read()
    f.close()
    conf = json.loads(buf)
    input_dict = [elem for i, elem in enumerate(conf["centreonBroker"]["input"]) if elem["name"] == inp][0]
    input_dict[key] = value
    f = open("/etc/centreon-broker/{}".format(filename), "w")
    f.write(json.dumps(conf, indent=2))
    f.close()

def broker_config_input_remove(name, inp, key):
    if name == 'central':
        filename = "central-broker.json"
    elif name == 'module':
        filename = "central-module.json"
    else:
        filename = "central-rrd.json"
    f = open("/etc/centreon-broker/{}".format(filename), "r")
    buf = f.read()
    f.close()
    conf = json.loads(buf)
    input_dict = [elem for i, elem in enumerate(conf["centreonBroker"]["input"]) if elem["name"] == inp][0]
    if key in input_dict:
      input_dict.pop(key)
    f = open("/etc/centreon-broker/{}".format(filename), "w")
    f.write(json.dumps(conf, indent=2))
    f.close()

def broker_config_log(name, key, value):
    if name == 'central':
        filename = "central-broker.json"
    elif name == 'module':
        filename = "central-module.json"
    else:
        filename = "central-rrd.json"
    f = open("/etc/centreon-broker/{}".format(filename), "r")
    buf = f.read()
    f.close()
    conf = json.loads(buf)
    loggers = conf["centreonBroker"]["log"]["loggers"]
    loggers[key] = value
    f = open("/etc/centreon-broker/{}".format(filename), "w")
    f.write(json.dumps(conf, indent=2))
    f.close()

def check_broker_stats_exist(name, key1, key2, timeout=TIMEOUT):
  limit = time.time() + timeout
  while time.time() < limit:
    if name == 'central':
        filename = "central-broker-master-stats.json"
    elif name == 'module':
        filename = "central-module-master-stats.json"
    else:
        filename = "central-rrd-master-stats.json"
    retry = True
    while retry:
      retry = False
      f = open("/var/lib/centreon-broker/{}".format(filename), "r")
      buf = f.read()
      f.close()

      try:
        conf = json.loads(buf)
      except:
        retry = True
    if key1 in conf:
      if key2 in conf[key1]:
        return True
    time.sleep(1)
  return False

def get_broker_stats_size(name, key, timeout=TIMEOUT):
  limit = time.time() + timeout
  retval = 0
  while time.time() < limit:
    if name == 'central':
        filename = "central-broker-master-stats.json"
    elif name == 'module':
        filename = "central-module-master-stats.json"
    else:
        filename = "central-rrd-master-stats.json"
    retry = True
    while retry:
      retry = False
      f = open("/var/lib/centreon-broker/{}".format(filename), "r")
      buf = f.read()
      f.close()
      try:
        conf = json.loads(buf)
      except:
        retry = True

    if key in conf:
      value = len(conf[key])
    else:
      value = 0
    if value > retval:
      retval = value
    elif retval != 0:
      return retval
    time.sleep(5)
  return retval

def get_metric_to_delete():
    files = [os.path.basename(x) for x in glob.glob("/var/lib/centreon/metrics/*.rrd")]
    ids = [int(f.split(".")[0]) for f in files]

    # Connect to the database
    connection = pymysql.connect(host='localhost',
                                 user='root',
                                 password='centreon',
                                 database='centreon_storage',
                                 charset='utf8mb4',
                                 cursorclass=pymysql.cursors.DictCursor)

    with connection:
        with connection.cursor() as cursor:
            # Read a single record
            sql = "SELECT `metric_id` FROM `metrics`"
            cursor.execute(sql)
            result = cursor.fetchall()
            ids_db = [r['metric_id'] for r in result]

    inter = list(set(ids) & set(ids_db))
    retval = inter[0]
    return retval

def run_reverse_bam(duration, interval):
    subp.Popen("broker/map_client.py {:f}".format(interval), shell=True, stdout=subp.PIPE, stdin=subp.PIPE)
    time.sleep(duration)
    getoutput("kill -9 $(ps aux | grep map_client.py | awk '{print $2}')")
