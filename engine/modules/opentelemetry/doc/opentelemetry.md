## Open-telemetry
### Principe
Engine can receive open telemetry data on a grpc server
A new module is added opentelemetry
It works like that:
* metrics are received
* extractors tries to extract host name and service description for each otl_data_point. 
* On success, it searches a check_result_builder used by the passive otel service. Then the check_result_builder converts otl_data_point in check_result and update passive service.

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
* otl_data_point: otl_data_point is the smallest unit of received request, otl_data_point class contains otl_data_point protobuf object and all his parents (resource, scope, metric)
* host serv extractors: When we receive otel metrics, we must extract host and service, this is his job. It can be configurable in order for example to search host name in otl_data_point attribute or in scope. host serv extractors also contains host serv allowed. This list is updated by register_host_serv command method
* otel_connector: a fake connector that is used to make the link between engine and otel module
* otl_server: a grpc server that accept otel collector incoming connections
* otl_check_result_builder: His final class as his configuration is done from the command line of the check. His job is to create a check result from otl_data_point.
* host_serv_list: in order to extract host and service, an host_serv extractor must known allowed host service pairs. As otel_connector may be notified of host service using it by register_host_serv method while otel module is not yet loaded. This object shared between otel_connector and host_serv_extractor is actualized from otel_connector::register_host_serv.

### How engine access to otl object
In otel_interface.hh, otel object interface are defined in engine commands namespace.
Object used by both otel module and engine are inherited from these interfaces.
Engine only knows a singleton of the interface open_telemetry_base. This singleton is initialized at otl module loading.

### How to configure it
We use a fake connector. When configuration is loaded, if a connector command line begins with "open_telemetry", we create an otel_connector. Arguments following "open_telemetry" are used to create an host service extractor and a check_result_builder. If otel module is loaded, we create extractor, otherwise, the otel_connector initialization will be done at otel module loading.
So user has to build one connector by host serv extractor, check_result_builder configuration.
Then received otel data_points will be converted in check_result.

### other configuration
other configuration parameters are stored in a dedicated json file. The path of this file is passed as argument in centengine.cfg
Example: broker_module=lib/libopentelemetry.so /etc/centreon_engine/otl_server.json
In this file there are grpc server parameters and some other parameters.

### telegraf
telegraf can start nagios plugins and send results to engine. So centreon opentelemetry library has two classes, one to convert telegraf nagios output to service check, and an http(s) server that gives to telegraf his configuration according to engine configuration.
First telegraf service commands must use opentelemetry fake connector and have a command line like:
```
/usr/lib/nagios/plugins/check_icmp 127.0.0.1
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
        command_line                   /usr/lib/nagios/plugins/check_icmp 127.0.0.1
        connector                      OTEL connector
    }
  ```
- connector.cfg
  ```
    define connector {
        connector_name                 OTEL connector
        connector_line                 open_telemetry --processor=nagios_telegraf --extractor=attributes --host_path=resourceMetrics.scopeMetrics.metrics.dataPoints.attributes.host --service_path=resourceMetrics.scopeMetrics.metrics.dataPoints.attributes.service
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
            "http_server" : {
                "port": 1443,
                "encryption": true,
                "public_cert": "server.crt",
                "private_key": "server.key"
            },
            "engine_otel_endpoint": "172.17.0.1:4317",
            "check_interval":60
        }
    }
  ```

### centreon monitoring agent

#### agent connects to engine
Even if all protobuf objects are opentelemetry objects, grpc communication is made in streaming mode. It is more efficient, it allows reverse connection (engine can connect to an agent running in a DMZ) and 
Engine can send configuration on each config update.
You can find all grpc definitions are agent/proto/agent.proto.
Every time engine configuration is updated, we calculate configuration for each connected agent and send it on the wire if we find a difference with the old configuration. That's why each connection has a ```agent::MessageToAgent _last_config``` attribute.
So, the opentelemetry engine server supports two services, opentelemetry service and agent streaming service.
OpenTelemetry data is different from telegraf one:
* host service attributes are stored in resource_metrics.resource.attributes
* performance data (min, max, critical lt, warning gt...) is stored in exemplar, service status is stored in status metric
  
