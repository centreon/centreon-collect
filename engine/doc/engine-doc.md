# Engine documentation {#mainpage}

## Whitelist (since 23.10)

In order to enforce security, user can add a whitelist to centreon-engine.
When the user add a file in /etc/centreon-engine-whitelist or in /usr/share/centreon-engine/whitelist.conf.d, centengine only executes commands that match to the expressions given in these files.
Beware, Commands are checked after macros replacement by values, the entire line is checked, the script and his arguments.

### whitelist format
whitelist files must contain regular expressions that will filter commands. An OR is made on expressions, if a command matches with one expression, it's allowed.
#### yaml
```yaml
whitelist:
  regex:
    - \/usr\/lib64\/nagios\/plugins\/check_icmp.*
    - \/usr\/lib\/centreon\/plugins\/centreon_linux_snmp\.pl.*
```
#### json
```json
{
    "whitelist":{
        "regex":[
            "/tmp/var/lib/centreon-engine/check.pl [1-9] 1.0.0.0"
        ]
    }
}
```

The main main job is done by whitelist class, it parses file and compares final commands to regular expressions
This class is a singleton which is replace by another instance each time conf is reloaded.

Checkable class inherited by service and host classes keeps the last result of whitelist's check in cache in order to reduce CPU whitelist usage.

## Extended configuration
Users can pass an additional configuration file to engine. Gorgone is not aware of this file, so users can override centengine.cfg configuration.
Each entry found in additional json configuration file overrides its twin in `centengine.cfg`.

### examples of command line
```sh
/usr/sbin/centengine --config-file=/tmp/centengine_extend.json /etc/centreon-engine/centengine.cfg

/usr/sbin/centengine --c /tmp/file1.json  --c /tmp/file2.json /etc/centreon-engine/centengine.cfg
```

In the second case, values of file1.json will override values of centengine.cfg and values of file2.json will override values of file1.json

### file format
```json
{   
    "send_recovery_notifications_anyways": true
}
```

### implementation detail
In `state.cc` all setters have two methods:
* `apply_from_cfg`
* `apply_from_json`.

On configuration update, we first parse the `centengine.cfg` and all the `*.cfg` files, and then we parse additional configuration files.

## Open-telemetry
### Principe
Engine can receive open telemetry data on a grpc server
A new module is added opentelemetry
It works like that:
* metrics are received
* extractors tries to extract host name and service description for each otl_data_point. On success, otl_data_point are pushed on fifos indexed by host, service
* a service that used these datas wants to do a check. The cmd line identifies the otl_check_result_builder that will construct check result from host service otl_data_point fifos. If converter achieves to build a result from metrics, it returns right now, if it doesn't, a handler will be called as soon as needed metrics will be available or timeout expires.

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
* otl_data_point fifo: a container that contains data points indexed by timestamp
* otl_data_point fifo container: fifos indexed by host service
* otel_connector: a fake connector that is used to make the link between engine and otel module
* otl_server: a grpc server that accept otel collector incoming connections
* otl_check_result_builder: This short lived object is created each time engine wants to do a check. His final class as his configuration is done from the command line of the check. His job is to create a check result from otl_data_point fifo container datas. It's destroyed when he achieved to create a check result or when timeout expires.
* host_serv_list: in order to extract host and service, an host_serv extractor must known allowed host service pairs. As otel_connector may be notified of host service using it by register_host_serv method while otel module is not yet loaded. This object shared between otel_connector and host_serv_extractor is actualized from otel_connector::register_host_serv.

### How engine access to otl object
In otel_interface.hh, otel object interface are defined in engine commands namespace.
Object used by both otel module and engine are inherited from these interfaces.
Engine only knows a singleton of the interface open_telemetry_base. This singleton is initialized at otl module loading.

### How to configure it
We use a fake connector. When configuration is loaded, if a connector command line begins with "open_telemetry", we create an otel_connector. Arguments following "open_telemetry" are used to create an host service extractor. If otel module is loaded, we create extractor, otherwise, the otel_connector initialization will be done at otel module loading.
So user has to build one connector by host serv extractor configuration.
Then commands can use these fake connectors (class otel_connector) to run checks.

### How a service do a check
When otel_connector::run is called, it calls the check method of open_telemetry singleton.
The check method of open_telemetry object will use command line passed to run to create an otl_check_result_builder object that has to convert metrics to check result.
The open_telemetry call sync_build_result_from_metrics, if it can't achieve to build a result, otl_check_result_builder is stored in a container.
When a metric of a waiting service is received, async_build_result_from_metrics of otl_check_result_builder is called.
In open_telemetry object, a second timer is also used to call async_time_out of otl_check_result_builder on timeout expire.

### other configuration
other configuration parameters are stored in a dedicated json file. The path of this file is passed as argument in centengine.cfg
Example: broker_module=lib/libopentelemetry.so /etc/centreon_engine/otl_server.json
In this file there are grpc server parameters and some other parameters.

## macros encryption
Engine can receive in its configuration encrypted macros.
If credential_encryption flag in centengine configuration is true, it will manage macros in that way:
* if /etc/centreon-engine/engine-context.json is filled with a correct app_secret and salt
  * if macro content begins with secret::, we will decrypt following string
  * if macro content begins with raw::, we erase prefix
  * else we return macro content
* else we return an error
  
