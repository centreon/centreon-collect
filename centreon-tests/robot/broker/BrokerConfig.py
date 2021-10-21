import json

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
                "core": "info",
                "config": "error",
                "sql": "debug",
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

"module": """{
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
                "core": "debug",
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
}}"""
}

def config_broker(name):
    if name == 'central':
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

def config_output(name, output, value):
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
    output_dict = [elem for i, elem in enumerate(conf["centreonBroker"]["output"]) if elem["name"] == "centreon-broker-master-rrd"][0]
    if value == "one_peer_retention":
        if "host" in output_dict:
            output_dict.pop("host")
        output_dict["one_peer_retention_mode"] = "yes"
    f = open("/etc/centreon-broker/{}".format(filename), "w")
    f.write(json.dumps(conf, indent=2))
    f.close()

#config_output("central", "centreon-broker-master-rrd", "one_peer_retention")