Example for metric output ```OK - 127.0.0.1: rta 0,010ms, lost 0%|rta=0,010ms;200,000;500,000;0; pl=0%;40;80;; rtmax=0,035ms;;;; rtmin=0,003ms;;;;```:
```json
resource_metrics {
  resource {
    attributes {
      key: "host.name"
      value {
        string_value: "host_1"
      }
    }
    attributes {
      key: "service.name"
      value {
        string_value: ""
      }
    }
  }
  scope_metrics {
    metrics {
      name: "status"
      description: "OK - 127.0.0.1: rta 0,010ms, lost 0%"
      gauge {
        data_points {
          time_unix_nano: 1719911975421977886
          as_int: 0
        }
      }
    }
    metrics {
      name: "rta"
      unit: "ms"
      gauge {
        data_points {
          time_unix_nano: 1719911975421977886
          exemplars {
            as_double: 500
            filtered_attributes {
              key: "crit_gt"
            }
          }
          exemplars {
            as_double: 0
            filtered_attributes {
              key: "crit_lt"
            }
          }
          exemplars {
            as_double: 200
            filtered_attributes {
              key: "warn_gt"
            }
          }
          exemplars {
            as_double: 0
            filtered_attributes {
              key: "warn_lt"
            }
          }
          exemplars {
            as_double: 0
            filtered_attributes {
              key: "min"
            }
          }
          as_double: 0
        }
      }
    }
    metrics {
      name: "pl"
      unit: "%"
      gauge {
        data_points {
          time_unix_nano: 1719911975421977886
          exemplars {
            as_double: 80
            filtered_attributes {
              key: "crit_gt"
            }
          }
          exemplars {
            as_double: 0
            filtered_attributes {
              key: "crit_lt"
            }
          }
          exemplars {
            as_double: 40
            filtered_attributes {
              key: "warn_gt"
            }
          }
          exemplars {
            as_double: 0
            filtered_attributes {
              key: "warn_lt"
            }
          }
          as_double: 0
        }
      }
    }
    metrics {
      name: "rtmax"
      unit: "ms"
      gauge {
        data_points {
          time_unix_nano: 1719911975421977886
          as_double: 0
        }
      }
    }
    metrics {
      name: "rtmin"
      unit: "ms"
      gauge {
        data_points {
          time_unix_nano: 1719911975421977886
          as_double: 0
        }
      }
    }
  }
}```

Parsing of this format is done by ```agent_check_result_builder``` class

Configuration of agent is divided in two parts:
* A common part to all agents: 
  ```protobuf
    uint32 check_interval = 2;
    //limit the number of active checks in order to limit charge
    uint32 max_concurrent_checks = 3;
    //period of metric exports (in seconds)
    uint32 export_period = 4;
    //after this timeout, process is killed (in seconds)
    uint32 check_timeout = 5;
  ```
* A list of services that agent has to check
  
The first part is owned by agent protobuf service (agent_service.cc), the second is build by a common code shared with telegraf server (conf_helper.hh)

So when centengine receives a HUP signal, opentelemetry::reload check configuration changes on each established connection and update also agent service conf part1 which is used to configure future incoming connections.

#### engine connects to agent

##### configuration
Each agent has its own grpc configuration. Each object in this array is a grpc configuration object like those we can find in Agent or server

An example:
```json
{
    "max_length_grpc_log": 0,
    "centreon_agent": {
        "check_interval": 10,
        "export_period": 15,
        "reverse_connections": [
            {
                "host": "127.0.0.1",
                "port": 4317
            }
        ]
    }
}
```

#### classes
From this configuration an agent_reverse_client object maintains a list of endpoints engine has to connect to. It manages also agent list updates.
It contains a map of to_agent_connector indexed by config.
The role to_agent_connector is to maintain an alive connection to agent (agent_connection class). It owns an agent_connection class and recreates it in case of network failure.
Agent_connection holds a weak_ptr to agent_connection to warn it about connection failure.