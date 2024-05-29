## Open-telemetry
### Principe
Engine can receive open telemetry data on a grpc server
A new module is added opentelemetry
It works like that:
* metrics are received
* extractors tries to extract host name and service description for each data_point. On success, data_point are pushed on fifos indexed by host, service
* a service that used these datas wants to do a check. The cmd line identifies the otl_converter that will construct check result from host service data_point fifos. If converter achieves to build a result from metrics, it returns right now, if it doesn't, a handler will be called as soon as needed metrics will be available or timeout expires.

### open telemetry request
The proto is organized like that
```json
{
    "resourceMetrics": [
        {
            "resource": {
                "attributes": [
                    {
                        "key": "attrib1",
                        "value": {
                            "stringValue": "attrib value"
                        }
                    }
                ]
            },
            "scopeMetrics": [
                {
                    "scope": {
                        "attributes": [
                            {
                                "key": "attrib1",
                                "value": {
                                    "stringValue": "attrib value"
                                }
                            }
                        ]
                    },
                    "metrics": [
                        {
                            "name": "check_icmp_warning_gt",
                            "gauge": {
                                "dataPoints": [
                                    {
                                        "timeUnixNano": "1707744430000000000",
                                        "asDouble": 200,
                                        "attributes": [
                                            {
                                                "key": "host",
                                                "value": {
                                                    "stringValue": "localhost"
                                                }
                                            },
                                            {
                                                "key": "perfdata",
                                                "value": {
                                                    "stringValue": "rta"
                                                }
                                            },
                                            {
                                                "key": "service",
                                                "value": {
                                                    "stringValue": "check_icmp"
                                                }
                                            },
                                            {
                                                "key": "unit",
                                                "value": {
                                                    "stringValue": "ms"
                                                }
                                            }
                                        ]
                                    },
                                    {
                                        "timeUnixNano": "1707744430000000000",
                                        "asDouble": 40,
                                        "attributes": [
                                            {
                                                "key": "host",
                                                "value": {
                                                    "stringValue": "localhost"
                                                }
                                            },
                                            {
                                                "key": "perfdata",
                                                "value": {
                                                    "stringValue": "pl"
                                                }
                                            },
                                            {
                                                "key": "service",
                                                "value": {
                                                    "stringValue": "check_icmp"
                                                }
                                            },
                                            {
                                                "key": "unit",
                                                "value": {
                                                    "stringValue": "%"
                                                }
                                            }
                                        ]
                                    }
                                ]
                            }
                        }
                    ]
                }
            ]
        }
    ]
}

```

### Concepts and classes
* data_point: data_point is the smallest unit of received request, data_point class contains data_point protobuf object and all his parents (resource, scope, metric)
* host serv extractors: When we receive otel metrics, we must extract host and service, this is his job. It can be configurable in order for example to search host name in data_point attribute or in scope. host serv extractors also contains host serv allowed. This list is updated by register_host_serv command method
* data_point fifo: a container that contains data points indexed by timestamp
* data_point fifo container: fifos indexed by host service
* otel_command: a fake connector that is used to make the link between engine and otel module
* otl_server: a grpc server that accept otel collector incoming connections
* otl_converter: This short lived object is created each time engine wants to do a check. His final class as his configuration is done from the command line of the check. His job is to create a check result from data_point fifo container datas. It's destroyed when he achieved to create a check result or when timeout expires.
* host_serv_list: in order to extract host and service, an host_serv extractor must known allowed host service pairs. As otel_command may be notified of host service using it by register_host_serv method while otel module is not yet loaded. This object shared between otel_command and host_serv_extractor is actualized from otel_command::register_host_serv.

### How engine access to otl object
In otel_interface.hh, otel object interface are defined in engine commands namespace.
Object used by both otel module and engine are inherited from these interfaces.
Engine only knows a singleton of the interface open_telemetry_base. This singleton is initialized at otl module loading.

### How to configure it
We use a fake connector. When configuration is loaded, if a connector command line begins with "open_telemetry", we create an otel_command. Arguments following "open_telemetry" are used to create an host service extractor. If otel module is loaded, we create extractor, otherwise, the otel_command initialization will be done at otel module loading.
So user has to build one connector by host serv extractor configuration.
Then commands can use these fake connectors (class otel_command) to run checks.

### How a service do a check
When otel_command::run is called, it calls the check method of open_telemetry singleton.
The check method of open_telemetry object will use command line passed to run to create an otl_converter object that has to convert metrics to check result.
The open_telemetry call sync_build_result_from_metrics, if it can't achieve to build a result, otl_converter is stored in a container.
When a metric of a waiting service is received, async_build_result_from_metrics of otl_converter is called.
In open_telemetry object, a second timer is also used to call async_time_out of otl_converter on timeout expire.

### other configuration
other configuration parameters are stored in a dedicated json file. The path of this file is passed as argument in centengine.cfg
Example: broker_module=lib/libopentelemetry.so /etc/centreon_engine/otl_server.json
In this file there are grpc server parameters and some other parameters.

### telegraf
telegraf can start nagios plugins and send results to engine. So centreon opentelemetry library has two classes, one to convert telegraf nagios output to service check, and an http(s) server that gives to telegraf his configuration according to engine configuration.
First telegraf service commands must use opentelemetry fake connector and have a command line like:
```
nagios_telegraf --cmd_line /usr/lib/nagios/plugins/check_icmp 127.0.0.1
```
When telegraf conf server receive an http request with one or several hosts in get parameters `http://localhost:1443/engine?host=host_1`, it searches open telemetry commands in host and service list and returns a configuration file in response body.
The problem of asio http server is that request handler are called by asio thread, so we use command_line_manager to scan hosts and service in a thread safe manner.

An example of configuration:
- commands.cfg
  ```
    define command {
        command_name                    test-notif
        command_line                    /tmp/var/lib/centreon-engine/notif.pl
    }
    define command {
        command_name                   otel_check_icmp
        command_line                   nagios_telegraf --cmd_line /usr/lib/nagios/plugins/check_icmp 127.0.0.1
        connector                      OTEL connector
    }
  ```
- connector.cfg
  ```
    define connector {
        connector_name                 OTEL connector
        connector_line                 open_telemetry attributes --host_attribute=data_point --host_key=host --service_attribute=data_point --service_key=service
    }
  ```
- centengine.cfg:
  ```
    cfg_file=/tmp/etc/centreon-engine/config0/hosts.cfg
    cfg_file=/tmp/etc/centreon-engine/config0/services.cfg
    cfg_file=/tmp/etc/centreon-engine/config0/commands.cfg
    cfg_file=/tmp/etc/centreon-engine/config0/hostgroups.cfg
    cfg_file=/tmp/etc/centreon-engine/config0/timeperiods.cfg
    cfg_file=/tmp/etc/centreon-engine/config0/connectors.cfg
    broker_module=/usr/lib64/centreon-engine/externalcmd.so
    broker_module=/usr/lib64/nagios/cbmod.so /tmp/etc/centreon-broker/central-module0.json
    broker_module=/usr/lib64/centreon-engine/libopentelemetry.so /tmp/etc/centreon-engine/config0/otl_server.json
    interval_length=60
    use_timezone=:Europe/Paris
  ```
- fichier de conf opentelemetry:
  ```json
    {
        "server": {
            "host": "0.0.0.0",
            "port": 4317
        },
        "max_length_grpc_log": 0,
        "telegraf_conf_server": {
            "port": 1443,
            "encryption": true,
            "engine_otel_endpoint": "172.17.0.1:4317",
            "certificate_path": "server.crt",
            "key_path": "server.key"
        }
    }
  ```
