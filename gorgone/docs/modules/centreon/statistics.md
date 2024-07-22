# Broker

## Description

This module aims to deal with statistics collection of Centreon Engine and Broker. It require action module to be configured on each poller and on the central.

## Configuration

| Directive        | Description                                                                                    | Default value                     |
| :--------------- | :--------------------------------------------------------------------------------------------- | :-------------------------------- |
| broker_cache_dir | Path to the Centreon Broker statistics directory (local) use to store node's broker statistics | `/var/lib/centreon/broker-stats/` |

The configuration needs a cron definition to unsure that statistics collection will be done cyclically.

#### Example

```yaml
name: statistics
package: "gorgone::modules::centreon::statistics::hooks"
enable: false
broker_cache_dir: "/var/lib/centreon/broker-stats/"
cron:
  - id: broker_stats
    timespec: "*/5 * * * *"
    action: BROKERSTATS
    parameters:
      timeout: 10
      collect_localhost: false
```

## Events

| Event              | Description                                       |
|:-------------------|:--------------------------------------------------|
| STATISTICSREADY    | Internal event to notify the core                 | 
| STATISTICSLISTENER | Internal Event to receive data from action module |
| BROKERSTATS        | Collect Centreon Broker statistics files on node  |
| ENGINESTATS        | Collect Centreon engine statistics on node        |

## API



### Collect Centreon engine statistics on every nodes configured

The api send back a token to monitor advancement. Please note this token don't allow to monitor the whole process, only the first part until action command are sent.


| Endpoint                    | Method |
|:----------------------------| :----- |
| /centreon/statistics/engine | `GET`  |

#### Headers

| Header | Value            |
| :----- | :--------------- |
| Accept | application/json |

#### Example

```bash
curl --request POST "https://hostname:8443/api/centreon/statistics/engine" \
  --header "Accept: application/json"
```

### Collect Centreon Broker statistics on one or several nodes

| Endpoint                        | Method |
| :------------------------------ | :----- |
| /centreon/statistics/broker     | `GET`  |
| /centreon/statistics/broker/:id | `GET`  |

#### Headers

| Header | Value            |
| :----- | :--------------- |
| Accept | application/json |

#### Path variables

| Variable | Description            |
| :------- | :--------------------- |
| id       | Identifier of the node |

#### Example

```bash
curl --request POST "https://hostname:8443/api/centreon/statistics/broker" \
  --header "Accept: application/json"
```

```bash
curl --request POST "https://hostname:8443/api/centreon/statistics/broker/2" \
  --header "Accept: application/json"
```
