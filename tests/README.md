# Centreon Tests

This sub-project contains functional tests for Centreon Broker, Engine and Connectors.
It is based on the [Robot Framework](https://robotframework.org/) with Python functions
we can find in the resources directory. The Python code is formatted using autopep8 and
robot files are formatted using `robocop format tests`.

## Getting Started

To get this project, you have to clone centreon-collect.

These tests are executed from the `centreon-tests/robot` folder and uses the [Robot Framework](https://robotframework.org/).

From a Centreon host, you need to install Robot Framework.

On AlmaLinux, we have to install some python packages, some perl packages:

```bash
dnf install "Development Tools" python3-devel -y
dnf install perl-HTTP-Daemon-SSL -y
dnf install perl-JSON -y
dnf install tzdata -y
```

On rpm based system, we have to execute the following commands (maybe to update a little):

```bash
yum install "Development Tools" python3-devel -y
yum install perl-HTTP-Daemon-SSL -y
yum install perl-JSON -y
yum install tzdata -y
```

On deb based system, we have to execute:


```bash
apt-get install python3-dev openssh-server tzdata
```

The `tzdata` package (the IANA time zone database, providing `/usr/share/zoneinfo`)
is required: some tests start several `centengine` processes with distinct `TZ`
environment variables (e.g. `Europe/Paris` and `America/New_York`) to check that
Broker evaluates notification timeperiods in each poller's own timezone. Without
`tzdata`, those timezones cannot be loaded and the tests silently fall back to the
machine's local timezone, which invalidates them.

Once these packages, we recommand to create a python virtual environment to play with robot framework.

You can do that as you prefer, here we use uv. The first step is to install it:

```bash
curl -LsSf https://astral.sh/uv/install.sh | less
```

Once installed, you have to create a virtual environment, we create it in the centreon-collect/tests directory:

```bash
cd centreon-collect/tests
uv venv --python=/usr/bin/python3 robotframework
```

And now, we can install the required python modules for our tests:

```bash
uv pip install -U -r requirements.txt
```

The list lives in `tests/requirements.txt`, which `create-dev-container.sh`
installs from as well: add a package there rather than in both places.

When you want to enable the virtual environment, you just have to execute the following command:

```bash
cd centreon-collect/tests
source robotframework/bin/activate
```

Now it should be possible to initialize several files to execute the tests with the following commands:

```bash
./init-proto.sh
./init-sql.sh
```

Then to run tests, you can use the following commands

```
robot -e unstable .
```

And it is also possible to execute a specific test, for example:

```
robot broker/sql.robot
```

## Implemented tests

Here are the currently implemented tests, grouped by the directory that contains
them. Each section is introduced by its number of tests.

## Table of contents

- [Bam](#bam) (79 tests)
- [Benchmarks](#benchmarks) (12 tests)
- [Broker](#broker) (94 tests)
- [Broker/database](#brokerdatabase) (15 tests)
- [Broker/engine](#brokerengine) (392 tests)
- [Ccc](#ccc) (8 tests)
- [Centralized/configuration](#centralizedconfiguration) (97 tests)
- [Connector perl](#connector-perl) (4 tests)
- [Connector ssh](#connector-ssh) (8 tests)
- [Engine](#engine) (150 tests)
- [Severities](#severities) (31 tests)
- [Vault](#vault) (22 tests)

### Bam

This chapter contains 79 tests.

1. **BABEST_SERVICE_CRITICAL**: With bbdo version 3.0.1, a BA of type 'best' with 2 serv, ba is critical only if the 2 services are critical
2. **BABOO**: With bbdo version 3.0.1, a BA of type 'worst' with 2 child services and another BA of type impact with a boolean rule returning if one of its two services are critical are created. These two BA are built from the same services and should have a similar behavior
3. **BABOOAND**: With bbdo version 3.0.1, a BA of type impact with a boolean rule returning if both of its two services are ok is created. When one condition is false, the and operator returns false as a result even if the other child is unknown.
4. **BABOOCOMPL**: With bbdo version 3.0.1, a BA of type impact with a complex boolean rule is configured. We check its correct behaviour following service updates.
5. **BABOOCOMPL_RELOAD**: With bbdo version 3.0.1, a BA of type impact with a complex boolean rule is configured. We check its correct behaviour following service updates.
6. **BABOOCOMPL_RESTART**: With bbdo version 3.0.1, a BA of type impact with a complex boolean rule is configured. We check its correct behaviour following service updates.
7. **BABOOOR**: With bbdo version 3.0.1, a BA of type 'worst' with 2 child services and another BA of type impact with a boolean rule returning if one of its two services are critical are created. These two BA are built from the same services and should have a similar behavior
8. **BABOOORREL**: With bbdo version 3.0.1, a BA of type impact with a boolean rule returning if one of its two services is ok is created. One of the two underlying services must change of state to change the ba state. For this purpose, we change the service state and reload cbd. So the rule is something like "False OR True" which is equal to True. And to pass from True to False, we change the second service.
9. **BAWORST**: With bbdo version 3.0.1, a BA of type 'worst' with two services is configured. We also check stats output
10. **BAWORST2**: a worst ba with a boolean kpi and a ba kpi
11. **BAWORST_ACK**:
     * **SCENARIO:** Acknowledging a service acknowledges the BA, and removing it unacknowledges the BA
     * **GIVEN** BBDO version is 3.0.1
     * **AND** a Business Activity of type "worst" is configured with two services
     * **WHEN** one of the services is acknowledged
     * **THEN** the Business Activity is acknowledged
     * **WHEN** the acknowledgement is removed from the service
     * **THEN** the Business Activity is no longer acknowledged
12. **BA_BOOL_KPI**: With bbdo version 3.0.1, a BA of type 'worst' with 1 boolean kpi
13. **BA_CHANGED**:
     * **SCENARIO:** Replace Service KPI with Boolean Rule KPI in Worst-type BA
     * **GIVEN** a BA of type "worst" is configured with one service KPI
     * **WHEN** the service KPI is replaced by a boolean rule KPI
     * **AND** Broker is reloaded
     * **THEN** the BA is correctly updated with the new KPI configuration
14. **BA_DEACTIVATED_SERVICE**:
     * **SCENARIO:** A KPI whose service is deactivated is dropped, the activation column saying so
     * **GIVEN** a non centralized platform, where BAM builds its own host/service mapping from the database
     * **AND** a BA of type "worst" with two service KPIs, service_302 and service_303
     * **WHEN** service_303 is deactivated -- its row says so and the configuration does not carry it
     * **THEN** BAM loads the mapping from the database
     * **AND** the KPI of service_303 is dropped
     * **AND** the BA still follows service_302
15. **BA_DISABLED**: create a disabled BA with timeperiods and reporting filter don't create error message
16. **BA_IMPACT_2KPI_SERVICES**: With bbdo version 3.0.1, a BA of type 'impact' with 2 serv, ba is critical only if the 2 services are critical
17. **BA_IMPACT_IMPACT**:
     * **GIVEN** a Business Activity (BA) of type "impact"
     * **AND** it has two child BAs of type "impact"
     * **AND** the first child has an impact of 90
     * **AND** the second child has an impact of 10
     * **WHEN** both child BAs are impacting
     * **THEN** the parent BA should be "critical"
     * **WHEN** both child BAs are not impacting
     * **THEN** the parent BA should be "ok"
18. **BA_RATIO_NUMBER_BA_4_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
19. **BA_RATIO_NUMBER_BA_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 2 services and one ba with 1 service
20. **BA_RATIO_PERCENT_BA_4_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
21. **BA_RATIO_PERCENT_BA_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio percent' with 2 serv an 1 ba with one service
22. **BA_SERVICE_PNAME_AFTER_RELOAD**:
     * **SCENARIO:** Verify that the parent_name of a BA service is not erased after a broker reload
     * **GIVEN** a BA "test" of type "worst" with its service "host_16:service_302"
     * **WHEN** I start broker and engine
     * **THEN** the BA service "test" should have a status of 0 within 30 seconds
     * **WHEN** I reload the broker
     * **THEN** the database should still contain a BA service with name "test" and parent_name "_Module_BAM_1"
23. **BEBAMIDT1**:
     * **GIVEN** a BA of type 'worst' with one service is configured
     * **AND** The BA is in critical state due to its service
     * **WHEN** a downtime is set on this service
     * **THEN** an inherited downtime is set to the BA
     * **WHEN** the downtime is removed from the service
     * **THEN** the inherited downtime is deleted from the BA
24. **BEBAMIDT2**:
     * **GIVEN** a BA of type 'worst' with one service is configured
     * **AND** the BA is in critical state due to its service
     * **AND** a downtime is set on this service
     * **THEN** an inherited downtime is set to the BA
     * **WHEN** Engine is restarted
     * **AND** Broker is restarted
     * **THEN** both downtimes are still present with no duplicates
     * **WHEN** the downtime is removed from the service
     * **THEN** the inherited downtime is deleted
25. **BEBAMIDTU1**:
     * **GIVEN** BBDO version 3.0.1 is running
     * **AND** a BA of type 'worst' with one service is configured
     * **AND** The BA is in critical state due to its service
     * **WHEN** a downtime is set on this service
     * **THEN** an inherited downtime is set to the BA
     * **WHEN** the downtime is removed from the service
     * **THEN** the inherited downtime is deleted from the BA
26. **BEBAMIDTU2**:
     * **GIVEN** BBDO version 3.0.1 is in use
     * **AND** a 'worst' type BA with one service is configured
     * **AND** The BA is in critical state due to its service
     * **WHEN** a downtime is set on this service
     * **THEN** an inherited downtime is set to the BA
     * **WHEN** Engine is restarted
     * **AND** Broker is restarted
     * **THEN** both downtimes are still present with no duplicates
     * **WHEN** the downtime is removed from the service
     * **THEN** the inherited downtime is deleted
27. **BEBAMIGNDT1**: A BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. A first downtime is cancelled, the BA is still OK, but when the second downtime is cancelled, the BA should be CRITICAL.
28. **BEBAMIGNDT2**: A BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. The first downtime reaches its end, the BA is still OK, but when the second downtime reaches its end, the BA should be CRITICAL.
29. **BEBAMIGNDTU1**: With bbdo version 3.0.1, a BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. A first downtime is cancelled, the BA is still OK, but when the second downtime is cancelled, the BA should be CRITICAL.
30. **BEBAMIGNDTU2**:
     * **GIVEN** BBDO version 3.0.1 is configured
     * **AND** a BA of type "worst" with two services is set up
     * **AND** the downtime policy on this BA is "Ignore the indicator in the calculation"
     * **AND** the BA is in a critical state due to the second critical service
     * **WHEN** two downtimes are applied to the second critical service
     * **THEN** the BA state should be OK due to the policy on indicators
     * **WHEN** the first downtime reaches its end
     * **THEN** the BA state should still be OK
     * **WHEN** the second downtime reaches its end
     * **THEN** the BA should be in a critical state
31. **BECBAMBRKIDT1**:
     * **GIVEN** BBDO3 / centralized config with notification_mode = broker
     * **AND** a 'worst' BA with one service in critical state
     * **AND** a downtime scheduled on the service via Broker gRPC sets an inherited downtime on the BA
     * **WHEN** the KPI service recovers (becomes OK) while still under downtime
     * **THEN** BAM removes the inherited downtime from the BA via the Broker downtime_manager
     (the inherited downtime removal is driven by BAM state recomputation, not by a gRPC delete)
32. **BECBAMBRKIDT2**:
     * **GIVEN** BBDO3 / centralized config with notification_mode = broker
     * **AND** a 'worst' BA with one service in critical state
     * **AND** a downtime scheduled on the service via Broker gRPC sets an inherited downtime on the BA
     * **WHEN** Engine is restarted (Broker stays up and remains the downtime authority)
     * **THEN** both the KPI downtime and the inherited downtime are still present
     (Engine, being aware that Broker owns downtimes, does not reset the depth on reload)
33. **BECBAMBRKIDT3**:
     * **GIVEN** BBDO3 / centralized config with notification_mode = broker
     * **AND** a 'worst' BA with one service in critical state
     * **AND** the BA is in critical state because of its service
     * **WHEN** a downtime is scheduled on this service via Broker gRPC
     * **THEN** Broker (not Engine) sets an inherited downtime on the BA virtual service
     * **WHEN** the downtime is removed from the service via Broker gRPC
     * **THEN** the inherited downtime is removed from the BA
34. **BECBAMBRKIDT4**:
     * **GIVEN** BBDO3 / centralized config with notification_mode = broker
     * **AND** a 'worst' BA with one service in critical state
     * **AND** a downtime scheduled on the service via Broker gRPC sets an inherited downtime on the BA
     * **WHEN** Broker is restarted (Engine stays up; Broker is the downtime authority)
     * **THEN** the started downtimes (the KPI downtime and the inherited BA downtime) are
     re-injected from the Broker cache and the scheduled_downtime_depth is restored to 1
     (started downtimes survive a Broker restart; depth is re-derived idempotently)
35. **BECBAMIDTU1**:
     * **GIVEN** BBDO version 3.0.1 is running with centralized configuration enabled
     * **AND** a BA of type 'worst' with one service is configured
     * **AND** The BA is in critical state due to its service
     * **WHEN** a downtime is set on this service
     * **THEN** an inherited downtime is set to the BA
     * **WHEN** the downtime is removed from the service
     * **THEN** the inherited downtime is deleted from the BA
36. **BECBAMIDTU2**:
     * **GIVEN** BBDO version 3.0.1 is in use
     * **AND** a 'worst' type BA with one service is configured
     * **AND** The BA is in critical state due to its service
     * **WHEN** a downtime is set on this service
     * **THEN** an inherited downtime is set to the BA
     * **WHEN** Engine is restarted
     * **AND** Broker is restarted
     * **THEN** both downtimes are still present with no duplicates
     * **WHEN** the downtime is removed from the service
     * **THEN** the inherited downtime is deleted
37. **BECBAMIGNDTU1**: With bbdo version 3.0.1, a BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. A first downtime is cancelled, the BA is still OK, but when the second downtime is cancelled, the BA should be CRITICAL.
38. **BECBAMIGNDTU2**:
     * **GIVEN** BBDO version 3.0.1 is configured
     * **AND** a BA of type "worst" with two services is set up
     * **AND** the downtime policy on this BA is "Ignore the indicator in the calculation"
     * **AND** the BA is in a critical state due to the second critical service
     * **WHEN** two downtimes are applied to the second critical service
     * **THEN** the BA state should be OK due to the policy on indicators
     * **WHEN** the first downtime reaches its end
     * **THEN** the BA state should still be OK
     * **WHEN** the second downtime reaches its end
     * **THEN** the BA should be in a critical state
39. **BECPB_BA_DURATION_EVENT**: use of pb_ba_duration_event message.
40. **BECPB_DIMENSION_BA_BV_RELATION_EVENT**: bbdo_version 3 use pb_dimension_ba_bv_relation_event message.
41. **BECPB_DIMENSION_BA_EVENT**: bbdo_version 3 use pb_dimension_ba_event message.
42. **BECPB_DIMENSION_BA_TIMEPERIOD_RELATION**: use of pb_dimension_ba_timeperiod_relation message.
43. **BECPB_DIMENSION_BV_EVENT**: bbdo_version 3 use pb_dimension_bv_event message.
44. **BECPB_DIMENSION_KPI_EVENT**: bbdo_version 3 use pb_dimension_kpi_event message.
45. **BECPB_DIMENSION_TIMEPERIOD**: use of pb_dimension_timeperiod message.
46. **BECPB_DIMENSION_TRUNCATE_TABLE**: use of pb_dimension_timeperiod message.
47. **BECPB_KPI_STATUS**: bbdo_version 3 use kpi_status message.
48. **BEPB_BA_DURATION_EVENT**: use of pb_ba_duration_event message.
49. **BEPB_DIMENSION_BA_BV_RELATION_EVENT**: bbdo_version 3 use pb_dimension_ba_bv_relation_event message.
50. **BEPB_DIMENSION_BA_EVENT**: bbdo_version 3 use pb_dimension_ba_event message.
51. **BEPB_DIMENSION_BA_TIMEPERIOD_RELATION**: use of pb_dimension_ba_timeperiod_relation message.
52. **BEPB_DIMENSION_BV_EVENT**: bbdo_version 3 use pb_dimension_bv_event message.
53. **BEPB_DIMENSION_KPI_EVENT**: bbdo_version 3 use pb_dimension_kpi_event message.
54. **BEPB_DIMENSION_TIMEPERIOD**: use of pb_dimension_timeperiod message.
55. **BEPB_DIMENSION_TRUNCATE_TABLE**: use of pb_dimension_timeperiod message.
56. **BEPB_KPI_STATUS**: bbdo_version 3 use kpi_status message.
57. **CBABEST_SERVICE_CRITICAL**: With bbdo version 3.0.1, a BA of type 'best' with 2 serv, ba is critical only if the 2 services are critical
58. **CBABOO**:
     * **SCENARIO:** A "worst" BA and an impact BA with an OR boolean rule built on the same 2 services behave identically when a service becomes CRITICAL
     * **GIVEN** a BA of type "worst" with service_302 and service_303 as KPIs
     * **AND** a BA of type "impact" with a boolean rule "{service_302} IS CRITICAL OR {service_303} IS CRITICAL"
     * **WHEN** service_302 becomes CRITICAL
     * **THEN** both BAs are CRITICAL
     * **WHEN** service_302 recovers to OK
     * **THEN** both BAs return to OK
     * **AND** this cycle is repeated 10 times
59. **CBABOOAND**:
     * **SCENARIO:** An AND boolean rule evaluates to CRITICAL as soon as one operand is false, even when the other service is UNKNOWN
     * **GIVEN** a BA of type "impact" with boolean rule "{service_302} IS OK AND {service_303} IS OK"
     * **AND** service_303 is passive and starts UNKNOWN
     * **WHEN** service_302 becomes CRITICAL
     * **THEN** the BA is CRITICAL (AND short-circuits on the first false operand)
60. **CBABOOCOMPL**:
     * **SCENARIO:** A BA with a complex AND/OR boolean rule over 20 services becomes OK only when at least one service in each AND group is OK
     * **GIVEN** a BA of type "impact" with a rule of 10 AND groups, each requiring at least one of 2 services to be OK
     * **WHEN** all 20 services are CRITICAL
     * **THEN** the BA is CRITICAL
     * **WHEN** odd-indexed services are set to OK one by one
     * **THEN** the BA remains CRITICAL until all AND groups have at least one OK service
     * **AND** the BA becomes OK once all AND groups are satisfied
61. **CBABOOCOMPL_RELOAD**:
     * **SCENARIO:** A broker reload does not alter a complex boolean rule state
     * **GIVEN** a BA of type "impact" with a complex AND/OR boolean rule over 20 services
     * **AND** all 20 services are CRITICAL, then odd-indexed services 1-13 are set to OK
     * **AND** the BA is still CRITICAL because even-indexed services remain CRITICAL
     * **WHEN** broker is reloaded at each remaining step (services 15, 17, 19 set to OK one by one)
     * **THEN** the BA state is identical before and after each broker reload
     * **AND** the BA becomes OK once all AND groups are satisfied
62. **CBABOOCOMPL_RESTART**:
     * **SCENARIO:** A broker restart does not alter a complex boolean rule state
     * **GIVEN** a BA of type "impact" with a complex AND/OR boolean rule over 20 services
     * **AND** all 20 services are CRITICAL, then odd-indexed services 1-13 are set to OK
     * **AND** the BA is still CRITICAL because even-indexed services remain CRITICAL
     * **WHEN** broker is restarted at each remaining step (services 15, 17, 19 set to OK one by one)
     * **THEN** the BA state is identical before and after each broker restart
     * **AND** the BA becomes OK once all AND groups are satisfied
63. **CBABOODEACTIVATEDSVC**:
     * **SCENARIO:** A KPI whose service is deactivated is dropped once the global cache knows the poller
     * **GIVEN** a centralized platform, where Broker answers the host/service questions from its global cache
     * **AND** a BA of type "worst" with two service KPIs, service_302 and service_303
     * **WHEN** the configuration is acknowledged, so that the cache holds the poller
     * **AND** service_303 is then deactivated -- its row says so and the export no longer carries it
     * **THEN** the cache no longer holds service_303
     * **AND** on the next start, where the cache is filled from the stored configuration, the KPI of service_303 is dropped
     * **AND** the KPI of service_302 is kept, the BA still following it
64. **CBABOOKPIKINDS**:
     * **SCENARIO:** A BA keeps its three kinds of KPI when the host/service ids come from the global cache
     * **GIVEN** a centralized platform, where Broker answers the host/service questions from its global cache
     * **AND** a child BA of type "worst" built on service_314
     * **AND** a parent BA of type "worst" holding one KPI of each kind: service_303, a boolean rule on service_302 and the child BA
     * **WHEN** the three services are OK
     * **THEN** the parent BA is OK
     * **WHEN** service_314 alone becomes CRITICAL
     * **THEN** the parent BA is CRITICAL, which its BA KPI alone can explain
     * **WHEN** service_302 alone becomes CRITICAL
     * **THEN** the parent BA is CRITICAL, which its boolean KPI alone can explain
     * **WHEN** service_303 alone becomes CRITICAL
     * **THEN** the parent BA is CRITICAL, which its service KPI alone can explain
65. **CBABOOOR**:
     * **SCENARIO:** An OR boolean rule evaluates to CRITICAL as soon as one operand is true, even when the other service is UNKNOWN
     * **GIVEN** a BA of type "impact" with boolean rule "{service_302} IS CRITICAL OR {service_303} IS CRITICAL"
     * **AND** service_303 is passive and starts UNKNOWN
     * **WHEN** service_302 becomes CRITICAL
     * **THEN** the BA is CRITICAL (OR short-circuits on the first true operand)
66. **CBABOOORREL**:
     * **SCENARIO:** Updating a boolean rule and reloading broker and engine takes effect correctly
     * **GIVEN** a BA of type "impact" with boolean rule "{service_302} IS OK OR {service_303} IS OK"
     * **WHEN** service_302 and service_303 are CRITICAL
     * **THEN** the BA is CRITICAL
     * **WHEN** the boolean rule is updated to "{service_302} IS OK OR {service_304} IS OK" and broker and engine are reloaded
     * **AND** service_304 is OK
     * **THEN** the BA is OK
     * **WHEN** the boolean rule is restored to "{service_302} IS OK OR {service_303} IS OK" and broker and engine are reloaded
     * **AND** service_302 and service_303 are CRITICAL
     * **THEN** the BA is CRITICAL again
67. **CBAWORST**:
     * **SCENARIO:** A BA of type "worst" reacts to KPI state changes and broker stats are valid after reload
     * **GIVEN** BBDO version is 3.0.1
     * **AND** a Business Activity of type "worst" is configured with two services
     * **WHEN** all services are OK
     * **THEN** the Business Activity is OK
     * **WHEN** one service becomes UNKNOWN
     * **THEN** the Business Activity is UNKNOWN
     * **WHEN** that service becomes WARNING
     * **THEN** the Business Activity is WARNING
     * **WHEN** another service becomes CRITICAL
     * **THEN** the Business Activity is CRITICAL
     * **AND** broker stats show expected endpoints state
     * **WHEN** broker and engine are reloaded
     * **THEN** broker stats still show expected endpoints state
     * **AND** the GetBa gRPC command returns a valid digraph output
68. **CBAWORST2**:
     * **SCENARIO:** A BA of type "worst" with a boolean KPI and a child BA KPI reacts correctly to state changes
     * **GIVEN** BBDO version is 3.0.1
     * **AND** a Business Activity of type "worst" is configured with a boolean KPI and a child BA KPI
     * **WHEN** all KPIs are in an OK state
     * **THEN** the Business Activity is OK
     * **WHEN** the boolean rule becomes CRITICAL
     * **THEN** the Business Activity is CRITICAL
     * **WHEN** the child BA also becomes CRITICAL
     * **THEN** the Business Activity is still CRITICAL with both KPIs reported
     * **WHEN** the boolean rule recovers to OK
     * **THEN** the Business Activity remains CRITICAL due to the child BA KPI
69. **CBAWORST_ACK**:
     * **SCENARIO:** Acknowledging a service acknowledges the BA, and removing it unacknowledges the BA
     * **GIVEN** BBDO version is 3.0.1
     * **AND** a Business Activity of type "worst" is configured with two services
     * **WHEN** one of the services is acknowledged
     * **THEN** the Business Activity is acknowledged
     * **WHEN** the acknowledgement is removed from the service
     * **THEN** the Business Activity is no longer acknowledged
70. **CBA_BOOL_KPI**: With bbdo version 3.0.1, a BA of type 'worst' with 1 boolean kpi
71. **CBA_CHANGED**:
     * **SCENARIO:** Replace Service KPI with Boolean Rule KPI in Worst-type BA
     * **GIVEN** a BA of type "worst" is configured with one service KPI
     * **WHEN** the service KPI is replaced by a boolean rule KPI
     * **AND** Broker is reloaded
     * **THEN** the BA is correctly updated with the new KPI configuration
72. **CBA_DISABLED**: create a disabled BA with timeperiods and reporting filter don't create error message
73. **CBA_IMPACT_2KPI_SERVICES**: With bbdo version 3.0.1, a BA of type 'impact' with 2 serv, ba is critical only if the 2 services are critical
74. **CBA_IMPACT_IMPACT**:
     * **GIVEN** a Business Activity (BA) of type "impact"
     * **AND** it has two child BAs of type "impact"
     * **AND** the first child has an impact of 90
     * **AND** the second child has an impact of 10
     * **WHEN** both child BAs are impacting
     * **THEN** the parent BA should be "critical"
     * **WHEN** both child BAs are not impacting
     * **THEN** the parent BA should be "ok"
75. **CBA_RATIO_NUMBER_BA_4_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
76. **CBA_RATIO_NUMBER_BA_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 2 services and one ba with 1 service
77. **CBA_RATIO_PERCENT_BA_4_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
78. **CBA_RATIO_PERCENT_BA_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio percent' with 2 serv an 1 ba with one service
79. **CBA_SERVICE_PNAME_AFTER_RELOAD**:
     * **SCENARIO:** Verify that the parent_name of a BA service is not erased after a broker reload
     * **GIVEN** a BA "test" of type "worst" with its service "host_16:service_302"
     * **WHEN** I start broker and engine
     * **THEN** the BA service "test" should have a status of 0 within 30 seconds
     * **WHEN** I reload the broker
     * **THEN** the database should still contain a BA service with name "test" and parent_name "_Module_BAM_1"

### Benchmarks

This chapter contains 12 tests.

1. **BENCH_BAM_REBUILD**:
     * **SCENARIO:** measure the BI rebuild of the event durations
     * **GIVEN** a configuration database holding ${nb_ba} BAs
     * **AND** a reporting history of ${events_per_ba} closed events per BA, under one 24x7 reporting period
     * **AND** every BA flagged must_be_rebuild
     * **WHEN** the central cbd is started alone, with no poller at all
     * **THEN** the time reporting_stream spends recomputing the durations is filed in the store
     * **AND** the number of durations written is filed alongside it
2. **BENCH_BAM_STARTUP**:
     * **SCENARIO:** measure the BAM configuration load of a central cbd
     * **GIVEN** a configuration database holding ${nb_hosts} hosts and their services
     * **AND** ${nb_ba} BAs, each with ${kpi_per_ba} service KPIs and ${boolexp_per_ba} boolean rules
     * **AND** ${nb_meta} meta-service KPIs
     * **WHEN** the central cbd is started alone, with no poller at all
     * **THEN** the duration of every step of reader_v2::read() is filed in the store
     * **AND** the resident memory of cbd once loaded is filed alongside them
3. **BENCH_LOAD_ACTIVE**:
     * **SCENARIO:** measure what a nominal poller costs its machine
     * **GIVEN** an engine with ${nb_hosts} hosts and their services, actively checked
     * **AND** the two cbd running in BBDO3 with unified_sql
     * **WHEN** the collect daemons are measured for ${duration}s after a ${warmup}s warm-up
     * **THEN** the CPU, the memory and the cost per check are filed in the store
     * **AND** the run is rejected if no check was actually running
4. **BENCH_LOAD_PASSIVE**:
     * **SCENARIO:** measure what processing one check result costs
     * **GIVEN** an engine whose services are all passive, so no plugin is ever forked
     * **AND** ${passive_rate} results submitted every second, at a steady rate
     * **WHEN** the collect daemons are measured for ${duration}s after a ${warmup}s warm-up
     * **THEN** the cost of the chain is filed with the exact number of results submitted
     * **AND** the run is rejected if the results never reached the database
5. **BENCH_RRD_METRIC_RETENTION**: Benchmark: inject 12 h of back-fill data through the retention buffer and measure merge latency.  Injects ${N_OLD_POINTS} old-timestamped pb_metric events per metric (${N_METRICS} metrics) via BBDO v3 directly to the central broker, then one current-time event per metric to trigger the junction merge. Reports injection throughput and end-to-end merge latency.
6. **BENCH_START_CENTRALIZED_COLD**:
     * **SCENARIO:** measure a startup where Broker owns the configuration
     * **GIVEN** a poller whose configuration lives on the broker side
     * **AND** no state.prot on the engine side, so nothing local to start from
     * **WHEN** broker and engine are started in new generation
     * **THEN** the cost of receiving and applying the whole configuration is filed
7. **BENCH_START_LEGACY**:
     * **SCENARIO:** measure a startup that parses the text configuration
     * **GIVEN** an engine configured with ${nb_hosts} hosts and their services as .cfg files
     * **AND** no state.prot, so the text files are what gets read
     * **WHEN** engine is started and reaches its event loop
     * **THEN** the duration of every startup phase is filed in the store
8. **BENCH_START_PROTO**:
     * **SCENARIO:** measure a startup that reads a serialized configuration
     * **GIVEN** a poller that has already received its configuration from the broker once
     * **AND** therefore left a state.prot behind
     * **WHEN** engine is started again, the broker still running
     * **THEN** the configuration is deserialized instead of parsed, and expand and resolve are skipped
     The first start has to go through the centralized path: a plain BBDO3 engine
     configured from .cfg files never writes a state.prot, so a legacy first start
     would leave nothing to measure and the test would time out waiting for it.
9. **EALLOC1**:
     * **SCENARIO:** count the heap allocations done while processing check results
     * **GIVEN** an engine with 50 hosts and 1000 services, all of them passive
     * **AND** heaptrack attached to the running centengine
     * **THEN** ${nb_checks} check results carrying a realistic output are processed
     * **AND** the trace is complete once heaptrack has been detached
10. **EALLOC2**:
     * **SCENARIO:** count the heap allocations of the nominal, active check profile
     * **GIVEN** an engine with 50 hosts and 1000 services, all actively checked once a second
     * **AND** heaptrack attached to the running centengine
     * **THEN** checks run for ${duration} and the allocations are attributed per stack
     * **AND** the count per check is derived from the parse_check_output ratio
11. **EALLOC3**:
     * **SCENARIO:** same as EALLOC2, with a command line the length of a real check
     * **GIVEN** an engine with 50 hosts and 1000 services, all actively checked once a second
     * **AND** every check command carrying ten arguments instead of none
     * **AND** heaptrack attached to the running centengine
     * **THEN** checks run for ${duration} and the allocations are attributed per stack
     * **AND** the cost of parsing an argument vector can be read against EALLOC2
     EALLOC2 runs a bare plugin path, which is not what production looks like: an
     expanded check command carries ten or so arguments, and misc::command_line
     rebuilds its std::vector<char*> from an empty capacity at every exec. Measuring
     the fork path on EALLOC2 alone therefore understates it. Run both against the
     same binary and the difference is the price of the argument vector.
     Careful when reading the difference: a longer command line also makes macro
     expansion produce a longer string, so the two runs differ in more than argv.
     Attribution per stack separates them — misc::command_line::parse on one side,
     the macro functions on the other — a comparison of totals would not.
12. **EALLOC4**:
     * **SCENARIO:** count the heap allocations of cbd while it stores results
     * **GIVEN** an engine with 50 hosts and 1000 services, all actively checked once a second
     * **AND** heaptrack attached to the central cbd instead of to centengine
     * **THEN** checks run for ${duration} and the allocations are attributed per stack
     * **AND** the perf data parsing path of unified_sql is the dominant one
     The three profiles above trace centengine, which never parses a perf data string:
     common::perfdata::parse_perfdata is called by unified_sql and by the lua module,
     both of them living in cbd, and by the agent. Engine's own parse_perfdata, in
     anomalydetection.cc, is an unrelated namesake. So anything done to the perf data
     parser is invisible to EALLOC1-3 by construction, and this profile is where it
     shows: cbd stores the two metrics of every check result of every service.
     Same workload as EALLOC2 on purpose -- same plugin, same bare command line -- so
     that the two traces answer "who allocates on this workload, Engine or Broker?"

### Broker

This chapter contains 94 tests.

1. **BC1**: Central and RRD brokers are started. Then we check they are correctly connected. RRD broker is stopped. The connection is lost. Then RRD broker is started again. The connection is re-established. Central broker is stopped. The connection is lost. Then Central broker is started again. The connection is re-established.
2. **BCL1**: Starting broker with option '-s foobar' should return an error
3. **BCL2**: Starting broker with option '-s5' should work
4. **BCL3**: Starting broker with options '-D' should work and activate diagnose mode
5. **BCL4**: Starting broker with options '-s2' and '-D' should work.
6. **BCPC1**:
     * **SCENARIO:** a valid centralized poller configuration passes the check
     * **GIVEN** a centralized engine configuration for 1 poller
     * **AND** broker is started so its gRPC server answers
     * **WHEN** CheckPollerConfig is called on the poller configuration directory
     * **THEN** ok is true and there is no ERROR diagnostic
7. **BCPC10**:
     * **SCENARIO:** an object dropped from the configuration under check reads as undefined
     * **GIVEN** poller 1 whose configuration has been ingested and acknowledged
     * **WHEN** host_5 is removed from its configuration and a dependency still references it
     * **AND** CheckPollerConfig is called on that directory
     * **THEN** host_5 reads as defined nowhere, not as living on poller 1 itself
8. **BCPC2**:
     * **SCENARIO:** a timeperiod excluding a non-existent one makes the check fail
     * **GIVEN** a centralized engine configuration with an invalid timeperiod (exclude -> unknown)
     * **WHEN** CheckPollerConfig is called on the poller configuration directory
     * **THEN** ok is false and an ERROR diagnostic names the unresolved exclusion
9. **BCPC3**:
     * **SCENARIO:** several invalid timeperiods are all reported (return shape)
     * **GIVEN** a centralized engine configuration with two invalid timeperiods
     * **WHEN** CheckPollerConfig is called on the poller configuration directory
     * **THEN** ok is false, the response holds a list of {severity, message} diagnostics,
     * **AND** both missing exclusions are reported as errors
10. **BCPC4**:
     * **SCENARIO:** a contact with no host notification commands makes the check fail
     * **GIVEN** a centralized engine configuration where contact U1 has no host_notification_commands
     * **WHEN** CheckPollerConfig is called on the poller configuration directory
     * **THEN** ok is false and an ERROR diagnostic reports the missing host notification commands
11. **BCPC5**:
     * **SCENARIO:** a contact group with a non-existing member makes the check fail
     * **GIVEN** a centralized engine configuration with a contact group referencing an undefined contact
     * **WHEN** CheckPollerConfig is called on the poller configuration directory
     * **THEN** ok is false and an ERROR diagnostic reports the missing contact
12. **BCPC6**:
     * **SCENARIO:** a command defined twice under the same name makes the check fail
     * **GIVEN** a centralized engine configuration with a command defined twice
     * **WHEN** CheckPollerConfig is called on the poller configuration directory
     * **THEN** ok is false and an ERROR diagnostic names the duplicated command
13. **BCPC7**:
     * **SCENARIO:** a host group with a non-existing member makes the check fail
     * **GIVEN** a centralized engine configuration with a host group referencing an undefined host
     * **WHEN** CheckPollerConfig is called on the poller configuration directory
     * **THEN** ok is false and an ERROR diagnostic names the missing host
14. **BCPC8**:
     * **SCENARIO:** a service group with a non-existing member makes the check fail
     * **GIVEN** a centralized engine configuration with a service group referencing an undefined service
     * **WHEN** CheckPollerConfig is called on the poller configuration directory
     * **THEN** ok is false and an ERROR diagnostic names the missing service
15. **BCPC9**:
     * **SCENARIO:** a dependency on a host of another poller is named as such
     * **GIVEN** two pollers whose configurations have been ingested and acknowledged
     * **AND** a host dependency of poller 1 whose dependent host belongs to poller 2
     * **WHEN** CheckPollerConfig is called on the configuration directory of poller 1
     * **THEN** ok is false, as at ingestion: a dependency does not cross a poller boundary
     * **AND** the ERROR names poller 2 instead of claiming the host is defined nowhere
16. **BDB1**:
     * **GIVEN** a broker with a wrong unified_sql db_host
     * **WHEN** cbd starts
     * **THEN** it should log an error about the connection
     * **AND** it should not crash
17. **BDB2**:
     * **GIVEN** a broker with a wrong unified_sql db_password
     * **WHEN** cbd starts
     * **THEN** it should log an error about access denied
     * **AND** it should not crash
18. **BDB3**:
     * **GIVEN** a broker with a correct unified_sql user password
     * **WHEN** cbd starts
     * **THEN** the connection to the database should be established
19. **BDBM1**:
     * **FEATURE:** Broker and Engine Start/Stop with MariaDB
     * **SCENARIO:** Start broker and engine, then start MariaDB with different connection counts
     * **GIVEN** the broker and engine are started
     * **WHEN** MariaDB is started after them
     * **AND** the broker is configured with connections_count set to 1 and 3
     * **THEN** the connection to the database should be established for each configured connection
20. **BEDB1**:
     * **GIVEN** the broker and engine are started,
     * **WHEN** MariaDB is started after them,
     * **THEN** the connection to the database should be established
21. **BEDB2**:
     * **FEATURE:** SQL Connections via gRPC API
     * **SCENARIO:** Start broker and engine, stop MariaDB, then start it again
     * **GIVEN** the broker and engine are running
     * **WHEN** MariaDB is stopped and then started again
     * **THEN** the gRPC API should provide information about SQL connections
22. **BEDB3**:
     * **FEATURE:** SQL Connections via gRPC API
     * **SCENARIO:** Start broker and engine, then stop MariaDB and then start it again
     * **GIVEN** broker and engine are running
     * **WHEN** MariaDB is stopped and then started again
     * **THEN** the gRPC API should provide information about SQL connections
23. **BFC1**:
     * **SCENARIO:** Start broker with valid and invalid filters on an output
     * **GIVEN** Broker is configured with filters "neb", "foo", and "bar" on the unified SQL output
     * **WHEN** Broker is started
     * **THEN** error messages should appear for invalid categories "foo" and "bar"
     * **AND** only the valid "neb" filter should be applied
24. **BFC2**:
     * **SCENARIO:** Start broker with only invalid filters on an output
     * **GIVEN** Broker is configured with filters "doe", "foo", and "bar" on the unified SQL output
     * **WHEN** Broker is started
     * **THEN** error messages should appear for invalid categories
25. **BGRPCSS1**:
     * **SCENARIO:** Two broker instances with grpc stream start and stop cleanly
     * **GIVEN** central broker with grpc output and rrd broker with grpc input
     * **WHEN** both brokers are started and stopped 5 times with 100ms interval in new generation mode
     * **THEN** no coredump occurs and the connection is established each time
26. **BGRPCSS2**:
     * **SCENARIO:** Single broker instance with grpc starts and stops 10 times with 300ms interval
     * **GIVEN** central broker with grpc output
     * **WHEN** the broker is started and stopped 10 times with 300ms interval in new generation mode
     * **THEN** no coredump occurs
27. **BGRPCSS3**:
     * **SCENARIO:** Single broker instance with grpc starts and stops 5 times with 100ms interval
     * **GIVEN** central broker with grpc output
     * **WHEN** the broker is started and stopped 5 times with 100ms interval in new generation mode
     * **THEN** no coredump occurs
28. **BGRPCSS4**:
     * **SCENARIO:** Single broker instance with grpc starts and stops 10 times with 1s interval
     * **GIVEN** central broker with grpc output
     * **WHEN** the broker is started and stopped 10 times with 1s interval in new generation mode
     * **THEN** no coredump occurs
29. **BGRPCSS5**:
     * **SCENARIO:** Reversed grpc acceptor with one_peer_retention_mode starts and stops without deadlock
     * **GIVEN** central broker with grpc output in one_peer_retention_mode with no host configured
     * **WHEN** the broker is started and stopped 5 times with 1s interval in new generation mode
     * **THEN** no deadlock occurs
30. **BGRPCSSU1**:
     * **SCENARIO:** Two broker instances with unified_sql and grpc stream start and stop cleanly
     * **GIVEN** central broker with unified_sql and grpc output and rrd broker with grpc input
     * **WHEN** both brokers are started and stopped 5 times with 100ms interval in new generation mode
     * **THEN** no coredump occurs and the connection is established each time
31. **BGRPCSSU2**:
     * **SCENARIO:** Single broker instance with unified_sql and grpc starts and stops 10 times with 300ms interval
     * **GIVEN** central broker with unified_sql and grpc output
     * **WHEN** the broker is started and stopped 10 times with 300ms interval in new generation mode
     * **THEN** no coredump occurs
32. **BGRPCSSU3**:
     * **SCENARIO:** Single broker instance with unified_sql and grpc starts and stops 5 times with 100ms interval
     * **GIVEN** central broker with unified_sql and grpc output
     * **WHEN** the broker is started and stopped 5 times with 100ms interval in new generation mode
     * **THEN** no coredump occurs
33. **BGRPCSSU4**:
     * **SCENARIO:** Single broker instance with unified_sql and grpc starts and stops 10 times with 1s interval
     * **GIVEN** central broker with unified_sql and grpc output
     * **WHEN** the broker is started and stopped 10 times with 1s interval in new generation mode
     * **THEN** no coredump occurs
34. **BGRPCSSU5**:
     * **SCENARIO:** Reversed grpc acceptor with unified_sql and one_peer_retention_mode starts and stops without deadlock
     * **GIVEN** central broker with unified_sql and grpc output in one_peer_retention_mode
     * **WHEN** the broker is started and stopped 5 times with 1s interval in new generation mode
     * **THEN** no deadlock occurs
35. **BSCSS1**: Start-Stop two instances of broker and no coredump
36. **BSCSS2**: Start/Stop 10 times broker with 300ms interval and no coredump
37. **BSCSS3**: Start-Stop one instance of broker with tcp connection and no coredump
38. **BSCSS4**: Start/Stop 10 times broker with 1sec interval and no coredump
39. **BSCSSC1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is enabled on client side.
40. **BSCSSC2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is disabled on client side.
41. **BSCSSCG1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on client side.
42. **BSCSSCGRR1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on output side. Reversed connection with retention and grpc transport protocol.
43. **BSCSSCGRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on output side. Reversed connection with retention and grpc transport protocol.
44. **BSCSSCRR1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is enabled on client side. Connection reversed with retention.
45. **BSCSSCRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is disabled on client side. Connection reversed with retention.
46. **BSCSSG1**: Start-Stop two instances of broker and no coredump
47. **BSCSSG2**: Start/Stop 10 times broker with 300ms interval and no coredump
48. **BSCSSG3**: Start-Stop one instance of broker with grpc connection and no coredump
49. **BSCSSG4**: Start/Stop 10 times broker with 1sec interval and no coredump
50. **BSCSSGA1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. An authorization token is added on the server. Error messages are raised.
51. **BSCSSGA2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. An authorization token is added on the server and also on the client. All looks ok.
52. **BSCSSGRR1**: Start-Stop two instances of broker and no coredump, reversed and retention, with transport protocol grpc, start-stop 5 times.
53. **BSCSSK1**:
     * **SCENARIO:** Client uses tcp but server expects grpc - connection fails
     * **GIVEN** central broker is configured with a bbdo_server input using tcp on port 5669
     * **AND** central broker is configured with a bbdo_client output using tcp to port 5670
     * **AND** rrd broker is configured with a bbdo_server input using grpc on port 5670
     * **WHEN** both brokers are started in new generation mode
     * **THEN** an error is raised on the client side about corrupted data
54. **BSCSSK2**: Start-Stop two instances of broker, server configured with tcp and client with grpc. No connection established and error raised on client side.
55. **BSCSSP1**: Start-Stop two instances of broker and no coredump. The server contains a listen address
56. **BSCSSPRR1**: Start-Stop two instances of broker and no coredump. The server contains a listen address, reversed and retention. centreon-broker-master-rrd is then a failover.
57. **BSCSSR1**: Start-Stop two instances of broker and no coredump. Connection with bbdo_server/bbdo_client and reversed.
58. **BSCSSRR1**: Start-Stop two instances of broker and no coredump. Connection with bbdo_server/bbdo_client, reversed and retention. centreon-broker-master-rrd is then a failover.
59. **BSCSSRR2**: Start/Stop 10 times broker with 300ms interval and no coredump, reversed and retention. centreon-broker-master-rrd is then a failover.
60. **BSCSST1**: Start-Stop two instances of broker and no coredump. Encryption is enabled on client side.
61. **BSCSST2**: Start-Stop two instances of broker and no coredump. Encryption is enabled on client side.
62. **BSCSSTG1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. This is not sufficient, then an error is raised.
63. **BSCSSTG2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. It works with good certificates and keys.
64. **BSCSSTG3**: Start-Stop two instances of broker. The connection cannot be established if the server private key is missing and an error message explains this issue.
65. **BSCSSTGRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. It works with good certificates and keys. Reversed grpc connection with retention.
66. **BSCSSTRR1**: Start-Stop two instances of broker and no coredump. Encryption is enabled. transport protocol is tcp, reversed and retention.
67. **BSCSSTRR2**: Start-Stop two instances of broker and no coredump. Encryption is enabled.
68. **BSS1**: Start-Stop two instances of broker and no coredump
69. **BSS2**: Start/Stop 10 times broker with 300ms interval and no coredump
70. **BSS3**: Start-Stop one instance of broker 5 times and no coredump
71. **BSS4**: Start/Stop 10 times broker with 1sec interval and no coredump
72. **BSS5**: Start-Stop with reversed connection on TCP acceptor with only one instance and no deadlock
73. **BSSU1**: Start-Stop two instances of broker with BBDO3 and no coredump
74. **BSSU2**: Start/Stop 10 times broker (BBDO3) with 300ms interval and no coredump
75. **BSSU3**: Start-Stop one instance of broker (BBDO3) and no coredump
76. **BSSU4**: Start/Stop 10 times broker with 1sec interval and no coredump
77. **BSSU5**: Start-Stop with reversed connection on TCP acceptor with only one instance and no deadlock
78. **CBDBM1**:
     * **SCENARIO:** Broker reconnects to MariaDB after startup with configurable connection count
     * **GIVEN** the broker and engine are started in new generation mode before MariaDB
     * **WHEN** MariaDB is started after them with connections_count set to 1 then 3
     * **THEN** the broker reconnects with the configured number of connections each time
79. **CBEDB1**:
     * **SCENARIO:** Broker connects to MariaDB when the database is started after it
     * **GIVEN** the broker and engine are started in new generation mode
     * **WHEN** MariaDB is started after them
     * **THEN** the connection to the database should be established
80. **CBEDB2**:
     * **FEATURE:** SQL Connections via gRPC API
     * **SCENARIO:** Start broker and engine, stop MariaDB, then start it again
     * **GIVEN** the broker and engine are running
     * **WHEN** MariaDB is stopped and then started again
     * **THEN** the gRPC API should provide information about SQL connections
81. **CBEDB3**:
     * **FEATURE:** SQL Connections via gRPC API
     * **SCENARIO:** Start broker and engine, then stop MariaDB and then start it again
     * **GIVEN** broker and engine are running
     * **WHEN** MariaDB is stopped and then started again
     * **THEN** the gRPC API should provide information about SQL connections
82. **CBLBD**:
     * **SCENARIO:** Broker starts with default logger levels when no loggers section is configured
     * **GIVEN** central broker configured without a loggers section
     * **WHEN** broker is started in new generation mode
     * **THEN** the gRPC API reports the expected default log levels for all loggers
83. **CBLDIS1**:
     * **SCENARIO:** Broker starts with core logs disabled - sql logs still produced
     * **GIVEN** central broker configured with core logs 'disabled' and sql logs at debug level
     * **WHEN** broker is started in new generation mode
     * **THEN** sql log entries are produced
     * **AND** no core log entries are produced
84. **CBLEC1**:
     * **SCENARIO:** Core log level changed live from trace to debug via gRPC API
     * **GIVEN** central broker started with core logs at trace level in new generation mode
     * **WHEN** the core log level is changed to debug via the gRPC API
     * **THEN** the gRPC API reports the new core log level as debug
85. **CBLEC2**:
     * **SCENARIO:** Setting an invalid log level via gRPC API raises an error
     * **GIVEN** central broker started with core logs at trace level in new generation mode
     * **WHEN** the core log level is set to the invalid value 'foo' via the gRPC API
     * **THEN** an error message about the unknown enum value is returned
86. **CBLEC3**:
     * **SCENARIO:** Setting log level for a non-existent logger via gRPC API raises an error
     * **GIVEN** central broker started with core logs at trace level in new generation mode
     * **WHEN** the log level of the non-existent 'foo' logger is set via the gRPC API
     * **THEN** an error message about the missing logger is returned
87. **CBSCSSK2**:
     * **SCENARIO:** Client uses grpc but server expects tcp - connection fails
     * **GIVEN** central broker is configured with a bbdo_server input using grpc on port 5669
     * **AND** central broker is configured with a bbdo_client output using grpc to port 5670
     * **AND** rrd broker is configured with a bbdo_server input using tcp on port 5670
     * **WHEN** both brokers are started in new generation mode
     * **THEN** an error is raised on the client side about invalid protocol header
88. **CBSS1**:
     * **SCENARIO:** Two broker instances start and stop cleanly 5 times in new generation mode
     * **GIVEN** central and rrd brokers configured in new generation mode
     * **WHEN** both brokers are started and stopped 5 times immediately
     * **THEN** no coredump occurs each time
89. **CBSS2**:
     * **SCENARIO:** Single broker instance starts and stops 10 times with 300ms interval in new generation mode
     * **GIVEN** central broker configured in new generation mode
     * **WHEN** the broker is started and stopped 10 times with 300ms interval
     * **THEN** no coredump occurs
90. **CBSS3**:
     * **SCENARIO:** Single broker instance starts and stops 5 times immediately in new generation mode
     * **GIVEN** central broker configured in new generation mode
     * **WHEN** the broker is started and stopped 5 times immediately
     * **THEN** no coredump occurs
91. **CBSS4**:
     * **SCENARIO:** Single broker instance starts and stops 10 times with 1s interval in new generation mode
     * **GIVEN** central broker configured in new generation mode
     * **WHEN** the broker is started and stopped 10 times with 1s interval
     * **THEN** no coredump occurs
92. **CBSS5**:
     * **SCENARIO:** Reversed TCP connection with one_peer_retention_mode starts and stops without deadlock in new generation mode
     * **GIVEN** central broker configured with one_peer_retention_mode and no host on the rrd output
     * **WHEN** the broker is started and stopped 5 times with 1s interval in new generation mode
     * **THEN** no deadlock occurs
93. **CBSS_CBD**:
     * **SCENARIO:** Broker restart with unified_sql preserves non-null service and host states
     * **GIVEN** broker and engine are started in new generation mode
     * **AND** broker is then restarted
     * **WHEN** services and hosts are queried from the database for 30 seconds
     * **THEN** no service or host state is null
94. **START_STOP_CBD**: restart cbd with unified_sql services state must not be null after restart

### Broker/database

This chapter contains 15 tests.

1. **DEDICATED_DB_CONNECTION_1_yes**: count database connection
2. **DEDICATED_DB_CONNECTION_2_yes**: count database connection
3. **DEDICATED_DB_CONNECTION_3_no**: count database connection
4. **DEDICATED_DB_CONNECTION_3_yes**: count database connection
5. **NetworkDBFail6**:
     * **GIVEN** a Broker configured with 5 database connections
     * **WHEN** the network connection to the database (port 3306) is disrupted for 60 seconds
     * **THEN** Broker should lose database connectivity during the outage
     * **AND** should resume normal operations after network restoration
6. **NetworkDBFail7**:
     * **GIVEN** Broker is running with 5 database connections
     * **AND** Engine is connected to Broker
     * **AND** database queries are being executed successfully
     * **WHEN** the network connection on port 3306 is repeatedly disrupted (6 cycles of 10s down / 10s up)
     * **THEN** Broker should handle the intermittent network failures
     * **AND** should acknowledge all events once the network is stable
7. **NetworkDBFail8**:
     * **GIVEN** Broker with unified_sql and 3 database connections
     * **WHEN** database network is blocked until failure detection
     * **THEN** Broker should log database errors
     * **AND** should recover and execute pending statements after network restoration
8. **NetworkDBFailU6**:
     * **GIVEN** Broker is running with unified_sql and 5 database connections
     * **AND** Engine is connected to Broker using BBDO3 protocol
     * **AND** database queries are being executed successfully
     * **WHEN** the network connection on port 3306 is blocked for 60 seconds
     * **THEN** database operations should fail during the network outage
     * **AND** Broker should recover and acknowledge events after network restoration
9. **NetworkDBFailU7**:
     * **GIVEN** Broker is running with unified_sql and 5 database connections
     * **AND** Engine is connected to Broker using BBDO3 protocol
     * **AND** database queries are being executed successfully
     * **WHEN** the network connection on port 3306 is repeatedly disrupted (6 cycles of 10s down / 10s up)
     * **THEN** Broker should handle the intermittent network failures
     * **AND** should acknowledge all events once the network is stable
10. **NetworkDBFailU8**:
     * **GIVEN** Broker is running with unified_sql, BBDO3 protocol and 3 database connections
     * **AND** Engine is connected to Broker
     * **AND** database queries are being executed successfully
     * **WHEN** the network connection on port 3306 is blocked indefinitely
     * **THEN** Broker should detect the database failure and log appropriate errors
     * **AND WHEN** the network is restored
     * **THEN** Broker should reconnect and successfully execute pending statements
11. **NetworkDbFail1**: network failure test between broker and database (shutting down connection for 100ms)
12. **NetworkDbFail2**: network failure test between broker and database (shutting down connection for 1s)
13. **NetworkDbFail3**: network failure test between broker and database (shutting down connection for 10s)
14. **NetworkDbFail4**: network failure test between broker and database (shutting down connection for 30s)
15. **NetworkDbFail5**: network failure test between broker and database (shutting down connection for 60s)

### Broker/engine

This chapter contains 392 tests.

1. **ANO_CFG_SENSITIVITY_SAVED**: cfg sensitivity saved in retention
2. **ANO_DT1**: downtime on dependent service is inherited by ano
3. **ANO_DT2**:
     * **GIVEN** a service and its AD,
     * **WHEN** we delete downtime on dependent service, AD must not be in downtime anymore
4. **ANO_DT3**: delete downtime on anomaly don t delete dependent service one
5. **ANO_DT4**:
     * **SCENARIO:** Removing downtime from service keeps it on anomaly detection
     * **GIVEN** an anomaly detection is attached to a service
     * **AND** a downtime is set on both the service and the anomaly detection
     * **WHEN** the downtime is removed from the service
     * **THEN** the downtime should still be present on the anomaly detection
6. **ANO_EXTCMD_SENSITIVITY_SAVED**: extcmd sensitivity saved in retention
7. **ANO_JSON_SENSITIVITY_NOT_SAVED**: json sensitivity not saved in retention
8. **ANO_NOFILE**: an anomaly detection without threshold file must be in unknown state
9. **ANO_NOFILE_VERIF_CONFIG_NO_ERROR**: An anomaly detection without threshold file doesn't display error on config check
10. **ANO_OUT_LOWER_THAN_LIMIT**: an anomaly detection with a perfdata lower than lower limit make a critical state
11. **ANO_OUT_UPPER_THAN_LIMIT**: an anomaly detection with a perfdata upper than upper limit make a critical state
12. **ANO_TOO_OLD_FILE**: An anomaly detection with an oldest threshold file must be in unknown state
13. **AOUTLU1**: an anomaly detection with a perfdata upper than upper limit make a critical state with bbdo 3
14. **BAM_STREAM_FILTER**: With bbdo version 3.0.1, a BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. we watch its events
15. **BEACK10**:
     * **SCENARIO:** acknowledgements survive a Broker restart (cache persistence).
     * **GIVEN** a BBDO3 configuration and an acknowledged critical service
     * **WHEN** Broker is restarted while Engine keeps running
     * **THEN** GetAcknowledgements still lists it (restored from the persisted cache, not re-sent by Engine)
     * **WHEN** the service recovers
     * **THEN** the restored acknowledgement is closed and leaves the cache.
16. **BEACK2**: Configuration is made with BBDO3. Engine has a critical service. An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to OK. And the acknowledgement in database is deleted.
17. **BEACK4**: Configuration is made with BBDO3. Engine has a critical service. An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The acknowledgement is removed and the comment in the comments table has its deletion_time column updated.
18. **BEACK6**: Configuration is made with BBDO3. Engine has a critical service. An external command is sent to acknowledge it ; the acknowledgement is sticky. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to WARNING. And the acknowledgement in database is still there.
19. **BEACK8**: Engine has a critical service. It is configured with BBDO 3. An external command is sent to acknowledge it ; the acknowledgement is normal. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to WARNING. And the acknowledgement in database is removed (not sticky).
20. **BEACK9**:
     * **SCENARIO:** the Broker cache exposes acknowledgements through gRPC.
     * **GIVEN** a BBDO3 configuration (unified_sql, so the cache is enabled)
     * **WHEN** a critical service is acknowledged
     * **THEN** the GetAcknowledgements gRPC endpoint lists it in the Broker cache
     * **WHEN** the service recovers
     * **THEN** the acknowledgement leaves the Broker cache (and is not re-ingested).
21. **BEATOI11**: external command SEND_CUSTOM_HOST_NOTIFICATION with option_number=1 should work
22. **BEATOI12**: external command SEND_CUSTOM_HOST_NOTIFICATION with option_number>7 should fail
23. **BEATOI13**: external command Schedule Service Downtime with duration<0 should fail
24. **BEATOI21**: external command ADD_HOST_COMMENT and DEL_HOST_COMMENT should work
25. **BEATOI22**: external command DEL_HOST_COMMENT with comment_id<0 should fail
26. **BEATOI23**: external command ADD_SVC_COMMENT with persistent=0 should work
27. **BEBDRRD1**: A service is forced checked then a downtime is set on this service via Broker gRPC. The service is forced checked again and the downtime is removed. Then we should not get any error in cbd RRD of kind 'ignored update error in file...'.
28. **BEBDTHOSTFIXED**:
     * **SCENARIO:** Host downtime via Broker gRPC (BBDO3)
     * **GIVEN** a host downtime is scheduled via Broker gRPC
     * **THEN** 21 downtimes appear in the database (1 host + 20 services)
     * **WHEN** the host downtime is deleted
     * **THEN** the database contains 0 downtimes
29. **BEBDTIM**: New services with several pollers are created. Then downtimes are set on all configured hosts via Broker gRPC. This results in 5250 downtimes (250 hosts × 21). Then all downtimes are removed.
30. **BEBDTMASS1**:
     * **SCENARIO:** Mass downtime scheduling via Broker gRPC (BBDO3)
     * **GIVEN** 3 pollers with 50 hosts and 20 services each
     * **WHEN** host downtimes are scheduled via Broker gRPC on 50 hosts
     * **THEN** 1050 downtimes appear in the database (1 host + 20 services each)
     * **WHEN** all host downtimes are deleted via Broker gRPC
     * **THEN** the database contains 0 downtimes
31. **BEBDTSVCFIXED**:
     * **SCENARIO:** Single service downtime via Broker gRPC (BBDO3)
     * **GIVEN** a service downtime is scheduled via Broker gRPC
     * **THEN** 1 downtime appears in the database
     * **WHEN** the downtime is deleted via Broker gRPC
     * **THEN** the database contains 0 downtimes
32. **BEBDTSVCFIXED_CHECK_DEPTH**:
     * **SCENARIO:** Service downtime depth via Broker gRPC (BBDO3)
     * **GIVEN** a service downtime is scheduled via Broker gRPC
     * **THEN** the service scheduled_downtime_depth is 1 in the database
     * **WHEN** the downtime is deleted
     * **THEN** the service scheduled_downtime_depth is 0
33. **BEBDTSVCREN**:
     * **SCENARIO:** Service downtime survives service rename (Broker gRPC, BBDO3)
     * **GIVEN** a service downtime is scheduled via Broker gRPC
     * **WHEN** the service is renamed via Engine config reload
     * **THEN** the downtime is still active (tracked by ID, not name)
     * **WHEN** the downtime is deleted
     * **THEN** the database contains 0 downtimes
34. **BECC1**: Broker/Engine communication with compression between central and poller
35. **BECMT_DEL_ALL**:
     * **SCENARIO:** bulk deletion of comments
     * **GIVEN** several comments on a host and on a service
     * **WHEN** DEL_ALL_HOST_COMMENTS / DEL_ALL_SVC_COMMENTS are sent
     * **THEN** every matching active comment gets a deletion_time
36. **BECMT_DEL_SVC**:
     * **SCENARIO:** deleting a service comment by id
     * **GIVEN** a service comment has been added by external command
     * **WHEN** a DEL_SVC_COMMENT external command is sent with its internal_id
     * **THEN** the matching row in the "comments" table gets a deletion_time
37. **BECMT_DOWNTIME**:
     * **SCENARIO:** a downtime owns a comment
     * **GIVEN** a fixed downtime is scheduled on a service
     * **THEN** a downtime comment is created in the "comments" table
     * **WHEN** the downtime is deleted
     * **THEN** the downtime comment gets a deletion_time
38. **BECMT_FLAPPING**:
     * **SCENARIO:** a flapping service owns a comment
     * **GIVEN** flap detection is enabled on a service
     * **WHEN** the service flaps
     * **THEN** a flapping comment is created in the "comments" table
     * **WHEN** the service state stabilizes and flapping stops
     * **THEN** the flapping comment gets a deletion_time
39. **BECMT_RETENTION**:
     * **SCENARIO:** a persistent comment survives an Engine restart
     * **GIVEN** a persistent host comment
     * **WHEN** Engine is restarted (retention preserved)
     * **THEN** the comment is still active in the "comments" table
40. **BECMT_RETENTION_ACK**:
     * **SCENARIO:** an acknowledgement comment is still deletable after a restart
     * **GIVEN** a service is acknowledged (a non-persistent ack comment is created)
     * **WHEN** Engine is restarted (retention preserved)
     * **AND** the service goes back to OK so the acknowledgement is cleared
     * **THEN** the ack comment is deleted, proving its id survived the restart on the notifier
41. **BECT1**: Broker/Engine communication with anonymous TLS between central and poller
42. **BECT2**: Broker/Engine communication with TLS between central and poller with key/cert
43. **BECT3**: Broker/Engine communication with anonymous TLS and ca certificate
44. **BECT4**: Broker/Engine communication with TLS between central and poller with key/cert and hostname forced
45. **BECT_GRPC1**: Broker/Engine communication with GRPC and with anonymous TLS between central and poller
46. **BECT_GRPC2**: Broker/Engine communication with TLS between central and poller with key/cert
47. **BECT_GRPC3**: Broker/Engine communication with anonymous TLS and ca certificate
48. **BECT_GRPC4**: Broker/Engine communication with TLS between central and poller with key/cert and hostname forced
49. **BECUSTOMHOSTVAR**: external command CHANGE_CUSTOM_HOST_VAR on SNMPVERSION
50. **BECUSTOMSVCVAR**: external command CHANGE_CUSTOM_SVC_VAR on CRITICAL
51. **BEDTHOSTFIXED**: A downtime is set on a host, the total number of downtimes is really 21 (1 for the host and 20 for its 20 services) then we delete this downtime and the number is 0.
52. **BEDTHOSTFIXED1**:
     * **SCENARIO:** Setting and Removing Downtime on a Host and its Services
     * **GIVEN** a downtime is set on a host
     * **THEN** the total number of downtimes is 21
     * **AND** this includes 1 for the host and 20 for its services
     * **WHEN** the downtime is deleted
     * **THEN** the total number of downtimes is 0
53. **BEDTMASS1**:
     * **SCENARIO:** Setting and Removing Downtimes on Configured Hosts and Services
     * **GIVEN** new services with several pollers are created
     * **WHEN** downtimes are set on all configured hosts
     * **THEN** the total number of downtimes, including impacted services, is 1050
     * **AND** all these downtimes are removed
     * **AND** the test is performed with BBDO 3.0.0
54. **BEDTMASS2**:
     * **SCENARIO:** Setting and Removing Downtimes on Configured Hosts and Services
     * **GIVEN** new services with several pollers are created
     * **WHEN** downtimes are set on all configured hosts
     * **THEN** the total number of downtimes, including impacted services, is 1050
     * **AND** all these downtimes are removed
     * **AND** the test is performed with BBDO 2.0.0
55. **BEDTRRD1**: A service is forced checked then a downtime is set on this service. The service is forced checked again and the downtime is removed. This test is done with BBDO 3.0.0. Then we should not get any error in cbd RRD of kind 'ignored update error in file...'.
56. **BEDTSVCFIXED**:
     * **GIVEN** a unique downtime set on a service
     * **WHEN** the downtime is removed
     * **THEN** the downtime is well removed
     * **AND** the number of downtimes is 0
57. **BEDTSVCFIXED1**:
     * **GIVEN** a configuration with BBDO3 and a unique downtime set on a service
     * **WHEN** the downtime is removed
     * **THEN** the downtime is well removed
     * **AND** the number of downtimes is 0
58. **BEDTSVCREN1**:
     * **GIVEN** a downtime set on a service
     * **WHEN** the service is renamed
     * **THEN** the downtime is still active on the renamed service
     * **WHEN** the downtime is removed from the renamed service
     * **THEN** the downtime is well removed
59. **BEDTSVCREN2**:
     * **GIVEN** a configuration with BBDO3 and a downtime set on a service
     * **WHEN** the service is renamed
     * **THEN** the downtime is still active on the renamed service
     * **WHEN** the downtime is removed from the renamed service
     * **THEN** the downtime is well removed
60. **BEDW**:
     * **SCENARIO:** Verify Broker configured with cache_config_directory listens to it
     * **GIVEN** the Central Broker is started with cache_config_directory set to a specific Directory
     * **AND** the pollers_config_directory is set to its default value: /var/lib/centreon-broker/pollers-configuration.
     * **WHEN** a file of the form <poller_id>.lck is created in the cache_config_directory
     * **THEN** Broker logs a message telling the file has been created
     * **WHEN** the corresponding configuration directory doesn't exist
     * **THEN** Broker logs a message telling the directory doesn't exist
61. **BEDWEN**:
     * **SCENARIO:** Verify Broker configured with cache_config_directory listens to it
     * **GIVEN** the Central Broker is started with cache_config_directory set to a specific Directory
     * **AND** the pollers_config_directory is set (default value) to /var/lib/centreon-broker/pollers-configuration.
     * **WHEN** a file of the form <poller_id>.lck is created in the cache_config_directory
     * **THEN** Broker logs a message telling the file has been created
     * **WHEN** the corresponding configuration directory doesn't exist
     * **THEN** Broker logs a message telling the directory doesn't exist
62. **BEDWEND**:
     * **SCENARIO:** Verify Broker configured with cache_config_directory creates the protobuf serialized configuration
     * **GIVEN** Central Broker is started with cache_config_directory set to a specific Directory
     * **AND** the pollers_config_directory is set (default value) to /var/lib/centreon-broker/pollers-configuration.
     * **AND** Central Broker has already sent a first configuration to Engine
     * **WHEN** a new configuration is put into the cache_config_directory
     * **THEN** Engine should be notified about the new configuration by Broker
     * **AND** Engine should update its configuration from a differential configuration
63. **BEDWENF**:
     * **SCENARIO:** Verify Broker configured with cache_config_directory creates the protobuf serialized configuration
     * **GIVEN** the Central Broker is started with cache_config_directory set to a specific Directory
     * **AND** the pollers_config_directory is set (default value) to /var/lib/centreon-broker/pollers-configuration.
     * **WHEN** the export is announced by a pollers.lck naming the poller whose directory is filled correctly
     * **THEN** Broker logs a message telling the batch has been announced
     * **AND** Broker dumps a file <poller_id>.prot in the pollers_conf directory
64. **BEEXTCMD1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0
65. **BEEXTCMD10**: external command CHANGE_MAX_SVC_CHECK_ATTEMPTS on bbdo2.0
66. **BEEXTCMD11**: external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo3.0
67. **BEEXTCMD12**: external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo2.0
68. **BEEXTCMD13**: external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo3.0
69. **BEEXTCMD14**: external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo2.0
70. **BEEXTCMD15**: external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo3.0
71. **BEEXTCMD16**: external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo2.0
72. **BEEXTCMD17**: external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo3.0
73. **BEEXTCMD18**: external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo2.0
74. **BEEXTCMD19**: external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo3.0
75. **BEEXTCMD2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0
76. **BEEXTCMD20**: external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo2.0
77. **BEEXTCMD21**: external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS and ENABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo3.0
78. **BEEXTCMD22**: external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS and ENABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo2.0
79. **BEEXTCMD23**:
     * **GIVEN** Engine and broker configured with BBDO3
     * **WHEN** the external command DISABLE_HOST_CHECK on host_1 is executed
     * **THEN** the host_1 host checks should be disabled
     * **WHEN** the external command ENABLE_HOST_CHECK on host_1 is executed
     * **THEN** the host_1 host checks should be enabled
80. **BEEXTCMD24**: external command DISABLE_HOST_CHECK and ENABLE_HOST_CHECK on bbdo2.0
81. **BEEXTCMD25**: external command DISABLE_HOST_EVENT_HANDLER and ENABLE_HOST_EVENT_HANDLER on bbdo3.0
82. **BEEXTCMD26**: external command DISABLE_HOST_EVENT_HANDLER and ENABLE_HOST_EVENT_HANDLER on bbdo2.0
83. **BEEXTCMD27**: external command DISABLE_HOST_FLAP_DETECTION and ENABLE_HOST_FLAP_DETECTION on bbdo3.0
84. **BEEXTCMD28**: external command DISABLE_HOST_FLAP_DETECTION and ENABLE_HOST_FLAP_DETECTION on bbdo2.0
85. **BEEXTCMD29**: external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo3.0
86. **BEEXTCMD3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0
87. **BEEXTCMD30**: external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo2.0
88. **BEEXTCMD31**: external command DISABLE_HOST_SVC_CHECKS and ENABLE_HOST_SVC_CHECKS on bbdo3.0
89. **BEEXTCMD32**: external command DISABLE_HOST_SVC_CHECKS and ENABLE_HOST_SVC_CHECKS on bbdo2.0
90. **BEEXTCMD33**: external command DISABLE_HOST_SVC_NOTIFICATIONS and ENABLE_HOST_SVC_NOTIFICATIONS on bbdo3.0
91. **BEEXTCMD34**: external command DISABLE_HOST_SVC_NOTIFICATIONS and ENABLE_HOST_SVC_NOTIFICATIONS on bbdo2.0
92. **BEEXTCMD35**: external command DISABLE_PASSIVE_HOST_CHECKS and ENABLE_PASSIVE_HOST_CHECKS on bbdo3.0
93. **BEEXTCMD36**: external command DISABLE_PASSIVE_HOST_CHECKS and ENABLE_PASSIVE_HOST_CHECKS on bbdo2.0
94. **BEEXTCMD37**: external command DISABLE_PASSIVE_SVC_CHECKS and ENABLE_PASSIVE_SVC_CHECKS on bbdo3.0
95. **BEEXTCMD38**: external command DISABLE_PASSIVE_SVC_CHECKS and ENABLE_PASSIVE_SVC_CHECKS on bbdo2.0
96. **BEEXTCMD39**: external command START_OBSESSING_OVER_HOST and STOP_OBSESSING_OVER_HOST on bbdo3.0
97. **BEEXTCMD4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0
98. **BEEXTCMD40**: external command START_OBSESSING_OVER_HOST and STOP_OBSESSING_OVER_HOST on bbdo2.0
99. **BEEXTCMD41**: external command START_OBSESSING_OVER_SVC and STOP_OBSESSING_OVER_SVC on bbdo3.0
100. **BEEXTCMD42**: external command START_OBSESSING_OVER_SVC and STOP_OBSESSING_OVER_SVC on bbdo2.0
101. **BEEXTCMD5**: external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo3.0
102. **BEEXTCMD6**: external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo2.0
103. **BEEXTCMD7**: external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo3.0
104. **BEEXTCMD8**: external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo2.0
105. **BEEXTCMD9**: external command CHANGE_MAX_SVC_CHECK_ATTEMPTS with bbdo3.0
106. **BEEXTCMD_COMPRESS_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and compressed grpc
107. **BEEXTCMD_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and grpc
108. **BEEXTCMD_GRPC2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 and grpc
109. **BEEXTCMD_GRPC3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 and grpc
110. **BEEXTCMD_GRPC4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 and grpc
111. **BEEXTCMD_REVERSE_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and reversed gRPC
112. **BEEXTCMD_REVERSE_GRPC2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 and grpc reversed
113. **BEEXTCMD_REVERSE_GRPC3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 and grpc reversed
114. **BEEXTCMD_REVERSE_GRPC4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 and grpc reversed
115. **BEHOSTCHECK**:
     * **GIVEN** Engine and Broker configured to work with BBDO 3
     * **WHEN** a schedule forced host check command on host host_1 is launched
     * **THEN** the result appears in the centreon_storage resources table
116. **BEHS1**: store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
117. **BEINSTANCE**: Instance to bdd
118. **BEINSTANCESTATUS**: Instance status to bdd
119. **BENCV**: Engine is configured with hosts/services. The first host has no customvariable. Then we add a customvariable to the first host and we reload engine. Then the host should have this new customvariable defined and centengine should not crash.
120. **BENHG1**:
     * **GIVEN** a Centreon platform with 3 Engine instances
     * **AND** Broker is configured with RRD, central and module outputs
     * **AND** the central broker has 5 database connections
     * **WHEN** I create a host group containing 3 hosts
     * **AND** I reload both Broker and Engine configurations
     * **THEN** the membership of all 3 hosts to the host group should be logged
     * **AND** all membership entries should appear within 45 seconds
121. **BENHG4**:
     * **GIVEN** a platform with 3 Engine instances and unified_sql output with 5 connections
     * **AND** detailed logging is enabled on module0 (neb debug, core and processing error)
     * **WHEN** I create host group 1 with 3 hosts and reload configurations
     * **THEN** at least 2 host memberships should be logged within 45 seconds
     * **WHEN** I rename host group 1 to "hostgroup_test" and reload configurations
     * **THEN** the hostgroup name should be updated in database within 60 seconds
122. **BENHGU1**:
     * **GIVEN** a Centreon platform with 3 Engine instances
     * **AND** Broker is configured with RRD, central and module outputs
     * **AND** Broker uses unified_sql output for database operations
     * **AND** SQL logging is enabled at info level
     * **AND** the unified_sql output has 5 database connections
     * **WHEN** I create a host group containing 3 hosts
     * **AND** I reload both Broker and Engine configurations
     * **THEN** the membership of all 3 hosts to the host group should be logged
     * **AND** all membership entries should appear within 45 seconds
123. **BENHGU2**:
     * **GIVEN** a platform with 3 Engine instances and unified_sql output with 5 connections
     * **AND** BBDO3 protocol is enabled
     * **WHEN** I create a host group with 3 hosts and reload configurations
     * **THEN** at least 2 host memberships should be logged within 45 seconds
124. **BENHGU3**:
     * **GIVEN** a platform with 4 Engine instances and unified_sql output with 5 connections
     * **AND** BBDO3 protocol is enabled with SQL debug logging
     * **WHEN** I create host group 1 across 4 pollers with 3 hosts each and reload
     * **THEN** host group 1 should contain 12 host members within 30 seconds
     * **WHEN** I remove the hostgroups configuration from poller 0 and reload
     * **THEN** host group 1 should contain only 9 host members within 30 seconds
125. **BENHGU4_BBDO2**:
     * **GIVEN** a platform with 3 Engine instances and unified_sql output with 5 connections
     * **AND** detailed trace/debug logging is enabled (sql, lua, core)
     * **AND** a Lua output dumps host groups to /tmp/lua-engine.log
     * **AND** BBDO protocol version is configured based on test parameter
     * **WHEN** I create host group 1 with 3 hosts and reload configurations
     * **THEN** all 3 host memberships should be logged and stored in database within 60 seconds
     * **AND** the hostgroup should appear in the Lua output file
     * **WHEN** I rename host group 1 to "hostgroup_test" and reload configurations
     * **THEN** the hostgroup name should be updated in database within 60 seconds
     * **AND** the renamed hostgroup should appear in the Lua output file
     * **WHEN** I remove the host group configuration and reload
     * **THEN** the hostgroup should be deleted from database within 60 seconds
     * **AND** no hostgroup should appear in the Lua output file after 10 seconds
126. **BENHGU4_BBDO3**:
     * **GIVEN** a platform with 3 Engine instances and unified_sql output with 5 connections
     * **AND** detailed trace/debug logging is enabled (sql, lua, core)
     * **AND** a Lua output dumps host groups to /tmp/lua-engine.log
     * **AND** BBDO protocol version is configured based on test parameter
     * **WHEN** I create host group 1 with 3 hosts and reload configurations
     * **THEN** all 3 host memberships should be logged and stored in database within 60 seconds
     * **AND** the hostgroup should appear in the Lua output file
     * **WHEN** I rename host group 1 to "hostgroup_test" and reload configurations
     * **THEN** the hostgroup name should be updated in database within 60 seconds
     * **AND** the renamed hostgroup should appear in the Lua output file
     * **WHEN** I remove the host group configuration and reload
     * **THEN** the hostgroup should be deleted from database within 60 seconds
     * **AND** no hostgroup should appear in the Lua output file after 10 seconds
127. **BENSG1**:
     * **SCENARIO:** Service group creation and synchronization across multiple pollers
     * **GIVEN** 3 Engine pollers and Broker are started in non-centralized mode
     * **AND** the unified SQL output is configured with 5 database connections
     * **WHEN** a service group is created on poller 0 with services service_1, service_2, service_3 from host_1
     * **AND** servicegroups.cfg is added to poller 0 configuration
     * **AND** Broker and Engine are reloaded
     * **THEN** the central broker log should confirm that all 3 services are members of service group 1 on instance 1
128. **BENSGU1**: New service group with several pollers and connections to DB with broker configured with unified_sql
129. **BENSGU2**: New service group with several pollers and connections to DB with broker configured with unified_sql
130. **BENSGU3_BBDO2**: New service group with several pollers and connections to DB with broker and rename this servicegroup
131. **BENSGU3_BBDO3**: New service group with several pollers and connections to DB with broker and rename this servicegroup
132. **BENSVC1**: New services with several pollers
133. **BEOTEL_CENTREON_AGENT_CEIP**:
     * **SCENARIO:** Agent and "centreon_storage.agent_information" Statistics
     * **GIVEN** Engine connected to Broker
     * **WHEN** an agent connects to Engine
     * **THEN** a message is sent to Broker that results in a new row in the "centreon_storage.agent_information" table.
134. **BEOTEL_CENTREON_AGENT_CHECK_COUNTER**:
     * **GIVEN** an agent with counter check, we expect to get the correct status for the centagent process running on windows host
135. **BEOTEL_CENTREON_AGENT_CHECK_DIFFERENT_INTERVAL**:
     * **GIVEN** a Centreon Engine with OpenTelemetry server module configured
     * **AND** an OTEL connector using centreon_agent processor with 5s export period
     * **AND** 3 passive services configured with different check intervals (1, 2, 3 minutes)
     * **AND** interval_length is set to 10 seconds
     * **WHEN** the Engine, Broker and Agent are started
     * **THEN** service_1 should execute checks every 10 seconds (1*10) with 5s tolerance
     * **AND** service_2 should execute checks every 20 seconds (2*10) with 5s tolerance
     * **AND** service_3 should execute checks every 30 seconds (3*10) with 5s tolerance
     * **AND** all check intervals should be verified within 80 seconds
136. **BEOTEL_CENTREON_AGENT_CHECK_EVENTLOG**:
     * **GIVEN** an agent with eventlog check, we expect status, output and metrics
137. **BEOTEL_CENTREON_AGENT_CHECK_FILES**:
     * **GIVEN** an agent with file check, we expect to get the correct status for files under monitoring on the Windows host
138. **BEOTEL_CENTREON_AGENT_CHECK_HEALTH**: agent check health and we expect to get it in check result
139. **BEOTEL_CENTREON_AGENT_CHECK_HOST**:
     * **GIVEN** an agent host checked by centagent, we set a first output to check command,
     modify it, reload engine and expect the new output in resource table
140. **BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED**: agent check host with encrypted connection and we expect to get it in check result
141. **BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED_ENCRYPTED_CREDENTIALS**:
     * **GIVEN** an agent host checked by centagent over an encrypted connection,
     Engine use credentials encryption and send encrypted commands
     we set a first output to check command,
     modify it, reload engine and expect the new output in resource table
142. **BEOTEL_CENTREON_AGENT_CHECK_HOST_NO_ENCRYPTED_CREDENTIALS**:
     * **GIVEN** an agent host checked by centagent over a non encrypted connection,
     Engine use credentials encryption, but send no encrypted commands
     we set a first output to check command,
     modify it, reload engine and expect the new output in resource table
143. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_CPU**: agent check service with native check cpu and we expect to get it in check result
144. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_MEMORY**: agent check service with native check memory and we expect to get it in check result
145. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_SERVICE**: agent check service with native check service and we expect to get it in check result
146. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_STORAGE**: agent check service with native check storage and we expect to get it in check result
147. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_UPTIME**: agent check service with native check uptime and we expect to get it in check result
148. **BEOTEL_CENTREON_AGENT_CHECK_PROCESS**:
     * **GIVEN** an agent with eventlog check, we expect to get the correct status for thr centagent process running on windows host
149. **BEOTEL_CENTREON_AGENT_CHECK_SERVICE**: agent check service and we expect to get it in check result
150. **BEOTEL_CENTREON_AGENT_CHECK_TASKSCHEDULER**:
     * **GIVEN** an agent with task scheduler check, we expect to get the correct status for the centagent process running on windows host
151. **BEOTEL_CENTREON_AGENT_LINUX_NO_DEFUNCT_PROCESS**: agent check host and we expect to get it in check result
152. **BEOTEL_CENTREON_AGENT_NO_TRUSTED_TOKEN**:
     * **GIVEN** the Centreon Engine is configured with OpenTelemetry server with encryption enabled with no trusted_token
     * **WHEN** the Centreon Agent attempts to connect with tls
     * **THEN** the connection should be accepted
153. **BEOTEL_CENTREON_AGENT_TOKEN**:
     * **GIVEN** the Centreon Engine is configured with OpenTelemetry server with encryption enabled
     * **WHEN** the Centreon Agent attempts to connect using an valid JWT token
     * **THEN** the connection should be accepted
     * **AND** the log should confirm that the token is valid
154. **BEOTEL_CENTREON_AGENT_TOKEN_AGENT_TELEGRAPH**:
     * **GIVEN** an OpenTelemetry server is configured with token-based connection
     * **AND** the Centreon Agent is configured with a valid token
     * **WHEN** the agent attempts to connect to the server
     * **THEN** the connection should be successful
     * **AND** the log should confirm that the token is valid
     * **AND** Telegraf should connect and send data to the engine
155. **BEOTEL_CENTREON_AGENT_TOKEN_AGENT_TELEGRAPH_2**:
     * **GIVEN** an OpenTelemetry server is configured with token-based connection
     * **AND** the Centreon Agent is configured with a valid token that will expire
     * **WHEN** the agent attempts to connect to the server
     * **THEN** the connection should be successful
     * **AND** the log should confirm that the token is valid
     * **AND** Telegraf should connect and send data to the engine
156. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED**:
     * **GIVEN** the OpenTelemetry server is configured with encryption enabled
     * **AND** the server uses a public certificate and private key for secure communication
     * **WHEN** the Centreon Agent attempts to connect using an expired JWT token
     * **THEN** the connection should be refused
     * **AND** the log should contain the message "Token is expired"
157. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED_WHILE_RUNNING**:
     * **GIVEN** the OpenTelemetry server is configured with encryption enabled
     * **AND** the server uses a public certificate and private key for secure communication
     * **WHEN** the Centreon Agent attempts to connect using an JWT token valid
     * **THEN** the connection should be accepted
     * **WHEN** the token expires
     * **THEN** the connection should be refused
     * **AND** the log should contain the message "Token is expired"
158. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED_WHILE_RUNNING_REVERSE**:
     * **GIVEN** the Centreon Engine is configured as client with token and the agent as server with encryption enables
     * **WHEN** the Centreon engine attempts to connect using an valid JWT token
     * **THEN** the connection should be accepted
     * **WHEN** the token expires
     * **THEN** the connection should be refused
     * **AND** the log should contain the message "Token is expired"
159. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRE_REVERSE**:
     * **GIVEN** the Centreon Engine is configured as client with token and the agent as server with encryption enables
     * **WHEN** the Centreon engine attempts to connect using an valid JWT token but expired
     * **THEN** the connection should be refused
     * **AND** the log should confirm that the token is expired
160. **BEOTEL_CENTREON_AGENT_TOKEN_MISSING_HEADER**:
     * **GIVEN** the Centreon Engine is configured with OpenTelemetry server with encryption enabled
     * **WHEN** the Centreon Agent attempts to connect without a JWT token
     * **THEN** the connection should be refused
     * **AND** the log should contain the message "UNAUTHENTICATED: No authorization header"
161. **BEOTEL_CENTREON_AGENT_TOKEN_REVERSE**:
     * **GIVEN** the Centreon Engine is configured as client with token and the agent as server with encryption enables
     * **WHEN** the Centreon engine attempts to connect using an valid JWT token
     * **THEN** the connection should be accepted
     * **AND** the log should confirm that the token is valid
162. **BEOTEL_CENTREON_AGENT_TOKEN_UNTRUSTED**:
     * **GIVEN** the OpenTelemetry server is configured with encryption enabled
     * **AND** the server uses a public certificate and private key for secure communication
     * **WHEN** the Centreon Agent attempts to connect using an invalid JWT token
     * **THEN** the connection should be refused
     * **AND** the log should contain the message "Token is not trusted"
163. **BEOTEL_CENTREON_AGENT_TOKEN_UNTRUSTED_REVERSE**:
     * **GIVEN** the Centreon Engine is configured as client with token and the agent as server with encryption enables
     * **WHEN** the Centreon engine attempts to connect using an invalid JWT token
     * **THEN** the connection should be refused
     * **AND** the log should confirm that the token is not trusted
164. **BEOTEL_CENTREON_AGENT_WHITE_LIST**:
     * **SCENARIO:** Enforcing command whitelist for agent checks
     * **GIVEN** a whitelist file is created with allowed commands for host_1
     * **AND** the engine, broker, and agent are configured and started
     * **WHEN** a check command matching the whitelist is executed for host_1
     * **THEN** the check result is accepted and stored in the resources table
     * **WHEN** a check command not matching the whitelist is configured for host_1 and engine is reloaded
     * **THEN** the command is rejected and a "command not allowed by whitelist" message appears in the log
165. **BEOTEL_INVALID_CHECK_COMMANDS_AND_ARGUMENTS**:
     * **GIVEN** the agent is configured with native checks for services
     * **AND** the OpenTelemetry server module is added
     * **AND** services are configured with incorrect check commands and arguments
     * **WHEN** the broker, engine, and agent are started
     * **THEN** the resources table should be updated with the correct status
     * **AND** appropriate error messages should be generated for invalid checks
166. **BEOTEL_REVERSE_CENTREON_AGENT_CHECK_HOST**: agent check host with reversed connection and we expect to get it in check result
167. **BEOTEL_REVERSE_CENTREON_AGENT_CHECK_HOST_CRYPTED**: agent check host with encrypted reversed connection and we expect to get it in check result
168. **BEOTEL_REVERSE_CENTREON_AGENT_CHECK_SERVICE**: agent check service with reversed connection and we expect to get it in check result
169. **BEOTEL_SERVE_TELEGRAF_CONFIGURATION_CRYPTED**: we configure engine with a telegraf conf server and we check telegraf conf file
170. **BEOTEL_SERVE_TELEGRAF_CONFIGURATION_NO_CRYPTED**: we configure engine with a telegraf conf server and we check telegraf conf file
171. **BEOTEL_SERVE_TELEGRAF_CONFIGURATION_NO_CRYPTED_1**:
     * **SCENARIO:** Serve telegraf configuration with a complex whitelist
     * **GIVEN** the engine is configured with a telegraf conf server and a complex whitelist
     * **WHEN** I request the telegraf conf file for host_1
     * **THEN** I should receive the expected telegraf configuration for host_1
     * **AND** service_3 should be blacklisted and unavailable for host_1
     * **WHEN** I request the telegraf conf file for host_2
     * **THEN** I should receive the expected telegraf configuration for host_2
     * **AND** service_5 should be blacklisted and unavailable for host_2
172. **BEOTEL_TELEGRAF_CHECK_HOST**: we send nagios telegraf formatted data and we expect to get it in check result
173. **BEOTEL_TELEGRAF_CHECK_SERVICE**:
     * **SCENARIO:** Handling of OK and CRITICAL check results from Telegraf input
     * **GIVEN** the OpenTelemetry server is ready
     * **WHEN** I send a Telegraf-formatted check result with status "OK" to the Engine
     * **THEN** the result should be stored in the Centreon Broker storage database with status "OK"
     * **WHEN** I send a Telegraf-formatted check result with status "CRITICAL" to the Engine
     * **THEN** the result should be stored in the Centreon Broker storage database with status "CRITICAL" and state type "SOFT"
     * **WHEN** I send a Telegraf-formatted check result with status "CRITICAL" to the Engine
     * **THEN** the result should be stored in the Centreon Broker storage database with status "CRITICAL" and state type "SOFT"
     * **WHEN** I send a Telegraf-formatted check result with status "CRITICAL" to the Engine
     * **THEN** the result should be stored in the Centreon Broker storage database with status "CRITICAL" and state type "HARD"
174. **BEPBBEE1**: central-module configured with bbdo_version 3.0 but not others. Unable to establish connection.
175. **BEPBBEE3**: bbdo_version 3 generates new bbdo protobuf service status messages.
176. **BEPBBEE4**: bbdo_version 3 generates new bbdo protobuf host status messages.
177. **BEPBBEE5**: bbdo_version 3 generates new bbdo protobuf service messages.
178. **BEPBCVS**: bbdo_version 3 communication of custom variables.
179. **BEPBHostParent**: bbdo_version 3 communication of host parent relations
180. **BEPBINST_CONF**: bbdo_version 3 communication of instance configuration.
181. **BEPBRI1**: bbdo_version 3 use pb_resource new bbdo protobuf ResponsiveInstance message.
182. **BEPHG1**:
     * **SCENARIO:** Hostgroups added then removed one by one stay consistent between the database and the broker cache
     * **GIVEN** a central broker, a rrd broker and 5 engine instances in centralized mode
     With 50 hosts each (250 hosts total, numbered 1 to 250) and 20 services per host
     * **WHEN** broker and engines are started and the initial configuration is applied
     * **AND** hostgroups hg1 to hg5 are added one by one:
     hg1 contains all 250 hosts,
     hg2 contains the 125 hosts with even IDs,
     hg3 contains the 83 hosts with IDs divisible by 3,
     hg4 contains the 62 hosts with IDs divisible by 4,
     hg5 contains the 50 hosts with IDs divisible by 5
     * **THEN** after each addition the database and the broker cache are consistent:
     the hosts_hostgroups table has the expected number of entries,
     the hostgroup name in the cache matches hostgroup_<id>,
     * **AND** the member count in the cache matches the database
     * **AND WHEN** hostgroups hg1 to hg5 are removed one by one
     * **THEN** after each removal the database shows zero entries for that group
     * **AND** the broker cache no longer reports any members for that group
183. **BEPOLLERTZ**:
     * **SCENARIO:** GetPollers reports each poller's own local timezone
     * **GIVEN** three centralized Engine pollers started with distinct TZ environment variables (Europe/Paris, America/New_York, Asia/Shanghai)
     * **WHEN** they connect to the central Broker in BBDO3 centralized configuration mode and advertise their local timezone in the Welcome message
     * **THEN** the GetPollers gRPC method returns the three pollers, each with the timezone it advertised
184. **BEPSG1**:
     * **SCENARIO:** Servicegroups added then removed one by one stay consistent between the database and the broker cache
     * **GIVEN** a central broker, a rrd broker and 5 engine instances in centralized mode
     With 50 hosts each (250 hosts total, numbered 1 to 250) and 20 services per host
     (5000 services total, numbered 1 to 5000)
     * **WHEN** broker and engines are started and the initial configuration is applied
     * **AND** servicegroups sg1 to sg5 are added one by one:
     sg1 contains all 5000 services,
     sg2 contains the 2500 services with even IDs,
     sg3 contains the 1666 services with IDs divisible by 3,
     sg4 contains the 1250 services with IDs divisible by 4,
     sg5 contains the 1000 services with IDs divisible by 5
     * **THEN** after each addition the database and the broker cache are consistent:
     the services_servicegroups table has the expected number of entries,
     the servicegroup name in the cache matches servicegroup_<id>,
     * **AND** the member count in the cache matches the database
     * **AND WHEN** servicegroups sg1 to sg5 are removed one by one
     * **THEN** after each removal the database shows zero entries for that group
     * **AND** the broker cache no longer reports any members for that group
185. **BERD1**:
     * **SCENARIO:** Starting/stopping Broker does not create duplicated events.
     * **GIVEN** the broker configuration central  is set to Lua output test-doubles-c.lua
     * **AND** the broker configuration module0 is set to with Lua output test-doubles.lua
     * **WHEN** the broker and engine are started
     * **THEN** the Lua virtual machine should be initialized in both broker and engine logs
     * **AND** the engine and broker should be connected
     * **WHEN** the broker is kindly stopped and cache is cleared
     * **AND** the broker is restarted
     * **AND** the engine is stopped and broker is kindly stopped
     * **THEN** the contents of /tmp/lua-engine.log and /tmp/lua.log should match
     * **AND** there should be no duplicate events in the logs
186. **BERD2**:
     * **SCENARIO:** Starting/stopping Engine does not create duplicated events.
     * **GIVEN** the broker configuration central  is set to Lua output test-doubles-c.lua
     * **AND** the broker configuration module0 is set to with Lua output test-doubles.lua
     * **WHEN** the broker and engine are started
     * **THEN** the Lua virtual machine should be initialized in both broker and engine logs
     * **AND** the engine and broker should be connected
     * **WHEN** the engine is stopped
     * **AND** the engine is restarted
     * **AND** the engine is stopped and broker is kindly stopped
     * **THEN** the contents of /tmp/lua-engine.log and /tmp/lua.log should match
     * **AND** there should be no duplicate events in the logs
187. **BERDUC1**:
     * **SCENARIO:** Starting/stopping Broker does not create duplicated events in usual cases with unified_sql and BBDO 3.0
     * **GIVEN** the broker configuration central is set to Lua output test-doubles-c.lua
     * **AND** the broker configuration module0 is set to Lua output test-doubles.lua
     * **WHEN** the broker and engine are started
     * **THEN** the Lua virtual machine should be initialized in both broker and engine logs
     * **AND** the engine and broker should be connected
     * **WHEN** the broker is kindly stopped
     * **AND** the cache is cleared
     * **AND** the broker is restarted
     * **AND** the engine is stopped and broker is kindly stopped again
     * **THEN** there should be no duplicate events in the logs
188. **BERDUC2**:
     * **SCENARIO:** Starting/stopping Engine does not create duplicated events in usual cases with unified_sql and BBDO 3.0
     * **GIVEN** the broker configuration central is set to Lua output test-doubles-c.lua
     * **AND** the broker configuration module0 is set to Lua output test-doubles.lua
     * **WHEN** the broker and engine are started
     * **THEN** the Lua virtual machine should be initialized in both broker and engine logs
     * **AND** the engine and broker should be connected
     * **WHEN** the engine is stopped
     * **AND** the cache is cleared
     * **AND** the engine is restarted
     * **AND** the engine is stopped and broker is kindly stopped
     * **THEN** there should be no duplicate events in the logs
189. **BERDUCA300**:
     * **SCENARIO:** When the engine is stopped, it should emit a stop event and receive an ack event with events to clean from broker.
     * **GIVEN** the broker configuration central is set to Lua output test-doubles-c.lua
     * **AND** the broker configuration module0 is set to Lua output test-doubles.lua
     * **WHEN** the broker and engine are started
     * **THEN** the Lua virtual machine should be initialized in both broker and engine logs
     * **AND** the engine and broker should be connected
     * **WHEN** the engine is stopped
     * **THEN** the engine should emit a stop event
     * **AND** the broker should receive the stop event
     * **AND** the broker should send an ack for handled events
     * **AND** the engine should receive the ack for handled events from the broker
190. **BERDUCA301**:
     * **SCENARIO:** When the engine is stopped, it should emit a stop event and receive an ack event with events to clean from broker with bbdo 3.0.1.
     * **GIVEN** the broker configuration central is set to Lua output test-doubles-c.lua
     * **AND** the broker configuration module0 is set to Lua output test-doubles.lua
     * **WHEN** the broker and engine are started
     * **THEN** the Lua virtual machine should be initialized in both broker and engine logs
     * **AND** the engine and broker should be connected
     * **WHEN** the engine is stopped
     * **THEN** the engine should emit a stop event
     * **AND** the broker should receive the stop event
     * **AND** the broker should send an ack for handled events
     * **AND** the engine should receive the ack for handled events from the broker
191. **BERES1**: store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
192. **BERRDREC1**: RRD retention startup merge — metric.  Given Engine and Broker are started and at least one metric .rrd file is created When Broker is stopped and a 2-point MetricRetentionBatch .prot file is planted ...    for that metric (timestamps: now-24h and now-12h) And Broker is restarted Then the RRD stream logs a startup merge message for that metric And the merge completes ("merging 2 buffered points") And the .prot file is deleted
193. **BERRDREC2**: RRD retention startup merge — status.  Given Engine and Broker are started and a forced service check has created ...    a status .rrd file for service_1 (host_id=1, service_id=1) When Broker is stopped and a 2-point StatusRetentionBatch .prot file is planted ...    for that index And Broker is restarted Then the RRD stream logs a startup merge message for that index And the merge completes And the .prot file is deleted
194. **BESAU2**: New hosts with action_url with more than 2000 characters
195. **BESERVCHECK**: external command CHECK_SERVICE_RESULT
196. **BESN3**: New hosts with notes with more than 500 characters
197. **BESNU1**: New hosts with notes_url with more than 2000 characters
198. **BESS1**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
199. **BESS2**:
     * **SCENARIO:** Start and stop Broker/Engine with Broker started first and Engine stopped first
     * **GIVEN** the Broker is started before the Engine and both use BBDO 3
     * **WHEN** the Engine is started after the Broker
     * **THEN** the connection between Engine and Broker should be established
     * **AND** the poller should be visible in the database
     * **WHEN** the Engine is stopped before the Broker
     * **THEN** the poller should be disabled and not visible in the database
     * **AND** neither Broker nor Engine should crash
200. **BESS3**: Start-Stop Broker/Engine - Engine started first - Engine stopped first
201. **BESS4**: Start-Stop Broker/Engine - Engine started first - Broker stopped first
202. **BESS5**: Start-Stop Broker/engine - Engine debug level is set to all, it should not hang
203. **BESS6_GRPC**:
     * **SCENARIO:** Verify Broker and Engine start and establish connections
     * **GIVEN** the Central Broker, RRD Broker, and Central Engine are started
     * **WHEN** we check the connection between them
     * **THEN** the connection should be well established
     * **AND** the central broker should have two peers connected: the central engine and the RRD broker
     * **AND** the RRD broker should correctly recognize its peer as the Central Broker
204. **BESS6_TCP**:
     * **SCENARIO:** Verify Broker and Engine start and establish connections
     * **GIVEN** the Central Broker, RRD Broker, and Central Engine are started
     * **WHEN** we check the connection between them
     * **THEN** the connection should be well established
     * **AND** the central broker should have two peers connected: the central engine and the RRD broker
     * **AND** the RRD broker should correctly recognize its peer as the Central Broker
205. **BESSBQ1**: A very bad queue file is written for broker. Broker and Engine are then started, Broker must read the file raising an error because of that file and then get data sent by Engine.
206. **BESSCTO**:
     * **SCENARIO:** Service commands time out due to missing Perl Connector
     * **GIVEN** the Engine is configured as usual but without the Perl Connector
     * **WHEN** the Engine executes its service commands
     * **THEN** the commands take too long and reach the timeout
     * **AND** the Engine starts and stops two times as a result
207. **BESSCTOWC**:
     * **SCENARIO:** Service commands time out due to missing Perl Connector
     * **GIVEN** the Engine is configured as usual with some commands using the Perl Connector
     * **WHEN** the Engine executes its service commands
     * **THEN** the commands take too long and reach the timeout
     * **AND** the Engine starts and stops two times as a result
208. **BESSG**:
     * **SCENARIO:** Broker handles connection and disconnection with Engine
     * **GIVEN** Broker is configured with only one output that is Graphite
     * **WHEN** the Engine starts and connects to the Broker
     * **THEN** the Broker must be able to handle the connection
     * **WHEN** the Engine stops
     * **THEN** the Broker must be able to handle the disconnection
209. **BESS_CRYPTED_GRPC1**: Start-Stop grpc version Broker/Engine - well configured
210. **BESS_CRYPTED_GRPC2**: Start-Stop grpc version Broker/Engine only server crypted
211. **BESS_CRYPTED_GRPC3**: Start-Stop grpc version Broker/Engine only engine crypted
212. **BESS_CRYPTED_REVERSED_GRPC1**: Start-Stop grpc version Broker/Engine - well configured
213. **BESS_CRYPTED_REVERSED_GRPC2**: Start-Stop grpc version Broker/Engine only engine server crypted
214. **BESS_CRYPTED_REVERSED_GRPC3**: Start-Stop grpc version Broker/Engine only engine crypted
215. **BESS_ENGINE_DELETE_HOST**: once engine and cbd started, stop and restart cbd, delete an host and reload engine, cbd mustn't core
216. **BESS_GRPC1**: Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped first
217. **BESS_GRPC2**: Start-Stop grpc version Broker/Engine - Broker started first - Engine stopped first
218. **BESS_GRPC3**: Start-Stop grpc version Broker/Engine - Engine started first - Engine stopped first
219. **BESS_GRPC4**: Start-Stop grpc version Broker/Engine - Engine started first - Broker stopped first
220. **BESS_GRPC5**: Start-Stop grpc version Broker/engine - Engine debug level is set to all, it should not hang
221. **BESS_GRPC_COMPRESS1**: Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped last compression activated
222. **BESS_RELOAD_OUTPUT_ADD**:
     * **SCENARIO:** Adding an output to broker config during a reload is ignored
     * **GIVEN** Broker and Engine are started with their standard configuration
     * **WHEN** a new output is appended to the broker configuration file
     * **AND** broker is reloaded
     * **THEN** an error message is logged stating the output cannot be added at runtime
     * **AND** the new output does not appear in the broker stats
223. **BESS_RELOAD_OUTPUT_REMOVE**:
     * **SCENARIO:** Removing an output from broker config during a reload is ignored
     * **GIVEN** Broker and Engine are started with their standard configuration
     * **WHEN** the RRD output is removed from the broker configuration file
     * **AND** broker is reloaded
     * **THEN** an error message is logged stating the output cannot be removed at runtime
     * **AND** the RRD output is still present in broker stats
224. **BETAG1**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Broker is started before.
225. **BETAG2**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
226. **BEUTAG1**: Engine is configured with some tags. When broker receives them through unified_sql stream, it stores them in the centreon_storage.tags table. Broker is started before.
227. **BEUTAG10**: some services are configured with tags on two pollers. Then tags are removed from some of them and in centreon_storage, we can observe resources_tags table updated.
228. **BEUTAG11**:
     * **SCENARIO:** Updating resource tags after changing several tags
     * **GIVEN** some services are configured with tags on two pollers
     * **THEN** the resources_tags table contains them
     * **WHEN** several tags are changed
     * **THEN** the resources_tags table is updated
229. **BEUTAG12**: Engine is configured with some tags. Group tags tag2, tag6 are set to hosts 1 and 2. Category tags tag4 and tag8 are added to hosts 2, 3, 4. The resources and resources_tags tables are well filled. The tag6 and tag8 are removed and resources_tags is also well updated.
230. **BEUTAG2**: Engine is configured with some tags. A new service is added with a tag. Broker should make the relations.
231. **BEUTAG3**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
232. **BEUTAG4**: Engine is configured with some tags. Group tags tag9, tag13 are set to services 1 and 3. Category tags tag3 and tag11 are added to services 1, 3, 5 and 6. The centreon_storage.resources and resources_tags tables are well filled.
233. **BEUTAG5**: Engine is configured with some tags. Group tags tag2, tag6 are set to hosts 1 and 2. Category tags tag4 and tag8 are added to hosts 2, 3, 4. The resources and resources_tags tables are well filled.
234. **BEUTAG6**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.resources_tags table. Engine is started before.
235. **BEUTAG7**: Some services are configured with tags on two pollers. Then tags configuration is modified.
236. **BEUTAG8**: Services have tags provided by templates.
237. **BEUTAG9**: hosts have tags provided by templates.
238. **BEUTAG_REMOVE_HOST_FROM_HOSTGROUP**: remove a host from hostgroup, reload, insert 2 host in the hostgroup must not make sql error
239. **BE_BACKSLASH_CHECK_RESULT**: external command PROCESS_SERVICE_CHECK_RESULT with \:
240. **BE_DEFAULT_NOTIFICATION_INTERVAL_IS_ZERO_SERVICE_RESOURCE**: default notification_interval must be set to NULL in services, hosts and resources tables.
241. **BE_FLAPPING_GLOBAL_ADAPTIVE**:
     * **SCENARIO:** the program wide flap detection commands update the flapping flag of every object they touch
     * **GIVEN** a passive host that flaps and a passive service of ANOTHER host that flaps, with their flapping flag set in the "hosts", "services" and "resources" tables
     * **WHEN** flap detection is disabled program wide with DISABLE_FLAP_DETECTION
     * **THEN** the flapping flag of both objects is cleared
     * **WHEN** flap detection is enabled program wide with ENABLE_FLAP_DETECTION
     * **THEN** the flapping flag of both objects is set back, each carried by its own adaptive status
     * **AND** no check result was needed for that
242. **BE_FLAPPING_HOST_ADAPTIVE**:
     * **SCENARIO:** re-enabling flap detection on a host updates the flapping flag without waiting for a check
     * **GIVEN** a passive host that flaps, with its flapping flag set in the "hosts" and "resources" tables
     * **WHEN** flap detection is disabled on that host
     * **THEN** the flapping flag is cleared in both tables
     * **WHEN** flap detection is enabled again on that host
     * **THEN** the flapping flag is set back in both tables, carried by an adaptive host status
     * **AND** no check result was needed for that
243. **BE_FLAPPING_HOST_RESOURCE**: With BBDO 3, flapping detection must be set in hosts and resources tables.
244. **BE_FLAPPING_SERVICE_ADAPTIVE**:
     * **SCENARIO:** re-enabling flap detection on a service updates the flapping flag without waiting for a check
     * **GIVEN** a passive service that flaps, with its flapping flag set in the "services" and "resources" tables
     * **WHEN** flap detection is disabled on that service
     * **THEN** the flapping flag is cleared in both tables
     * **WHEN** flap detection is enabled again on that service
     * **THEN** the flapping flag is set back in both tables, carried by an adaptive service status
     * **AND** no check result was needed for that
245. **BE_FLAPPING_SERVICE_RESOURCE**: With BBDO 3, flapping detection must be set in services and resources tables.
246. **BE_FLAPPING_SERVICE_STOP_NO_EXTRA_CHECK**:
     * **SCENARIO:** the end of a flapping is published by the check result that caused it
     * **GIVEN** a passive service that flaps
     * **WHEN** stable check results are sent until Engine logs the end of the flapping
     * **AND** no further check result is sent
     * **THEN** the flapping flag is cleared in the "services" and "resources" tables
     * **AND** it did not wait for one more check result to get there
247. **BE_NOTIF_OVERFLOW**: bbdo 2.0 notification number =40000. make an overflow => notification_number null in db
248. **BE_TIME_NULL_SERVICE_RESOURCE**: With BBDO 3, notification_interval time must be set to NULL on 0 in services, hosts and resources tables.
249. **BRCS1**: Broker reverse connection stopped
250. **BRCTS1**: Broker reverse connection too slow
251. **BRCTSMN**:
     * **GIVEN** Broker, Engine configured as usual
     * **AND** map also connected to Broker with a filter allowing only 'neb' category
     * **WHEN** Engine sends pb_service, pb_host, pb_service_status and pb_host_status
     * **THEN** map receives correctly them.
252. **BRCTSMNS**:
     * **GIVEN** Broker, Engine configured as usual
     * **AND** map also connected to Broker with a filter allowing 'neb' and 'storage' categories
     * **WHEN** Engine sends pb_service, pb_host, pb_service_status, pb_host_status and metrics
     * **THEN** Map receives correctly them.
253. **BRGC1**: Broker good reverse connection
254. **BROKER_CACHE_HOST_NOTIF_DEPENDENCY**:
     * **SCENARIO:** the broker cache evaluates a host notification dependency, queried through gRPC
     * **GIVEN** host_2 depending on host_1 (notification failing on DOWN) fed to the broker cache in BBDO3
     * **WHEN** the master host_1 is UP then DOWN HARD
     * **THEN** NotificationAuthorizedByDependencies returns True then False for host_2, and always True for the independent host_3
255. **BROKER_CACHE_SVC_NOTIF_DEPENDENCY**:
     * **SCENARIO:** the broker cache evaluates a service notification dependency, queried through gRPC
     * **GIVEN** service_2 (host_2) depending on service_1 (host_1), notification failing on CRITICAL, fed to the broker cache in BBDO3
     * **WHEN** the master service_1 is CRITICAL HARD then OK HARD
     * **THEN** NotificationAuthorizedByDependencies returns False then True for (host_2, service_2)
256. **BRRDCDDID1**:
     * **SCENARIO:** RRD metrics deletion from index ids with rrdcached
     * **GIVEN** Broker is configured with an rrd output using the rrdcached socket
     * **AND** 3 metrics exist in the storage database
     * **WHEN** Broker and Engine are started and connected
     * **AND** a remove graphs request is sent for 2 indexes
     * **THEN** Broker logs that these indexes are erased from the database
     * **AND** the index status rrd files are removed from disk
     * **AND** the metrics rrd files matching these indexes are removed from disk
257. **BRRDCDDIDDB1**: RRD metrics deletion from index ids with a query in centreon_storage with rrdcached.
258. **BRRDCDDIDU1**: RRD metrics deletion from index ids with unified sql output with rrdcached.
259. **BRRDCDDM1**:
     * **SCENARIO:** RRD metrics deletion from metric ids with rrdcached
     * **GIVEN** Broker is configured with an rrd output using the rrdcached socket
     * **AND** 3 metrics exist in the storage database
     * **WHEN** Broker and Engine are started and connected
     * **AND** a remove graphs request is sent for these 3 metrics
     * **THEN** Broker logs that the metrics are erased from the database
     * **AND** the 3 corresponding rrd files are removed from disk
260. **BRRDCDDMDB1**: RRD metrics deletion from metric ids with a query in centreon_storage and rrdcached.
261. **BRRDCDDMID1**:
     * **SCENARIO:** RRD deletion of non existing metrics and indexes with rrdcached
     * **GIVEN** Broker is configured with an rrd output using the rrdcached socket
     * **WHEN** Broker and Engine are started and connected
     * **AND** a remove graphs request is sent for indexes and metrics that do not exist
     * **THEN** Broker logs that these indexes and metrics do not appear in the storage database
262. **BRRDCDDMIDU1**: RRD deletion of non existing metrics and indexes with rrdcached
263. **BRRDCDDMU1**:
     * **SCENARIO:** RRD metric deletion from metric ids with unified_sql output and rrdcached
     * **GIVEN** Broker is configured with a unified_sql output and an rrd output using the rrdcached socket
     * **AND** 3 metrics exist in the storage database
     * **WHEN** Broker and Engine are started and connected
     * **AND** a remove graphs request is sent for these 3 metrics
     * **THEN** Broker logs that the metrics are erased from the database
     * **AND** the 3 corresponding rrd files are removed from disk
264. **BRRDCDRB1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with storage/sql sql output and rrdcached.
265. **BRRDCDRBDB1**: RRD metric rebuild with a query in centreon_storage and unified sql with rrdcached
266. **BRRDCDRBU1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with unified_sql output and rrdcached.
267. **BRRDCDRBUDB1**: RRD metric rebuild with a query in centreon_storage and unified sql with rrdcached
268. **BRRDDIDDB1**: RRD metrics deletion from index ids with a query in centreon_storage.
269. **BRRDDIDU1**: RRD metrics deletion from index ids with unified sql output.
270. **BRRDDMDB1**: RRD metrics deletion from metric ids with a query in centreon_storage.
271. **BRRDDMIDU1**: RRD deletion of non existing metrics and indexes
272. **BRRDDMU1**: RRD metric deletion on table metric with unified sql output
273. **BRRDRBDB1**: RRD metric rebuild with a query in centreon_storage and unified sql
274. **BRRDRBUDB1**:
     * **SCENARIO:** RRD metric rebuild triggered from the storage database with unified_sql output
     * **GIVEN** Broker is configured with a unified_sql output and BBDO3
     * **AND** 3 metrics exist in the storage database
     * **WHEN** Broker and Engine are started and connected
     * **AND** 3 indexes to rebuild are collected (forcing service checks if needed)
     * **AND** these indexes are flagged to rebuild in the storage database and Broker is reloaded
     * **THEN** RRD starts, rebuilds and finishes rebuilding the matching metrics
     * **AND** the rebuilt rrd metric files hold the expected average value
275. **BRRDRMU1**:
     * **SCENARIO:** RRD metric rebuild through the gRPC API with unified_sql output
     * **GIVEN** Broker is configured with a unified_sql output and BBDO3
     * **AND** 3 metrics exist in the storage database
     * **WHEN** Broker and Engine are started and connected
     * **AND** 3 indexes to rebuild are collected (forcing service checks if needed)
     * **AND** a rebuild request is sent for these indexes through the gRPC API
     * **THEN** Central sends the metrics to rebuild and RRD starts, rebuilds and finishes them
     * **AND** the rebuilt rrd metric files hold the expected average value and RW group permission
     * **AND** the rebuilt rrd status files hold the expected average value
276. **BRRDSTATUS**: We are working with BBDO3. This test checks status are correctly handled independently from their value.
277. **BRRDSTATUSRETENTION**: We are working with BBDO3. This test checks status are not sent twice after Engine reload.
278. **BRRDUPLICATE**: RRD metric rebuild with a query in centreon_storage and unified sql with duplicate rows in database
279. **BRRDWM1**: We are working with BBDO3. This test checks protobuf metrics and status are sent to cbd RRD.
280. **CBD_RELOAD_AND_FILTERS**: We start engine/broker with a classical configuration. All is up and running. Some filters are added to the rrd output and cbd is reloaded. All is still up and running but some events are rejected. Then all is newly set as filter and all events are sent to rrd broker.
281. **CBD_RELOAD_AND_FILTERS_WITH_OPR**: We start engine/broker with an almost classical configuration, just the connection between cbd central and cbd rrd is reversed with one peer retention. All is up and running. Some filters are added to the rrd output and cbd is reloaded. All is still up and running but some events are rejected. Then all is newly set as filter and all events are sent to rrd broker.
282. **DTIM**: New services with several pollers are created. Then downtimes are set on all configured hosts. This action results on 5250 downtimes if we also count impacted services. Then all these downtimes are removed. This test is done with BBDO 3.0.1
283. **DT_LOG_BROKER**: notification_mode=broker: a host+service downtime handled by Broker (via gRPC) writes the SAME DOWNTIME ALERT STARTED and CANCELLED lines to the storage logs table as Engine does (see DT_LOG_ENGINE in downtimes.robot).
284. **DT_LOG_ENGINE**: Reference (notification_mode=engine): a host+service downtime writes the DOWNTIME ALERT STARTED and CANCELLED lines to the storage logs table (the GUI monitoring log).
285. **EBBM1**: A service status contains metrics that do not fit in a float number.
286. **EBBM2**:
     * **SCENARIO:** an output carrying more metrics than the cap allows is truncated
     * **GIVEN** a central broker whose unified_sql output carries max_perfdata=3
     * **AND** its perfdata logger low enough to let a warning through
     * **WHEN** a passive result carrying the five metrics m0 to m4 is submitted
     * **THEN** the central log announces the truncation to three of about five
     * **AND** the service carries m0, m1 and m2 in the metrics table
     * **AND** m4 never reaches it
287. **EBBM3**:
     * **SCENARIO:** without the option, every metric of the output is kept
     * **GIVEN** a central broker whose unified_sql output carries no max_perfdata
     * **WHEN** a passive result carrying the five metrics m0 to m4 is submitted
     * **THEN** the service carries all five of them
     * **AND** nothing is said about any truncation
     The witness of EBBM2, and not a formality: a run where the metrics never
     arrive at all, or where the submitted output holds three of them, would
     satisfy EBBM2 just as well. This is what ties the truncation to the
     option rather than to a broken pipeline.
288. **EBBPS1**: 1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table resources.
289. **EBBPS2**: 1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table services.
290. **EBDP1**: Four new pollers are started and then we remove Poller3.
291. **EBDP2**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
292. **EBDP3**: Three new pollers are started, then they are killed. It is still possible to remove Poller2 if removed from the configuration.
293. **EBDP4**: Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by Broker.
294. **EBDP5**: Four new pollers are started and then we remove Poller3.
295. **EBDP6**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
296. **EBDP7**: Three new pollers are started, then they are killed. It is still possible to remove Poller2 if removed from the configuration.
297. **EBDP8**: Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by broker.
298. **EBDP_CENTRALIZED_PROT**:
     * **SCENARIO:** removing a poller also removes the configuration Broker stores for it
     * **GIVEN** three pollers in centralized mode, each with its configuration acknowledged
     * **AND** Broker therefore holds a <id>.prot for each of them
     * **WHEN** every Engine is stopped and Poller3 is removed through the gRPC command
     * **THEN** 3.prot is gone, so a later start cannot load a poller that left the platform
     * **AND** the configurations of the two remaining pollers are untouched
299. **EBDP_GRPC2**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
300. **EBMSSM**: 1000 services are configured with 100 metrics each. The rrd output is removed from the broker configuration. GetSqlManagerStats is called to measure writes into data_bin.
301. **EBMSSMDBD**: 1000 services are configured with 100 metrics each. The rrd output is removed from the broker configuration. While metrics are written in the database, we stop the database and then restart it. Broker must recover its connection to the database and continue to write metrics.
302. **EBMSSMPART**:
     * **SCENARIO:** Broker continues writing metrics after partition recreation
     * **GIVEN** 1000 services are configured with 100 metrics each
     * **AND** the rrd output is removed from the broker configuration
     * **AND** the data_bin table is configured with two partitions "p1" and "p2"
     * **AND** "p1" contains old data
     * **AND** "p2" contains current data
     * **WHEN** metrics are being written in the database
     * **AND** the "p2" partition is removed
     * **AND** the "p2" partition is recreated
     * **THEN** the broker must recover its connection to the database
     * **AND** it must continue writing metrics
     * **WHEN** a last service check is forced
     * **THEN** its metrics must be written in the database
303. **EBPN0**: Verify if child is in queue when parent is down.
304. **EBPN1**: verify relation parent child when delete parent.
305. **EBPN2**: verify relation parent child when delete child.
306. **EBPS2**: 1000 services are configured with 20 metrics each. The rrd output is removed from the broker configuration to avoid to write too many rrd files. While metrics are written in bulk, the database is stopped. This must not crash broker.
307. **EBSAU2**: New services with action_url with more than 2000 characters
308. **EBSN3**: New services with notes with more than 500 characters
309. **EBSN4**: New hosts with No Alias / Alias and have A Template
310. **EBSNU1**: New services with notes_url with more than 2000 characters
311. **ENRSCHE1**: Verify that next check of a rescheduled host is made at last_check + interval_check
312. **FILTER_ON_LUA_EVENT**: stream connector with a bad configured filter generate a log error message
313. **FIRST_NOTIF_DELAY_EQUAL_RETRY_INTERVAL**:
     * **SCENARIO:** first notification delay equal to the retry interval
     * **GIVEN** a service whose first_notification_delay equals its retry interval
     * **WHEN** the service enters a CRITICAL HARD state
     * **THEN** the CRITICAL notification is sent after the delay
314. **FIRST_NOTIF_DELAY_GT_RETRY_INTERVAL**:
     * **SCENARIO:** first notification delay greater than the retry interval
     * **GIVEN** a service whose first_notification_delay is greater than its retry interval
     * **WHEN** the service enters a CRITICAL HARD state
     * **THEN** the CRITICAL notification is sent after the delay
315. **FIRST_NOTIF_DELAY_LT_RETRY_INTERVAL**:
     * **SCENARIO:** first notification delay smaller than the retry interval
     * **GIVEN** a service whose first_notification_delay is smaller than its retry interval
     * **WHEN** the service enters a CRITICAL HARD state
     * **THEN** the CRITICAL notification is sent after the delay
316. **FLAPPING_NOTIF**:
     * **SCENARIO:** a flapping service triggers a FLAPPINGSTART notification
     * **GIVEN** a service with flap detection and flapping notifications enabled
     * **WHEN** the service state oscillates enough to start flapping
     * **THEN** a FLAPPINGSTART notification is sent
317. **FORCED_NOTIF**:
     * **SCENARIO:** a forced custom notification bypasses the notification-enabled check
     * **GIVEN** a host with notifications disabled
     * **WHEN** a forced custom notification is sent for the host
     * **THEN** the notification is sent despite notifications being disabled
318. **GRPC_CLOUD_FAILURE**: simulate a broker failure in cloud environment, we provide a muted grpc server and there must remain only one grpc connection. Then we start broker and connection must be ok
319. **GRPC_RECONNECT**: We restart broker and engine must reconnect to it and send data
320. **HOSTGRP_NOTIF_DEPENDENCY**:
     * **SCENARIO:** host notifications are suppressed by a hostgroup dependency
     * **GIVEN** a hostgroup depending on another hostgroup that has already been notified
     * **WHEN** a host of the dependent hostgroup enters a DOWN state
     * **THEN** it sends no notification because its dependency already notified
321. **HOST_DOWN_ALERT**:
     * **SCENARIO:** a host going down raises a host alert
     * **GIVEN** a host configured with notifications
     * **WHEN** the host enters a DOWN HARD state
     * **THEN** a HOST ALERT is logged
322. **HOST_DOWN_NOTIF**:
     * **SCENARIO:** a host going down triggers a DOWN notification
     * **GIVEN** a host configured with notifications
     * **WHEN** the host enters a DOWN HARD state
     * **THEN** a DOWN notification is sent to its contact
323. **HOST_DOWN_NOTIF_BROKER**:
     * **SCENARIO:** in notification_mode=broker, Broker decides a HOST notification and dispatches its execution to the poller
     * **GIVEN** a host with a contact, in notification_mode=broker (BBDO3)
     * **WHEN** the host enters a DOWN HARD state
     * **THEN** Broker dispatches the host notification execution to the supervising poller
     * **AND** the poller runs the contact notification command (no local Engine decision)
324. **HOST_NOTIF_DEPENDENCY**:
     * **SCENARIO:** a host notification is suppressed by a host dependency
     * **GIVEN** a host depending on another host that has already been notified
     * **WHEN** the dependent host enters a DOWN state
     * **THEN** it sends no notification because its dependency already notified
325. **HOST_REC_NOTIF**:
     * **SCENARIO:** a host recovery triggers a recovery notification
     * **GIVEN** a host that sent a DOWN notification
     * **WHEN** the host returns to an UP HARD state
     * **THEN** a RECOVERY notification is sent
326. **HOST_REC_NOTIF_WITH_DT**:
     * **SCENARIO:** host notifications across a downtime episode until recovery
     * **GIVEN** a DOWN host for which a downtime is scheduled
     * **WHEN** the downtime suppresses notifications, then is removed while the host is still DOWN, and the host later returns to UP
     * **THEN** no notification is sent during the downtime, a DOWN notification is sent once it is removed, and a RECOVERY notification is sent on recovery
327. **LCDNU**: the lua cache updates correctly service cache.
328. **LCDNUH**: the lua cache updates correctly host cache
329. **LOGV2DB2**: log-v2 disabled old log disabled check broker sink
330. **LOGV2DF2**: log-v2 disabled old log disabled check logfile sink
331. **LOGV2EB1**: Checking broker sink when log-v2 is enabled and legacy logs are disabled.
332. **LOGV2EBU1**: Checking broker sink when log-v2 is enabled and legacy logs are disabled with bbdo3.
333. **LOGV2EF1**: log-v2 enabled    old log disabled check logfile sink
334. **LUA_CACHE_SAVE_BBDO3**:
     * **GIVEN** a engine broker configured in bbdo2, we check that services and hosts are stored in bbdo3 format in cache
     To do that we compare host and service event with lua cache
335. **MOVE_HOST_OF_HOSTGROUP_TO_ANOTHER_POLLER**:
     * **SCENARIO:** Moving hosts between pollers without losing hostgroup tag
     * **GIVEN** two pollers each with two hosts
     * **AND** all hosts belong to the same hostgroup
     * **WHEN** I move two hosts from one poller to the other
     * **THEN** the hostgroup tag of the moved hosts is not erased
336. **NON_TLS_CONNECTION_WARNING**:
     * **GIVEN** an agent starts a non-TLS connection,
     we expect to get a warning message.
337. **NON_TLS_CONNECTION_WARNING_ENCRYPTED**:
     * **GIVEN** agent with encrypted connection, we expect no warning message.
338. **NON_TLS_CONNECTION_WARNING_FULL**:
     * **GIVEN** an agent starts a non-TLS connection,
     we expect to get a warning message.
     After 1 hour, we expect to get a warning message about the connection time expired
     * **AND** the connection killed.
339. **NON_TLS_CONNECTION_WARNING_FULL_REVERSED**:
     * **GIVEN** an agent starts a non-TLS connection reverse,
     we expect to get a warning message.
     After 1 hour, we expect to get a warning message about the connection time expired
     * **AND** the connection killed.
340. **NON_TLS_CONNECTION_WARNING_REVERSED**:
     * **GIVEN** an agent starts a non-TLS connection reversed,
     we expect to get a warning message.
341. **NON_TLS_CONNECTION_WARNING_REVERSED_ENCRYPTED**:
     * **GIVEN** agent with encrypted reversed connection, we expect no warning message.
342. **NO_FILTER_NO_ERROR**: no filter configured => no filter error.
343. **REC_NOTIF_OUTSIDE_TP_NOT_SENT**:
     * **SCENARIO:** a recovery is not sent outside the notification period when the flag is off
     * **GIVEN** a CRITICAL service inside its notification period
     * **WHEN** the service recovers outside its notification period and send_recovery_notifications_anyway is off
     * **THEN** the CRITICAL notification is sent but no RECOVERY notification is sent
344. **REC_NOTIF_OUTSIDE_TP_SENT_WHEN_ENABLED**:
     * **SCENARIO:** a recovery is sent outside the notification period when the flag is on
     * **GIVEN** a CRITICAL service inside its notification period
     * **WHEN** the service recovers outside its notification period and send_recovery_notifications_anyway is on
     * **THEN** both the CRITICAL and the RECOVERY notifications are sent
345. **RENAME_PARENT**:
     * **GIVEN** an host with a parent host. We rename the parent host and check if the child host is still linked to the parent.
     Engine mustn't crash and log an error on reload.
346. **RLCode**: Test if reloading LUA code in a stream connector applies the changes
347. **RRD1**: RRD metric rebuild asked with gRPC API. Three non existing indexes IDs are selected then an error message is sent. This is done with unified_sql output.
348. **SDER**: The check attempts and the max check attempts of (host_1,service_1) are changed to 280 thanks to the retention.dat file. Then Engine and Broker are started and Broker should write these values in the services and resources tables. We only test the services table because we need a resources table that allows bigger numbers for these two attributes. But we see that Broker doesn't crash anymore.
349. **SEVERAL_FILTERS_ON_LUA_EVENT**: Two stream connectors with different filters are configured.
350. **SRVGRP_NOTIF_DEPENDENCY**:
     * **SCENARIO:** service notifications are suppressed by a servicegroup dependency
     * **GIVEN** a servicegroup depending on another servicegroup that has already been notified
     * **WHEN** a service of the dependent servicegroup enters a CRITICAL state
     * **THEN** it sends no notification because its dependency already notified
351. **SRV_CRIT_NOTIF**:
     * **SCENARIO:** a critical service triggers a notification
     * **GIVEN** a service configured with notifications enabled
     * **WHEN** the service enters a non-OK HARD state
     * **THEN** a CRITICAL notification is sent to its contact
352. **SRV_CRIT_NOTIF_BROKER**:
     * **SCENARIO:** in notification_mode=broker, Broker decides the notification and dispatches its execution to the poller
     * **GIVEN** a service with a contact, in notification_mode=broker (BBDO3)
     * **WHEN** the service enters a CRITICAL HARD state
     * **THEN** Broker dispatches the notification execution to the supervising poller
     * **AND** the poller runs the contact notification command (no local Engine decision)
353. **SRV_NOTIF_ALLOWED_BY_WLIST**:
     * **SCENARIO:** a service notification command allowed by the whitelist is executed
     * **GIVEN** a service in a non-OK HARD state and a whitelist allowing its notification command
     * **WHEN** the notification is triggered
     * **THEN** the notification command is executed
354. **SRV_NOTIF_BLOCKED_BY_WLIST**:
     * **SCENARIO:** a service notification command blocked by the whitelist is not executed
     * **GIVEN** a service in a non-OK HARD state and a whitelist not allowing its notification command
     * **WHEN** the notification is triggered
     * **THEN** the notification command is rejected by the whitelist
355. **SRV_NOTIF_BROKER_RESTART**:
     * **SCENARIO:** in notification_mode=broker, the notification chain survives a Broker restart
     * **GIVEN** a service that sent a CRITICAL notification decided by Broker
     * **WHEN** Broker is gracefully restarted (persisting and re-injecting its notification state)
     * **THEN** on the service recovery Broker still dispatches a RECOVERY notification to the very contact told about the problem
     proving the notification number and notified-contact set were persisted across the restart
356. **SRV_NOTIF_DEPENDENCY**:
     * **SCENARIO:** a service notification is suppressed by a service dependency
     * **GIVEN** a service depending on another service that has already been notified
     * **WHEN** the dependent service enters a CRITICAL state
     * **THEN** it sends no notification because its dependency already notified
357. **SRV_NOTIF_ESCALATIONS**:
     * **SCENARIO:** service notifications follow the configured escalations
     * **GIVEN** services with notification escalations to different contact groups
     * **WHEN** the services stay CRITICAL across successive notifications
     * **THEN** each escalation level notifies its configured contact group in turn
358. **SRV_NOTIF_ESCALATIONS_BROKER**:
     * **SCENARIO:** in notification_mode=broker, Broker resolves the escalations and dispatches the right contact group at each level
     * **GIVEN** services with notification escalations to different contact groups, in notification_mode=broker (BBDO3)
     * **WHEN** the services stay CRITICAL across successive notifications
     * **THEN** Broker (not Engine) picks the escalation contact group for each notification number
     * **AND** the poller runs the notification command for the contacts Broker selected
359. **SRV_NOTIF_MULTIPLE_COMMANDS**:
     * **SCENARIO:** a contact with several notification commands runs them all
     * **GIVEN** a contact configured with two service notification commands
     * **WHEN** a service enters a CRITICAL HARD state
     * **THEN** a notification is sent through each configured command
360. **SRV_NOTIF_ROUTED_TO_CORRECT_CTCT**:
     * **SCENARIO:** each service notification is routed to its own contact
     * **GIVEN** two services each assigned to a different contact
     * **WHEN** both services enter a CRITICAL HARD state
     * **THEN** each contact receives the notification for its own service
361. **SRV_NOTIF_SUPPR_BY_EMPTY_TP**:
     * **SCENARIO:** an empty notification period suppresses notifications
     * **GIVEN** a CRITICAL service whose notification period is then set to none
     * **WHEN** the service state changes
     * **THEN** no notification is sent because the notifier is out of its notification period
362. **SRV_NOTIF_SUPPR_DURING_DT**:
     * **SCENARIO:** notifications are suppressed while a service is in downtime
     * **GIVEN** a service with a scheduled downtime
     * **WHEN** the service enters a CRITICAL state during the downtime
     * **THEN** no notification is sent during the downtime
     * **AND** once the downtime is removed the CRITICAL and then RECOVERY notifications are sent
363. **SRV_NOTIF_SUPPR_DURING_DT_BROKER**:
     * **SCENARIO:** in notification_mode=broker, notifications are suppressed while a service is in downtime
     * **GIVEN** a service with a contact, in notification_mode=broker (BBDO3)
     * **AND** a scheduled downtime set on the service via Broker gRPC
     * **WHEN** the service enters a CRITICAL HARD state during the downtime
     * **THEN** Broker does not dispatch any notification execution
     * **AND** once the downtime is removed Broker dispatches the CRITICAL then RECOVERY notifications
364. **SRV_NOTIF_SUPPR_DURING_FLAPPING_BROKER**:
     * **SCENARIO:** in notification_mode=broker, notifications are suppressed while a service is flapping
     * **GIVEN** a passive service with a contact, in notification_mode=broker (BBDO3)
     * **AND** flap detection enabled on that service
     * **WHEN** the service flaps and then enters a CRITICAL HARD state
     * **THEN** Broker suppresses the notification because of the flapping
     * **AND** no execution is dispatched to the poller
     * **WHEN** flap detection is disabled on the service
     * **THEN** Broker dispatches the CRITICAL notification
365. **SRV_NOTIF_THEN_RM_AND_RELOAD**:
     * **SCENARIO:** removing a notified service that is in downtime and reloading does not crash Engine
     * **GIVEN** a service in a non-OK HARD state that has sent a notification and is under a scheduled downtime
     * **WHEN** the service is removed from the configuration and Engine and Broker are reloaded
     * **THEN** Engine keeps running without crashing
366. **SRV_REC_NOTIF**:
     * **SCENARIO:** a service recovery triggers a recovery notification
     * **GIVEN** a service that sent a CRITICAL notification
     * **WHEN** the service returns to an OK HARD state
     * **THEN** a RECOVERY notification is sent
367. **SRV_REC_NOTIF_AFTER_ACK**:
     * **SCENARIO:** a recovery notification is sent after an acknowledged problem recovers
     * **GIVEN** a CRITICAL service that has been acknowledged
     * **WHEN** the service returns to an OK HARD state
     * **THEN** a RECOVERY notification is sent
368. **SRV_REC_NOTIF_AFTER_ACK_BROKER**:
     * **SCENARIO:** in notification_mode=broker, a recovery notification is sent after an acknowledged problem recovers
     * **GIVEN** a service with a contact, in notification_mode=broker (BBDO3)
     * **WHEN** the service enters CRITICAL HARD, is acknowledged, then a new CRITICAL check occurs
     * **THEN** Broker suppresses the notification because of the acknowledgement
     * **AND WHEN** the service returns to OK HARD Broker dispatches the RECOVERY notification
369. **SRV_STATE_ALERTS_SOFT_AND_HARD**:
     * **SCENARIO:** successive state changes raise SOFT then HARD service alerts
     * **GIVEN** a service configured with several check attempts
     * **WHEN** the service stays CRITICAL over successive checks
     * **THEN** SOFT 1, SOFT 2 and HARD 3 service alerts are logged
370. **STORAGE_ON_LUA**: The category 'storage' is applied on the stream connector. Only events of this category should be sent to this stream.
371. **STUPID_FILTER**: Unified SQL is configured with only the bbdo category as filter. An error is raised by broker and broker should run correctly.
372. **Service_increased_huge_check_interval**:
     * **SCENARIO:** New services with huge check interval at creation time.
     * **GIVEN** Engine and Broker are configured with 1 poller and 10 hosts
     * **WHEN** Engine is started
     * **THEN** host_1 should be pending
     * **WHEN** a check result with metrics is processed for service_1
     * **THEN** metrics should be created and sent to rrd broker
     * **WHEN** service_1 metrics are analyzed
     * **THEN** metrics should have minimal heartbeat of 3000 and pdp_per_row of 300
     * **WHEN** a new service is created with a check interval of 90
     * **AND** Engine is reloaded
     * **THEN** the new service should be pending
     * **WHEN** a check result with metrics is processed for the new service
     * **THEN** metrics should be created and sent to rrd Broker
     * **WHEN** new service metrics are analyzed
     * **THEN** metrics should have minimal heartbeat of 54000 and pdp_per_row of 5400
373. **Services_and_bulks_1**: One service is configured with one metric with a name of 150 to 1021 characters.
374. **Services_and_bulks_2**: One service is configured with one metric with a name of 150 to 1021 characters.
375. **Start_Stop_Broker_Engine_1**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
376. **Start_Stop_Broker_Engine_2**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
377. **Start_Stop_Engine_Broker_1**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
378. **Start_Stop_Engine_Broker_2**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
379. **UNIFIED_SQL_FILTER**: With bbdo version 3.0.1, we watch events written or rejected in unified_sql
380. **VICT_ONE_CHECK_METRIC**: victoria metrics metric output
381. **VICT_ONE_CHECK_METRIC_AFTER_FAILURE**: victoria metrics metric output after victoria shutdown
382. **VICT_ONE_CHECK_STATUS**:
     * **SCENARIO:** Victoria metrics status output
     * **GIVEN** Broker is configured with a unified_sql output and a victoria_metrics output
     * **AND** a mock HTTP server listening for the victoria_metrics requests
     * **WHEN** Engine processes an OK service check result for service_314
     * **THEN** the mock server receives a status request with value 100
     * **WHEN** Engine processes a WARNING hard service result for service_314
     * **THEN** the mock server receives a status request with value 75
     * **WHEN** Engine processes a CRITICAL hard service result for service_314
     * **THEN** the mock server receives a status request with value 0
383. **Whitelist_Directory_NotReadable**:
     * **GIVEN** a centengine started by centreon-engine user, whitelist directories are not readable and centengine must log an error
384. **Whitelist_Directory_Rights**: log if /etc/centreon-engine-whitelist has not mandatory rights or owner
385. **Whitelist_Empty_Directory**: log if /etc/centreon-engine-whitelist is empty
386. **Whitelist_Host**: Test on allowed and forbidden commands for hosts
387. **Whitelist_No_Whitelist_Directory**: log if /etc/centreon-engine-whitelist doesn't exist
388. **Whitelist_NotReadable**:
     * **GIVEN** a centengine started by centreon-engine user, whitelist files are not readable and centengine must log an error
389. **Whitelist_Perl_Connector**: test allowed and forbidden commands for services
390. **Whitelist_Service**: test allowed and forbidden commands for services
391. **Whitelist_Service_EH**: test allowed and forbidden event handler for services
392. **metric_mapping**: Check if metric name exists using a stream connector

### Ccc

This chapter contains 8 tests.

1. **BECCC1**: ccc without port fails with an error message
2. **BECCC2**: ccc with -p 51001 connects to central cbd gRPC server.
3. **BECCC3**: ccc with -p 50001 connects to centengine gRPC server.
4. **BECCC4**: ccc with -p 51001 -l returns the available functions from Broker gRPC server
5. **BECCC5**: ccc with -p 51001 -l GetVersion returns an error because we can't execute a command with -l.
6. **BECCC6**: ccc with -p 51001 GetVersion{} calls the GetVersion command
7. **BECCC7**: ccc with -p 51001 GetVersion{"idx":1} returns an error because the input message is wrong.
8. **BECCC8**: ccc with -p 50001 EnableServiceNotifications{"names":{"host_name": "host_1", "service_name": "service_1"}} works and returns an empty message.

### Centralized/configuration

This chapter contains 97 tests.

1. **BECFGVAL1_batch**:
     * **SCENARIO:** PHP pushes an invalid poller configuration without asking for a CheckPollerConfig
     * **GIVEN** a centralized engine configuration where contact U1 has no host_notification_commands
     * **AND** Broker is started in centralized mode
     * **WHEN** the configuration change is notified to Broker (no CheckPollerConfig call)
     * **THEN** Broker refuses to push the configuration to the poller
     * **AND** it does not store the poller .prot configuration
2. **BECFGVAL1_per_poller**:
     * **SCENARIO:** PHP pushes an invalid poller configuration without asking for a CheckPollerConfig
     * **GIVEN** a centralized engine configuration where contact U1 has no host_notification_commands
     * **AND** Broker is started in centralized mode
     * **WHEN** the configuration change is notified to Broker (no CheckPollerConfig call)
     * **THEN** Broker refuses to push the configuration to the poller
     * **AND** it does not store the poller .prot configuration
3. **BECFGVAL2_batch**:
     * **SCENARIO:** PHP pushes a poller configuration with a contact group referencing an undefined contact
     * **GIVEN** a centralized engine configuration with a contact group whose member does not exist
     * **AND** Broker is started in centralized mode
     * **WHEN** the configuration change is notified to Broker (no CheckPollerConfig call)
     * **THEN** Broker refuses to push the configuration to the poller
     * **AND** it does not store the poller .prot configuration
4. **BECFGVAL2_per_poller**:
     * **SCENARIO:** PHP pushes a poller configuration with a contact group referencing an undefined contact
     * **GIVEN** a centralized engine configuration with a contact group whose member does not exist
     * **AND** Broker is started in centralized mode
     * **WHEN** the configuration change is notified to Broker (no CheckPollerConfig call)
     * **THEN** Broker refuses to push the configuration to the poller
     * **AND** it does not store the poller .prot configuration
5. **BECFGVAL3_batch**:
     * **SCENARIO:** PHP pushes a poller configuration with a host dependency referencing an undefined host
     * **GIVEN** a centralized engine configuration with a host dependency whose dependent host does not exist
     * **AND** Broker is started in centralized mode
     * **WHEN** the configuration change is notified to Broker (no CheckPollerConfig call)
     * **THEN** Broker refuses to push the configuration to the poller
     * **AND** it does not store the poller .prot configuration
6. **BECFGVAL3_per_poller**:
     * **SCENARIO:** PHP pushes a poller configuration with a host dependency referencing an undefined host
     * **GIVEN** a centralized engine configuration with a host dependency whose dependent host does not exist
     * **AND** Broker is started in centralized mode
     * **WHEN** the configuration change is notified to Broker (no CheckPollerConfig call)
     * **THEN** Broker refuses to push the configuration to the poller
     * **AND** it does not store the poller .prot configuration
7. **BECFGVAL4_batch**:
     * **SCENARIO:** PHP pushes a poller configuration with a service dependency referencing an undefined service
     * **GIVEN** a centralized engine configuration with a service dependency whose dependent service does not exist
     * **AND** Broker is started in centralized mode
     * **WHEN** the configuration change is notified to Broker (no CheckPollerConfig call)
     * **THEN** Broker refuses to push the configuration to the poller
     * **AND** it does not store the poller .prot configuration
8. **BECFGVAL4_per_poller**:
     * **SCENARIO:** PHP pushes a poller configuration with a service dependency referencing an undefined service
     * **GIVEN** a centralized engine configuration with a service dependency whose dependent service does not exist
     * **AND** Broker is started in centralized mode
     * **WHEN** the configuration change is notified to Broker (no CheckPollerConfig call)
     * **THEN** Broker refuses to push the configuration to the poller
     * **AND** it does not store the poller .prot configuration
9. **BECFGVAL5_batch**:
     * **SCENARIO:** PHP pushes a poller configuration with a host escalation referencing an undefined contact group
     * **GIVEN** a centralized engine configuration with a host escalation whose contact group does not exist
     * **AND** Broker is started in centralized mode
     * **WHEN** the configuration change is notified to Broker (no CheckPollerConfig call)
     * **THEN** Broker refuses to push the configuration to the poller
     * **AND** it does not store the poller .prot configuration
10. **BECFGVAL5_per_poller**:
     * **SCENARIO:** PHP pushes a poller configuration with a host escalation referencing an undefined contact group
     * **GIVEN** a centralized engine configuration with a host escalation whose contact group does not exist
     * **AND** Broker is started in centralized mode
     * **WHEN** the configuration change is notified to Broker (no CheckPollerConfig call)
     * **THEN** Broker refuses to push the configuration to the poller
     * **AND** it does not store the poller .prot configuration
11. **BECFGVAL6_batch**:
     * **SCENARIO:** PHP pushes a poller configuration with a service escalation referencing an undefined contact group
     * **GIVEN** a centralized engine configuration with a service escalation whose contact group does not exist
     * **AND** Broker is started in centralized mode
     * **WHEN** the configuration change is notified to Broker (no CheckPollerConfig call)
     * **THEN** Broker refuses to push the configuration to the poller
     * **AND** it does not store the poller .prot configuration
12. **BECFGVAL6_per_poller**:
     * **SCENARIO:** PHP pushes a poller configuration with a service escalation referencing an undefined contact group
     * **GIVEN** a centralized engine configuration with a service escalation whose contact group does not exist
     * **AND** Broker is started in centralized mode
     * **WHEN** the configuration change is notified to Broker (no CheckPollerConfig call)
     * **THEN** Broker refuses to push the configuration to the poller
     * **AND** it does not store the poller .prot configuration
13. **BECFGVAL7_batch**:
     * **SCENARIO:** PHP pushes a poller configuration with a host referencing an undefined notification period
     * **GIVEN** a centralized engine configuration where a host has a non-existing notification period
     * **AND** Broker is started in centralized mode
     * **WHEN** the configuration change is notified to Broker (no CheckPollerConfig call)
     * **THEN** Broker refuses to push the configuration to the poller
     * **AND** it does not store the poller .prot configuration
14. **BECFGVAL7_per_poller**:
     * **SCENARIO:** PHP pushes a poller configuration with a host referencing an undefined notification period
     * **GIVEN** a centralized engine configuration where a host has a non-existing notification period
     * **AND** Broker is started in centralized mode
     * **WHEN** the configuration change is notified to Broker (no CheckPollerConfig call)
     * **THEN** Broker refuses to push the configuration to the poller
     * **AND** it does not store the poller .prot configuration
15. **BECFGVAL8_batch**:
     * **SCENARIO:** PHP pushes a poller configuration with a service referencing an undefined notification period
     * **GIVEN** a centralized engine configuration where a service has a non-existing notification period
     * **AND** Broker is started in centralized mode
     * **WHEN** the configuration change is notified to Broker (no CheckPollerConfig call)
     * **THEN** Broker refuses to push the configuration to the poller
     * **AND** it does not store the poller .prot configuration
16. **BECFGVAL8_per_poller**:
     * **SCENARIO:** PHP pushes a poller configuration with a service referencing an undefined notification period
     * **GIVEN** a centralized engine configuration where a service has a non-existing notification period
     * **AND** Broker is started in centralized mode
     * **WHEN** the configuration change is notified to Broker (no CheckPollerConfig call)
     * **THEN** Broker refuses to push the configuration to the poller
     * **AND** it does not store the poller .prot configuration
17. **BECFGVAL9_batch**:
     * **SCENARIO:** PHP pushes a valid configuration for a poller that is not connected
     * **GIVEN** a valid centralized engine configuration for poller 1
     * **AND** Broker is started in centralized mode while Engine is left stopped
     * **WHEN** the configuration change is notified to Broker
     * **THEN** Broker prepares and stores the poller configuration once
     * **AND** it consumes the .lck, new-1.prot being what says the delivery is pending
     * **AND** nothing at all happens about that poller on the following cycles
18. **BECFGVAL9_per_poller**:
     * **SCENARIO:** PHP pushes a valid configuration for a poller that is not connected
     * **GIVEN** a valid centralized engine configuration for poller 1
     * **AND** Broker is started in centralized mode while Engine is left stopped
     * **WHEN** the configuration change is notified to Broker
     * **THEN** Broker prepares and stores the poller configuration once
     * **AND** it consumes the .lck, new-1.prot being what says the delivery is pending
     * **AND** nothing at all happens about that poller on the following cycles
19. **BECNHG1**:
     * **SCENARIO:** Host group synchronization across 3 pollers in centralized configuration
     * **GIVEN** a centralized engine with 3 pollers
     * **AND** broker is configured with RRD, central module, and SQL debug logging
     * **AND** database connections are set to 5 for both SQL and perfdata outputs
     * **WHEN** I start the broker and engine with new generation
     * **AND** I add a host group containing 3 hosts (host_1, host_2, host_3)
     * **AND** I notify broker of the engine configuration change
     * **THEN** the logs should confirm membership of all 3 hosts to the host group
     * **AND** each host should be properly associated with host group 1 on instance 1
20. **BECNHG3_batch**:
     * **SCENARIO:** Host group synchronization across 4 pollers in centralized configuration
     * **GIVEN** 4 pollers and Broker are started in centralized mode
     * **WHEN** hostgroup_1 is added with 3 hosts per poller (12 total)
     * **THEN** Broker receives all 12 hosts as hostgroup_1 members
     * **WHEN** hostgroup configuration files are removed sequentially from each poller
     * **THEN** Broker progressively removes corresponding hosts from database
     * **AND** hostgroup_1 membership decreases from 12 → 9 → 6 → 3 → 0
     * **AND** this holds whichever shape announces the export, pollers.lck or <ID>.lck
21. **BECNHG3_per_poller**:
     * **SCENARIO:** Host group synchronization across 4 pollers in centralized configuration
     * **GIVEN** 4 pollers and Broker are started in centralized mode
     * **WHEN** hostgroup_1 is added with 3 hosts per poller (12 total)
     * **THEN** Broker receives all 12 hosts as hostgroup_1 members
     * **WHEN** hostgroup configuration files are removed sequentially from each poller
     * **THEN** Broker progressively removes corresponding hosts from database
     * **AND** hostgroup_1 membership decreases from 12 → 9 → 6 → 3 → 0
     * **AND** this holds whichever shape announces the export, pollers.lck or <ID>.lck
22. **BECNHG4**:
     * **SCENARIO:** Host group rename synchronization in centralized configuration
     * **GIVEN** 3 pollers and Broker are started in centralized mode
     * **WHEN** hostgroup_1 is created on poller 1 with hosts: host_1, host_2, host_3
     * **THEN** Broker receives hostgroup_1 with its 3 members
     * **WHEN** hostgroup_1 is renamed to hostgroup_test
     * **THEN** Broker updates the hostgroup name to hostgroup_test in database
     * **AND** the same 3 hosts remain as members of hostgroup_test
23. **BECNHG5**:
     * **SCENARIO:** Host group removal and recreation with same hosts in centralized configuration
     * **GIVEN** 3 pollers and Broker are started in centralized mode
     * **WHEN** hostgroup_1 is created on each poller with different hosts
     * **THEN** Broker receives all hostgroups with their respective members
     * **WHEN** hostgroup_1 is removed from poller 1 and hostgroup_2 is created with the same hosts
     * **THEN** Broker updates the database to reflect the changes
     * **AND** hostgroup_2 contains the 3 hosts from poller 1
     * **AND** hostgroup_1 still contains the 6 hosts from pollers 2 and 3
24. **BECNSG1**:
     * **SCENARIO:** Service group creation and synchronization in centralized configuration
     * **GIVEN** 3 pollers and Broker are started in centralized mode
     * **WHEN** a service group is created on poller 1 with 3 services from host_1
     * **THEN** Broker receives the service group configuration
     * **AND** the 3 services are registered as members of the service group in logs
25. **BECNSG2_batch**:
     * **FEATURE:** Service Groups Management with Unified SQL Database
     * **SCENARIO:** Create 4 service groups (3 services each) across 4 pollers, then progressively
     remove servicegroups.cfg files to validate database consistency.
     Given: 4 Engine pollers + central Broker with unified SQL + BBDO3 + debug logs
     When: Create service groups and add servicegroups.cfg to each poller
     Then: Database should show 12 associations in services_servicegroups table
     When: Remove servicegroups.cfg from pollers sequentially
     Then: Associations should decrease by 3 for each removal (12→9→6→3→0)
     And: This holds whichever shape announces the export, pollers.lck or <ID>.lck
     Validates: Service group associations are correctly maintained during config changes
26. **BECNSG2_per_poller**:
     * **FEATURE:** Service Groups Management with Unified SQL Database
     * **SCENARIO:** Create 4 service groups (3 services each) across 4 pollers, then progressively
     remove servicegroups.cfg files to validate database consistency.
     Given: 4 Engine pollers + central Broker with unified SQL + BBDO3 + debug logs
     When: Create service groups and add servicegroups.cfg to each poller
     Then: Database should show 12 associations in services_servicegroups table
     When: Remove servicegroups.cfg from pollers sequentially
     Then: Associations should decrease by 3 for each removal (12→9→6→3→0)
     And: This holds whichever shape announces the export, pollers.lck or <ID>.lck
     Validates: Service group associations are correctly maintained during config changes
27. **BECNSG3_batch**:
     * **SCENARIO:** A servicegroup spread over three pollers is followed by the Lua cache through its whole life
     * **GIVEN** a central broker with a Lua output dumping the groups held in its cache
     * **AND** three pollers in centralized configuration
     * **WHEN** a servicegroup gathering services of the three pollers is added and announced
     * **THEN** the database holds its nine members and the Lua cache reports it by name
     * **WHEN** the servicegroup is renamed on the three pollers and announced again
     * **THEN** the database and the Lua cache both report the new name
     * **WHEN** the servicegroup is removed from the three pollers and announced again
     * **THEN** the database no longer holds any of its members
     * **AND** the Lua cache no longer reports any service group
     Note: the centralized configuration currently breaks the broker cache.
28. **BECNSG3_per_poller**:
     * **SCENARIO:** A servicegroup spread over three pollers is followed by the Lua cache through its whole life
     * **GIVEN** a central broker with a Lua output dumping the groups held in its cache
     * **AND** three pollers in centralized configuration
     * **WHEN** a servicegroup gathering services of the three pollers is added and announced
     * **THEN** the database holds its nine members and the Lua cache reports it by name
     * **WHEN** the servicegroup is renamed on the three pollers and announced again
     * **THEN** the database and the Lua cache both report the new name
     * **WHEN** the servicegroup is removed from the three pollers and announced again
     * **THEN** the database no longer holds any of its members
     * **AND** the Lua cache no longer reports any service group
     Note: the centralized configuration currently breaks the broker cache.
29. **BECNSVC1**:
     * **GIVEN** a Centreon platform with 3 pollers configured
     * **AND** 50 hosts distributed across pollers (17+17+16)
     * **AND** initially 20 services per host
     * **AND** BBDO3 protocol with unified SQL output enabled
     * **WHEN** the number of services per host is progressively increased
     * **AND** the configuration is hot-reloaded 3 times (20→24→28 services/host)
     * **THEN** each poller should monitor the correct number of resources
     * **AND** poller 1 should monitor exactly (17 hosts × services) + 17 hosts
     * **AND** poller 2 should monitor exactly (17 hosts × services) + 17 hosts
     * **AND** poller 3 should monitor exactly (16 hosts × services) + 16 hosts
     * **AND** each verification should complete within 30 seconds
     * **AND** the load balancing should remain stable during scaling
30. **BECNTAG1_batch**:
     * **FEATURE:** Tag associations in Broker gRPC cache with centralized configuration
     * **BACKGROUND:**
     * **GIVEN** 4 pollers are configured with 5 hosts each (20 total) and 20 services per host (400 total)
     * **AND** 4 tags are defined on every poller, one of each TagType:
     tag1 (id=1, SERVICEGROUP=0), tag2 (id=1, HOSTGROUP=1),
     tag3 (id=1, SERVICECATEGORY=2), tag4 (id=1, HOSTCATEGORY=3)
     * **AND** Broker and Engine are started in centralized (BBDO3) mode
     * **SCENARIO:** Phase 1 - All pollers assign tags
     * **GIVEN** all 4 pollers assign group_tags and category_tags to every host and service
     * **WHEN** Broker and Engine are started and synchronized
     * **THEN** the broker gRPC cache returns exactly the 20 expected hosts with HOSTGROUP tag 'tag2'
     * **AND** the broker gRPC cache returns exactly the 20 expected hosts with HOSTCATEGORY tag 'tag4'
     * **AND** the broker gRPC cache returns 400 services with SERVICEGROUP tag 'tag1', all on the 20 expected hosts
     * **AND** the broker gRPC cache returns 400 services with SERVICECATEGORY tag 'tag3', all on the 20 expected hosts
     * **SCENARIO:** Phase 2 - Tags removed from poller 3
     * **GIVEN** the initial state has 20 tagged hosts and 400 tagged services
     * **WHEN** group_tags and category_tags are removed from poller 3 and broker is notified
     * **THEN** the broker gRPC cache returns exactly hosts from pollers 0-2 with HOSTGROUP tag 'tag2'
     * **AND** the broker gRPC cache returns exactly hosts from pollers 0-2 with HOSTCATEGORY tag 'tag4'
     * **AND** the broker gRPC cache returns 300 services with SERVICEGROUP tag 'tag1', all on hosts from pollers 0-2
     * **AND** the broker gRPC cache returns 300 services with SERVICECATEGORY tag 'tag3', all on hosts from pollers 0-2
     * **SCENARIO:** Phase 3 - Tags removed from poller 2
     * **GIVEN** poller 3 tags have already been removed
     * **WHEN** group_tags and category_tags are removed from poller 2 and broker is notified
     * **THEN** the broker gRPC cache returns exactly hosts from pollers 0-1 with HOSTGROUP tag 'tag2'
     * **AND** the broker gRPC cache returns exactly hosts from pollers 0-1 with HOSTCATEGORY tag 'tag4'
     * **AND** the broker gRPC cache returns 200 services with SERVICEGROUP tag 'tag1', all on hosts from pollers 0-1
     * **AND** the broker gRPC cache returns 200 services with SERVICECATEGORY tag 'tag3', all on hosts from pollers 0-1
     * **SCENARIO:** Phase 4 - Tags removed from all remaining pollers
     * **GIVEN** pollers 2 and 3 tags have already been removed
     * **WHEN** group_tags and category_tags are removed from pollers 0 and 1 and broker is notified
     * **THEN** the broker gRPC cache returns 0 hosts with HOSTGROUP tag 'tag2'
     * **AND** the broker gRPC cache returns 0 hosts with HOSTCATEGORY tag 'tag4'
     * **AND** the broker gRPC cache returns 0 services with SERVICEGROUP tag 'tag1'
     * **AND** the broker gRPC cache returns 0 services with SERVICECATEGORY tag 'tag3'
     * **SCENARIO:** Phase 5 - Tag cache is empty (no orphan tags)
     * **GIVEN** all tags have been removed from all pollers
     * **WHEN** GetTags gRPC is called
     * **THEN** the broker tag cache returns an empty list (no orphan tags remain)
31. **BECNTAG1_per_poller**:
     * **FEATURE:** Tag associations in Broker gRPC cache with centralized configuration
     * **BACKGROUND:**
     * **GIVEN** 4 pollers are configured with 5 hosts each (20 total) and 20 services per host (400 total)
     * **AND** 4 tags are defined on every poller, one of each TagType:
     tag1 (id=1, SERVICEGROUP=0), tag2 (id=1, HOSTGROUP=1),
     tag3 (id=1, SERVICECATEGORY=2), tag4 (id=1, HOSTCATEGORY=3)
     * **AND** Broker and Engine are started in centralized (BBDO3) mode
     * **SCENARIO:** Phase 1 - All pollers assign tags
     * **GIVEN** all 4 pollers assign group_tags and category_tags to every host and service
     * **WHEN** Broker and Engine are started and synchronized
     * **THEN** the broker gRPC cache returns exactly the 20 expected hosts with HOSTGROUP tag 'tag2'
     * **AND** the broker gRPC cache returns exactly the 20 expected hosts with HOSTCATEGORY tag 'tag4'
     * **AND** the broker gRPC cache returns 400 services with SERVICEGROUP tag 'tag1', all on the 20 expected hosts
     * **AND** the broker gRPC cache returns 400 services with SERVICECATEGORY tag 'tag3', all on the 20 expected hosts
     * **SCENARIO:** Phase 2 - Tags removed from poller 3
     * **GIVEN** the initial state has 20 tagged hosts and 400 tagged services
     * **WHEN** group_tags and category_tags are removed from poller 3 and broker is notified
     * **THEN** the broker gRPC cache returns exactly hosts from pollers 0-2 with HOSTGROUP tag 'tag2'
     * **AND** the broker gRPC cache returns exactly hosts from pollers 0-2 with HOSTCATEGORY tag 'tag4'
     * **AND** the broker gRPC cache returns 300 services with SERVICEGROUP tag 'tag1', all on hosts from pollers 0-2
     * **AND** the broker gRPC cache returns 300 services with SERVICECATEGORY tag 'tag3', all on hosts from pollers 0-2
     * **SCENARIO:** Phase 3 - Tags removed from poller 2
     * **GIVEN** poller 3 tags have already been removed
     * **WHEN** group_tags and category_tags are removed from poller 2 and broker is notified
     * **THEN** the broker gRPC cache returns exactly hosts from pollers 0-1 with HOSTGROUP tag 'tag2'
     * **AND** the broker gRPC cache returns exactly hosts from pollers 0-1 with HOSTCATEGORY tag 'tag4'
     * **AND** the broker gRPC cache returns 200 services with SERVICEGROUP tag 'tag1', all on hosts from pollers 0-1
     * **AND** the broker gRPC cache returns 200 services with SERVICECATEGORY tag 'tag3', all on hosts from pollers 0-1
     * **SCENARIO:** Phase 4 - Tags removed from all remaining pollers
     * **GIVEN** pollers 2 and 3 tags have already been removed
     * **WHEN** group_tags and category_tags are removed from pollers 0 and 1 and broker is notified
     * **THEN** the broker gRPC cache returns 0 hosts with HOSTGROUP tag 'tag2'
     * **AND** the broker gRPC cache returns 0 hosts with HOSTCATEGORY tag 'tag4'
     * **AND** the broker gRPC cache returns 0 services with SERVICEGROUP tag 'tag1'
     * **AND** the broker gRPC cache returns 0 services with SERVICECATEGORY tag 'tag3'
     * **SCENARIO:** Phase 5 - Tag cache is empty (no orphan tags)
     * **GIVEN** all tags have been removed from all pollers
     * **WHEN** GetTags gRPC is called
     * **THEN** the broker tag cache returns an empty list (no orphan tags remain)
32. **BECNTAG2_batch**:
     * **FEATURE:** Tag rename is reflected in the Broker gRPC cache
     * **BACKGROUND:**
     * **GIVEN** 4 pollers configured with 5 hosts each (20 total) and 20 services per host
     * **AND** 4 tags (id=1, one per TagType) initially named tag1..tag4 on every poller
     * **AND** tags assigned to all hosts and services
     * **SCENARIO:** Tag names are updated in the broker cache after rename on all pollers
     * **GIVEN** the initial state has 20 tagged hosts and 400 tagged services
     * **WHEN** all 4 pollers rename their tags to tag11..tag14 (same ids, new names)
     * **THEN** broker GetTags returns exactly the 4 entries with the new names
     * **AND** GetHostsByTag with the new HOSTGROUP name returns all 20 hosts
     * **AND** GetServicesByTag with the new SERVICEGROUP name returns all 400 services
33. **BECNTAG2_per_poller**:
     * **FEATURE:** Tag rename is reflected in the Broker gRPC cache
     * **BACKGROUND:**
     * **GIVEN** 4 pollers configured with 5 hosts each (20 total) and 20 services per host
     * **AND** 4 tags (id=1, one per TagType) initially named tag1..tag4 on every poller
     * **AND** tags assigned to all hosts and services
     * **SCENARIO:** Tag names are updated in the broker cache after rename on all pollers
     * **GIVEN** the initial state has 20 tagged hosts and 400 tagged services
     * **WHEN** all 4 pollers rename their tags to tag11..tag14 (same ids, new names)
     * **THEN** broker GetTags returns exactly the 4 entries with the new names
     * **AND** GetHostsByTag with the new HOSTGROUP name returns all 20 hosts
     * **AND** GetServicesByTag with the new SERVICEGROUP name returns all 400 services
34. **BECNTAG3_batch**:
     * **FEATURE:** GetTags gRPC returns correct content while tags are active
     * **BACKGROUND:**
     * **GIVEN** 4 pollers configured with 5 hosts each (20 total) and 20 services per host
     * **AND** 4 tags (id=1, one per TagType) named tag1..tag4 on every poller
     * **AND** tags assigned to all hosts and services
     * **SCENARIO:** GetTags returns 4 entries with the correct names while tags are active
     * **WHEN** Broker and Engine are started and synchronized
     * **THEN** GetTags returns exactly 4 entries
     * **AND** the entry names are exactly {tag1, tag2, tag3, tag4}
35. **BECNTAG3_per_poller**:
     * **FEATURE:** GetTags gRPC returns correct content while tags are active
     * **BACKGROUND:**
     * **GIVEN** 4 pollers configured with 5 hosts each (20 total) and 20 services per host
     * **AND** 4 tags (id=1, one per TagType) named tag1..tag4 on every poller
     * **AND** tags assigned to all hosts and services
     * **SCENARIO:** GetTags returns 4 entries with the correct names while tags are active
     * **WHEN** Broker and Engine are started and synchronized
     * **THEN** GetTags returns exactly 4 entries
     * **AND** the entry names are exactly {tag1, tag2, tag3, tag4}
36. **BECNTAG4_batch**:
     * **FEATURE:** Broker cache is repopulated after broker restart with tags active
     * **BACKGROUND:**
     * **GIVEN** 4 pollers configured with 5 hosts each (20 total) and 20 services per host
     * **AND** 4 tags (id=1, one per TagType) named tag1..tag4 assigned to all hosts/services
     * **SCENARIO:** After broker restart, GetTags and GetHostsByTag return correct data
     * **GIVEN** broker and engine are started and synchronized
     * **AND** GetTags returns 4 entries before broker stops
     * **WHEN** broker is stopped and restarted (engine keeps running)
     * **THEN** GetTags returns the same 4 entries after restart
     * **AND** GetHostsByTag returns all 20 expected hosts
     * **AND** GetServicesByTag returns all 400 expected services
37. **BECNTAG4_per_poller**:
     * **FEATURE:** Broker cache is repopulated after broker restart with tags active
     * **BACKGROUND:**
     * **GIVEN** 4 pollers configured with 5 hosts each (20 total) and 20 services per host
     * **AND** 4 tags (id=1, one per TagType) named tag1..tag4 assigned to all hosts/services
     * **SCENARIO:** After broker restart, GetTags and GetHostsByTag return correct data
     * **GIVEN** broker and engine are started and synchronized
     * **AND** GetTags returns 4 entries before broker stops
     * **WHEN** broker is stopped and restarted (engine keeps running)
     * **THEN** GetTags returns the same 4 entries after restart
     * **AND** GetHostsByTag returns all 20 expected hosts
     * **AND** GetServicesByTag returns all 400 expected services
38. **BECPN0**:
     * **FEATURE:** Parent-Child Host Dependency Management
     As a monitoring administrator
     I want child host checks to be queued when parent hosts are down
     So that unnecessary checks are avoided
39. **BECPN1_batch**:
     * **FEATURE:** Parent Host Deletion Management
     As a monitoring administrator
     I want parent-child relationships to be cleaned up when parent hosts are deleted
     So that orphaned relationships don't exist in the system
     * **SCENARIO:** Parent-child relationship cleanup on parent deletion
     * **GIVEN** host_1 is configured as parent of host_2
     * **AND** the monitoring system is running
     * **AND** the parent-child relationship exists in the database
     * **WHEN** I delete host_1 from the configuration
     * **AND** I notify Broker about that change in the engine configuration
     * **THEN** host_2 should have no parent hosts
     * **AND** the parent-child relationship should be removed from the database
40. **BECPN1_per_poller**:
     * **FEATURE:** Parent Host Deletion Management
     As a monitoring administrator
     I want parent-child relationships to be cleaned up when parent hosts are deleted
     So that orphaned relationships don't exist in the system
     * **SCENARIO:** Parent-child relationship cleanup on parent deletion
     * **GIVEN** host_1 is configured as parent of host_2
     * **AND** the monitoring system is running
     * **AND** the parent-child relationship exists in the database
     * **WHEN** I delete host_1 from the configuration
     * **AND** I notify Broker about that change in the engine configuration
     * **THEN** host_2 should have no parent hosts
     * **AND** the parent-child relationship should be removed from the database
41. **BECPN2_batch**:
     * **FEATURE:** Child Host Deletion Management
     As a monitoring administrator
     I want parent-child relationships to be cleaned up when child hosts are deleted
     So that orphaned relationships don't exist in the system
     * **SCENARIO:** Parent-child relationship cleanup on child deletion
     * **GIVEN** host_1 is configured as parent of host_2
     * **AND** the monitoring system is running
     * **AND** the parent-child relationship exists in the database
     * **WHEN** I delete host_2 from the configuration
     * **AND** I notify Broker of a change in the engine configuration
     * **THEN** host_1 should have no child hosts
     * **AND** the parent-child relationship should be removed from the database
42. **BECPN2_per_poller**:
     * **FEATURE:** Child Host Deletion Management
     As a monitoring administrator
     I want parent-child relationships to be cleaned up when child hosts are deleted
     So that orphaned relationships don't exist in the system
     * **SCENARIO:** Parent-child relationship cleanup on child deletion
     * **GIVEN** host_1 is configured as parent of host_2
     * **AND** the monitoring system is running
     * **AND** the parent-child relationship exists in the database
     * **WHEN** I delete host_2 from the configuration
     * **AND** I notify Broker of a change in the engine configuration
     * **THEN** host_1 should have no child hosts
     * **AND** the parent-child relationship should be removed from the database
43. **BECSS1**:
     * **SCENARIO:** Broker sends configuration to engine in new generation
     * **GIVEN** an engine configuration is provided to the broker
     * **AND** the broker and engine are started in new generation (broker first)
     * **AND** the protocol is bbdo3
     * **WHEN** the broker detects the configuration for the engine
     * **THEN** the broker sends the configuration to the engine
     * **THEN** both broker and engine are stopped (engine first)
44. **BECSS2**:
     * **SCENARIO:** Broker sends configuration to engine in new generation
     * **GIVEN** an engine configuration is provided to the broker
     * **AND** the broker and engine are started in new generation (broker first)
     * **AND** the protocol is bbdo3
     * **WHEN** the broker detects the configuration for the engine
     * **THEN** the broker sends the configuration to the engine
     * **THEN** both broker and engine are stopped (engine first)
45. **BECSS3**:
     * **SCENARIO:** Broker sends configuration to engine in new generation
     * **GIVEN** an engine configuration is provided to the broker
     * **AND** the broker and engine are started in new generation (engine first)
     * **AND** the protocol is bbdo3
     * **WHEN** the broker detects the configuration for the engine
     * **THEN** the broker sends the configuration to the engine
     * **THEN** both broker and engine are stopped (engine first)
46. **BECSS4**:
     * **SCENARIO:** Broker sends configuration to engine in new generation
     * **GIVEN** an engine configuration is provided to the broker
     * **AND** the broker and engine are started in new generation (engine first)
     * **AND** the protocol is bbdo3
     * **WHEN** the broker detects the configuration for the engine
     * **THEN** the broker sends the configuration to the engine
     * **THEN** both broker and engine are stopped (broker first)
47. **BECSSBQ1**: A very bad queue file is written for broker. Broker and Engine are then started, Broker must read the file raising an error because of that file and then get data sent by Engine.
48. **BECSS_CRYPTED_GRPC1**:
     * **SCENARIO:** Repeated start/stop cycles with gRPC and mutual TLS in centralized configuration mode
     * **GIVEN** a centralized Engine configuration with gRPC and server-side TLS encryption
     * **WHEN** Broker and Engine are started for the first time
     * **THEN** Broker detects the lock file, sends the configuration to Engine and receives the ack
     * **AND** the database shows 50 enabled hosts and 1000 enabled services for poller 1
     * **WHEN** Engine is stopped
     * **THEN** all hosts for poller 1 are disabled in the database
     * **WHEN** Broker and Engine are restarted (4 additional times)
     * **THEN** both reload from their cached configuration files (.prot for Broker, state.prot for Engine)
     * **AND** no new configuration is exchanged
     * **AND** the database consistently shows 50 enabled hosts and 1000 enabled services
49. **BECSS_CRYPTED_GRPC2**: Start-Stop grpc version Broker/Engine only server crypted
50. **BECSS_CRYPTED_REVERSED_GRPC1**: Start-Stop grpc version Broker/Engine - well configured
51. **BECSS_CRYPTED_REVERSED_GRPC2**: Start-Stop grpc version Broker/Engine only engine server crypted
52. **BECSS_CRYPTED_REVERSED_GRPC3**: Start-Stop grpc version Broker/Engine only engine crypted
53. **BECSS_ENGINE_DELETE_HOST**: once engine and cbd started, stop and restart cbd, delete an host and reload engine, cbd mustn't core
54. **BECSS_GRPC1**:
     * **SCENARIO:** Broker sends configuration to engine in new generation
     * **GIVEN** an engine configuration is provided to the broker
     * **AND** the broker and engine are started in new generation (broker first)
     * **AND** the protocol is bbdo3
     * **WHEN** the broker detects the configuration for the engine
     * **THEN** the broker sends the configuration to the engine
     * **THEN** both broker and engine are stopped (engine first)
55. **BECSS_GRPC2**:
     * **SCENARIO:** Broker sends configuration to engine in new generation
     * **GIVEN** an engine configuration is provided to the broker
     * **AND** the broker and engine are started in new generation (broker first)
     * **AND** the protocol is bbdo3
     * **WHEN** the broker detects the configuration for the engine
     * **THEN** the broker sends the configuration to the engine
     * **THEN** both broker and engine are stopped (engine first)
56. **BECSS_GRPC3**:
     * **SCENARIO:** Broker sends configuration to engine in new generation
     * **GIVEN** an engine configuration is provided to the broker
     * **AND** the broker and engine are started in new generation (engine first)
     * **AND** the protocol is bbdo3
     * **WHEN** the broker detects the configuration for the engine
     * **THEN** the broker sends the configuration to the engine
     * **THEN** both broker and engine are stopped (engine first)
57. **BECSS_GRPC4**:
     * **SCENARIO:** Broker sends configuration to engine in new generation
     * **GIVEN** an engine configuration is provided to the broker
     * **AND** the broker and engine are started in new generation (engine first)
     * **AND** the protocol is bbdo3
     * **WHEN** the broker detects the configuration for the engine
     * **THEN** the broker sends the configuration to the engine
     * **THEN** both broker and engine are stopped (broker first)
58. **BECSS_GRPC_COMPRESS1**: Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped last compression activated
59. **BECTAG1**:
     * **FEATURE:** Tag Management between Engine and Broker
     As a Centreon administrator
     I want to configure tags in Engine
     So that Broker stores them correctly in centreon_storage.tags table
     * **BACKGROUND:**
     * **GIVEN** Engine is configured with centralized setup
     * **AND** Broker components (central, rrd, module) are configured
     * **AND** Database logging is enabled with debug/trace level
     * **AND** Retention data is cleared
     * **SCENARIO:** Initial tag configuration
     * **GIVEN** Engine is configured with 20 tags
     * **WHEN** Broker and Engine are started
     * **THEN** 20 tags should be added/modified in logs
     * **AND** INSERT statements should be executed in tags table
     * **AND** Configuration file should match database content
     * **AND** Tag IDs should be consistent
     * **SCENARIO:** Tag configuration modification
     * **GIVEN** Initial configuration with 20 tags is loaded
     * **WHEN** Configuration is modified to 30 tags
     * **AND** Engine configuration change is notified
     * **THEN** 10 additional tags should be added/modified
     * **AND** Configuration file should still match database content
     * **AND** Tag IDs should remain consistent
     * **SCENARIO:** Tag configuration reduction
     * **GIVEN** Configuration with 30 tags is loaded
     * **WHEN** Configuration is reduced to 11 tags starting at ID 50
     * **AND** Engine configuration change is notified
     * **THEN** 11 tags should be present in final configuration
     * **AND** Unused tags should be implicitly removed
     * **AND** Configuration file should match database content
     * **AND** Tag IDs should be consistent with new range
60. **BECWATCH1**:
     * **SCENARIO:** PHP notifies several poller configurations in a burst
     * **GIVEN** a centralized platform with 3 pollers, all connected
     * **WHEN** three changed configurations are notified one after another
     * **THEN** Broker handles the whole burst as a single batch
     * **AND** it reads the stored poller configurations only once for it
     * **WHEN** the same three configurations are notified again, unchanged
     * **THEN** that burst is a single batch too
61. **BECWATCH2_batch**:
     * **SCENARIO:** the watched cache directory is moved out of the way and back
     * **GIVEN** a centralized platform with 1 poller and Broker started
     * **WHEN** the cache directory is renamed, so the inotify watch is lost
     * **THEN** Broker reports the loss and cannot establish the watch again
     * **WHEN** the directory is put back
     * **THEN** Broker establishes the watch again without waiting for the slow period
     * **AND** a configuration pushed afterwards is detected, whichever shape
     announces it -- pollers.lck or <ID>.lck
62. **BECWATCH2_per_poller**:
     * **SCENARIO:** the watched cache directory is moved out of the way and back
     * **GIVEN** a centralized platform with 1 poller and Broker started
     * **WHEN** the cache directory is renamed, so the inotify watch is lost
     * **THEN** Broker reports the loss and cannot establish the watch again
     * **WHEN** the directory is put back
     * **THEN** Broker establishes the watch again without waiting for the slow period
     * **AND** a configuration pushed afterwards is detected, whichever shape
     announces it -- pollers.lck or <ID>.lck
63. **BECWATCH3**:
     * **SCENARIO:** PHP announces an export through the poller batch file
     * **GIVEN** a centralized platform with 3 pollers, all connected
     * **WHEN** the three configurations are announced by a single pollers.lck
     * **AND** no individual <id>.lck is written
     * **THEN** Broker handles the three of them in one pass
     * **AND** it reads the stored poller configurations only once
     * **AND** it consumes the batch file
     * **AND** this holds whether the file was renamed into place or written directly
64. **BECWATCH4_batch**:
     * **SCENARIO:** two hosts swap pollers within a single export
     * **GIVEN** a centralized platform with 2 pollers of 5 hosts each
     * **WHEN** host_1 moves to poller 2 and host_6 moves to poller 1
     * **AND** both configurations are announced together, by one pollers.lck or
     by one <ID>.lck each
     * **THEN** the global diff turns each move into a modification, not a removal
     * **AND** both hosts stay enabled, each attached to its new poller
65. **BECWATCH4_per_poller**:
     * **SCENARIO:** two hosts swap pollers within a single export
     * **GIVEN** a centralized platform with 2 pollers of 5 hosts each
     * **WHEN** host_1 moves to poller 2 and host_6 moves to poller 1
     * **AND** both configurations are announced together, by one pollers.lck or
     by one <ID>.lck each
     * **THEN** the global diff turns each move into a modification, not a removal
     * **AND** both hosts stay enabled, each attached to its new poller
66. **CANO_CFG_SENSITIVITY_SAVED**:
     * **GIVEN** an anomaly detection service is configured with a specific sensitivity value in configuration
     * **AND** the threshold file contains prediction data with sensitivity parameters
     * **WHEN** the engine and broker are started and then stopped
     * **THEN** the configuration-based sensitivity value should be persisted in the retention data
     because CFG sensitivity parameters are properly saved during retention processing
67. **CANO_DT1**:
     * **GIVEN** an anomaly detection service is configured with a dependent service relationship
     * **AND** both services are running normally
     * **WHEN** a downtime is scheduled on the dependent service
     * **THEN** the dependent service should enter downtime state
     * **AND** the anomaly detection service should automatically inherit the downtime
     because anomaly detection services inherit downtime from their dependent services
68. **CANO_DT2**:
     * **GIVEN** an anomaly detection service is configured with a dependent service relationship
     * **AND** both services are running normally
     * **WHEN** a downtime is scheduled on the dependent service
     * **THEN** the anomaly detection service should automatically enter downtime
     * **WHEN** the downtime is deleted from the dependent service
     * **THEN** the anomaly detection service should automatically exit downtime
     because anomaly detection downtime should follow its dependent service downtime state
69. **CANO_DT3**:
     * **GIVEN** an anomaly detection service is configured with a dependent service relationship
     * **AND** both services are running normally
     * **WHEN** a downtime is scheduled on the dependent service
     * **THEN** the anomaly detection service should automatically enter downtime
     * **WHEN** the downtime is deleted from the anomaly detection service
     * **THEN** the dependent service should remain in its original downtime state
     because deleting downtime on anomaly detection should not affect dependent service downtimes
70. **CANO_DT4**:
     * **SCENARIO:** Removing downtime from service keeps it on anomaly detection
     * **GIVEN** an anomaly detection is attached to a service
     * **AND** a downtime is set on both the service and the anomaly detection
     * **WHEN** the downtime is removed from the service
     * **THEN** the downtime should still be present on the anomaly detection
71. **CANO_EXTCMD_SENSITIVITY_SAVED**:
     * **GIVEN** an anomaly detection service is configured with threshold data
     * **AND** the service is running with initial sensitivity parameters
     * **WHEN** an external command updates the anomaly sensitivity value
     * **AND** the engine and broker are stopped
     * **THEN** the updated sensitivity value should be persisted in the retention data
     because external command sensitivity changes are properly saved during retention processing
72. **CANO_JSON_SENSITIVITY_NOT_SAVED**:
     * **GIVEN** an anomaly detection service is configured with threshold data including sensitivity
     * **AND** the threshold file contains prediction data with a specific sensitivity value
     * **WHEN** the engine and broker are started and then stopped
     * **THEN** the sensitivity value should not be persisted in the retention data
     because JSON sensitivity parameters are not saved during retention processing
73. **CANO_NOFILE**:
     * **GIVEN** an anomaly detection service is configured for metric monitoring
     * **AND** the threshold configuration file is missing from the system
     * **WHEN** the service processes a check result with critical state
     * **THEN** the anomaly detection service must transition to UNKNOWN state
     because it cannot determine thresholds without the configuration file
74. **CANO_OUT_LOWER_THAN_LIMIT**:
     * **GIVEN** an anomaly detection service is configured with valid threshold data
     * **AND** the threshold file contains lower and upper limits for the metric
     * **WHEN** a service check provides performance data below the lower threshold limit
     * **THEN** the anomaly detection service must transition to CRITICAL state
     because the metric value indicates an anomalous condition requiring attention
75. **CANO_OUT_UPPER_THAN_LIMIT**:
     * **GIVEN** an anomaly detection service is configured with valid threshold data
     * **AND** the threshold file contains lower and upper limits for the metric
     * **WHEN** a service check provides performance data above the upper threshold limit
     * **THEN** the anomaly detection service must transition to CRITICAL state
     because the metric value indicates an anomalous condition requiring attention
76. **CANO_TOO_OLD_FILE**:
     * **GIVEN** an anomaly detection service is configured with metric monitoring
     * **AND** a threshold file exists but contains outdated prediction data
     * **WHEN** the service processes a check result with performance data
     * **THEN** the anomaly detection service must transition to UNKNOWN state
     because the threshold data is too old to be reliable for current predictions
77. **CAOUTLU1**:
     * **GIVEN** an anomaly detection service is configured with valid threshold data using BBDO3 protocol
     * **AND** the threshold file contains lower and upper limits for the metric
     * **WHEN** a service check provides performance data above the upper threshold limit
     * **THEN** the anomaly detection service must transition to CRITICAL state
     * **AND** the resources table should contain SERVICE, HOST and ANOMALY_DETECTION type entries
78. **CBEUDHOSTS**:
     * **GIVEN** a Centreon platform with 3 pollers configured
     * **AND** 50 hosts distributed across pollers (17+17+16)
     * **AND** initially 20 services per host
     * **AND** BBDO3 protocol with unified SQL output enabled
     * **WHEN** the number of services per host is progressively increased
     * **AND** the configuration is hot-reloaded 3 times (20→24→28 services/host)
     * **THEN** each poller should monitor the correct number of resources
     * **AND** poller 1 should monitor exactly (17 hosts × services) + 17 hosts
     * **AND** poller 2 should monitor exactly (17 hosts × services) + 17 hosts
     * **AND** poller 3 should monitor exactly (16 hosts × services) + 16 hosts
     * **AND** the load balancing should remain stable during scaling
79. **CCCRC1**:
     * **GIVEN** a topology Poller1 -> Relay1 -> central cbd
     * **WHEN** Engine connects to the relay
     * **THEN** the relay sends a ConfigRequest to the central for poller 1
     * **AND** the central logs the receipt of that ConfigRequest.
80. **CCCRC2_batch**:
     * **SCENARIO:** The central answers a relay asking for a configuration it already holds
     * **GIVEN** a topology Poller1 -> Relay3 -> central cbd
     * **AND** a poller configuration is pre-created before starting the central broker
     * **WHEN** the central processes the configuration and the relay sends a ConfigRequest
     * **THEN** the central sends a non-unknown DiffState to the relay.
81. **CCCRC2_per_poller**:
     * **SCENARIO:** The central answers a relay asking for a configuration it already holds
     * **GIVEN** a topology Poller1 -> Relay3 -> central cbd
     * **AND** a poller configuration is pre-created before starting the central broker
     * **WHEN** the central processes the configuration and the relay sends a ConfigRequest
     * **THEN** the central sends a non-unknown DiffState to the relay.
82. **CCCRC3_batch**:
     * **SCENARIO:** A relay forwards the configuration to Engine and its acknowledgement back to the central
     * **GIVEN** a topology Poller1 -> Relay3 -> central cbd
     * **AND** a poller configuration is pre-created before starting the central broker
     * **WHEN** Engine connects through the relay and the central sends a DiffState
     * **THEN** the relay forwards the DiffState to Engine
     * **AND** the relay forwards the DiffStateAck back to the central.
83. **CCCRC3_per_poller**:
     * **SCENARIO:** A relay forwards the configuration to Engine and its acknowledgement back to the central
     * **GIVEN** a topology Poller1 -> Relay3 -> central cbd
     * **AND** a poller configuration is pre-created before starting the central broker
     * **WHEN** Engine connects through the relay and the central sends a DiffState
     * **THEN** the relay forwards the DiffState to Engine
     * **AND** the relay forwards the DiffStateAck back to the central.
84. **CCCRC4_batch**:
     * **SCENARIO:** A configuration pushed after the initial one reaches Engine through the relay
     * **GIVEN** a topology Poller1 -> Relay3 -> central cbd
     * **AND** a poller configuration is pre-created before starting central
     * **WHEN** Engine connects and gets the initial config via relay
     * **AND** PHP pushes a new config for poller 1 (5 extra hosts)
     * **THEN** the central sends a new DiffState to the relay
     * **AND** the central receives a new DiffStateAck.
85. **CCCRC4_per_poller**:
     * **SCENARIO:** A configuration pushed after the initial one reaches Engine through the relay
     * **GIVEN** a topology Poller1 -> Relay3 -> central cbd
     * **AND** a poller configuration is pre-created before starting central
     * **WHEN** Engine connects and gets the initial config via relay
     * **AND** PHP pushes a new config for poller 1 (5 extra hosts)
     * **THEN** the central sends a new DiffState to the relay
     * **AND** the central receives a new DiffStateAck.
86. **CCCRC5_batch**:
     * **SCENARIO:** Engine migrating from one relay to another is served through the new one
     * **GIVEN** Engine initially connected to central via Relay3 (poller_id=4)
     * **WHEN** Engine migrates to Relay4 (poller_id=5)
     * **THEN** the central sends ConfigRevoke to Relay3
     * **AND** serves the configuration to Engine via Relay4.
87. **CCCRC5_per_poller**:
     * **SCENARIO:** Engine migrating from one relay to another is served through the new one
     * **GIVEN** Engine initially connected to central via Relay3 (poller_id=4)
     * **WHEN** Engine migrates to Relay4 (poller_id=5)
     * **THEN** the central sends ConfigRevoke to Relay3
     * **AND** serves the configuration to Engine via Relay4.
88. **CCCRC6_batch**:
     * **SCENARIO:** A configuration pushed while the central is down is served once it is back
     * **GIVEN** Engine connected via Relay3 with initial config established
     * **WHEN** the central is stopped cleanly and a new config is pushed during the outage
     * **THEN** after the central restarts, the relay reconnects and the new DiffState
     is forwarded to Engine via the relay, and central receives a new DiffStateAck.
89. **CCCRC6_per_poller**:
     * **SCENARIO:** A configuration pushed while the central is down is served once it is back
     * **GIVEN** Engine connected via Relay3 with initial config established
     * **WHEN** the central is stopped cleanly and a new config is pushed during the outage
     * **THEN** after the central restarts, the relay reconnects and the new DiffState
     is forwarded to Engine via the relay, and central receives a new DiffStateAck.
90. **CCCRC7_batch**:
     * **SCENARIO:** GetTopology reports the relay and the poller sitting behind it
     * **GIVEN** Engine connected via Relay3 with initial config established
     * **WHEN** GetTopology is called on the central gRPC endpoint
     * **THEN** the response contains Relay3 as a direct broker with poller 1 as its poller.
91. **CCCRC7_per_poller**:
     * **SCENARIO:** GetTopology reports the relay and the poller sitting behind it
     * **GIVEN** Engine connected via Relay3 with initial config established
     * **WHEN** GetTopology is called on the central gRPC endpoint
     * **THEN** the response contains Relay3 as a direct broker with poller 1 as its poller.
92. **Centralized_Start_Stop_Broker_Engine_1**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
93. **Centralized_Start_Stop_Broker_Engine_2**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
94. **Centralized_Start_Stop_Engine_Broker_1**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
95. **Centralized_Start_Stop_Engine_Broker_2**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
96. **RENAME_PARENT_batch**:
     * **FEATURE:** Parent Host Rename Management
     As a monitoring administrator
     I want parent-child relationships to be maintained when parent hosts are renamed
     So that dependencies remain intact after configuration changes
     * **SCENARIO:** Parent-child relationship maintained on parent rename
     * **GIVEN** host_1 is configured as parent of host_2
     * **AND** the monitoring system is running
     * **AND** the parent-child relationship exists
     * **WHEN** I rename host_1 to host_1_new
     * **AND** I update host_2 parent reference to host_1_new
     * **AND** I reload the engine configuration
     * **THEN** host_2 should have host_1_new as parent
     * **AND** the engine should not crash
     * **AND** the configuration reload should complete successfully
97. **RENAME_PARENT_per_poller**:
     * **FEATURE:** Parent Host Rename Management
     As a monitoring administrator
     I want parent-child relationships to be maintained when parent hosts are renamed
     So that dependencies remain intact after configuration changes
     * **SCENARIO:** Parent-child relationship maintained on parent rename
     * **GIVEN** host_1 is configured as parent of host_2
     * **AND** the monitoring system is running
     * **AND** the parent-child relationship exists
     * **WHEN** I rename host_1 to host_1_new
     * **AND** I update host_2 parent reference to host_1_new
     * **AND** I reload the engine configuration
     * **THEN** host_2 should have host_1_new as parent
     * **AND** the engine should not crash
     * **AND** the configuration reload should complete successfully

### Connector perl

This chapter contains 4 tests.

1. **CCONPERL**:
     * **SCENARIO:** Single host check via Perl Connector in centralized configuration
     * **GIVEN** a centralized engine and broker configuration with the Perl Connector
     * **WHEN** a forced host check is scheduled on host_1
     * **THEN** the check execution result should appear in the engine log file.
2. **CCONPERLM**:
     * **SCENARIO:** Ten host checks via Perl Connector in centralized configuration
     * **GIVEN** a centralized engine and broker configuration with the Perl Connector on ten hosts
     * **WHEN** a forced check is scheduled on each of the ten hosts
     * **THEN** the check execution result for each host should appear in the engine log file.
3. **CONPERL**: The test.pl script is launched using the perl connector. Then we should find its execution in the engine log file.
4. **CONPERLM**: Ten forced checks are scheduled on ten hosts configured with the Perl Connector. The we get the result of each of them.

### Connector ssh

This chapter contains 8 tests.

1. **CTest6Hosts**:
     * **SCENARIO:** SSH checks succeed on 6 hosts in centralized configuration
     * **GIVEN** a centralized engine and broker configuration with 6 hosts reachable via SSH
     * **WHEN** forced checks are scheduled on all 6 hosts
     * **THEN** the expected output for each host address should appear in the log.
2. **CTestBadPwd**:
     * **SCENARIO:** SSH check with wrong password fails in centralized configuration
     * **GIVEN** a centralized engine and broker configuration with a wrong SSH password on host_1
     * **WHEN** a forced host check is scheduled
     * **THEN** a connection failure message for the bad password should appear in the log.
3. **CTestBadUser**:
     * **SCENARIO:** SSH check with unknown user fails in centralized configuration
     * **GIVEN** a centralized engine and broker configuration with an unknown SSH user on host_1
     * **WHEN** a forced host check is scheduled
     * **THEN** a connection failure message for the unknown user should appear in the log.
4. **CTestWhiteList**:
     * **SCENARIO:** SSH check blocked then allowed by whitelist in centralized configuration
     * **GIVEN** a centralized engine and broker configuration with a whitelist restricting SSH checks
     * **WHEN** a forced host check is scheduled and the command is not whitelisted
     * **THEN** a security restriction message should appear in the log.
     * **WHEN** the whitelist is updated to allow the SSH command
     * **THEN** the check should succeed and the expected output should appear in the log.
5. **Test6Hosts**: as 127.0.0.x point to the localhost address we will simulate check on 6 hosts
6. **TestBadPwd**: test bad password
7. **TestBadUser**: test unknown user
8. **TestWhiteList**: as 127.0.0.x point to the localhost address we will simulate check on 6 hosts

### Engine

This chapter contains 150 tests.

1. **CEBSN5**:
     * **GIVEN** a centralized Engine configuration where contactgroup_1 is empty and inherits from a full template
     * **AND** the template defines alias, members, and contactgroup_members
     * **WHEN** Engine and Broker are started
     * **THEN** contactgroup_1 resolves with the template's alias, members, and sub-groups
2. **CEBSN6**:
     * **GIVEN** a centralized Engine configuration where contactgroup_1 is full and inherits from a full template
     * **AND** both the group and template define alias, members, and contactgroup_members
     * **WHEN** Engine and Broker are started
     * **THEN** contactgroup_1's own values take precedence over the template's values
3. **CEBSN7**:
     * **GIVEN** a centralized Engine started with contactgroup_1 having only one member
     * **AND** after start, contactgroup_1 is modified to be empty and inherit from a full template
     * **WHEN** the new configuration is sent to Engine via Broker notification
     * **THEN** contactgroup_1 resolves with the template's alias, members, and sub-groups
4. **CEBSN8**:
     * **GIVEN** a centralized Engine started with contactgroup_1 having one member
     * **AND** after start, contactgroup_1 is modified to be full and inherit from a full template
     * **WHEN** the new configuration is sent to Engine via Broker notification
     * **THEN** contactgroup_1's own values take precedence and template values for overlapping fields are not used
5. **CECI0**:
     * **GIVEN** a centralized Engine configuration where John_Doe is empty and inherits from a full contact template
     * **AND** the template defines all notification settings, addresses, and custom variables
     * **WHEN** Engine and Broker are started
     * **THEN** John_Doe resolves with all values from the template
6. **CECI1**:
     * **GIVEN** a centralized Engine configuration where John_Doe is full and inherits from a full contact template
     * **AND** both the contact and template define all notification settings and addresses
     * **WHEN** Engine and Broker are started
     * **THEN** John_Doe's own values take precedence over the template's values for all fields
7. **CECI2**:
     * **GIVEN** a centralized Engine started with a minimal contact configuration
     * **AND** after start, John_Doe is made empty and a full contact template is added
     * **WHEN** the new configuration is sent to Engine via Broker notification
     * **THEN** John_Doe resolves with all values from the template
8. **CECI3**:
     * **GIVEN** a centralized Engine started with a minimal contact configuration
     * **AND** after start, John_Doe is made full with its own values and a full contact template is added
     * **WHEN** the new configuration is sent to Engine via Broker notification
     * **THEN** John_Doe's own values take precedence over the template's values for all fields
9. **CECMI0**:
     * **GIVEN** a centralized Engine configuration with a command template having a command_line
     * **AND** the command inherits from the template with its own command_line deleted
     * **WHEN** Engine and Broker are started
     * **THEN** the command's resolved command_line matches the template value
10. **CECMI1**:
     * **GIVEN** a centralized Engine configuration with a command template having a command_line
     * **AND** the command inherits from the template but keeps its own command_line
     * **WHEN** Engine and Broker are started
     * **THEN** the command's resolved command_line is the command's own value, not the template's
11. **CECMI2**:
     * **GIVEN** a centralized Engine already started with a basic configuration
     * **AND** a command template with a command_line is added with the command inheriting from it and its own command_line deleted
     * **WHEN** the new configuration is sent via Broker notification
     * **THEN** the command's resolved command_line matches the template value
12. **CECMI3**:
     * **GIVEN** a centralized Engine already started with a basic configuration
     * **AND** a command template with a command_line is added with the command inheriting from it while keeping its own command_line
     * **WHEN** the new configuration is sent via Broker notification
     * **THEN** the command's resolved command_line is the command's own value, not the template's
13. **CECOI0**:
     * **GIVEN** a centralized Engine configuration with a connector template having a connector_line
     * **AND** the connector inherits from the template with its own connector_line deleted
     * **WHEN** Engine and Broker are started
     * **THEN** the connector's resolved connector_line matches the template value
14. **CECOI1**:
     * **GIVEN** a centralized Engine configuration with a connector template having a connector_line
     * **AND** the connector inherits from the template but keeps its own connector_line
     * **WHEN** Engine and Broker are started
     * **THEN** the connector's resolved connector_line is the connector's own value, not the template's
15. **CECOI2**:
     * **GIVEN** a centralized Engine already started with a basic configuration
     * **AND** a connector template with a connector_line is added with the connector inheriting from it and its own connector_line deleted
     * **WHEN** the new configuration is sent via Broker notification
     * **THEN** the connector's resolved connector_line matches the template value
16. **CECOI3**:
     * **GIVEN** a centralized Engine already started with a basic configuration
     * **AND** a connector template with a connector_line is added with the connector inheriting from it while keeping its own connector_line
     * **WHEN** the new configuration is sent via Broker notification
     * **THEN** the connector's resolved connector_line is the connector's own value, not the template's
17. **CEESI0**:
     * **GIVEN** Engine is configured with centralized configuration
     * **AND** a service escalation is defined for a service group containing host_1..3/service_1..3
     * **AND** a host group containing host_6 and host_7 is defined
     * **WHEN** Broker and Engine are started
     * **THEN** each service in the service group gets the escalation applied
     * **AND** services outside the service group have no escalation
18. **CEESI1**:
     * **GIVEN** Engine is configured with centralized configuration
     * **AND** a service escalation with no fields inherits from a full service escalation template
     * **WHEN** Broker and Engine are started
     * **THEN** services in the service group get the escalation settings from the template on engine start
19. **CEESI2**:
     * **GIVEN** Engine is configured with centralized configuration
     * **AND** a service escalation with full fields inherits from a full service escalation template
     * **WHEN** Broker and Engine are started
     * **THEN** the escalation own values take precedence over template values on engine start
     * **AND** services outside the service group have no escalation
20. **CEESI3**:
     * **GIVEN** Engine is configured with centralized configuration and started
     * **AND** a service escalation with no fields is changed to inherit from a full service escalation template
     * **WHEN** Broker notifies Engine of the new configuration
     * **THEN** services in the new template's service group get the escalation settings
     * **AND** services in the old service group have no escalation after the configuration change
21. **CEESI4**:
     * **GIVEN** Engine is configured with centralized configuration and started
     * **AND** a service escalation with full fields is changed to also inherit from a full service escalation template
     * **WHEN** Broker notifies Engine of the new configuration
     * **THEN** the escalation own values take precedence over template values after the configuration change
     * **AND** services outside the escalation's service group have no escalation
22. **CEESI5**:
     * **GIVEN** Engine is configured with centralized configuration
     * **AND** a host escalation is defined for a host group containing host_1, host_2, and host_3
     * **WHEN** Broker and Engine are started
     * **THEN** each host in the host group gets the escalation applied
23. **CEESI6**:
     * **GIVEN** Engine is configured with centralized configuration
     * **AND** a host escalation with no fields inherits from a full host escalation template
     * **WHEN** Broker and Engine are started
     * **THEN** hosts in the host group get all escalation settings from the template on engine start
24. **CEESI7**:
     * **GIVEN** Engine is configured with centralized configuration
     * **AND** a host escalation with full fields inherits from a full host escalation template
     * **WHEN** Broker and Engine are started
     * **THEN** the escalation own values take precedence over template values for hosts in its group
     * **AND** hosts in the template's host group have no escalation on engine start
25. **CEESI8**:
     * **GIVEN** Engine is configured with centralized configuration and started
     * **AND** a host escalation with no fields is changed to inherit from a full host escalation template
     * **WHEN** Broker notifies Engine of the new configuration
     * **THEN** hosts in the new template's host group get the escalation settings
     * **AND** hosts in the old host group have no escalation after the configuration change
26. **CEESI9**:
     * **GIVEN** Engine is configured with centralized configuration and started
     * **AND** a host escalation with full fields is changed to also inherit from a full host escalation template
     * **WHEN** Broker notifies Engine of the new configuration
     * **THEN** the escalation own values take precedence over template values for hosts in its group after the configuration change
     * **AND** hosts in the template's host group have no escalation after the configuration change
27. **CEFHCU1**:
     * **GIVEN** Engine is configured with hosts in centralized mode
     * **WHEN** we force check one host 5 times
     * **THEN** the host transitions through SOFT and HARD DOWN states
     * **AND** the resources table is cleared before starting broker
28. **CEFHCU2**:
     * **GIVEN** Engine is configured with hosts in centralized mode
     * **WHEN** we force check one host 5 times
     * **THEN** the host transitions through SOFT and HARD DOWN states
29. **CEHGI0**:
     * **GIVEN** a hostgroup with no fields inheriting from a full template
     * **WHEN** Engine starts with centralized configuration
     * **THEN** the hostgroup alias, notes, notes_url, action_url and members are inherited from the template
30. **CEHGI1**:
     * **GIVEN** a full hostgroup inheriting from a full template
     * **WHEN** Engine starts with centralized configuration
     * **THEN** the hostgroup's own fields take precedence over the template's fields
31. **CEHGI2**:
     * **GIVEN** a hostgroup with no fields inheriting from a full template
     * **WHEN** Broker notifies Engine of new centralized configuration
     * **THEN** the hostgroup alias, notes, notes_url, action_url and members are inherited from the template
32. **CEHGI3**:
     * **GIVEN** a full hostgroup inheriting from a full template
     * **WHEN** Broker notifies Engine of new centralized configuration
     * **THEN** the hostgroup's own fields take precedence over the template's fields
33. **CEHI0**:
     * **GIVEN** a centralized Engine configuration with an empty host inheriting from a full template
     * **WHEN** Engine and Broker are started in newGeneration mode
     * **THEN** the host should inherit all fields from the template
34. **CEHI1**:
     * **GIVEN** a centralized Engine configuration with a full host inheriting from a full template
     * **WHEN** Engine and Broker are started in newGeneration mode
     * **THEN** the host's own values should take precedence over the template values
35. **CEHI2**:
     * **GIVEN** a centralized Engine configuration with an empty host inheriting from a full template
     * **WHEN** Broker notifies Engine of the new centralized configuration
     * **THEN** the host should inherit all fields from the template
36. **CEHI3**:
     * **GIVEN** a centralized Engine configuration with a fully configured host inheriting from a full template
     * **WHEN** Broker notifies Engine of the new centralized configuration
     * **THEN** the host's own values take precedence over the template values
37. **CEMACROS**:
     * **GIVEN** Engine is configured with centralized configuration
     * **WHEN** a forced service check is scheduled
     * **THEN** the macros ADMINEMAIL and ADMINPAGER are replaced in check outputs
38. **CEMACROS_NOTIF**:
     * **GIVEN** Engine is configured with centralized configuration
     * **WHEN** a service enters a critical state triggering a notification
     * **THEN** the macros ADMINEMAIL and ADMINPAGER are replaced in notification commands
39. **CEMACROS_SEMICOLON**:
     * **GIVEN** Engine is configured with centralized configuration
     * **WHEN** a forced service check is scheduled with a macro containing a semicolon
     * **THEN** the macro value with semicolons is correctly expanded in check outputs
40. **CEMTI0**:
     * **GIVEN** a host using a chain of 4 template levels each defining a custom variable
     * **WHEN** Engine starts with centralized configuration
     * **THEN** all custom variables from every template level are present on the host
41. **CENGINE_MANY_CHECKS**:
     * **GIVEN** Engine is configured in centralized mode with many services and a unique check on each service with its own env variables
     * **WHEN** Broker sends the configuration to Engine and all checks are executed
     * **THEN** the correct check results are found in logs with expected args and service macros
42. **CEPC1**:
     * **GIVEN** Engine is configured with a Perl connector
     * **WHEN** Engine starts
     * **THEN** the Perl connector is launched and data becomes available
43. **CERL**:
     * **GIVEN** Engine is started and writing logs to centengine.log
     * **WHEN** the log file is removed
     * **THEN** Engine continues running but the log file is gone
     * **AND WHEN** Engine is reloaded the centengine.log file is recreated
44. **CESGI0**:
     * **GIVEN** a servicegroup with no fields inheriting from a full template
     * **WHEN** Engine starts with centralized configuration
     * **THEN** the servicegroup alias, notes, notes_url, action_url and members are inherited from the template
45. **CESGI1**:
     * **GIVEN** a full servicegroup inheriting from a full template
     * **WHEN** Engine starts with centralized configuration
     * **THEN** the servicegroup's own fields take precedence over the template's fields
46. **CESGI2**:
     * **GIVEN** a servicegroup with no fields inheriting from a full template
     * **WHEN** Broker notifies Engine of the new centralized configuration
     * **THEN** the servicegroup alias, notes, notes_url, action_url and members are inherited from the template
47. **CESGI3**:
     * **GIVEN** a full servicegroup inheriting from a full template
     * **WHEN** Broker notifies Engine of the new centralized configuration
     * **THEN** the servicegroup's own fields take precedence over the template's fields
48. **CESI0**:
     * **GIVEN** Engine is configured with centralized configuration
     * **AND** a service template with full settings is defined
     * **AND** service_1 has no direct settings and inherits from the template
     * **WHEN** Broker and Engine are started
     * **THEN** service_1 inherits all settings from the service template on engine start
49. **CESI1**:
     * **GIVEN** Engine is configured with centralized configuration
     * **AND** both service_1 and a service template have full settings
     * **AND** service_1 inherits from the template
     * **WHEN** Broker and Engine are started
     * **THEN** service_1 own values take precedence over template values on engine start
50. **CESI2**:
     * **GIVEN** Engine is configured with centralized configuration and started
     * **AND** a service template with full settings is defined after start
     * **AND** service_1 has no direct settings and inherits from the template
     * **WHEN** Broker notifies Engine of new centralized configuration
     * **THEN** service_1 inherits all settings from the service template on engine reload
51. **CESI3**:
     * **GIVEN** Engine is configured with centralized configuration and started
     * **AND** both service_1 and a service template have full settings defined after start
     * **AND** service_1 inherits from the template
     * **WHEN** Broker notifies Engine of new centralized configuration
     * **THEN** service_1 own values take precedence over template values on engine reload
52. **CESS1**:
     * **GIVEN** one Engine instance is configured with a module broker
     * **WHEN** the Engine is started and stopped 5 times with no delay
     * **THEN** no coredump is produced
53. **CESS2**:
     * **GIVEN** one Engine instance is configured with a module broker
     * **WHEN** the Engine is started and stopped 5 times with 300ms delay
     * **THEN** no coredump is produced
54. **CESS3**:
     * **GIVEN** three Engine instances are configured with a module broker
     * **WHEN** the Engine is started and stopped 5 times with no delay
     * **THEN** no coredump is produced
55. **CESS4**:
     * **GIVEN** three Engine instances are configured with a module broker
     * **WHEN** the Engine is started and stopped 5 times with 300ms delay
     * **THEN** no coredump is produced
56. **CESSCTO**:
     * **GIVEN** the Engine is configured without the Perl connector
     * **WHEN** the Engine executes its service commands
     * **THEN** the commands take too long and reach the timeout
     * **AND** the Engine starts and stops as a result
57. **CESSCTOWC**:
     * **GIVEN** the Engine is configured with some commands using the Perl connector
     * **WHEN** the Engine executes its service commands
     * **THEN** the commands take too long and reach the timeout
     * **AND** the Engine starts and stops as a result
58. **CESSOCWNV**:
     * **GIVEN** the Engine is configured with a valid old configuration concerning cbmod
     * **WHEN** the Engine is started
     * **THEN** the Engine starts correctly
     * **AND** the Engine stops correctly
59. **CESS_STATS**:
     * **GIVEN** the Engine is started with centralized configuration
     * **WHEN** we read the Engine's stats file
     * **THEN** the Engine must not crash
60. **CEVOCWNV**:
     * **GIVEN** the Engine is configured with a valid old configuration concerning cbmod
     * **WHEN** the Engine is started to check the configuration
     * **THEN** the Engine reads it as expected
61. **CEXT_CONF1**:
     * **GIVEN** Engine is configured with a module broker
     * **WHEN** Engine starts with an extended JSON configuration overriding log levels
     * **THEN** the log levels from the extended conf are applied at startup
62. **CEXT_CONF2**:
     * **GIVEN** Engine is configured with a module broker and an empty extended JSON conf
     * **WHEN** the extended conf is updated with new log levels and Engine is reloaded
     * **THEN** the new log levels from the updated extended conf are applied after reload
63. **CE_FD_LIMIT**:
     * **GIVEN** the Engine is configured with a low file descriptor limit
     * **WHEN** the Engine is started
     * **THEN** the Engine should not crash
     * **AND** the file descriptor limit should be set correctly
64. **CE_HOST_DOWN_DISABLE_SERVICE_CHECKS**:
     * **GIVEN** Engine is configured with centralized configuration and host_down_disable_service_checks enabled
     * **WHEN** a host goes DOWN
     * **THEN** all its services are switched to UNKNOWN hard state
     * **AND WHEN** the host recovers all services return to OK hard state
65. **CE_HOST_UNREACHABLE_DISABLE_SERVICE_CHECKS**:
     * **GIVEN** Engine is configured with centralized configuration and host_down_disable_service_checks enabled
     * **WHEN** a parent host goes DOWN causing a child host to become UNREACHABLE
     * **THEN** all services on the unreachable host are switched to UNKNOWN hard state
66. **CVERIF**:
     * **GIVEN** centengine is configured normally
     * **WHEN** centengine is started in verification mode
     * **THEN** it does not log in its file
67. **CVERIFY_CONF**:
     * **GIVEN** Engine and broker are configured with module
     * **AND** the engine configuration includes deprecated options
     * **WHEN** Engine starts
     * **THEN** a warning message for 'auto_reschedule_checks' is logged
     * **AND** a warning message for 'auto_rescheduling_interval' is logged
     * **AND** a warning message for 'auto_rescheduling_window' is logged
68. **EBSN5**: Verify contactgroup inheritance : contactgroup(empty) inherit from template (full) , on Start Engine
69. **EBSN6**: Verify contactgroup inheritance : contactgroup(full) inherit from template (full) , on Start Engine
70. **EBSN7**: Verify contactgroup inheritance : contactgroup(empty) inherit from template (full) , on Reload Engine
71. **EBSN8**: Verify contactgroup inheritance : contactgroup(full) inherit from template (full) , on Reload Engine
72. **ECEMPTYNAME**:
     * **SCENARIO:** a contact with no name is rejected
     * **GIVEN** an engine configuration with a contact that has no contact_name
     * **WHEN** centengine verifies the configuration (-v)
     * **THEN** the configuration is reported invalid (non-zero return code and "Contact has no name")
73. **ECGEMPTYNAME**:
     * **SCENARIO:** a contact group with no name is rejected
     * **GIVEN** an engine configuration with a contact group that has no contactgroup_name
     * **WHEN** centengine verifies the configuration (-v)
     * **THEN** the configuration is reported invalid (non-zero return code and "Contactgroup has no name")
74. **ECGNCM**:
     * **SCENARIO:** a contact group with a non-existing member is rejected
     * **GIVEN** an engine configuration with a contact group referencing an undefined contact
     * **WHEN** centengine verifies the configuration (-v)
     * **THEN** the configuration is reported invalid (non-zero return code and the missing contact is named)
75. **ECGRELOAD**:
     * **SCENARIO:** centengine refuses a reload adding a contact group with an undefined member
     * **GIVEN** centengine is started and ready with a valid configuration
     * **WHEN** a contact group referencing an undefined contact is added and a reload is triggered
     * **THEN** the reload is rejected (it logs the error) and centengine keeps running with the previous configuration
76. **ECGRSTART**:
     * **SCENARIO:** centengine refuses to start with a contact group referencing an undefined contact
     * **GIVEN** an engine configuration with a contact group whose member does not exist
     * **WHEN** centengine is started on that configuration
     * **THEN** it refuses to start: it exits with a failure code instead of running
77. **ECI0**: Verify contact inheritance : contact(empty) inherit from template (full), on Start Engine
78. **ECI1**: Verify contact inheritance : contact(full) inherit from template (full) , on Start Engine
79. **ECI2**: Verify contact inheritance : contact(empty) inherit from template (full) , on Reload Engine
80. **ECI3**: Verify contact inheritance : contact(full) inherit from template (full) , on Reload Engine
81. **ECMI0**: Verify command inheritance : command(empty) inherit from template (full) , on Start Engine
82. **ECMI1**: Verify command inheritance : command(full) inherit from template (full) , on Start Engine
83. **ECMI2**: Verify command inheritance : command(empty) inherit from template (full) , on Reload Engine
84. **ECMI3**: Verify command inheritance : command(full) inherit from template (full) , on reload Engine
85. **ECNHNC**:
     * **SCENARIO:** a contact without host notification commands is rejected
     * **GIVEN** an engine configuration where contact U1 has no host_notification_commands
     * **WHEN** centengine verifies the configuration (-v)
     * **THEN** the configuration is reported invalid (non-zero return code and at least one error)
86. **ECOI0**: Verify connector inheritance : connector(empty) inherit from template (full) , on Start Engine
87. **ECOI1**: Verify connector inheritance : connector(full) inherit from template (full) , on Start Engine
88. **ECOI2**: Verify connector inheritance : connector(empty) inherit from template (full) , on Reload Engine
89. **ECOI3**: Verify connector inheritance : connector(full) inherit from template (full) , on Reload Engine
90. **ECRELOAD**:
     * **SCENARIO:** centengine refuses a reload of an invalid centengine.cfg and keeps running
     * **GIVEN** centengine is started and ready with a valid configuration
     * **WHEN** centengine.cfg is made invalid (contact U1 without host_notification_commands) and a reload is triggered
     * **THEN** the reload is rejected (it logs the error) and centengine keeps running with the previous configuration
91. **ECRSTART**:
     * **SCENARIO:** centengine refuses to start with an invalid centengine.cfg
     * **GIVEN** an engine configuration where contact U1 has no host_notification_commands
     * **WHEN** centengine is started on that configuration
     * **THEN** it refuses to start: it exits with a failure code instead of running
92. **EESI0**: Verify service escalation : create service escalation for every service in a service group
93. **EESI1**: Verify service escalation  inheritance : escalation(empty) inherit from template (full) , on Start Engine
94. **EESI2**: Verify service escalation  inheritance : escalation(full) inherit from template (full) , on Start Engine
95. **EESI3**: Verify service escalation  inheritance : escalation(empty) inherit from template (full) , on Reload Engine
96. **EESI4**: Verify service escalation  inheritance : escalation(full) inherit from template (full) , on Reload Engine
97. **EESI5**: Verfiy host escalation : create host escalation for every host in the hostgroup
98. **EESI6**: Verify host escalation inheritance : escalation(empty) inherit from template (full) , on Start Engine   
99. **EESI7**: Verify host escalation inheritance : escalation(full) inherit from template (full) , on Start Engine    
100. **EESI8**: Verify host escalation inheritance : escalation(empty) inherit from template (full) , on Reload Engine   
101. **EESI9**: Verify host escalation inheritance : escalation(full) inherit from template (full) , on Reload Engine    
102. **EFHC1**: Engine is configured with hosts and we force check one 5 times with bbdo2
103. **EFHC2**: Engine is configured with hosts and we force check on one 5 times on bbdo2
104. **EFHCU1**: Engine is configured with hosts and we force checks on one 5 times on bbdo3. Bbdo3 has no impact on this behavior. resources table is cleared before starting broker.
105. **EFHCU2**: Engine is configured with hosts and we force checks on one 5 times on bbdo3. Bbdo3 has no impact on this behavior.
106. **EHGI0**: Verify hostgroup inheritance : hostgroup(empty) inherit from template (full) , on Start Engine
107. **EHGI1**: Verify hostgroup inheritance : hostgroup(full) inherit from template (full) , on Start Engine
108. **EHGI2**: Verify hostgroup inheritance : hostgroup(empty) inherit from template (full) , on Reload Engine
109. **EHGI3**: Verify hostgroup inheritance : hostgroup(full) inherit from template (full) , on Reload Engine
110. **EHGNCM**:
     * **SCENARIO:** a host group with a non-existing member is rejected
     * **GIVEN** an engine configuration with a host group referencing an undefined host
     * **WHEN** centengine verifies the configuration (-v)
     * **THEN** the configuration is reported invalid (non-zero return code and the missing host is named)
111. **EHGRELOAD**:
     * **SCENARIO:** centengine refuses a reload adding a host group with an undefined member
     * **GIVEN** centengine is started and ready with a valid configuration
     * **WHEN** a host group referencing an undefined host is added and a reload is triggered
     * **THEN** the reload is rejected (it logs the error) and centengine keeps running with the previous configuration
112. **EHGRSTART**:
     * **SCENARIO:** centengine refuses to start with a host group referencing an undefined host
     * **GIVEN** an engine configuration with a host group whose member does not exist
     * **WHEN** centengine is started on that configuration
     * **THEN** it refuses to start: it exits with a failure code instead of running
113. **EHI0**: Verify inheritance host : host(empty) inherit from template (full) , on Start Engine
114. **EHI1**: Verify inheritance host : host(full) inherit from template (full) , on Start engine
115. **EHI2**: Verify inheritance host : host(empty) inherit from template (full) , on Reload engine
116. **EHI3**: Verify inheritance host : host(full) inherit from template (full) , on engine Reload
117. **EMACROS**: macros ADMINEMAIL and ADMINPAGER are replaced in check outputs
118. **EMACROS_NOTIF**: macros ADMINEMAIL and ADMINPAGER are replaced in notification commands
119. **EMACROS_SEMICOLON**: Macros with a semicolon are used even if they contain a semicolon.
120. **EMTI0**: Verify multiple inheritance host
121. **ENGINE_MANY_CHECKS**:
     * **GIVEN** a engine with many services and a unique check on each service with it's own env variables
     We expect correct check result in logs and we checks returned args and service macros
122. **EPC1**: Check with perl connector
123. **ERL**: Engine is started and writes logs in centengine.log. Then we remove the log file. The file disappears but Engine is still writing into it. Engine is reloaded and the centengine.log should appear again.
124. **ESGI0**: Verify servicegroup inheritance : servicegroup(empty) inherit from template (full) , on Start Engine
125. **ESGI1**: Verify servicegroup inheritance : servicegroup(empty) inherit from template (full) , on Start Engine
126. **ESGI2**: Verify servicegroup inheritance : servicegroup(empty) inherit from template (full) , on Reload Engine
127. **ESGI3**: Verify servicegroup inheritance : servicegroup(empty) inherit from template (full) , on Reload Engine
128. **ESGNCM**:
     * **SCENARIO:** a service group with a non-existing member is rejected
     * **GIVEN** an engine configuration with a service group referencing an undefined service
     * **WHEN** centengine verifies the configuration (-v)
     * **THEN** the configuration is reported invalid (non-zero return code and the missing service is named)
129. **ESGRELOAD**:
     * **SCENARIO:** centengine refuses a reload adding a service group with an undefined member
     * **GIVEN** centengine is started and ready with a valid configuration
     * **WHEN** a service group referencing an undefined service is added and a reload is triggered
     * **THEN** the reload is rejected (it logs the error) and centengine keeps running with the previous configuration
130. **ESGRSTART**:
     * **SCENARIO:** centengine refuses to start with a service group referencing an undefined service
     * **GIVEN** an engine configuration with a service group whose member does not exist
     * **WHEN** centengine is started on that configuration
     * **THEN** it refuses to start: it exits with a failure code instead of running
131. **ESI0**: Verify inheritance service : Service(empty) inherit from template (full) , on Start Engine
132. **ESI1**: Verify inheritance service : Service(full) inherit from template (full) , on Start Engine
133. **ESI2**: Verify inheritance service : Service(empty) inherit from template (full) , on Reload Engine
134. **ESI3**: Verify inheritance service : Service(full) inherit from template (full) , on Reload Engine
135. **ESS1**: Start-Stop (0s between start/stop) 5 times one instance of engine and no coredump
136. **ESS2**: Start-Stop (300ms between start/stop) 5 times one instance of engine and no coredump
137. **ESS3**: Start-Stop (0s between start/stop) 5 times three instances of engine and no coredump
138. **ESS4**: Start-Stop (300ms between start/stop) 5 times three instances of engine and no coredump
139. **ESSCTO**:
     * **SCENARIO:** Engine services timeout due to missing Perl connector
     * **GIVEN** the Engine is configured as usual without the Perl connector
     * **WHEN** the Engine executes its service commands
     * **THEN** the commands take too long and reach the timeout
     * **AND** the Engine starts and stops four times as a result
140. **ESSCTOWC**:
     * **SCENARIO:** Engine services timeout due to missing Perl connector
     * **GIVEN** the Engine is configured as usual with some command using the Perl connector
     * **WHEN** the Engine executes its service commands
     * **THEN** the commands take too long and reach the timeout
     * **AND** the Engine starts and stops four times as a result
141. **ESSOCWNV**:
     * **SCENARIO:** Engine is started with a valid old configuration (concerning cbmod)
     * **GIVEN** the Engine is configured with a valid old configuration
     * **WHEN** the Engine is started
     * **THEN** the Engine starts correctly
     * **AND** the Engine stops correctly
142. **ESS_STATS**:
     * **SCENARIO:** Reading the stats file after Engine has started
     * **GIVEN** the Engine is started
     * **WHEN** we read the Engine's stats file
     * **THEN** Engine must not crash
143. **EVOCWNV**:
     * **SCENARIO:** The new Engine checks the old configuration (concerning cbmod)
     * **GIVEN** the Engine is configured with a valid old configuration
     * **WHEN** the Engine is started to check the configuration
     * **THEN** the Engine reads it as expected
144. **EXT_CONF1**: Engine configuration is overidden by json conf
145. **EXT_CONF2**: Engine configuration is overidden by json conf after reload
146. **E_FD_LIMIT**: Engine here is started with a low file descriptor limit. The engine should not crash and limit should be set.
147. **E_HOST_DOWN_DISABLE_SERVICE_CHECKS**: host_down_disable_service_checks is set to 1, host down switch all services to UNKNOWN
148. **E_HOST_UNREACHABLE_DISABLE_SERVICE_CHECKS**: host_down_disable_service_checks is set to 1, host unreachable switch all services to UNKNOWN
149. **VERIF**:
     * **WHEN** centengine is started in verification mode, it does not log in its file.
150. **VERIFY_CONF**: Scenario Verify deprecated engine configuration options are logged as warnings Given the engine and broker are configured with module 1 And the engine configuration is set with deprecated options When the engine is started Then a warning message for 'auto_reschedule_checks' should be logged And a warning message for 'auto_rescheduling_interval' should be logged And a warning message for 'auto_rescheduling_window' should be logged And the engine should be stopped

### Severities

This chapter contains 31 tests.

1. **BECSEV1**:
     * **FEATURE:** Severity Management between Engine and Broker
     As a Centreon administrator
     I want to configure severities in Engine
     So that Broker stores them correctly in centreon_storage.severities table
     * **BACKGROUND:**
     * **GIVEN** Engine is configured with centralized setup
     * **AND** Broker components (central, rrd, module) are configured
     * **AND** Database logging is enabled with debug/trace level
     * **AND** Retention data is cleared
     * **SCENARIO:** Initial severity configuration
     * **GIVEN** Engine is configured with 20 severities
     * **WHEN** Broker and Engine are started
     * **THEN** 20 severities should be added/modified in logs
     * **AND** INSERT statements should be executed in severities table
     * **AND** Configuration file should match database content
     * **AND** Severity IDs should be consistent
     * **SCENARIO:** Severity configuration modification
     * **GIVEN** Initial configuration with 20 severities is loaded
     * **WHEN** Configuration is modified to 30 severities
     * **AND** Engine configuration change is notified
     * **THEN** 10 additional severities should be added/modified
     * **AND** Configuration file should still match database content
     * **AND** Severity IDs should remain consistent
     * **SCENARIO:** Severity configuration reduction
     * **GIVEN** Configuration with 30 severities is loaded
     * **WHEN** Configuration is reduced to 11 severities starting at ID 50
     * **AND** Engine configuration change is notified
     * **THEN** 11 severities should be present in final configuration
     * **AND** Unused severities should be implicitly removed
     * **AND** Configuration file should match database content
     * **AND** Severity IDs should be consistent with new range
2. **BECSEV2**:
     * **SCENARIO:** Severity db_ids correctly restored after broker restart
     * **GIVEN** broker and engine are started with 20 severities configured on poller 1
     * **AND** services 1 to 4 are linked to severity 11
     * **AND** severities are correctly inserted in DB with non-zero db_ids in broker cache
     * **WHEN** broker is restarted while engine keeps running
     * **AND** the poller reconnects so the config is reprocessed against an already populated DB
     * **THEN** severity db_ids should still be non-zero in broker cache
     * **AND** services should still have correct severity_id in the resources table
3. **BECSEV3**:
     * **SCENARIO:** Severity db_ids correctly restored after broker restart with lost prot files
     * **GIVEN** broker and engine are started with 20 severities configured on poller 1
     * **AND** services 1 to 4 are linked to severity 11
     * **AND** severities are correctly inserted in DB with non-zero db_ids in broker cache
     * **WHEN** broker is restarted after losing its prot files (simulating a fresh broker with existing DB)
     * **AND** engine sends its full configuration back (DiffState unknown path)
     * **THEN** _add_severities_mariadb is called with all-duplicate rows (ON DUPLICATE KEY UPDATE)
     * **AND** LAST_INSERT_ID() returns 0 for all rows, potentially overwriting db_ids in cache with 0
     * **THEN** severity db_ids should still be non-zero in broker cache
     * **AND** services should still have correct severity_id in the resources table
4. **BECSEV4_batch**:
     * **FEATURE:** Severity presence in Broker gRPC cache with centralized configuration
     * **BACKGROUND:**
     * **GIVEN** 4 pollers are configured with 5 hosts each (20 total) and 20 services per host
     * **AND** each poller defines 2 severities: id=1/SERVICE/level=1 and id=2/HOST/level=2
     * **AND** severity 1 is assigned to all services, severity 2 to all hosts
     * **AND** Broker and Engine are started in centralized (BBDO3) mode
     * **SCENARIO:** Phase 1 — All pollers active
     * **THEN** the broker gRPC cache contains severity (1, SERVICE, level=1)
     * **AND** the broker gRPC cache contains severity (2, HOST, level=2)
     * **SCENARIO:** Phase 2 — Severity removed from poller 3
     * **WHEN** severities are removed from poller 3 and broker is notified
     * **THEN** the broker cache STILL contains severity (1, SERVICE) (pollers 0-2 have it)
     * **AND** the broker cache STILL contains severity (2, HOST)
     * **SCENARIO:** Phase 3 — Severity removed from poller 2
     * **WHEN** severities are removed from poller 2 and broker is notified
     * **THEN** the broker cache STILL contains severity (1, SERVICE) (pollers 0-1 have it)
     * **AND** the broker cache STILL contains severity (2, HOST)
     * **SCENARIO:** Phase 4 — Severity removed from all remaining pollers
     * **WHEN** severities are removed from pollers 0 and 1 and broker is notified
     * **THEN** the broker cache contains 0 severities
     * **SCENARIO:** Phase 5 — Severity cache is empty (no orphan entries)
     * **THEN** GetSeverities returns an empty list
5. **BECSEV4_per_poller**:
     * **FEATURE:** Severity presence in Broker gRPC cache with centralized configuration
     * **BACKGROUND:**
     * **GIVEN** 4 pollers are configured with 5 hosts each (20 total) and 20 services per host
     * **AND** each poller defines 2 severities: id=1/SERVICE/level=1 and id=2/HOST/level=2
     * **AND** severity 1 is assigned to all services, severity 2 to all hosts
     * **AND** Broker and Engine are started in centralized (BBDO3) mode
     * **SCENARIO:** Phase 1 — All pollers active
     * **THEN** the broker gRPC cache contains severity (1, SERVICE, level=1)
     * **AND** the broker gRPC cache contains severity (2, HOST, level=2)
     * **SCENARIO:** Phase 2 — Severity removed from poller 3
     * **WHEN** severities are removed from poller 3 and broker is notified
     * **THEN** the broker cache STILL contains severity (1, SERVICE) (pollers 0-2 have it)
     * **AND** the broker cache STILL contains severity (2, HOST)
     * **SCENARIO:** Phase 3 — Severity removed from poller 2
     * **WHEN** severities are removed from poller 2 and broker is notified
     * **THEN** the broker cache STILL contains severity (1, SERVICE) (pollers 0-1 have it)
     * **AND** the broker cache STILL contains severity (2, HOST)
     * **SCENARIO:** Phase 4 — Severity removed from all remaining pollers
     * **WHEN** severities are removed from pollers 0 and 1 and broker is notified
     * **THEN** the broker cache contains 0 severities
     * **SCENARIO:** Phase 5 — Severity cache is empty (no orphan entries)
     * **THEN** GetSeverities returns an empty list
6. **BECSEV5_batch**:
     * **FEATURE:** Severity level change is reflected in the Broker gRPC cache
     * **BACKGROUND:**
     * **GIVEN** 4 pollers configured with 5 hosts each (20 total) and 20 services per host
     * **AND** each poller defines severity id=1 (SERVICE, level=1) and id=2 (HOST, level=2)
     * **AND** severities assigned to all hosts and services
     * **SCENARIO:** Severity levels are updated in the broker cache after modification
     * **GIVEN** the initial state has severity (1, SERVICE, level=1) and (2, HOST, level=2)
     * **WHEN** all 4 pollers update their severities to level=4 (SERVICE) and level=5 (HOST)
     * **THEN** broker GetSeverities returns (1, SERVICE, level=4) and (2, HOST, level=5)
7. **BECSEV5_per_poller**:
     * **FEATURE:** Severity level change is reflected in the Broker gRPC cache
     * **BACKGROUND:**
     * **GIVEN** 4 pollers configured with 5 hosts each (20 total) and 20 services per host
     * **AND** each poller defines severity id=1 (SERVICE, level=1) and id=2 (HOST, level=2)
     * **AND** severities assigned to all hosts and services
     * **SCENARIO:** Severity levels are updated in the broker cache after modification
     * **GIVEN** the initial state has severity (1, SERVICE, level=1) and (2, HOST, level=2)
     * **WHEN** all 4 pollers update their severities to level=4 (SERVICE) and level=5 (HOST)
     * **THEN** broker GetSeverities returns (1, SERVICE, level=4) and (2, HOST, level=5)
8. **BECSEV6_batch**:
     * **FEATURE:** GetSeverities gRPC returns correct content while severities are active
     * **BACKGROUND:**
     * **GIVEN** 4 pollers configured with 5 hosts each (20 total) and 20 services per host
     * **AND** each poller defines severity id=1 (SERVICE, level=1) and id=2 (HOST, level=2)
     * **AND** severities assigned to all hosts and services
     * **SCENARIO:** GetSeverities returns 2 entries with correct metadata when all pollers are active
     * **WHEN** Broker and Engine are started and synchronized
     * **THEN** GetSeverities returns exactly 2 entries
     * **AND** severity (1, SERVICE, level=1) is present
     * **AND** severity (2, HOST, level=2) is present
9. **BECSEV6_per_poller**:
     * **FEATURE:** GetSeverities gRPC returns correct content while severities are active
     * **BACKGROUND:**
     * **GIVEN** 4 pollers configured with 5 hosts each (20 total) and 20 services per host
     * **AND** each poller defines severity id=1 (SERVICE, level=1) and id=2 (HOST, level=2)
     * **AND** severities assigned to all hosts and services
     * **SCENARIO:** GetSeverities returns 2 entries with correct metadata when all pollers are active
     * **WHEN** Broker and Engine are started and synchronized
     * **THEN** GetSeverities returns exactly 2 entries
     * **AND** severity (1, SERVICE, level=1) is present
     * **AND** severity (2, HOST, level=2) is present
10. **BECSEV7_batch**:
     * **FEATURE:** Broker cache is repopulated after broker restart with severities active
     * **BACKGROUND:**
     * **GIVEN** 4 pollers configured with 5 hosts each (20 total) and 20 services per host
     * **AND** each poller defines severity id=1 (SERVICE, level=1) and id=2 (HOST, level=2)
     * **AND** severities assigned to all hosts and services
     * **SCENARIO:** After broker restart, GetSeverities returns correct data
     * **GIVEN** broker and engine are started and synchronized
     * **AND** GetSeverities returns 2 entries before broker stops
     * **WHEN** broker is stopped and restarted (engine keeps running)
     * **THEN** GetSeverities returns the same 2 entries after restart
11. **BECSEV7_per_poller**:
     * **FEATURE:** Broker cache is repopulated after broker restart with severities active
     * **BACKGROUND:**
     * **GIVEN** 4 pollers configured with 5 hosts each (20 total) and 20 services per host
     * **AND** each poller defines severity id=1 (SERVICE, level=1) and id=2 (HOST, level=2)
     * **AND** severities assigned to all hosts and services
     * **SCENARIO:** After broker restart, GetSeverities returns correct data
     * **GIVEN** broker and engine are started and synchronized
     * **AND** GetSeverities returns 2 entries before broker stops
     * **WHEN** broker is stopped and restarted (engine keeps running)
     * **THEN** GetSeverities returns the same 2 entries after restart
12. **BESEV1**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Broker is started before.
13. **BESEV2**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Engine is started before.
14. **BETUHSEV1**: Hosts have severities provided by templates.
15. **BETUSEV1**: Services have severities provided by templates.
16. **BEUHSEV1**: Four hosts have a severity added. Then we remove the severity from host 1. Then we change severity 10 to severity8 for host 3.
17. **BEUHSEV2**: Seven hosts are configured with a severity on two pollers. Then we remove severities from the first and second hosts of the first poller but only the severity from the first host of the second poller.
18. **BEUSEV1**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Broker is started before.
19. **BEUSEV2**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Engine is started before.
20. **BEUSEV3**: Four services have a severity added. Then we remove the severity from service 1. Then we change severity 11 to severity7 for service 3.
21. **BEUSEV4**: Seven services are configured with a severity on two pollers. Then we remove severities from the first and second services of the first poller but only the severity from the first service of the second poller. Then only severities no more used should be removed from the database.
22. **CBESEV1**:
     * **SCENARIO:** Severities stored in database when Broker starts first (centralized)
     * **GIVEN** Engine is configured with centralized setup and 20 severities
     * **AND** Broker components (central, rrd, module) are configured
     * **AND** retention data is cleared
     * **WHEN** Broker is started before Engine
     * **THEN** severity20 should be of level 5 with icon_id 1
     * **AND** severity1 should be of level 1 with icon_id 5
23. **CBESEV2**:
     * **SCENARIO:** Severities stored in database when Engine starts first (centralized)
     * **GIVEN** Engine is configured with centralized setup and 20 severities
     * **AND** Broker components (central, rrd, module) are configured
     * **AND** retention data is cleared
     * **WHEN** Engine is started before Broker
     * **THEN** severity20 should be of level 5 with icon_id 1
     * **AND** severity1 should be of level 1 with icon_id 5
24. **CBETUHSEV1**:
     * **GIVEN** hosts on two pollers using templates that define severities
     (template_1: severity 2 on poller 0, severity 6 on poller 1;
     template_2: severity 4 on poller 0, severity 10 on poller 1),
     * **WHEN** the engine and broker are started with centralized configuration,
     * **THEN** host 2 and host 4 should have severity_id=2
     * **AND** host 5 should have severity_id=4
     * **AND** host 31 should have severity_id=6
     * **AND** host 33 should have severity_id=10.
25. **CBETUSEV1**:
     * **SCENARIO:** Service severities inherited from templates via unified SQL (centralized)
     * **GIVEN** Engine is configured with centralized setup across 2 pollers and 20 severities each
     * **AND** service templates with severity assignments are configured
     * **AND** Broker is configured with unified SQL output and BBDO3
     * **WHEN** Engine and Broker are started
     * **THEN** services inheriting template_1 on poller 1 should have severity_id=1
     * **AND** services inheriting template_2 on poller 1 should have severity_id=3
     * **AND** services inheriting template_1 on poller 2 should have severity_id=3
     * **AND** services inheriting template_2 on poller 2 should have severity_id=5
26. **CBEUHSEV1**:
     * **GIVEN** four hosts with a severity added,
     * **WHEN** we remove the severity from host 1
     * **AND** we change severity 10 to severity 8 for host 3,
     * **THEN** host 2 should still have severity_id=10
     * **AND** host 4 should still have severity_id=10
     * **AND** host 3 should have severity_id=8
     * **AND** host 1 should have no severity.
27. **CBEUHSEV2**:
     * **SCENARIO:** Host severities removed on one poller and changed on another are applied poller by poller
     * **GIVEN** seven hosts configured with severities on two pollers,
     * **WHEN** we remove severities from hosts on the first poller
     * **AND** we change host 28's severity from 16 to 14 on the second poller,
     * **THEN** host 26 should still have severity_id=18
     * **AND** host 27 should still have severity_id=18
     * **AND** host 28 should have severity_id=14
     * **AND** hosts 3, 4 and 5 on the first poller should have no severity.
28. **CBEUSEV1**:
     * **SCENARIO:** Severities stored via unified SQL when Broker starts first (centralized)
     * **GIVEN** Engine is configured with centralized setup and 20 severities
     * **AND** Broker is configured with unified SQL output and BBDO3
     * **AND** retention data is cleared
     * **WHEN** Broker is started before Engine
     * **THEN** severity20 should be of level 5 with icon_id 1
     * **AND** severity1 should be of level 1 with icon_id 5
29. **CBEUSEV2**:
     * **SCENARIO:** Severities stored via unified SQL when Engine starts first (centralized)
     * **GIVEN** Engine is configured with centralized setup and 20 severities
     * **AND** Broker is configured with unified SQL output and BBDO3
     * **AND** retention data is cleared
     * **WHEN** Engine is started before Broker
     * **THEN** severity20 should be of level 5 with icon_id 1
     * **AND** severity1 should be of level 1 with icon_id 5
30. **CBEUSEV3**:
     * **SCENARIO:** Service severity removal and change via unified SQL (centralized)
     * **GIVEN** Engine is configured with centralized setup and 20 severities
     * **AND** Broker is configured with unified SQL output and BBDO3
     * **AND** severity 11 is assigned to services 1, 2, 3 and 4
     * **WHEN** Engine and Broker are started
     * **THEN** service (1, 1) should have severity_id=11
     * **WHEN** severity is removed from all services and reassigned (11 to services 2,4 and 7 to service 3)
     * **AND** Engine and Broker are reloaded
     * **THEN** service (1, 3) should have severity_id=7
     * **AND** service (1, 1) should have no severity
31. **CBEUSEV4**:
     * **SCENARIO:** Severity removal across two pollers via unified SQL (centralized)
     * **GIVEN** Engine is configured with centralized setup across 2 pollers and 20 severities each
     * **AND** severity 19 is assigned to services 2,4 on poller 1 and services 501,502 on poller 2
     * **AND** severity 17 is assigned to services 3,5 on poller 1 and service 503 on poller 2
     * **AND** Broker is configured with unified SQL output and BBDO3
     * **WHEN** Engine and Broker are started
     * **THEN** all services should have their expected severity_id values
     * **WHEN** severities are removed from poller 1 services and severity files reduced to 18
     * **AND** severity 17 is kept on service 503 of poller 2
     * **AND** Engine and Broker are reloaded
     * **THEN** service (26, 503) should still have severity_id=17
     * **AND** services on poller 1 that lost their severity should have severity_id=None

### Vault

This chapter contains 22 tests.

1. **BAEBC**: Broker is used to AES256 decrypt a content not well encrypted
2. **BAEBS**: Broker is used to AES256 encrypt a content but the salt is wrong.
3. **BAEOK**: Broker is used to AES256 encrypt a content.
4. **BASV**: Broker accesses to the vault to get database credentials but vault is stopped.
5. **BAV**: Broker accesses to the vault to get database credentials.
6. **BWVC1**: Broker is tuned with a wrong vault configuration and the env file doesn't exist.
7. **BWVC2**: Broker is tuned with a wrong vault configuration and the env file exists with a wrong content.
8. **BWVC3**: Broker is tuned with an env file containing a strange key APP_SECRET and a wrong vault configuration.
9. **BWVC4**: Broker is tuned with an env file containing a strange key APP_SECRET and a vault configuration with a bad json.
10. **BWVC5**: Broker is tuned with strange keys APP_SECRET and salt.
11. **BWVC6**: Broker is tuned with strange keys APP_SECRET and salt that are not base64 encoded.
12. **CBAEBC**:
     * **SCENARIO:** AES256 decryption of non-encrypted content returns an error
     * **GIVEN** broker is started in centralized mode
     * **WHEN** AES256 decryption is attempted on content that is not properly encrypted
     * **THEN** broker returns an error indicating the content is not AES256 encrypted
13. **CBAEBS**:
     * **SCENARIO:** AES256 encryption with invalid base64 salt returns an error
     * **GIVEN** broker is started in centralized mode
     * **WHEN** AES256 encryption is attempted with a non-base64 salt
     * **THEN** broker returns an error about illegal base64 characters
14. **CBAEOK**:
     * **SCENARIO:** AES256 encrypt then decrypt returns the original content
     * **GIVEN** broker is started in centralized mode
     * **WHEN** AES256 encryption is applied to content
     * **THEN** the decrypted result matches the original content
15. **CBASV**:
     * **SCENARIO:** Broker with vault configured but vault server down logs an error
     * **GIVEN** broker is started in centralized mode
     * **AND** the vault server is not running
     * **WHEN** broker is configured to retrieve credentials from the vault
     * **THEN** broker logs an error about the inactive http server
16. **CBAV**:
     * **SCENARIO:** Broker retrieves database password from a running vault
     * **GIVEN** broker is started in centralized mode
     * **AND** a vault is running with valid credentials
     * **WHEN** broker is configured to retrieve the database password from the vault
     * **THEN** broker logs that the database password was retrieved from vault
17. **CBWVC1**:
     * **SCENARIO:** Broker with missing vault env file logs an error
     * **GIVEN** broker configured with a wrong vault configuration and no env file
     * **WHEN** broker starts
     * **THEN** broker logs an error that the env file could not be opened
18. **CBWVC2**:
     * **SCENARIO:** Broker with env file missing APP_SECRET logs an error
     * **GIVEN** broker configured with a wrong vault configuration
     * **AND** an env file with invalid content (no APP_SECRET)
     * **WHEN** broker starts
     * **THEN** broker logs an error about missing APP_SECRET
19. **CBWVC3**:
     * **SCENARIO:** Broker with wrong vault file path logs a JSON parse error
     * **GIVEN** broker configured with a strange APP_SECRET and a non-existent vault file
     * **WHEN** broker starts
     * **THEN** broker logs an error about the wrong vault file
20. **CBWVC4**:
     * **SCENARIO:** Broker with malformed vault JSON file logs an error
     * **GIVEN** broker configured with a strange APP_SECRET and a vault file missing required keys
     * **WHEN** broker starts
     * **THEN** broker logs an error about the malformed vault file
21. **CBWVC5**:
     * **SCENARIO:** Broker with non-string salt in vault file logs a type error
     * **GIVEN** broker configured with a strange APP_SECRET and a vault file with numeric salt
     * **WHEN** broker starts
     * **THEN** broker logs an error about the bad encryption type
22. **CBWVC6**:
     * **SCENARIO:** Broker with non-base64 salt in vault file logs an encoding error
     * **GIVEN** broker configured with APP_SECRET and a vault file containing non-base64 salt
     * **WHEN** broker starts
     * **THEN** broker logs an error about the bad base64 encoding


912 tests currently implemented.
