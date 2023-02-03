from os import makedirs
from os.path import exists, dirname
import pymysql.cursors
import time
import shutil
import socket
import sys
import time
from datetime import datetime
from subprocess import getoutput
import subprocess as subp
from robot.api import logger
import json
import glob
import os.path
import grpc
import broker_pb2
import broker_pb2_grpc
from google.protobuf import empty_pb2
from robot.libraries.BuiltIn import BuiltIn

TIMEOUT = 30

BuiltIn().import_resource('db_variables.robot')
DB_NAME_STORAGE = BuiltIn().get_variable_value("${DBName}")
DB_NAME_CONF = BuiltIn().get_variable_value("${DBNameConf}")
DB_USER = BuiltIn().get_variable_value("${DBUser}")
DB_PASS = BuiltIn().get_variable_value("${DBPass}")
DB_HOST = BuiltIn().get_variable_value("${DBHost}")
DB_PORT = BuiltIn().get_variable_value("${DBPort}")
VAR_ROOT = BuiltIn().get_variable_value("${VarRoot}")
ETC_ROOT = BuiltIn().get_variable_value("${EtcRoot}")


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
        "command_file": "{7}/lib/centreon-broker/command.sock",
        "cache_directory": "{7}/lib/centreon-broker",
        "log": {{
            "log_pid": true,
            "flush_period": 1,
            "directory": "{7}/log/centreon-broker/",
            "filename": "",
            "max_size": 0,
            "loggers": {{
                "core": "trace",
                "config": "error",
                "sql": "error",
                "processing": "error",
                "perfdata": "error",
                "bbdo": "error",
                "tcp": "debug",
                "tls": "error",
                "lua": "error",
                "bam": "error",
                "grpc": "off"
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
                "db_host": "{2}",
                "db_port": "{3}",
                "db_user": "{4}",
                "db_password": "{5}",
                "db_name": "{6}",
                "queries_per_transaction": "1000",
                "connections_count": "3",
                "read_timeout": "1",
                "type": "sql"
            }},
            {{
                "name": "centreon-broker-master-rrd",
                "port": "5670",
                "buffering_timeout": "0",
                "host": "127.0.0.1",
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
                "db_host": "{2}",
                "db_port": "{3}",
                "db_user": "{4}",
                "db_password": "{5}",
                "db_name": "{6}",
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
                "json_fifo": "{7}/lib/centreon-broker/central-broker-master-stats.json"
            }}
        ],
        "grpc": {{
            "port": 51001
        }}
    }}
}}""",

    "module": """{{
    "centreonBroker": {{
        "broker_id": {0},
        "broker_name": "{1}",
        "poller_id": 1,
        "poller_name": "Central",
        "module_directory": "/usr/share/centreon/lib/centreon-broker",
        "log_timestamp": false,
        "log_thread_id": false,
        "event_queue_max_size": 100000,
        "command_file": "",
        "cache_directory": "{7}/lib/centreon-engine",
        "log": {{
            "log_pid": true,
            "flush_period": 1,
            "directory": "{7}/log/centreon-broker/",
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
                "bam": "debug",
                "grpc": "debug"
            }}
        }},
        "output": [
            {{
                "name": "central-module-master-output",
                "port": "5669",
                "host": "127.0.0.1",
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
                "json_fifo": "{7}/lib/centreon-engine/central-module-master-stats.json"
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
        "cache_directory": "{7}/lib/centreon-broker",
        "log": {{
            "log_pid": true,
            "flush_period": 1,
            "directory": "{7}/log/centreon-broker/",
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
                "metrics_path": "{7}/lib/centreon/metrics/",
                "status_path": "{7}/lib/centreon/status/",
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
                "json_fifo": "{7}/lib/centreon-broker/central-rrd-master-stats.json"
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
        "command_file": "{7}/lib/centreon-broker/command.sock",
        "cache_directory": "{7}/lib/centreon-broker",
        "log": {{
            "directory": "{7}/log/centreon-broker/",
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
                "bam": "error",
                "grpc": "error"
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
                "db_host": "{2}",
                "db_port": "{3}",
                "db_user": "{4}",
                "db_password": "{5}",
                "db_name": "{6}",
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
                "db_host": "{2}",
                "db_port": "{3}",
                "db_user": "{4}",
                "db_password": "{5}",
                "db_name": "{6}",
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
                "json_fifo": "{7}/lib/centreon-broker/central-broker-master-stats.json"
            }}
        ],
        "grpc": {{
            "port": 51001
        }}
    }}
}}""",
}


def _apply_conf(name, callback):
    if name == 'central':
        filename = "central-broker.json"
    elif name.startswith('module'):
        filename = "central-{}.json".format(name)
    else:
        filename = "central-rrd.json"

    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "r")
    buf = f.read()
    f.close()
    conf = json.loads(buf)
    callback(conf)
    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "w")
    f.write(json.dumps(conf, indent=2))
    f.close()


def config_broker(name, poller_inst: int = 1):
    makedirs(ETC_ROOT, mode=0o777, exist_ok=True)
    makedirs(VAR_ROOT, mode=0o777, exist_ok=True)
    makedirs(ETC_ROOT + "/centreon-broker", mode=0o777, exist_ok=True)
    makedirs(VAR_ROOT + "/log/centreon-broker/", mode=0o777, exist_ok=True)
    makedirs(VAR_ROOT + "/lib/centreon-broker/", mode=0o777, exist_ok=True)

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
        filename = "central-module0.json"
    else:
        if not exists(VAR_ROOT + "/lib/centreon/metrics/"):
            makedirs(VAR_ROOT + "/lib/centreon/metrics/")
        if not exists(VAR_ROOT + "/lib/centreon/status/"):
            makedirs(VAR_ROOT + "/lib/centreon/status/")
        if not exists(VAR_ROOT + "/lib/centreon/metrics/tmpl_15552000_300_0.rrd"):
            getoutput(
                "rrdcreate " + VAR_ROOT + "/lib/centreon/metrics/tmpl_15552000_300_0.rrd DS:value:ABSOLUTE:3000:U:U RRA:AVERAGE:0.5:1:864000")
        broker_id = 2
        broker_name = "central-rrd-master"
        filename = "central-rrd.json"

    if name == 'module':
        for i in range(poller_inst):
            broker_name = ETC_ROOT + "/centreon-broker/central-module{}.json".format(
                i)
            buf = config[name].format(
                broker_id, "central-module-master{}".format(i), "", "", "", "", "", VAR_ROOT)
            conf = json.loads(buf)
            conf["centreonBroker"]["poller_id"] = i + 1

            f = open(broker_name, "w")
            f.write(json.dumps(conf, indent=2))
            f.close()
    else:
        f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "w")
        f.write(config[name].format(broker_id, broker_name,
                DB_HOST, DB_PORT, DB_USER, DB_PASS, DB_NAME_STORAGE, VAR_ROOT))
        f.close()


def change_broker_tcp_output_to_grpc(name: str):
    def output_to_grpc(conf):
        output_dict = conf["centreonBroker"]["output"]
        for i, v in enumerate(output_dict):
            if v["type"] == "ipv4":
                v["type"] = "grpc"
    _apply_conf(name, output_to_grpc)

def add_path_to_rrd_output(name: str, path: str):
    def rrd_output(conf):
        output_dict = conf["centreonBroker"]["output"]
        for i, v in enumerate(output_dict):
            if v["type"] == "rrd":
                v["path"] = path
    _apply_conf(name, rrd_output)


def add_path_to_rrd_output(name: str, path: str):
    def rrd_output(conf):
        output_dict = conf["centreonBroker"]["output"]
        for i, v in enumerate(output_dict):
            if v["type"] == "rrd":
                v["path"] = path
    _apply_conf(name, rrd_output)


def add_path_to_rrd_output(name: str, path: str):
    def rrd_output(conf):
        output_dict = conf["centreonBroker"]["output"]
        for i, v in enumerate(output_dict):
            if v["type"] == "rrd":
                v["path"] = path
    _apply_conf(name, rrd_output)


def change_broker_tcp_input_to_grpc(name: str):
    def input_to_grpc(conf):
        input_dict = conf["centreonBroker"]["input"]
        for i, v in enumerate(input_dict):
            if v["type"] == "ipv4":
                v["type"] = "grpc"
    _apply_conf(name, input_to_grpc)


def add_broker_crypto(json_dict, add_cert: bool, only_ca_cert: bool):
    json_dict["encryption"] = "yes"
    if (add_cert):
        json_dict["ca_certificate"] = "/tmp/ca_1234.crt"
        if (only_ca_cert == False):
            json_dict["public_cert"] = "/tmp/server_1234.crt"
            json_dict["private_key"] = "/tmp/server_1234.key"


def add_broker_tcp_input_grpc_crypto(name: str, add_cert: bool, reversed: bool):
    def crypto_modifier(conf):
        input_dict = conf["centreonBroker"]["input"]
        for i, v in enumerate(input_dict):
            if v["type"] == "grpc":
                add_broker_crypto(v, add_cert, reversed)
    _apply_conf(name, crypto_modifier)


def add_broker_tcp_output_grpc_crypto(name: str, add_cert: bool, reversed: bool):
    def crypto_modifier(conf):
        input_dict = conf["centreonBroker"]["output"]
        for i, v in enumerate(input_dict):
            if v["type"] == "grpc":
                add_broker_crypto(v, add_cert, not reversed)
    _apply_conf(name, crypto_modifier)


def add_host_to_broker_output(name: str, output_name: str, host_ip: str):
    def modifier(conf):
        input_dict = conf["centreonBroker"]["output"]
        for i, v in enumerate(input_dict):
            if (v["name"] == output_name):
                v["host"] = host_ip
    _apply_conf(name, modifier)


def add_host_to_broker_input(name: str, input_name: str, host_ip: str):
    def modifier(conf):
        input_dict = conf["centreonBroker"]["input"]
        for i, v in enumerate(input_dict):
            if (v["name"] == input_name):
                v["host"] = host_ip
    _apply_conf(name, modifier)


def remove_host_from_broker_output(name: str, output_name: str):
    def modifier(conf):
        input_dict = conf["centreonBroker"]["output"]
        for i, v in enumerate(input_dict):
            if (v["name"] == output_name):
                v.pop("host")
    _apply_conf(name, modifier)


def remove_host_from_broker_input(name: str, input_name: str):
    def modifier(conf):
        input_dict = conf["centreonBroker"]["input"]
        for i, v in enumerate(input_dict):
            if (v["name"] == input_name):
                v.pop("host")
    _apply_conf(name, modifier)


def change_broker_compression_output(config_name: str, output_name: str, compression_value: str):
    def compression_modifier(conf):
        output_dict = conf["centreonBroker"]["output"]
        for i, v in enumerate(output_dict):
            if (v["name"] == output_name):
                v["compression"] = compression_value
    _apply_conf(config_name, compression_modifier)


def change_broker_compression_input(config_name: str, input_name: str, compression_value: str):
    def compression_modifier(conf):
        input_dict = conf["centreonBroker"]["input"]
        for i, v in enumerate(input_dict):
            if (v["name"] == input_name):
                v["compression"] = compression_value
    _apply_conf(config_name, compression_modifier)


def config_broker_sql_output(name, output):
    if name == 'central':
        filename = "central-broker.json"
    elif name.startswith('module'):
        filename = "central-{}.json".format(name)
    else:
        filename = "central-rrd.json"

    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "r")
    buf = f.read()
    f.close()
    conf = json.loads(buf)
    output_dict = conf["centreonBroker"]["output"]
    for i, v in enumerate(output_dict):
        if v["type"] == "sql" or v["type"] == "storage" or v["type"] == "unified_sql":
            output_dict.pop(i)
    if output == 'unified_sql':
        output_dict.append({
            "name": "central-broker-unified-sql",
            "db_type": "mysql",
            "db_host": DB_HOST,
            "db_port": DB_PORT,
            "db_user": DB_USER,
            "db_password": DB_PASS,
            "db_name": DB_NAME_STORAGE,
            "interval": "60",
            "length": "15552000",
            "queries_per_transaction": "20000",
            "connections_count": "4",
            "read_timeout": "60",
            "buffering_timeout": "0",
            "retry_interval": "60",
            "check_replication": "no",
            "type": "unified_sql",
            "store_in_data_bin": "yes",
            "insert_in_index_data": "1"
        })
    elif output == 'sql/perfdata':
        output_dict.append({
            "name": "central-broker-master-sql",
            "db_type": "mysql",
            "retry_interval": "5",
            "buffering_timeout": "0",
            "db_host": DB_HOST,
            "db_port": DB_PORT,
            "db_user": DB_USER,
            "db_password": DB_PASS,
            "db_name": DB_NAME_STORAGE,
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
            "db_host": DB_HOST,
            "db_port": DB_PORT,
            "db_user": DB_USER,
            "db_password": DB_PASS,
            "db_name": DB_NAME_STORAGE,
            "queries_per_transaction": "1000",
            "read_timeout": "1",
            "check_replication": "no",
            "store_in_data_bin": "yes",
            "connections_count": "3",
            "insert_in_index_data": "1",
            "type": "storage"
        })
    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "w")
    f.write(json.dumps(conf, indent=2))
    f.close()


def broker_config_clear_outputs_except(name, ex: list):
    if name == 'central':
        filename = "central-broker.json"
    elif name.startswith('module'):
        filename = "central-{}.json".format(name)
    else:
        filename = "central-rrd.json"

    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "r")
    buf = f.read()
    f.close()
    conf = json.loads(buf)
    output_dict = conf["centreonBroker"]["output"]
    for i, v in enumerate(output_dict):
        if v["type"] not in ex:
            output_dict.pop(i)

    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "w")
    f.write(json.dumps(conf, indent=2))
    f.close()


def broker_config_add_item(name, key, value):
    if name == 'central':
        filename = "central-broker.json"
    elif name == 'rrd':
        filename = "central-rrd.json"
    elif name.startswith('module'):
        filename = "central-{}.json".format(name)

    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "r")
    buf = f.read()
    f.close()
    conf = json.loads(buf)
    conf["centreonBroker"][key] = value
    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "w")
    f.write(json.dumps(conf, indent=2))
    f.close()


def broker_config_remove_item(name, key):
    if name == 'central':
        filename = "central-broker.json"
    elif name == 'rrd':
        filename = "central-rrd.json"
    elif name.startswith('module'):
        filename = "central-{}.json".format(name)

    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "r")
    buf = f.read()
    f.close()
    conf = json.loads(buf)
    conf["centreonBroker"].pop(key)
    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "w")
    f.write(json.dumps(conf, indent=2))
    f.close()


def broker_config_add_lua_output(name, output, luafile):
    if name == 'central':
        filename = "central-broker.json"
    elif name.startswith('module'):
        filename = "central-{}.json".format(name)
    else:
        filename = "central-rrd.json"

    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "r")
    buf = f.read()
    f.close()
    conf = json.loads(buf)
    output_dict = conf["centreonBroker"]["output"]
    output_dict.append({
        "name": output,
        "path": luafile,
        "type": "lua"
    })
    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "w")
    f.write(json.dumps(conf, indent=2))
    f.close()


def broker_config_output_set(name, output, key, value):
    if name == 'central':
        filename = "central-broker.json"
    elif name.startswith('module'):
        filename = "central-{}.json".format(name)
    else:
        filename = "central-rrd.json"
    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "r")
    buf = f.read()
    f.close()
    conf = json.loads(buf)
    output_dict = [elem for i, elem in enumerate(
        conf["centreonBroker"]["output"]) if elem["name"] == output][0]
    output_dict[key] = value
    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "w")
    f.write(json.dumps(conf, indent=2))
    f.close()


def broker_config_output_set_json(name, output, key, value):
    if name == 'central':
        filename = "central-broker.json"
    elif name.startswith('module'):
        filename = "central-{}.json".format(name)
    else:
        filename = "central-rrd.json"
    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "r")
    buf = f.read()
    f.close()
    conf = json.loads(buf)
    output_dict = [elem for i, elem in enumerate(
        conf["centreonBroker"]["output"]) if elem["name"] == output][0]
    j = json.loads(value)
    output_dict[key] = j
    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "w")
    f.write(json.dumps(conf, indent=2))
    f.close()


def broker_config_output_remove(name, output, key):
    if name == 'central':
        filename = "central-broker.json"
    elif name.startswith('module'):
        filename = "central-{}.json".format(name)
    else:
        filename = "central-rrd.json"
    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "r")
    buf = f.read()
    f.close()
    conf = json.loads(buf)
    output_dict = [elem for i, elem in enumerate(
        conf["centreonBroker"]["output"]) if elem["name"] == output][0]
    if key in output_dict:
        output_dict.pop(key)
    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "w")
    f.write(json.dumps(conf, indent=2))
    f.close()


def broker_config_input_set(name, inp, key, value):
    if name == 'central':
        filename = "central-broker.json"
    elif name.startswith('module'):
        filename = "central-{}.json".format(name)
    else:
        filename = "central-rrd.json"
    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "r")
    buf = f.read()
    f.close()
    conf = json.loads(buf)
    input_dict = [elem for i, elem in enumerate(
        conf["centreonBroker"]["input"]) if elem["name"] == inp][0]
    input_dict[key] = value
    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "w")
    f.write(json.dumps(conf, indent=2))
    f.close()


def broker_config_input_remove(name, inp, key):
    if name == 'central':
        filename = "central-broker.json"
    elif name.startswith('module'):
        filename = "central-{}.json".format(name)
    else:
        filename = "central-rrd.json"
    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "r")
    buf = f.read()
    f.close()
    conf = json.loads(buf)
    input_dict = [elem for i, elem in enumerate(
        conf["centreonBroker"]["input"]) if elem["name"] == inp][0]
    if key in input_dict:
        input_dict.pop(key)
    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "w")
    f.write(json.dumps(conf, indent=2))
    f.close()


def broker_config_log(name, key, value):
    if name == 'central':
        filename = "central-broker.json"
    elif name.startswith('module'):
        filename = "central-{}.json".format(name)
    else:
        filename = "central-rrd.json"
    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "r")
    buf = f.read()
    f.close()
    conf = json.loads(buf)
    loggers = conf["centreonBroker"]["log"]["loggers"]
    loggers[key] = value
    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "w")
    f.write(json.dumps(conf, indent=2))
    f.close()


def broker_config_flush_log(name, value):
    if name == 'central':
        filename = "central-broker.json"
    elif name.startswith('module'):
        filename = "central-{}.json".format(name)
    else:
        filename = "central-rrd.json"
    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "r")
    buf = f.read()
    f.close()
    conf = json.loads(buf)
    log = conf["centreonBroker"]["log"]
    log["flush_period"] = value
    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "w")
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
        while retry and time.time() < limit:
            retry = False
            f = open(VAR_ROOT + "/lib/centreon-broker/{}".format(filename), "r")
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
        while retry and time.time() < limit:
            retry = False
            f = open(VAR_ROOT + "/lib/centreon-broker/{}".format(filename), "r")
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


##
# @brief Gets count indexes that does not exist in index_data.
#
# @param count:int The number of indexes to get.
#
# @return a list of index ids.
#
def get_not_existing_indexes(count: int):
    # Connect to the database
    connection = pymysql.connect(host=DB_HOST,
                                 user=DB_USER,
                                 password=DB_PASS,
                                 database=DB_NAME_STORAGE,
                                 charset='utf8mb4',
                                 cursorclass=pymysql.cursors.DictCursor)

    ids_db = []
    with connection:
        with connection.cursor() as cursor:
            # Read a single record
            sql = "SELECT `id` FROM `index_data`"
            cursor.execute(sql)
            result = cursor.fetchall()
            index = 1
            for r in result:
                while int(r['id']) > index:
                    ids_db.append(index)
                    if len(ids_db) == count:
                        return ids_db
                    index += 1
                index += 1
            while len(ids_db) < count:
                ids_db.append(index)
                index += 1

    return ids_db


##
# @brief Gets count indexes from available ones.
#
# @param count:int The number of indexes to get.
#
# @return a list of index ids.
#
def get_indexes_to_delete(count: int):
    files = [os.path.basename(x) for x in glob.glob(
        VAR_ROOT + "/lib/centreon/metrics/[0-9]*.rrd")]
    ids = [int(f.split(".")[0]) for f in files]

    # Connect to the database
    connection = pymysql.connect(host=DB_HOST,
                                 user=DB_USER,
                                 password=DB_PASS,
                                 database=DB_NAME_STORAGE,
                                 charset='utf8mb4',
                                 cursorclass=pymysql.cursors.DictCursor)

    ids_db = set()
    with connection:
        with connection.cursor() as cursor:
            # Read a single record
            sql = "SELECT `metric_id`,`index_id` FROM `metrics`"
            cursor.execute(sql)
            result = cursor.fetchall()
            for r in result:
                if int(r['metric_id']) in ids:
                    ids_db.add(int(r['index_id']))
                if len(ids_db) == count:
                    retval = list(ids_db)
                    retval.sort()
                    return retval

    retval = list(ids_db)
    retval.sort()
    return retval


##
# @brief Gets count metrics that does not exist.
#
# @param count:int The number of metrics to get.
#
# @return a list of metric ids.
#
def get_not_existing_metrics(count: int):
    files = [os.path.basename(x) for x in glob.glob(
        VAR_ROOT + "/lib/centreon/metrics/[0-9]*.rrd")]
    ids = [int(f.split(".")[0]) for f in files]

    # Connect to the database
    connection = pymysql.connect(host=DB_HOST,
                                 user=DB_USER,
                                 password=DB_PASS,
                                 database=DB_NAME_STORAGE,
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
    index = 1
    retval = []
    while len(retval) < count:
        if not index in inter:
            retval.append(index)
        index += 1
    return retval


##
# @brief Gets count metrics from available ones.
#
# @param count:int The number of metrics to get.
#
# @return a list of metric ids.
#
def get_metrics_to_delete(count: int):
    files = [os.path.basename(x) for x in glob.glob(
        VAR_ROOT + "/lib/centreon/metrics/[0-9]*.rrd")]
    ids = [int(f.split(".")[0]) for f in files]

    # Connect to the database
    connection = pymysql.connect(host=DB_HOST,
                                 user=DB_USER,
                                 password=DB_PASS,
                                 database=DB_NAME_STORAGE,
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
    return inter[:count]

##
# @brief creat metrics from available ones.
#
# @param count:int The number of metrics to create.
#


def create_metrics(count: int):
    files = [os.path.basename(x) for x in glob.glob(
        VAR_ROOT + "/lib/centreon/metrics/[0-9]*.rrd")]
    ids = [int(f.split(".")[0]) for f in files]

    # Connect to the database
    connection = pymysql.connect(host=DB_HOST,
                                 user=DB_USER,
                                 password=DB_PASS,
                                 database=DB_NAME_STORAGE,
                                 charset='utf8mb4',
                                 cursorclass=pymysql.cursors.DictCursor)

    with connection:
        with connection.cursor() as cursor:
            # Read a single record
            sql = "SELECT `metric_id` FROM `metrics`"
            cursor.execute(sql)
            result = cursor.fetchall()
            ids_db = [r['metric_id'] for r in result]
            if list(set(ids) & set(ids_db)) == []:
                sql = "DELETE FROM metrics"
                cursor.execute(sql)
                connection.commit()
                sql = "SELECT `id` FROM `index_data`"
                cursor.execute(sql)
                result = cursor.fetchall()
                ids_index = [r['id'] for r in result]
                if ids_index == []:
                    sql = "INSERT INTO index_data (host_id, service_id) VALUES (1, 1)"
                    cursor.execute(sql)
                    ids_index.append(cursor.lastrowid)
                for c in range(count):
                    sql = "INSERT INTO metrics (index_id,metric_name,unit_name,warn,warn_low,warn_threshold_mode,crit,crit_low,crit_threshold_mode,min,max,current_value,data_source_type) VALUES ('{}','metric_{}','unit_{}','10','1','0','1','1','0','0','100','25','0')".format(
                        ids_index[0], c, c)
                    cursor.execute(sql)
                    ids_metric = cursor.lastrowid
                    shutil.copy(VAR_ROOT + "/lib/centreon/metrics/tmpl_15552000_300_0.rrd",
                                VAR_ROOT + "/lib/centreon/metrics/{}.rrd".format(ids_metric))
                    logger.console("create metric file {}".format(ids_metric))
                connection.commit()


def run_reverse_bam(duration, interval):
    subp.Popen("broker/map_client.py {:f}".format(interval),
               shell=True, stdout=subp.PIPE, stdin=subp.PIPE)
    time.sleep(duration)
    getoutput("kill -9 $(ps aux | grep map_client.py | awk '{print $2}')")


##
# @brief Get count indexes that are available to rebuild them.
#
# @param count is the number of indexes to get.
#
# @return a list of indexes
def get_indexes_to_rebuild(count: int):
    files = [os.path.basename(x) for x in glob.glob(
        VAR_ROOT + "/lib/centreon/metrics/[0-9]*.rrd")]
    ids = [int(f.split(".")[0]) for f in files]

    # Connect to the database
    connection = pymysql.connect(host=DB_HOST,
                                 user=DB_USER,
                                 password=DB_PASS,
                                 database=DB_NAME_STORAGE,
                                 charset='utf8mb4',
                                 cursorclass=pymysql.cursors.DictCursor)
    retval = []
    with connection:
        with connection.cursor() as cursor:
            # Read a single record
            sql = "SELECT `metric_id`,`index_id` FROM `metrics`"
            cursor.execute(sql)
            result = cursor.fetchall()
            for r in result:
                if int(r['metric_id']) in ids:
                    logger.console(
                        "building data for metric {}".format(r['metric_id']))
                    start = int(time.time()) - 24 * 60 * 60 * 30
                    # We go back to 30 days with steps of 5 mn
                    value1 = int(r['metric_id'])
                    value2 = 0
                    value = value1
                    cursor.execute("DELETE FROM data_bin WHERE id_metric={} AND ctime >= {}".format(
                        r['metric_id'], start))
                    for i in range(0, 24 * 60 * 60 * 30, 60 * 5):
                        cursor.execute("INSERT INTO data_bin (id_metric, ctime, value, status) VALUES ({},{},{},'0')".format(
                            r['metric_id'], start + i, value))
                        if value == value1:
                            value = value2
                        else:
                            value = value1
                    connection.commit()
                    retval.append(int(r['index_id']))

                if len(retval) == count:
                    return retval

    # if the loop is already and retval length is not sufficiently long, we
    # still return what we get.
    return retval


##
# @brief Returns metric ids matching the given indexes.
#
# @param indexes a list of indexes from index_data
#
# @return a list of metric ids.
def get_metrics_matching_indexes(indexes):
    # Connect to the database
    connection = pymysql.connect(host=DB_HOST,
                                 user=DB_USER,
                                 password=DB_PASS,
                                 database=DB_NAME_STORAGE,
                                 charset='utf8mb4',
                                 cursorclass=pymysql.cursors.DictCursor)
    with connection:
        with connection.cursor() as cursor:
            # Read a single record
            sql = "SELECT `metric_id` FROM `metrics` WHERE `index_id` IN ({})".format(
                ','.join(map(str, indexes)))
            cursor.execute(sql)
            result = cursor.fetchall()
            retval = [int(r['metric_id']) for r in result]
            return retval


##
# @brief send a gRPC command to remove graphs (by indexes or by metrics)
#
# @param port the gRPC port to use to send the command
# @param indexes a list of indexes
# @param metrics a list of metrics
#
def remove_graphs(port, indexes, metrics, timeout=10):
    limit = time.time() + timeout
    while time.time() < limit:
        time.sleep(1)
        with grpc.insecure_channel("127.0.0.1:{}".format(port)) as channel:
            stub = broker_pb2_grpc.BrokerStub(channel)
            trm = broker_pb2.ToRemove()
            trm.index_ids.extend(indexes)
            trm.metric_ids.extend(metrics)
            try:
                stub.RemoveGraphs(trm)
                break
            except:
                logger.console("gRPC server not ready")


##
# @brief send a query to the db to remove graphs (by indexes or by metrics)
#
# @param indexes a list of indexes
# @param metrics a list of metrics
#
def remove_graphs_from_db(indexes, metrics, timeout=10):
    logger.console("rem1")
    connection = pymysql.connect(host=DB_HOST,
                                 user=DB_USER,
                                 password=DB_PASS,
                                 database=DB_NAME_STORAGE,
                                 charset='utf8mb4',
                                 cursorclass=pymysql.cursors.DictCursor)

    ids_db = []
    with connection:
        with connection.cursor() as cursor:
            if len(indexes) > 0:
                str_indexes = [str(i) for i in indexes]
                sql = "UPDATE index_data SET to_delete=1 WHERE id in ({})".format(
                    ",".join(str_indexes))
                logger.console(sql)
                cursor.execute(sql)
            if len(metrics) > 0:
                str_metrics = [str(i) for i in metrics]
                sql = "UPDATE metrics SET to_delete=1 WHERE metric_id in ({})".format(
                    ",".join(str_metrics))
                logger.console(sql)
                cursor.execute(sql)
            connection.commit()


##
# @brief Execute the gRPC command RebuildRRDGraphs()
#
# @param port The port to use with gRPC.
# @param indexes The list of indexes corresponding to metrics to rebuild.
#
def rebuild_rrd_graphs(port, indexes, timeout: int = TIMEOUT):
    logger.console("start gRPC server")
    limit = time.time() + timeout
    while time.time() < limit:
        logger.console("gRPC server on while")
        time.sleep(1)
        with grpc.insecure_channel("127.0.0.1:{}".format(port)) as channel:
            stub = broker_pb2_grpc.BrokerStub(channel)
            k = 0.0
            idx = broker_pb2.IndexIds()
            idx.index_ids.extend(indexes)
            try:
                stub.RebuildRRDGraphs(idx)
                break
            except:
                logger.console("gRPC server not ready")


##
# @brief Send a query to the db to rebuild graphs
#
# @param indexes The list of indexes corresponding to metrics to rebuild.
#
def rebuild_rrd_graphs_from_db(indexes, timeout: int = TIMEOUT):
    connection = pymysql.connect(host=DB_HOST,
                                 user=DB_USER,
                                 password=DB_PASS,
                                 database=DB_NAME_STORAGE,
                                 charset='utf8mb4',
                                 cursorclass=pymysql.cursors.DictCursor)

    ids_db = []
    with connection:
        with connection.cursor() as cursor:
            if len(indexes) > 0:
                sql = "UPDATE index_data SET must_be_rebuild='1' WHERE id in ({})".format(
                    ",".join(map(str, indexes)))
                logger.console(sql)
                cursor.execute(sql)
                connection.commit()


##
# @brief Compare the average value for an RRD metric on the last 30 days with
# a value.
#
# @param metric The metric id
# @param float The value to compare with.
#
# @return A boolean.
def compare_rrd_average_value(metric, value: float):
    res = getoutput("rrdtool graph dummy --start=end-30d --end=now"
                    " DEF:x=" + VAR_ROOT +
                    "/lib/centreon/metrics/{}.rrd:value:AVERAGE VDEF:xa=x,AVERAGE PRINT:xa:%lf"
                    .format(metric))
    lst = res.split('\n')
    if len(lst) >= 2:
        res = float(lst[1].replace(',', '.'))
        err = abs(res - float(value)) / float(value)
        logger.console(
            f"expected value: {value} - result value: {res} - err: {err}")
        return err < 0.01
    else:
        logger.console(
            "It was impossible to get the average value from the file " + VAR_ROOT + "/lib/centreon/metrics/{}.rrd from the last 30 days".format(metric))
        return True


##
# @brief Call the GetSqlManagerStats function by gRPC and checks there are
# count active connections.
#
# @param count The expected number of active connections.
#
# @return A boolean.
def check_sql_connections_count_with_grpc(port, count, timeout=TIMEOUT):
    limit = time.time() + timeout
    while time.time() < limit:
        time.sleep(1)
        with grpc.insecure_channel("127.0.0.1:{}".format(port)) as channel:
            stub = broker_pb2_grpc.BrokerStub(channel)
            try:
                res = stub.GetSqlManagerStats(empty_pb2.Empty())
                if len(res.connections) < count:
                    continue
                for c in res.connections:
                    if c.down_since:
                        continue
                return True
            except:
                logger.console("gRPC server not ready")
    return False


##
# @brief Call the GetSqlManagerStats function by gRPC and checks there are
# count active connections.
#
# @param count The expected number of active connections.
#
# @return A boolean.
def check_all_sql_connections_down_with_grpc(port, timeout=TIMEOUT):
    limit = time.time() + timeout
    while time.time() < limit:
        time.sleep(1)
        with grpc.insecure_channel("127.0.0.1:{}".format(port)) as channel:
            stub = broker_pb2_grpc.BrokerStub(channel)
            try:
                res = stub.GetSqlManagerStats(empty_pb2.Empty())
                for c in res.connections:
                    if c.up_since:
                        continue
                return True
            except:
                logger.console("gRPC server not ready")
    return False


##
# @brief Add the bam configuration to broker.
#
# @param name The broker name to consider.
#
def add_bam_config_to_broker(name):
    if name == 'central':
        filename = "central-broker.json"
    elif name.startswith('module'):
        filename = "central-{}.json".format(name)
    else:
        filename = "central-rrd.json"

    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "r")
    buf = f.read()
    f.close()
    conf = json.loads(buf)
    output_dict = conf["centreonBroker"]["output"]
    output_dict.append({
        "name": "centreon-bam-monitoring",
        "cache": "yes",
        "check_replication": "no",
        "command_file": VAR_ROOT + "/lib/centreon-engine/config0/rw/centengine.cmd",
        "db_host": DB_HOST,
        "db_name": DB_NAME_CONF,
        "db_password": DB_PASS,
        "db_port": DB_PORT,
        "db_type": "mysql",
        "db_user": DB_USER,
        "queries_per_transaction": "0",
        "storage_db_name": DB_NAME_STORAGE,
        "type": "bam"
    })
    output_dict.append({
        "name": "centreon-bam-reporting",
        "filters": {
            "category": [
                "bam"
            ]
        },
        "check_replication": "no",
        "db_host": DB_HOST,
        "db_name": DB_NAME_STORAGE,
        "db_password": DB_PASS,
        "db_port": DB_PORT,
        "db_type": "mysql",
        "db_user": DB_USER,
        "queries_per_transaction": "0",
        "type": "bam_bi"
    })
    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "w")
    f.write(json.dumps(conf, indent=2))
    f.close()


def check_poller_disabled_in_database(poller_id: int, timeout: int):
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    "SELECT DISTINCT enabled FROM hosts WHERE instance_id = {} AND enabled > 0".format(poller_id))
                result = cursor.fetchall()
                if len(result) == 0:
                    return True
        time.sleep(5)
    return False


def check_poller_enabled_in_database(poller_id: int, timeout: int):
    limit = time.time() + timeout
    while time.time() < limit:
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)

        with connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    "SELECT DISTINCT enabled FROM hosts WHERE instance_id = {} AND enabled > 0".format(poller_id))
                result = cursor.fetchall()
                if len(result) > 0:
                    return True
        time.sleep(5)
    return False
