# Autodiscovery

## Description

This module aims to export Centreon monitoring data (statuses and metrics) using openmetrics format.

The module can export it in a local file and/or Prometheus Pushgateway.

## Configuration

| Directive                           | Description                                                                                                    | Default value                                                                                                            |
|:------------------------------------|:---------------------------------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------|
| update\_interval                    | Time in seconds defining frequency at which datas will be exported                                             | `300`                                                                                                                    |
| export\_file                        | Boolean to enable local file                                                                                   | `1`                                                                                                                      |
| file                                | Path to the openmetrics local file                                                                             | `/usr/share/centreon/www/exporters/prometheus`                                                                           |
| tmp\_file                           | Path to the temporary openmetrics local file                                                                   | `$file . '.tmp'`                                                                                                         |
| export\_gateway                     | Boolean to enable Prometheus Pushgateway                                                                       | `0`                                                                                                                      |
| prometheus\_gateway\_address        | Prometheus Pushgateway address                                                                                 | `http://localhost`                                                                                                       |
| prometheus\_gateway\_port           | Prometheus Pushgateway port                                                                                    | `9091`                                                                                                                   |
| prometheus\_gateway\_job            | Prometheus Pushgateway job                                                                                     | `monitoring`                                                                                                             |
| prometheus\_gateway\_instance       | Prometheus Pushgateway instance                                                                                | `production`                                                                                                             |
| prometheus\_gateway\_wipe\_interval | Time in seconds defining frequency at which gateway datas will be deleted                                      | `86400`                                                                                                                  |
| prometheus\_gateway\_user           | Prometheus Pushgateway basic username                                                                          | ``                                                                                                                       |
| prometheus\_gateway\_password       | Prometheus Pushgateway basic password                                                                          | ``                                                                                                                       |
| prometheus\_gateway\_insecure       | Boolean to accept Prometheus Pushgateway insecure SSL connections                                              | `0`                                                                                                                      |
| add\_metrics                        | Boolean to export metrics                                                                                      | `1`                                                                                                                      |
| add\_metrics_metadata               | Boolean to export metadata with metrics                                                                        | `1`                                                                                                                      |
| add\_status                         | Boolean to export hosts and services statuses                                                                  | `1`                                                                                                                      |
| add\_state                          | Boolean to export hosts and services states (SOFT/HARD)                                                        | `0`                                                                                                                      |
| add\_acknowledged                   | Boolean to export services acknowledges                                                                        | `0`                                                                                                                      |
| filter\_hosts\_from\_hg\_matching   | Export only hosts/services/metrics belonging to hostgroups matching the regexp                                 | ``                                                                                                                       |
| exclude\_hosts\_from\_hg\_matching  | Exclude hosts/services/metrics belonging to hostgroups matching the regexp                                     | ``                                                                                                                       |
| display\_hg\_matching               | Display only hostgroups matching the regexp into the variables `%(hostgroup_names)` and `$(hostgroup_aliases)` | ``                                                                                                                       |
| host\_state\_type\_metadata         | Define type metadata for the host state                                                                        | `# TYPE host_state gauge`                                                                                                |
| host\_state\_help\_metadata         | Define help metadata for the host state                                                                        | `# HELP host_state 0 is SOFT, 1 is HARD`                                                                                 |
| host\_state\_template               | Define openmetrics host state exported                                                                         | `host_state{host='%(host_name)'} %(host_state)`                                                                          |
| host\_status\_type\_metadata        | Define type metadata for the host status                                                                       | `# TYPE host_status gauge`                                                                                               |
| host\_status\_help\_metadata        | Define help metadata for the host status                                                                       | `# HELP host_status 0 is UP, 1 is DOWN, 2 is UNREACHABLE, 4 is PENDING`                                                  |
| host\_status\_template              | Define openmetrics host status exported                                                                        | `host_status{host="%(host_name)"} %(host_status)`                                                                        |
| service\_state\_type\_metadata      | Define type metadata for the service state                                                                     | `# TYPE service_state gauge`                                                                                             |
| service\_state\_help\_metadata      | Define help metadata for the service state                                                                     | `# HELP service_state 0 is SOFT, 1 is HARD`                                                                              |
| service\_state\_template            | Define openmetrics service state exported                                                                      | `service_state{host="%(host_name)",service="%(service_description)"} %(service_state)`                                   |
| service\_ack\_type\_metadata        | Define type metadata for the service acknowledge                                                               | `# TYPE service_ack gauge`                                                                                               |
| service\_ack\_help\_metadata        | Define help metadata for the service acknowledge                                                               | `# HELP service_ack 0 is unacknowledged, 1 is acknowledged`                                                              |
| service\_ack\_template              | Define openmetrics service acknowledge exported                                                                | `service_ack{host="%(host_name)",service="%(service_description)"} %(service_acknowledged)`                              |
| service\_status_type\_metadata      | Define type metadata for the service status                                                                    | `# TYPE service_status gauge`                                                                                            |
| service\_status_help\_metadata      | Define help metadata for the service status                                                                    | `# HELP service_status is OK, 1 is WARNING, 2 is CRITICAL, 3 is UNKNOWN, 4 is PENDING`                                   |
| service\_status\_template           | Define openmetrics service status exported                                                                     | `service_status{host="%(host_name)",service="%(service_description)"} %(service_status)`                                 |
| metric\_template                    | Define openmetrics metrics exported                                                                            | `%(metric_name){host="%(host_name)",service="%(service_description)",dimensions="%(metric_dimensions)"} %(metric_value)` |

#### Example

```yaml
name: prometheus
package: "gorgone::modules::centreon::prometheus::hooks"
enable: true
```

## Events

| Event                   | Description                                     |
|:------------------------|:------------------------------------------------|
| CENTREONPROMETHEUSREADY | Internal event to notify the core               |
| CENTREONPROMETHEUSUPDATE| Export Centreon monitoring datas                |

## API

### Export centreon monitoring datas

| Endpoint                    | Method |
|:----------------------------|:-------|
| /centreon/prometheus/update | `POST` |

#### Headers

| Header       | Value            |
|:-------------|:-----------------|
| Accept       | application/json |
| Content-Type | application/json |

#### Body

Empty body

#### Example

```bash
curl --request POST "https://hostname:8443/api/centreon/prometheus/update" \
  --header "Accept: application/json" \
  --header "Content-Type: application/json" \
  --data "{}"
```
