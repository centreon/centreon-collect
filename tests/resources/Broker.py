import signal
from os import setsid
from os import makedirs
from os.path import exists, dirname
import pymysql.cursors
import time
import re
import shutil
import psutil
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
from google.protobuf.json_format import MessageToJson
from robot.libraries.BuiltIn import BuiltIn
from Common import DB_NAME_STORAGE, DB_NAME_CONF, DB_USER, DB_PASS, DB_HOST, DB_PORT, VAR_ROOT, ETC_ROOT, TESTS_PARAMS

TIMEOUT = 30


config = {
    "central": """{{
    "centreonBroker": {{
        "broker_id": {0},
        "broker_name": "{1}",
        "poller_id": 1,
        "poller_name": "Central",
        "module_directory": "/usr/share/centreon/lib/centreon-broker",
        "log_timestamp": "yes",
        "log_thread_id": "no",
        "event_queue_max_size": 100000,
        "command_file": "{7}/lib/centreon-broker/command.sock",
        "cache_directory": "{7}/lib/centreon-broker",
        "log": {{
            "log_pid": "yes",
            "log_source": "no",
            "flush_period": 0,
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
                "functions": "trace",
                "grpc": "debug"
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
            "log_pid": "yes",
            "flush_period": 0,
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
                "grpc": "debug",
                "neb": "debug"
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
            "log_pid": "yes",
            "log_source": "no",
            "flush_period": 0,
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


def config_broker(name: str, poller_inst: int = 1):
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
        if not exists(f"{VAR_ROOT}/lib/centreon/metrics/"):
            makedirs(f"{VAR_ROOT}/lib/centreon/metrics/")
        if not exists(f"{VAR_ROOT}/lib/centreon/status/"):
            makedirs(f"{VAR_ROOT}/lib/centreon/status/")
        if not exists(f"{VAR_ROOT}/lib/centreon/metrics/tmpl_15552000_300_0.rrd"):
            getoutput(
                f"rrdcreate {VAR_ROOT}/lib/centreon/metrics/tmpl_15552000_300_0.rrd DS:value:ABSOLUTE:3000:U:U RRA:AVERAGE:0.5:1:864000")
        broker_id = 2
        broker_name = "central-rrd-master"
        filename = "central-rrd.json"

    default_bbdo_version = TESTS_PARAMS.get("default_bbdo_version")
    default_transport = TESTS_PARAMS.get("default_transport")
    if name == 'module':
        for i in range(poller_inst):
            broker_name = f"{ETC_ROOT}/centreon-broker/central-module{i}.json"
            buf = config[name].format(
                broker_id, f"central-module-master{i}", "", "", "", "", "", VAR_ROOT)
            conf = json.loads(buf)
            conf["centreonBroker"]["poller_name"] = f"Poller{i}"
            conf["centreonBroker"]["poller_id"] = i + 1

            f = open(broker_name, "w")
            f.write(json.dumps(conf, indent=2))
            f.close()
            if default_bbdo_version is not None:
                broker_config_add_item(
                    f"{name}{i}", "bbdo_version", default_bbdo_version)
            if default_transport == "grpc":
                config_broker_bbdo_output(
                    f"{name}{i}", "bbdo_client", "5669", "grpc", "localhost")

    else:
        f = open(f"{ETC_ROOT}/centreon-broker/{filename}", "w")
        f.write(config[name].format(broker_id, broker_name,
                DB_HOST, DB_PORT, DB_USER, DB_PASS, DB_NAME_STORAGE, VAR_ROOT))
        f.close()
        if default_bbdo_version is not None:
            if default_bbdo_version >= "3.0.0" and (name == "central" or name == "central_map"):
                config_broker_sql_output(name, 'unified_sql')
            broker_config_add_item(
                name, "bbdo_version", default_bbdo_version)
        if default_transport == "grpc":
            if name == "central" or name == "central_map":
                config_broker_bbdo_input(
                    name, "bbdo_server", "5669", "grpc")
                config_broker_bbdo_output(
                    name, "bbdo_client", "5670", "grpc", "localhost")
            else:
                config_broker_bbdo_input(
                    name, "bbdo_server", "5670", "grpc")


def change_broker_tcp_output_to_grpc(name: str):
    def output_to_grpc(conf):
        output_dict = conf["centreonBroker"]["output"]
        for i, v in enumerate(output_dict):
            if v["type"] == "ipv4":
                v["type"] = "grpc"
            if "transport_protocol" in v:
                v["transport_protocol"] = "grpc"
    _apply_conf(name, output_to_grpc)


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
            if "transport_protocol" in v:
                v["transport_protocol"] = "grpc"
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


def config_broker_remove_rrd_output(name):
    if name == 'central':
        filename = "central-broker.json"
    elif name.startswith('module'):
        filename = "central-{}.json".format(name)
    else:
        filename = "central-rrd.json"
    conf = {}
    with open(f"{ETC_ROOT}/centreon-broker/{filename}", "r") as f:
        buf = f.read()
        conf = json.loads(buf)
        output_dict = conf["centreonBroker"]["output"]
        for i, v in enumerate(output_dict):
            if "rrd" in v["name"] and v["type"] == "ipv4":
                output_dict.pop(i)
                break

    with open(f"{ETC_ROOT}/centreon-broker/{filename}", "w") as f:
        f.write(json.dumps(conf, indent=2))


def config_broker_bbdo_input(name, stream, port, proto, host=None):
    if stream != "bbdo_server" and stream != "bbdo_client":
        raise Exception(
            "config_broker_bbdo_input_output() function only accepts stream in ('bbdo_server', 'bbdo_client')")
    if stream == "bbdo_client" and host is None:
        raise Exception("A bbdo_client must specify a host to connect to")

    input_name = f"{name}-broker-master-input"
    if name == 'central':
        filename = "central-broker.json"
    elif name.startswith('module'):
        filename = "central-{}.json".format(name)
    else:
        filename = "central-rrd.json"
        input_name = "central-rrd-master-input"
    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "r")
    buf = f.read()
    f.close()
    conf = json.loads(buf)
    io_dict = conf["centreonBroker"]["input"]
    # Cleanup
    for i, v in enumerate(io_dict):
        if (v["type"] == "ipv4" or v["type"] == "grpc" or v["type"] == "bbdo_client" or v["type"] == "bbdo_server") and v["port"] == port:
            io_dict.pop(i)
    stream = {
        "name": input_name,
        "port": f"{port}",
        "transport_protocol": proto,
        "type": stream,
    }
    if host is not None:
        stream["host"] = host
    io_dict.append(stream)
    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "w")
    f.write(json.dumps(conf, indent=2))
    f.close()


def config_broker_bbdo_output(name, stream, port, proto, host=None):
    if stream != "bbdo_server" and stream != "bbdo_client":
        raise Exception(
            "config_broker_bbdo_output() function only accepts stream in ('bbdo_server', 'bbdo_client')")
    if stream == "bbdo_client" and host is None:
        raise Exception("A bbdo_client must specify a host to connect to")

    output_name = f"{name}-broker-master-output"
    if name == 'central':
        filename = "central-broker.json"
        output_name = 'centreon-broker-master-rrd'
    elif name.startswith('module'):
        filename = "central-{}.json".format(name)
        output_name = 'central-module-master-output'
    else:
        filename = "central-rrd.json"
    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "r")
    buf = f.read()
    f.close()
    conf = json.loads(buf)
    io_dict = conf["centreonBroker"]["output"]
    # Cleanup
    for i, v in enumerate(io_dict):
        if (v["type"] == "ipv4" or v["type"] == "grpc" or v["type"] == "bbdo_client" or v["type"] == "bbdo_server") and v["port"] == port:
            io_dict.pop(i)
    stream = {
        "name": f"{output_name}",
        "port": f"{port}",
        "transport_protocol": proto,
        "type": stream,
    }
    if host is not None:
        stream["host"] = host
    io_dict.append(stream)
    f = open(ETC_ROOT + "/centreon-broker/{}".format(filename), "w")
    f.write(json.dumps(conf, indent=2))
    f.close()


def config_broker_sql_output(name, output, queries_per_transaction: int = 20000):
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
    str_queries_per_transaction = str(queries_per_transaction)
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
            "queries_per_transaction": str_queries_per_transaction,
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

    with open(ETC_ROOT + "/centreon-broker/{}".format(filename), "r") as f:
        buf = f.read()
    conf = json.loads(buf)
    output_dict = conf["centreonBroker"]["output"]
    for i, v in enumerate(output_dict):
        if v["type"] not in ex:
            output_dict.pop(i)

    with open(ETC_ROOT + "/centreon-broker/{}".format(filename), "w") as f:
        f.write(json.dumps(conf, indent=2))


def config_broker_victoria_output():
    filename = "central-broker.json"

    with open(ETC_ROOT + "/centreon-broker/{}".format(filename), "r") as f:
        buf = f.read()
    conf = json.loads(buf)
    output_dict = conf["centreonBroker"]["output"]
    for i, v in enumerate(output_dict):
        if v["type"] == "victoria_metrics":
            output_dict.pop(i)
    output_dict.append({
        "name": "victoria_metrics",
        "type": "victoria_metrics",
        "db_host": "localhost",
        "db_port": "8000",
        "db_user": "toto",
        "db_password": "titi",
        "queries_per_transaction": "1",
    })
    with open(ETC_ROOT + "/centreon-broker/{}".format(filename), "w") as f:
        f.write(json.dumps(conf, indent=2))


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
    f = open(f"{ETC_ROOT}/centreon-broker/{filename}", "r")
    buf = f.read()
    f.close()
    conf = json.loads(buf)
    output_dict = [elem for i, elem in enumerate(
        conf["centreonBroker"]["output"]) if elem["name"] == output][0]
    output_dict[key] = value
    with open(f"{ETC_ROOT}/centreon-broker/{filename}", "w") as f:
        f.write(json.dumps(conf, indent=2))


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
    with open(f"{ETC_ROOT}/centreon-broker/{filename}", "r") as f:
        buf = f.read()
    conf = json.loads(buf)
    output_dict = [elem for i, elem in enumerate(
        conf["centreonBroker"]["output"]) if elem["name"] == output][0]
    if key in output_dict:
        output_dict.pop(key)
        if key == "host":
            if output_dict["type"] == "bbdo_client":
                output_dict["type"] = "bbdo_server"
    with open(f"{ETC_ROOT}/centreon-broker/{filename}", "w") as f:
        f.write(json.dumps(conf, indent=2))


def broker_config_input_set(name, inp, key, value):
    if name == 'central':
        filename = "central-broker.json"
    elif name.startswith('module'):
        filename = "central-{}.json".format(name)
    else:
        filename = "central-rrd.json"
    with open(f"{ETC_ROOT}/centreon-broker/{filename}", "r") as f:
        buf = f.read()

    conf = json.loads(buf)
    input_dict = [elem for i, elem in enumerate(
        conf["centreonBroker"]["input"]) if elem["name"] == inp][0]
    input_dict[key] = value
    if key == "host" and input_dict["type"] == "bbdo_server":
        input_dict["type"] = "bbdo_client"
    with open(f"{ETC_ROOT}/centreon-broker/{filename}", "w") as f:
        f.write(json.dumps(conf, indent=2))


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


def broker_config_source_log(name, value):
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
    log["log_source"] = value
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
            f = open(f"{VAR_ROOT}/lib/centreon-broker/{filename}", "r")
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
            with open(f"{VAR_ROOT}/lib/centreon-broker/{filename}", "r") as f:
                buf = f.read()
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


def delete_all_rrd_metrics():
    """! remove all rrd metrics files
    """
    with os.scandir(VAR_ROOT + "/lib/centreon/metrics/") as it:
        for entry in it:
            if entry.is_file():
                os.remove(entry.path)


def check_rrd_info(metric_id: int, key: str, value, timeout: int = 60):
    """!  execute rrdtool info and check one value of the returned informations
    @param metric_id
    @param key key to search in the rrdtool info result
    @param value value to search in the rrdtool info result fot key
    @param timeout  timeout for metric file creation
    @return True if key = value found
    """

    limit = time.time() + timeout
    while time.time() < limit:
        res = getoutput(
            f"rrdtool info {VAR_ROOT}/lib/centreon/metrics/{metric_id}.rrd")
        escaped_key = key.replace("[", "\\[").replace("]", "\\]")
        line_search = re.compile(
            f"{escaped_key}\s*=\s*{value}")
        for line in res.splitlines():
            if (line_search.match(line)):
                return True
        time.sleep(5)
    return False


def get_metrics_for_service(service_id: int, metric_name: str = "%", timeout: int = 60):
    """! scan data base every 5s to extract metric ids for a service

    @param service_id id of the service
    @param timeout  timeout in second
    @return array of metric ids
    """
    limit = time.time() + timeout

    select_request = f"SELECT metric_id FROM metrics JOIN index_data ON index_id=id WHERE service_id={service_id} and metric_name like '{metric_name}'"
    while time.time() < limit:
        # Connect to the database
        connection = pymysql.connect(host=DB_HOST,
                                     user=DB_USER,
                                     password=DB_PASS,
                                     database=DB_NAME_STORAGE,
                                     charset='utf8mb4',
                                     cursorclass=pymysql.cursors.DictCursor)
        with connection:
            with connection.cursor() as cursor:
                cursor.execute(select_request)
                result = cursor.fetchall()
                metric_array = [r['metric_id'] for r in result]
                if len(metric_array) > 0:
                    logger.console(
                        f"metrics {metric_array} found for service {service_id}")
                    return metric_array
                time.sleep(10)
    logger.console(f"no metric found for service_id={service_id}")
    return None


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
    pro = subp.Popen("broker/map_client.py {:f}".format(interval),
                     shell=True, stdout=subp.PIPE, stdin=subp.PIPE, preexec_fn=setsid)
    time.sleep(duration)
    os.killpg(os.getpgid(pro.pid), signal.SIGKILL)


def start_map():
    global map_process
    map_process = subp.Popen("broker/map_client_types.py",
                             shell=True, stdout=subp.DEVNULL, stdin=subp.DEVNULL)


def clear_map_logs():
    with open('/tmp/map-output.log', 'w') as f:
        f.write("")


def check_map_output(categories_str, expected_events, timeout: int = TIMEOUT):
    retval = False
    limit = time.time() + timeout
    while time.time() < limit:
        output = ""
        try:
            with open('/tmp/map-output.log', 'r') as f:
                output = f.readlines()
        except FileNotFoundError:
            time.sleep(5)
            continue

        categories = list(map(int, categories_str))
        r = re.compile(r"^type: ([0-9]*)'?$")
        cat = {}
        expected = list(map(int, expected_events))
        lines = list(filter(r.match, output))
        retval = True
        for o in lines:
            m = r.match(o)
            elem = int(m.group(1))
            if elem in expected:
                expected.remove(elem)
            c = elem >> 16
            if c in categories:
                cat[c] = 1
            elif c != 2:
                logger.console(f"Category {c} not expected")
                retval = False
                break
        if retval and len(categories) != len(cat):
            logger.console(
                f"There are {len(categories)} categories expected whereas {len(cat)} are sent to map")
            retval = False

        if retval and len(expected) > 0:
            logger.console(f"Events of types {str(expected)} not sent")
            retval = False

        if retval:
            break
        time.sleep(5)

    return retval


def get_map_output():
    global map_process
    return map_process.communicate()[0]


def stop_map():
    for proc in psutil.process_iter():
        if 'map_client_type' in proc.name():
            logger.console(
                f"process '{proc.name()}' containing map_client_type found: stopping it")
            proc.terminate()
            try:
                logger.console("Waiting for 30s map_client_type to stop")
                proc.wait(30)
            except:
                logger.console("map_client_type don't want to stop => kill")
                proc.kill()

    for proc in psutil.process_iter():
        if 'map_client_type' in proc.name():
            logger.console(f"process '{proc.name()}' still alive")
            logger.console("map_client_type don't want to stop => kill")
            proc.kill()

    logger.console("map_client_type stopped")


##
# @brief Get count indexes that are available to rebuild them.
#
# @param count is the number of indexes to get.
#
# @return a list of indexes
def get_indexes_to_rebuild(count: int, nb_day=180):
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
                    index_id = int(r['index_id'])
                    logger.console(
                        "building data for metric {} index_id {}".format(r['metric_id'], index_id))
                    # We go back to 180 days with steps of 5 mn
                    start = int(time.time()/86400)*86400 - \
                        24 * 60 * 60 * nb_day
                    value = int(r['metric_id']) // 2
                    status_value = index_id % 3
                    cursor.execute("DELETE FROM data_bin WHERE id_metric={} AND ctime >= {}".format(
                        r['metric_id'], start))
                    # We set the value to a constant on 180 days
                    for i in range(0, 24 * 60 * 60 * nb_day, 60 * 5):
                        cursor.execute("INSERT INTO data_bin (id_metric, ctime, value, status) VALUES ({},{},{},'{}')".format(
                            r['metric_id'], start + i, value, status_value))
                    connection.commit()
                    retval.append(index_id)

                if len(retval) == count:
                    return retval

    # if the loop is already and retval length is not sufficiently long, we
    # still return what we get.
    return retval


##
# @brief add a value at the mid of the first day of each metric
#
#
# @return a list of indexes of pair <time of oldest value>, <metric id>
def add_duplicate_metrics():
    connection = pymysql.connect(host=DB_HOST,
                                 user=DB_USER,
                                 password=DB_PASS,
                                 database=DB_NAME_STORAGE,
                                 charset='utf8mb4',
                                 cursorclass=pymysql.cursors.DictCursor)
    retval = []
    with connection:
        with connection.cursor() as cursor:
            sql = "SELECT * FROM(SELECT  min(ctime) AS ctime, count(*) AS nb, id_metric FROM data_bin GROUP BY id_metric) s WHERE nb > 100"
            cursor.execute(sql)
            result = cursor.fetchall()
            for metric_info in result:
                # insert a duplicate value at the mid of the day
                low_limit = metric_info['ctime'] + 43200 - 60
                upper_limit = metric_info['ctime'] + 43200 + 60
                cursor.execute(
                    f"INSERT INTO data_bin SELECT * FROM data_bin WHERE id_metric={metric_info['id_metric']} AND ctime BETWEEN {low_limit} AND {upper_limit}")
                retval.append({metric_info['id_metric'], metric_info['ctime']})
            connection.commit()
    return retval


##
# @brief check that metrics are not a NaN during one day
#
# @param an array of pair <time of oldest value>, <metric id> returned by add_duplicate_metrics
#
# @return true or false
def check_for_NaN_metric(add_duplicate_metrics_ret):
    for min_timestamp, metric_id in add_duplicate_metrics_ret:
        max_timestamp = min_timestamp + 86400
        res = getoutput(
            f"rrdtool dump {VAR_ROOT}/lib/centreon/metrics/{metric_id}.rrd")
        # we search a string like <!-- 2022-12-07 23: 00: 00 CET / 1670450400 - -> < row > <v > NaN < /v > </row >
        row_search = re.compile(
            r"(\d+)\s+\-\-\>\s+\<row\>\<v\>([0-9\.e+NaN]+)")
        for line in res.splitlines():
            extract = row_search.search(line)
            if extract is not None:
                ts = int(extract.group(1))
                val = extract.group(2)
                if ts > min_timestamp and ts < max_timestamp and val == "NaN":
                    return False
    return True


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


def broker_set_sql_manager_stats(port: int, stmt: int, queries: int, timeout=TIMEOUT):
    limit = time.time() + timeout
    while time.time() < limit:
        time.sleep(1)
        with grpc.insecure_channel("127.0.0.1:{}".format(port)) as channel:
            stub = broker_pb2_grpc.BrokerStub(channel)
            opts = broker_pb2.SqlManagerStatsOptions()
            opts.slowest_statements_count = stmt
            opts.slowest_queries_count = queries
            try:
                stub.SetSqlManagerStats(opts)
                break
            except:
                logger.console("gRPC server not ready")


def broker_get_sql_manager_stats(port: int, query, timeout=TIMEOUT):
    limit = time.time() + timeout
    while time.time() < limit:
        time.sleep(1)
        with grpc.insecure_channel("127.0.0.1:{}".format(port)) as channel:
            stub = broker_pb2_grpc.BrokerStub(channel)
            con = broker_pb2.SqlConnection()
            try:
                res = stub.GetSqlManagerStats(con)
                logger.console(res)
                res = MessageToJson(res)
                logger.console(res)
                res = json.loads(res)
                for c in res["connections"]:
                    if "slowestQueries" in c:
                        for q in c["slowestQueries"]:
                            if query in q["query"]:
                                return q["duration"]
                    if "slowestStatements" in c:
                        for q in c["slowestStatements"]:
                            if query in q["statementQuery"]:
                                return q["duration"]
            except:
                logger.console("gRPC server not ready")
    return -1

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
def rebuild_rrd_graphs_from_db(indexes):
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
    res = getoutput(f"rrdtool graph dummy --start=end-180d --end=now"
                    " DEF:x={VAR_ROOT}/lib/centreon/metrics/{metric}.rrd:value:AVERAGE VDEF:xa=x,AVERAGE PRINT:xa:%lf")
    lst = res.split('\n')
    if len(lst) >= 2:
        res = float(lst[1].replace(',', '.'))
        err = abs(res - float(value)) / float(value)
        logger.console(
            f"expected value: {value} - result value: {res} - err: {err}")
        return err < 0.005
    else:
        logger.console(
            f"It was impossible to get the average value from the file {VAR_ROOT}/lib/centreon/metrics/{metric}.rrd from the last 30 days")
        return True


def compare_rrd_status_average_value(index_id, value: int):
    """Compare the average value for an RRD metric on the last 30 days with a value.
    index_id is the index of the status
    average value expected is 100 if value=0, 75 if value=1, 0 if value=2
    """
    res = getoutput(f"rrdtool graph dummy --start=end-180d --end=now"
                    " DEF:x=" + VAR_ROOT +
                    "/lib/centreon/status/{index_id}.rrd:value:AVERAGE VDEF:xa=x,AVERAGE PRINT:xa:%lf")
    lst = res.split('\n')
    if len(lst) >= 2:
        res = float(lst[1].replace(',', '.'))
        expected = {1: 75, 2: 0}.get(value, 100)
        return expected == res
    else:
        logger.console(
            f"It was impossible to get the average value from the file {VAR_ROOT}/lib/centreon/statuss/{index_id}.rrd from the last 30 days")
        return True


def compare_rrd_average_value_with_grpc(metric, key, value: float):
    """! Compare the average value for an RRD metric.
    @param metric The metric id
    @param key The key to search in the rrd info
    @param float The value to compare with.
    @return True if value pointed by key is equal to value param.
    """
    res = getoutput(
        f"rrdtool info {VAR_ROOT}/lib/centreon/metrics/{metric}.rrd"
    )
    lst = res.split('\n')
    if len(lst) >= 2:
        for l in lst:
            if key in l:
                last_update = int(l.split('=')[1])
                logger.console(f"{key}: {last_update}")
                return last_update == value*60
    else:
        logger.console(
            f"It was impossible to get the average value from the file {VAR_ROOT}/lib/centreon/metrics/{metric}.rrd")
        return False


def check_sql_connections_count_with_grpc(port, count, timeout=TIMEOUT):
    """!Call the GetSqlManagerStats function by gRPC and checks there are count active connections.
    @param port grpc port
    @param count number of expected connections
    @param timeout timeout in seconds
    @return  True is nb connections is equal to count
    """
    limit = time.time() + timeout
    while time.time() < limit:
        time.sleep(1)
        with grpc.insecure_channel("127.0.0.1:{}".format(port)) as channel:
            stub = broker_pb2_grpc.BrokerStub(channel)
            try:
                res = stub.GetSqlManagerStats(empty_pb2.Empty())
                if len(res.connections) < count:
                    continue
                count = 0
                for c in res.connections:
                    if c.down_since:
                        pass
                    else:
                        count += 1
                if count == 3:
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

    f = open(f"{ETC_ROOT}/centreon-broker/{filename}", "r")
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

##
# @brief send a gRPC command to remove by name a poller
#
# @param port the gRPC port to use
# @param name the poller name
#


def remove_poller(port, name, timeout=TIMEOUT):
    limit = time.time() + timeout
    while time.time() < limit:
        logger.console(f"Try to call removePoller by name on port {port}")
        time.sleep(1)
        with grpc.insecure_channel("127.0.0.1:{}".format(port)) as channel:
            stub = broker_pb2_grpc.BrokerStub(channel)
            ref = broker_pb2.GenericNameOrIndex()
            ref.str = name
            try:
                stub.RemovePoller(ref)
                break
            except:
                logger.console("gRPC server not ready")

##
# @brief send a gRPC command to remove by id a poller
#
# @param port the gRPC port to use
# @param name the poller name
#


def remove_poller_by_id(port, idx, timeout=TIMEOUT):
    limit = time.time() + timeout
    while time.time() < limit:
        logger.console(
            f"Try to call removePoller by id (={idx}) on port {port}")
        time.sleep(1)
        with grpc.insecure_channel("127.0.0.1:{}".format(port)) as channel:
            stub = broker_pb2_grpc.BrokerStub(channel)
            ref = broker_pb2.GenericNameOrIndex()
            ref.idx = int(idx)
            try:
                stub.RemovePoller(ref)
                break
            except:
                logger.console("gRPC server not ready")


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


def get_broker_log_level(port, name, log, timeout=TIMEOUT):
    limit = time.time() + timeout
    while time.time() < limit:
        logger.console("Try to call GetLogInfo")
        time.sleep(1)
        with grpc.insecure_channel("127.0.0.1:{}".format(port)) as channel:
            stub = broker_pb2_grpc.BrokerStub(channel)
            ref = broker_pb2.GenericString()
            ref.str_arg = log
            try:
                res = stub.GetLogInfo(ref)
                res = res.level[log]
                return res
                break
            except:
                logger.console("gRPC server not ready")


def set_broker_log_level(port, name, log, level, timeout=TIMEOUT):
    limit = time.time() + timeout
    while time.time() < limit:
        logger.console("Try to call SetLogLevel")
        time.sleep(1)
        with grpc.insecure_channel("127.0.0.1:{}".format(port)) as channel:
            stub = broker_pb2_grpc.BrokerStub(channel)
            ref = broker_pb2.LogLevel()
            ref.logger = log
            try:
                ref.level = broker_pb2.LogLevel.LogLevelEnum.Value(
                    level.upper())
            except ValueError as value_error:
                res = str(value_error)
                break

            try:
                res = stub.SetLogLevel(ref)
                break
            except grpc.RpcError as rpc_error:
                if rpc_error.code() == grpc.StatusCode.INVALID_ARGUMENT:
                    res = rpc_error.details()
                break
            except:
                logger.console("gRPC server not ready")
    return res


##
# @brief Call the GetGenericStats function by gRPC
# it works with both engine and broker
#
# @param port of the grpc server
#
# @return process__stat__pb2.pb_process_stat
def get_broker_process_stat(port, timeout=10):
    limit = time.time() + timeout
    while time.time() < limit:
        time.sleep(1)
        with grpc.insecure_channel("127.0.0.1:{}".format(port)) as channel:
            # same for engine and broker
            stub = broker_pb2_grpc.BrokerStub(channel)
            try:
                res = stub.GetProcessStats(empty_pb2.Empty())
                return res
            except:
                logger.console("gRPC server not ready")
    logger.console("unable to get process stats")
    return None


def parse_victoria_body(request_body: str):
    victoria_payload = {}
    for field_val in request_body.split(','):
        if field_val == "status" or field_val == "metric":
            victoria_payload["type"] = field_val
        elif " " in field_val:
            last_data = field_val.split(' ')  # last tag
            victoria_payload[key_val[0]] = key_val[1]
            key_val = last_data[1].split('=')  # value
            victoria_payload[key_val[0]] = key_val[1]
            victoria_payload["time_stamp"] = int(last_data[2])
        else:
            key_val = field_val.split('=')
            if (len(key_val) > 1):
                victoria_payload[key_val[0]] = key_val[1]
    return victoria_payload


def check_victoria_data(request_body: str, data_type: str, min_timestamp: int,  **to_check):
    for line in request_body.splitlines():
        datas = parse_victoria_body(line)
        if datas["type"] != data_type:
            continue
        if min_timestamp > datas["time_stamp"]:
            continue
        all_ok = True
        for key in to_check.keys():
            if to_check[key] != datas[key]:
                all_ok = False
        if all_ok:
            return True
    return False


def check_victoria_metric(request_body: str, min_timestamp: int,  **to_check):
    return check_victoria_data(request_body, "metric", min_timestamp, **to_check)


def check_victoria_status(request_body: str, min_timestamp: int,  **to_check):
    return check_victoria_data(request_body, "status", min_timestamp, **to_check)


def dump_ba(port, index: int, filename: str):
    with grpc.insecure_channel(f"127.0.0.1:{port}") as channel:
        stub = broker_pb2_grpc.BrokerStub(channel)
        info = broker_pb2.BaInfo()
        info.id = index
        info.output_file = filename
        try:
            stub.GetBa(info)
            logger.console(f"BA {index} dump to {filename}")
        except:
            logger.console("gRPC server not ready")

def check_all_services_with_status(host: str, service_like:str, status: int, legacy:bool=False, timeout:int=TIMEOUT):
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
                # Read a single record
                if legacy:
                    sql = f"SELECT count(*) FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='{host}' AND s.description LIKE '{service_like}' AND s.state <> {status}"
                else:
                    sql = f"SELECT count(*) FROM resources WHERE name like '{service_like}' AND parent_name='{host}' AND status <> {status}"
                cursor.execute(sql)
                result = cursor.fetchall()
                logger.console(result[0]['count(*)'])
                if result and result[0] and 'count(*)' in result[0] and int(result[0]['count(*)']) == 0:
                    return True
                time.sleep(1)
    return False

def check_last_checked_services_with_given_metric_more_than(metric_like: str, now: int, goal: int, timeout:int=TIMEOUT):
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
                # Read a single record
                sql = f"SELECT count(s.last_check) FROM metrics m LEFT JOIN index_data i ON m.index_id = i.id LEFT JOIN services s ON s.host_id = i.host_id AND s.service_id = i.service_id WHERE metric_name LIKE '{metric_like}' AND s.last_check >= {now}"
                logger.console(f"SELECT count(s.last_check) FROM metrics m LEFT JOIN index_data i ON m.index_id = i.id LEFT JOIN services s ON s.host_id = i.host_id AND s.service_id = i.service_id WHERE metric_name LIKE '{metric_like}' AND s.last_check >= {now}")
                cursor.execute(sql)
                result = cursor.fetchall()
                logger.console(result[0])
                if result and result[0] and 'count(s.last_check)' in result[0] and int(result[0]['count(s.last_check)']) >= int(goal):
                    return True
                time.sleep(1)
    return False
