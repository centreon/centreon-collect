# Centreon Tests

This sub-project contains functional tests for Centreon Broker, Engine and Connectors.
It is based on the [Robot Framework](https://robotframework.org/) with Python functions
we can find in the resources directory. The Python code is formatted using autopep8 and
robot files are formatted using `robottidy --overwrite tests`.

## Getting Started

To get this project, you have to clone centreon-collect.

These tests are executed from the `centreon-tests/robot` folder and uses the [Robot Framework](https://robotframework.org/).

From a Centreon host, you need to install Robot Framework.

On AlmaLinux, we have to install some python packages, some perl packages:

```bash
dnf install "Development Tools" python3-devel -y
dnf install perl-HTTP-Daemon-SSL -y
dnf install perl-JSON -y
```

On rpm based system, we have to execute the following commands (maybe to update a little):

```bash
yum install "Development Tools" python3-devel -y
yum install perl-HTTP-Daemon-SSL -y
yum install perl-JSON -y
```

On deb based system, we have to execute:


```bash
apt-get install python3-dev openssh-server
```

Once these packages, we recommand to create a python virtual environment to play with robot framework.

You can do that as you prefer, here we use uv. The first step is to install it:

```bash
curl -LsSf https://astral.sh/uv/install.sh | less
```

Once installed, you have to create a virtual environment, we create it in the centreon-collect/tests directory:

```bash
cd centreon-collect/tests
uv venv --python=python3.11 robotframework
```

And now, we can install the required python modules for our tests:

```bash
uv pip install -U robotframework \
        robotframework-databaselibrary \
        robotframework-examples pymysql \
        robotframework-requests psutil \
        robotframework-httpctrl boto3 \
        GitPython unqlite py-cpuinfo pyjwt \
        grpcio grpcio_tools
```

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

In order to execute bench tests (broker-engine/bench.robot), you need also to
install py-cpuinfo, cython, unqlite and boto3

```bash
uv pip install py-cpuinfo cython unqlite gitpython boto3
```

## Implemented tests

Here is the list of the currently implemented tests:

### Bam
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
11. **BAWORST_ACK**: **SCENARIO:** Acknowledging a service acknowledges the BA, and removing it unacknowledges the BA

     * **GIVEN** BBDO version is 3.0.1
     * **AND** a Business Activity of type "worst" is configured with two services
     * **WHEN** one of the services is acknowledged
     * **THEN** the Business Activity is acknowledged
     * **WHEN** the acknowledgement is removed from the service
     * **THEN** the Business Activity is no longer acknowledged
12. **BA_BOOL_KPI**: With bbdo version 3.0.1, a BA of type 'worst' with 1 boolean kpi
13. **BA_CHANGED**: **SCENARIO:** Replace Service KPI with Boolean Rule KPI in Worst-type BA

     * **GIVEN** a BA of type "worst" is configured with one service KPI
     * **WHEN** the service KPI is replaced by a boolean rule KPI
     * **AND** Broker is reloaded
     * **THEN** the BA is correctly updated with the new KPI configuration
14. **BA_DISABLED**: create a disabled BA with timeperiods and reporting filter don't create error message
15. **BA_IMPACT_2KPI_SERVICES**: With bbdo version 3.0.1, a BA of type 'impact' with 2 serv, ba is critical only if the 2 services are critical
16. **BA_IMPACT_IMPACT**: 
     * **GIVEN** a Business Activity (BA) of type "impact"
     * **AND** it has two child BAs of type "impact"
     * **AND** the first child has an impact of 90
     * **AND** the second child has an impact of 10
     * **WHEN** both child BAs are impacting
     * **THEN** the parent BA should be "critical"
     * **WHEN** both child BAs are not impacting
     * **THEN** the parent BA should be "ok"
17. **BA_RATIO_NUMBER_BA_4_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
18. **BA_RATIO_NUMBER_BA_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 2 services and one ba with 1 service
19. **BA_RATIO_PERCENT_BA_4_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
20. **BA_RATIO_PERCENT_BA_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio percent' with 2 serv an 1 ba with one service
21. **BA_SERVICE_PNAME_AFTER_RELOAD**: **SCENARIO:** Verify that the parent_name of a BA service is not erased after a broker reload

     * **GIVEN** a BA "test" of type "worst" with its service "host_16:service_302"
     * **WHEN** I start broker and engine
     * **THEN** the BA service "test" should have a status of 0 within 30 seconds
     * **WHEN** I reload the broker
     * **THEN** the database should still contain a BA service with name "test" and parent_name "_Module_BAM_1"
22. **BEBAMIDT1**: 
     * **GIVEN** a BA of type 'worst' with one service is configured
     * **AND** The BA is in critical state due to its service
     * **WHEN** a downtime is set on this service
     * **THEN** an inherited downtime is set to the BA
     * **WHEN** the downtime is removed from the service
     * **THEN** the inherited downtime is deleted from the BA
23. **BEBAMIDT2**: 
     * **GIVEN** a BA of type 'worst' with one service is configured
     * **AND** the BA is in critical state due to its service
     * **AND** a downtime is set on this service
     * **THEN** an inherited downtime is set to the BA
     * **WHEN** Engine is restarted
     * **AND** Broker is restarted
     * **THEN** both downtimes are still present with no duplicates
     * **WHEN** the downtime is removed from the service
     * **THEN** the inherited downtime is deleted
24. **BEBAMIDTU1**: 
     * **GIVEN** BBDO version 3.0.1 is running
     * **AND** a BA of type 'worst' with one service is configured
     * **AND** The BA is in critical state due to its service
     * **WHEN** a downtime is set on this service
     * **THEN** an inherited downtime is set to the BA
     * **WHEN** the downtime is removed from the service
     * **THEN** the inherited downtime is deleted from the BA
25. **BEBAMIDTU2**: 
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
26. **BEBAMIGNDT1**: A BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. A first downtime is cancelled, the BA is still OK, but when the second downtime is cancelled, the BA should be CRITICAL.
27. **BEBAMIGNDT2**: A BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. The first downtime reaches its end, the BA is still OK, but when the second downtime reaches its end, the BA should be CRITICAL.
28. **BEBAMIGNDTU1**: With bbdo version 3.0.1, a BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. A first downtime is cancelled, the BA is still OK, but when the second downtime is cancelled, the BA should be CRITICAL.
29. **BEBAMIGNDTU2**: 
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
30. **BECBAMIDTU1**: 
     * **GIVEN** BBDO version 3.0.1 is running with centralized configuration enabled
     * **AND** a BA of type 'worst' with one service is configured
     * **AND** The BA is in critical state due to its service
     * **WHEN** a downtime is set on this service
     * **THEN** an inherited downtime is set to the BA
     * **WHEN** the downtime is removed from the service
     * **THEN** the inherited downtime is deleted from the BA
31. **BECBAMIDTU2**: 
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
32. **BECBAMIGNDTU1**: With bbdo version 3.0.1, a BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. A first downtime is cancelled, the BA is still OK, but when the second downtime is cancelled, the BA should be CRITICAL.
33. **BECBAMIGNDTU2**: 
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
34. **BECPB_BA_DURATION_EVENT**: use of pb_ba_duration_event message.
35. **BECPB_DIMENSION_BA_BV_RELATION_EVENT**: bbdo_version 3 use pb_dimension_ba_bv_relation_event message.
36. **BECPB_DIMENSION_BA_EVENT**: bbdo_version 3 use pb_dimension_ba_event message.
37. **BECPB_DIMENSION_BA_TIMEPERIOD_RELATION**: use of pb_dimension_ba_timeperiod_relation message.
38. **BECPB_DIMENSION_BV_EVENT**: bbdo_version 3 use pb_dimension_bv_event message.
39. **BECPB_DIMENSION_KPI_EVENT**: bbdo_version 3 use pb_dimension_kpi_event message.
40. **BECPB_DIMENSION_TIMEPERIOD**: use of pb_dimension_timeperiod message.
41. **BECPB_DIMENSION_TRUNCATE_TABLE**: use of pb_dimension_timeperiod message.
42. **BECPB_KPI_STATUS**: bbdo_version 3 use kpi_status message.
43. **BEPB_BA_DURATION_EVENT**: use of pb_ba_duration_event message.
44. **BEPB_DIMENSION_BA_BV_RELATION_EVENT**: bbdo_version 3 use pb_dimension_ba_bv_relation_event message.
45. **BEPB_DIMENSION_BA_EVENT**: bbdo_version 3 use pb_dimension_ba_event message.
46. **BEPB_DIMENSION_BA_TIMEPERIOD_RELATION**: use of pb_dimension_ba_timeperiod_relation message.
47. **BEPB_DIMENSION_BV_EVENT**: bbdo_version 3 use pb_dimension_bv_event message.
48. **BEPB_DIMENSION_KPI_EVENT**: bbdo_version 3 use pb_dimension_kpi_event message.
49. **BEPB_DIMENSION_TIMEPERIOD**: use of pb_dimension_timeperiod message.
50. **BEPB_DIMENSION_TRUNCATE_TABLE**: use of pb_dimension_timeperiod message.
51. **BEPB_KPI_STATUS**: bbdo_version 3 use kpi_status message.
52. **CBABEST_SERVICE_CRITICAL**: With bbdo version 3.0.1, a BA of type 'best' with 2 serv, ba is critical only if the 2 services are critical
53. **CBABOO**: **SCENARIO:** A "worst" BA and an impact BA with an OR boolean rule built on the same 2 services behave identically when a service becomes CRITICAL

     * **GIVEN** a BA of type "worst" with service_302 and service_303 as KPIs
     * **AND** a BA of type "impact" with a boolean rule "{service_302} IS CRITICAL OR {service_303} IS CRITICAL"
     * **WHEN** service_302 becomes CRITICAL
     * **THEN** both BAs are CRITICAL
     * **WHEN** service_302 recovers to OK
     * **THEN** both BAs return to OK
     * **AND** this cycle is repeated 10 times
54. **CBABOOAND**: **SCENARIO:** An AND boolean rule evaluates to CRITICAL as soon as one operand is false, even when the other service is UNKNOWN

     * **GIVEN** a BA of type "impact" with boolean rule "{service_302} IS OK AND {service_303} IS OK"
     * **AND** service_303 is passive and starts UNKNOWN
     * **WHEN** service_302 becomes CRITICAL
     * **THEN** the BA is CRITICAL (AND short-circuits on the first false operand)
55. **CBABOOCOMPL**: **SCENARIO:** A BA with a complex AND/OR boolean rule over 20 services becomes OK only when at least one service in each AND group is OK

     * **GIVEN** a BA of type "impact" with a rule of 10 AND groups, each requiring at least one of 2 services to be OK
     * **WHEN** all 20 services are CRITICAL
     * **THEN** the BA is CRITICAL
     * **WHEN** odd-indexed services are set to OK one by one
     * **THEN** the BA remains CRITICAL until all AND groups have at least one OK service
     * **AND** the BA becomes OK once all AND groups are satisfied
56. **CBABOOCOMPL_RELOAD**: **SCENARIO:** A broker reload does not alter a complex boolean rule state

     * **GIVEN** a BA of type "impact" with a complex AND/OR boolean rule over 20 services
     * **AND** all 20 services are CRITICAL, then odd-indexed services 1-13 are set to OK
     * **AND** the BA is still CRITICAL because even-indexed services remain CRITICAL
     * **WHEN** broker is reloaded at each remaining step (services 15, 17, 19 set to OK one by one)
     * **THEN** the BA state is identical before and after each broker reload
     * **AND** the BA becomes OK once all AND groups are satisfied
57. **CBABOOCOMPL_RESTART**: **SCENARIO:** A broker restart does not alter a complex boolean rule state

     * **GIVEN** a BA of type "impact" with a complex AND/OR boolean rule over 20 services
     * **AND** all 20 services are CRITICAL, then odd-indexed services 1-13 are set to OK
     * **AND** the BA is still CRITICAL because even-indexed services remain CRITICAL
     * **WHEN** broker is restarted at each remaining step (services 15, 17, 19 set to OK one by one)
     * **THEN** the BA state is identical before and after each broker restart
     * **AND** the BA becomes OK once all AND groups are satisfied
58. **CBABOOOR**: **SCENARIO:** An OR boolean rule evaluates to CRITICAL as soon as one operand is true, even when the other service is UNKNOWN

     * **GIVEN** a BA of type "impact" with boolean rule "{service_302} IS CRITICAL OR {service_303} IS CRITICAL"
     * **AND** service_303 is passive and starts UNKNOWN
     * **WHEN** service_302 becomes CRITICAL
     * **THEN** the BA is CRITICAL (OR short-circuits on the first true operand)
59. **CBABOOORREL**: **SCENARIO:** Updating a boolean rule and reloading broker and engine takes effect correctly

     * **GIVEN** a BA of type "impact" with boolean rule "{service_302} IS OK OR {service_303} IS OK"
     * **WHEN** service_302 and service_303 are CRITICAL
     * **THEN** the BA is CRITICAL
     * **WHEN** the boolean rule is updated to "{service_302} IS OK OR {service_304} IS OK" and broker and engine are reloaded
     * **AND** service_304 is OK
     * **THEN** the BA is OK
     * **WHEN** the boolean rule is restored to "{service_302} IS OK OR {service_303} IS OK" and broker and engine are reloaded
     * **AND** service_302 and service_303 are CRITICAL
     * **THEN** the BA is CRITICAL again
60. **CBAWORST**: **SCENARIO:** A BA of type "worst" reacts to KPI state changes and broker stats are valid after reload

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
61. **CBAWORST2**: **SCENARIO:** A BA of type "worst" with a boolean KPI and a child BA KPI reacts correctly to state changes

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
62. **CBAWORST_ACK**: **SCENARIO:** Acknowledging a service acknowledges the BA, and removing it unacknowledges the BA

     * **GIVEN** BBDO version is 3.0.1
     * **AND** a Business Activity of type "worst" is configured with two services
     * **WHEN** one of the services is acknowledged
     * **THEN** the Business Activity is acknowledged
     * **WHEN** the acknowledgement is removed from the service
     * **THEN** the Business Activity is no longer acknowledged
63. **CBA_BOOL_KPI**: With bbdo version 3.0.1, a BA of type 'worst' with 1 boolean kpi
64. **CBA_CHANGED**: **SCENARIO:** Replace Service KPI with Boolean Rule KPI in Worst-type BA

     * **GIVEN** a BA of type "worst" is configured with one service KPI
     * **WHEN** the service KPI is replaced by a boolean rule KPI
     * **AND** Broker is reloaded
     * **THEN** the BA is correctly updated with the new KPI configuration
65. **CBA_DISABLED**: create a disabled BA with timeperiods and reporting filter don't create error message
66. **CBA_IMPACT_2KPI_SERVICES**: With bbdo version 3.0.1, a BA of type 'impact' with 2 serv, ba is critical only if the 2 services are critical
67. **CBA_IMPACT_IMPACT**: 
     * **GIVEN** a Business Activity (BA) of type "impact"
     * **AND** it has two child BAs of type "impact"
     * **AND** the first child has an impact of 90
     * **AND** the second child has an impact of 10
     * **WHEN** both child BAs are impacting
     * **THEN** the parent BA should be "critical"
     * **WHEN** both child BAs are not impacting
     * **THEN** the parent BA should be "ok"
68. **CBA_RATIO_NUMBER_BA_4_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
69. **CBA_RATIO_NUMBER_BA_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 2 services and one ba with 1 service
70. **CBA_RATIO_PERCENT_BA_4_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
71. **CBA_RATIO_PERCENT_BA_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio percent' with 2 serv an 1 ba with one service
72. **CBA_SERVICE_PNAME_AFTER_RELOAD**: **SCENARIO:** Verify that the parent_name of a BA service is not erased after a broker reload

     * **GIVEN** a BA "test" of type "worst" with its service "host_16:service_302"
     * **WHEN** I start broker and engine
     * **THEN** the BA service "test" should have a status of 0 within 30 seconds
     * **WHEN** I reload the broker
     * **THEN** the database should still contain a BA service with name "test" and parent_name "_Module_BAM_1"

### Benchmarks
1. **BENCH_RRD_METRIC_RETENTION**: Benchmark: inject 12 h of back-fill data through the retention buffer and measure merge latency.  Injects ${N_OLD_POINTS} old-timestamped pb_metric events per metric (${N_METRICS} metrics) via BBDO v3 directly to the central broker, then one current-time event per metric to trigger the junction merge. Reports injection throughput and end-to-end merge latency.

### Broker
1. **BC1**: Central and RRD brokers are started. Then we check they are correctly connected. RRD broker is stopped. The connection is lost. Then RRD broker is started again. The connection is re-established. Central broker is stopped. The connection is lost. Then Central broker is started again. The connection is re-established.
2. **BCL1**: Starting broker with option '-s foobar' should return an error
3. **BCL2**: Starting broker with option '-s5' should work
4. **BCL3**: Starting broker with options '-D' should work and activate diagnose mode
5. **BCL4**: Starting broker with options '-s2' and '-D' should work.
6. **BDB1**: 
     * **GIVEN** a broker with a wrong unified_sql db_host
     * **WHEN** cbd starts
     * **THEN** it should log an error about the connection
     * **AND** it should not crash
7. **BDB2**: 
     * **GIVEN** a broker with a wrong unified_sql db_password
     * **WHEN** cbd starts
     * **THEN** it should log an error about access denied
     * **AND** it should not crash
8. **BDB3**: 
     * **GIVEN** a broker with a correct unified_sql user password
     * **WHEN** cbd starts
     * **THEN** the connection to the database should be established
9. **BDBM1**: **FEATURE:** Broker and Engine Start/Stop with MariaDB

     **SCENARIO:** Start broker and engine, then start MariaDB with different connection counts

     * **GIVEN** the broker and engine are started
     * **WHEN** MariaDB is started after them
     * **AND** the broker is configured with connections_count set to 1 and 3
     * **THEN** the connection to the database should be established for each configured connection
10. **BEDB1**: 
     * **GIVEN** the broker and engine are started,
     * **WHEN** MariaDB is started after them,
     * **THEN** the connection to the database should be established
11. **BEDB2**: **FEATURE:** SQL Connections via gRPC API

     **SCENARIO:** Start broker and engine, stop MariaDB, then start it again

     * **GIVEN** the broker and engine are running
     * **WHEN** MariaDB is stopped and then started again
     * **THEN** the gRPC API should provide information about SQL connections
12. **BEDB3**: **FEATURE:** SQL Connections via gRPC API

     **SCENARIO:** Start broker and engine, then stop MariaDB and then start it again

     * **GIVEN** broker and engine are running
     * **WHEN** MariaDB is stopped and then started again
     * **THEN** the gRPC API should provide information about SQL connections
13. **BFC1**: **SCENARIO:** Start broker with valid and invalid filters on an output

     * **GIVEN** Broker is configured with filters "neb", "foo", and "bar" on the unified SQL output
     * **WHEN** Broker is started
     * **THEN** error messages should appear for invalid categories "foo" and "bar"
     * **AND** only the valid "neb" filter should be applied
14. **BFC2**: **SCENARIO:** Start broker with only invalid filters on an output

     * **GIVEN** Broker is configured with filters "doe", "foo", and "bar" on the unified SQL output
     * **WHEN** Broker is started
     * **THEN** error messages should appear for invalid categories
15. **BGRPCSS1**: **SCENARIO:** Two broker instances with grpc stream start and stop cleanly

     * **GIVEN** central broker with grpc output and rrd broker with grpc input
     * **WHEN** both brokers are started and stopped 5 times with 100ms interval in new generation mode
     * **THEN** no coredump occurs and the connection is established each time
16. **BGRPCSS2**: **SCENARIO:** Single broker instance with grpc starts and stops 10 times with 300ms interval

     * **GIVEN** central broker with grpc output
     * **WHEN** the broker is started and stopped 10 times with 300ms interval in new generation mode
     * **THEN** no coredump occurs
17. **BGRPCSS3**: **SCENARIO:** Single broker instance with grpc starts and stops 5 times with 100ms interval

     * **GIVEN** central broker with grpc output
     * **WHEN** the broker is started and stopped 5 times with 100ms interval in new generation mode
     * **THEN** no coredump occurs
18. **BGRPCSS4**: **SCENARIO:** Single broker instance with grpc starts and stops 10 times with 1s interval

     * **GIVEN** central broker with grpc output
     * **WHEN** the broker is started and stopped 10 times with 1s interval in new generation mode
     * **THEN** no coredump occurs
19. **BGRPCSS5**: **SCENARIO:** Reversed grpc acceptor with one_peer_retention_mode starts and stops without deadlock

     * **GIVEN** central broker with grpc output in one_peer_retention_mode with no host configured
     * **WHEN** the broker is started and stopped 5 times with 1s interval in new generation mode
     * **THEN** no deadlock occurs
20. **BGRPCSSU1**: **SCENARIO:** Two broker instances with unified_sql and grpc stream start and stop cleanly

     * **GIVEN** central broker with unified_sql and grpc output and rrd broker with grpc input
     * **WHEN** both brokers are started and stopped 5 times with 100ms interval in new generation mode
     * **THEN** no coredump occurs and the connection is established each time
21. **BGRPCSSU2**: **SCENARIO:** Single broker instance with unified_sql and grpc starts and stops 10 times with 300ms interval

     * **GIVEN** central broker with unified_sql and grpc output
     * **WHEN** the broker is started and stopped 10 times with 300ms interval in new generation mode
     * **THEN** no coredump occurs
22. **BGRPCSSU3**: **SCENARIO:** Single broker instance with unified_sql and grpc starts and stops 5 times with 100ms interval

     * **GIVEN** central broker with unified_sql and grpc output
     * **WHEN** the broker is started and stopped 5 times with 100ms interval in new generation mode
     * **THEN** no coredump occurs
23. **BGRPCSSU4**: **SCENARIO:** Single broker instance with unified_sql and grpc starts and stops 10 times with 1s interval

     * **GIVEN** central broker with unified_sql and grpc output
     * **WHEN** the broker is started and stopped 10 times with 1s interval in new generation mode
     * **THEN** no coredump occurs
24. **BGRPCSSU5**: **SCENARIO:** Reversed grpc acceptor with unified_sql and one_peer_retention_mode starts and stops without deadlock

     * **GIVEN** central broker with unified_sql and grpc output in one_peer_retention_mode
     * **WHEN** the broker is started and stopped 5 times with 1s interval in new generation mode
     * **THEN** no deadlock occurs
25. **BSCSS1**: Start-Stop two instances of broker and no coredump
26. **BSCSS2**: Start/Stop 10 times broker with 300ms interval and no coredump
27. **BSCSS3**: Start-Stop one instance of broker with tcp connection and no coredump
28. **BSCSS4**: Start/Stop 10 times broker with 1sec interval and no coredump
29. **BSCSSC1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is enabled on client side.
30. **BSCSSC2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is disabled on client side.
31. **BSCSSCG1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on client side.
32. **BSCSSCGRR1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on output side. Reversed connection with retention and grpc transport protocol.
33. **BSCSSCGRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on output side. Reversed connection with retention and grpc transport protocol.
34. **BSCSSCRR1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is enabled on client side. Connection reversed with retention.
35. **BSCSSCRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is disabled on client side. Connection reversed with retention.
36. **BSCSSG1**: Start-Stop two instances of broker and no coredump
37. **BSCSSG2**: Start/Stop 10 times broker with 300ms interval and no coredump
38. **BSCSSG3**: Start-Stop one instance of broker with grpc connection and no coredump
39. **BSCSSG4**: Start/Stop 10 times broker with 1sec interval and no coredump
40. **BSCSSGA1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. An authorization token is added on the server. Error messages are raised.
41. **BSCSSGA2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. An authorization token is added on the server and also on the client. All looks ok.
42. **BSCSSGRR1**: Start-Stop two instances of broker and no coredump, reversed and retention, with transport protocol grpc, start-stop 5 times.
43. **BSCSSK1**: **SCENARIO:** Client uses tcp but server expects grpc - connection fails

     * **GIVEN** central broker is configured with a bbdo_server input using tcp on port 5669
     * **AND** central broker is configured with a bbdo_client output using tcp to port 5670
     * **AND** rrd broker is configured with a bbdo_server input using grpc on port 5670
     * **WHEN** both brokers are started in new generation mode
     * **THEN** an error is raised on the client side about corrupted data
44. **BSCSSK2**: Start-Stop two instances of broker, server configured with tcp and client with grpc. No connection established and error raised on client side.
45. **BSCSSP1**: Start-Stop two instances of broker and no coredump. The server contains a listen address
46. **BSCSSPRR1**: Start-Stop two instances of broker and no coredump. The server contains a listen address, reversed and retention. centreon-broker-master-rrd is then a failover.
47. **BSCSSR1**: Start-Stop two instances of broker and no coredump. Connection with bbdo_server/bbdo_client and reversed.
48. **BSCSSRR1**: Start-Stop two instances of broker and no coredump. Connection with bbdo_server/bbdo_client, reversed and retention. centreon-broker-master-rrd is then a failover.
49. **BSCSSRR2**: Start/Stop 10 times broker with 300ms interval and no coredump, reversed and retention. centreon-broker-master-rrd is then a failover.
50. **BSCSST1**: Start-Stop two instances of broker and no coredump. Encryption is enabled on client side.
51. **BSCSST2**: Start-Stop two instances of broker and no coredump. Encryption is enabled on client side.
52. **BSCSSTG1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. This is not sufficient, then an error is raised.
53. **BSCSSTG2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. It works with good certificates and keys.
54. **BSCSSTG3**: Start-Stop two instances of broker. The connection cannot be established if the server private key is missing and an error message explains this issue.
55. **BSCSSTGRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. It works with good certificates and keys. Reversed grpc connection with retention.
56. **BSCSSTRR1**: Start-Stop two instances of broker and no coredump. Encryption is enabled. transport protocol is tcp, reversed and retention.
57. **BSCSSTRR2**: Start-Stop two instances of broker and no coredump. Encryption is enabled.
58. **BSS1**: Start-Stop two instances of broker and no coredump
59. **BSS2**: Start/Stop 10 times broker with 300ms interval and no coredump
60. **BSS3**: Start-Stop one instance of broker 5 times and no coredump
61. **BSS4**: Start/Stop 10 times broker with 1sec interval and no coredump
62. **BSS5**: Start-Stop with reversed connection on TCP acceptor with only one instance and no deadlock
63. **BSSU1**: Start-Stop two instances of broker with BBDO3 and no coredump
64. **BSSU2**: Start/Stop 10 times broker (BBDO3) with 300ms interval and no coredump
65. **BSSU3**: Start-Stop one instance of broker (BBDO3) and no coredump
66. **BSSU4**: Start/Stop 10 times broker with 1sec interval and no coredump
67. **BSSU5**: Start-Stop with reversed connection on TCP acceptor with only one instance and no deadlock
68. **CBDBM1**: **SCENARIO:** Broker reconnects to MariaDB after startup with configurable connection count

     * **GIVEN** the broker and engine are started in new generation mode before MariaDB
     * **WHEN** MariaDB is started after them with connections_count set to 1 then 3
     * **THEN** the broker reconnects with the configured number of connections each time
69. **CBEDB1**: 
     * **GIVEN** the broker and engine are started in new generation mode
     * **WHEN** MariaDB is started after them
     * **THEN** the connection to the database should be established
70. **CBEDB2**: **FEATURE:** SQL Connections via gRPC API

     **SCENARIO:** Start broker and engine, stop MariaDB, then start it again

     * **GIVEN** the broker and engine are running
     * **WHEN** MariaDB is stopped and then started again
     * **THEN** the gRPC API should provide information about SQL connections
71. **CBEDB3**: **FEATURE:** SQL Connections via gRPC API

     **SCENARIO:** Start broker and engine, then stop MariaDB and then start it again

     * **GIVEN** broker and engine are running
     * **WHEN** MariaDB is stopped and then started again
     * **THEN** the gRPC API should provide information about SQL connections
72. **CBLBD**: **SCENARIO:** Broker starts with default logger levels when no loggers section is configured

     * **GIVEN** central broker configured without a loggers section
     * **WHEN** broker is started in new generation mode
     * **THEN** the gRPC API reports the expected default log levels for all loggers
73. **CBLDIS1**: **SCENARIO:** Broker starts with core logs disabled - sql logs still produced

     * **GIVEN** central broker configured with core logs 'disabled' and sql logs at debug level
     * **WHEN** broker is started in new generation mode
     * **THEN** sql log entries are produced
     * **AND** no core log entries are produced
74. **CBLEC1**: **SCENARIO:** Core log level changed live from trace to debug via gRPC API

     * **GIVEN** central broker started with core logs at trace level in new generation mode
     * **WHEN** the core log level is changed to debug via the gRPC API
     * **THEN** the gRPC API reports the new core log level as debug
75. **CBLEC2**: **SCENARIO:** Setting an invalid log level via gRPC API raises an error

     * **GIVEN** central broker started with core logs at trace level in new generation mode
     * **WHEN** the core log level is set to the invalid value 'foo' via the gRPC API
     * **THEN** an error message about the unknown enum value is returned
76. **CBLEC3**: **SCENARIO:** Setting log level for a non-existent logger via gRPC API raises an error

     * **GIVEN** central broker started with core logs at trace level in new generation mode
     * **WHEN** the log level of the non-existent 'foo' logger is set via the gRPC API
     * **THEN** an error message about the missing logger is returned
77. **CBSCSSK2**: **SCENARIO:** Client uses grpc but server expects tcp - connection fails

     * **GIVEN** central broker is configured with a bbdo_server input using grpc on port 5669
     * **AND** central broker is configured with a bbdo_client output using grpc to port 5670
     * **AND** rrd broker is configured with a bbdo_server input using tcp on port 5670
     * **WHEN** both brokers are started in new generation mode
     * **THEN** an error is raised on the client side about invalid protocol header
78. **CBSS1**: **SCENARIO:** Two broker instances start and stop cleanly 5 times in new generation mode

     * **GIVEN** central and rrd brokers configured in new generation mode
     * **WHEN** both brokers are started and stopped 5 times immediately
     * **THEN** no coredump occurs each time
79. **CBSS2**: **SCENARIO:** Single broker instance starts and stops 10 times with 300ms interval in new generation mode

     * **GIVEN** central broker configured in new generation mode
     * **WHEN** the broker is started and stopped 10 times with 300ms interval
     * **THEN** no coredump occurs
80. **CBSS3**: **SCENARIO:** Single broker instance starts and stops 5 times immediately in new generation mode

     * **GIVEN** central broker configured in new generation mode
     * **WHEN** the broker is started and stopped 5 times immediately
     * **THEN** no coredump occurs
81. **CBSS4**: **SCENARIO:** Single broker instance starts and stops 10 times with 1s interval in new generation mode

     * **GIVEN** central broker configured in new generation mode
     * **WHEN** the broker is started and stopped 10 times with 1s interval
     * **THEN** no coredump occurs
82. **CBSS5**: **SCENARIO:** Reversed TCP connection with one_peer_retention_mode starts and stops without deadlock in new generation mode

     * **GIVEN** central broker configured with one_peer_retention_mode and no host on the rrd output
     * **WHEN** the broker is started and stopped 5 times with 1s interval in new generation mode
     * **THEN** no deadlock occurs
83. **CBSS_CBD**: **SCENARIO:** Broker restart with unified_sql preserves non-null service and host states

     * **GIVEN** broker and engine are started in new generation mode
     * **AND** broker is then restarted
     * **WHEN** services and hosts are queried from the database for 30 seconds
     * **THEN** no service or host state is null
84. **START_STOP_CBD**: restart cbd with unified_sql services state must not be null after restart

### Broker/database
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
1. **ANO_CFG_SENSITIVITY_SAVED**: cfg sensitivity saved in retention
2. **ANO_DT1**: downtime on dependent service is inherited by ano
3. **ANO_DT2**: 
     * **GIVEN** a service and its AD,
     * **WHEN** we delete downtime on dependent service, AD must not be in downtime anymore
4. **ANO_DT3**: delete downtime on anomaly don t delete dependent service one
5. **ANO_DT4**: **SCENARIO:** Removing downtime from service keeps it on anomaly detection

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
15. **BEACK1**: **SCENARIO:** Acknowledging a critical service

     * **GIVEN** Engine has a critical service
     * **WHEN** an external command is sent to acknowledge it
     * **THEN** the "centreon_storage.acknowledgements" table is updated with this acknowledgement
     * **AND** a log in "centreon_storage.logs" concerning this acknowledgement is added.
     * **WHEN** the service is set to OK
     * **THEN** the acknowledgement is deleted from the Engine
     But it remains open in the database
16. **BEACK2**: Configuration is made with BBDO3. Engine has a critical service. An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to OK. And the acknowledgement in database is deleted.
17. **BEACK3**: Engine has a critical service. An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The acknowledgement is removed and the comment in the comments table has its deletion_time column updated.
18. **BEACK4**: Configuration is made with BBDO3. Engine has a critical service. An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The acknowledgement is removed and the comment in the comments table has its deletion_time column updated.
19. **BEACK5**: Engine has a critical service. An external command is sent to acknowledge it ; the acknowledgement is sticky. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to WARNING. And the acknowledgement in database is still there.
20. **BEACK6**: Configuration is made with BBDO3. Engine has a critical service. An external command is sent to acknowledge it ; the acknowledgement is sticky. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to WARNING. And the acknowledgement in database is still there.
21. **BEACK8**: Engine has a critical service. It is configured with BBDO 3. An external command is sent to acknowledge it ; the acknowledgement is normal. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to WARNING. And the acknowledgement in database is removed (not sticky).
22. **BEATOI11**: external command SEND_CUSTOM_HOST_NOTIFICATION with option_number=1 should work
23. **BEATOI12**: external command SEND_CUSTOM_HOST_NOTIFICATION with option_number>7 should fail
24. **BEATOI13**: external command Schedule Service Downtime with duration<0 should fail
25. **BEATOI21**: external command ADD_HOST_COMMENT and DEL_HOST_COMMENT should work
26. **BEATOI22**: external command DEL_HOST_COMMENT with comment_id<0 should fail
27. **BEATOI23**: external command ADD_SVC_COMMENT with persistent=0 should work
28. **BECC1**: Broker/Engine communication with compression between central and poller
29. **BECT1**: Broker/Engine communication with anonymous TLS between central and poller
30. **BECT2**: Broker/Engine communication with TLS between central and poller with key/cert
31. **BECT3**: Broker/Engine communication with anonymous TLS and ca certificate
32. **BECT4**: Broker/Engine communication with TLS between central and poller with key/cert and hostname forced
33. **BECT_GRPC1**: Broker/Engine communication with GRPC and with anonymous TLS between central and poller
34. **BECT_GRPC2**: Broker/Engine communication with TLS between central and poller with key/cert
35. **BECT_GRPC3**: Broker/Engine communication with anonymous TLS and ca certificate
36. **BECT_GRPC4**: Broker/Engine communication with TLS between central and poller with key/cert and hostname forced
37. **BECUSTOMHOSTVAR**: external command CHANGE_CUSTOM_HOST_VAR on SNMPVERSION
38. **BECUSTOMSVCVAR**: external command CHANGE_CUSTOM_SVC_VAR on CRITICAL
39. **BEDTHOSTFIXED**: A downtime is set on a host, the total number of downtimes is really 21 (1 for the host and 20 for its 20 services) then we delete this downtime and the number is 0.
40. **BEDTHOSTFIXED1**: **SCENARIO:** Setting and Removing Downtime on a Host and its Services

     * **GIVEN** a downtime is set on a host
     * **THEN** the total number of downtimes is 21
     * **AND** this includes 1 for the host and 20 for its services
     * **WHEN** the downtime is deleted
     * **THEN** the total number of downtimes is 0
41. **BEDTMASS1**: **SCENARIO:** Setting and Removing Downtimes on Configured Hosts and Services

     * **GIVEN** new services with several pollers are created
     * **WHEN** downtimes are set on all configured hosts
     * **THEN** the total number of downtimes, including impacted services, is 1050
     * **AND** all these downtimes are removed
     * **AND** the test is performed with BBDO 3.0.0
42. **BEDTMASS2**: **SCENARIO:** Setting and Removing Downtimes on Configured Hosts and Services

     * **GIVEN** new services with several pollers are created
     * **WHEN** downtimes are set on all configured hosts
     * **THEN** the total number of downtimes, including impacted services, is 1050
     * **AND** all these downtimes are removed
     * **AND** the test is performed with BBDO 2.0.0
43. **BEDTRRD1**: A service is forced checked then a downtime is set on this service. The service is forced checked again and the downtime is removed. This test is done with BBDO 3.0.0. Then we should not get any error in cbd RRD of kind 'ignored update error in file...'.
44. **BEDTSVCFIXED**: 
     * **GIVEN** a unique downtime set on a service
     * **WHEN** the downtime is removed
     * **THEN** the downtime is well removed
     * **AND** the number of downtimes is 0
45. **BEDTSVCFIXED1**: 
     * **GIVEN** a configuration with BBDO3 and a unique downtime set on a service
     * **WHEN** the downtime is removed
     * **THEN** the downtime is well removed
     * **AND** the number of downtimes is 0
46. **BEDTSVCREN1**: 
     * **GIVEN** a downtime set on a service
     * **WHEN** the service is renamed
     * **THEN** the downtime is still active on the renamed service
     * **WHEN** the downtime is removed from the renamed service
     * **THEN** the downtime is well removed
47. **BEDTSVCREN2**: 
     * **GIVEN** a configuration with BBDO3 and a downtime set on a service
     * **WHEN** the service is renamed
     * **THEN** the downtime is still active on the renamed service
     * **WHEN** the downtime is removed from the renamed service
     * **THEN** the downtime is well removed
48. **BEDW**: **SCENARIO:** Verify Broker configured with cache_config_directory listens to it

     * **GIVEN** the Central Broker is started with cache_config_directory set to a specific Directory
     * **AND** the pollers_config_directory is set to its default value: /var/lib/centreon-broker/pollers-configuration.
     * **WHEN** a file of the form <poller_id>.lck is created in the cache_config_directory
     * **THEN** Broker logs a message telling the file has been created
     * **WHEN** the corresponding configuration directory doesn't exist
     * **THEN** Broker logs a message telling the directory doesn't exist
49. **BEDWEN**: **SCENARIO:** Verify Broker configured with cache_config_directory listens to it

     * **GIVEN** the Central Broker is started with cache_config_directory set to a specific Directory
     * **AND** the pollers_config_directory is set (default value) to /var/lib/centreon-broker/pollers-configuration.
     * **WHEN** a file of the form <poller_id>.lck is created in the cache_config_directory
     * **THEN** Broker logs a message telling the file has been created
     * **WHEN** the corresponding configuration directory doesn't exist
     * **THEN** Broker logs a message telling the directory doesn't exist
50. **BEDWEND**: **SCENARIO:** Verify Broker configured with cache_config_directory creates the protobuf serialized configuration

     * **GIVEN** Central Broker is started with cache_config_directory set to a specific Directory
     * **AND** the pollers_config_directory is set (default value) to /var/lib/centreon-broker/pollers-configuration.
     * **AND** Central Broker has already sent a first configuration to Engine
     * **WHEN** a new configuration is put into the cache_config_directory
     * **THEN** Engine should be notified about the new configuration by Broker
     * **AND** Engine should update its configuration from a differential configuration
51. **BEDWENF**: **SCENARIO:** Verify Broker configured with cache_config_directory creates the protobuf serialized configuration

     * **GIVEN** the Central Broker is started with cache_config_directory set to a specific Directory
     * **AND** the pollers_config_directory is set (default value) to /var/lib/centreon-broker/pollers-configuration.
     * **WHEN** a file of the form <poller_id>.lck is created after the <poller_id> directory is filled correctly
     * **THEN** Broker logs a message telling the file has been created
     * **AND** Broker dumps a file <poller_id>.prot in the pollers_conf directory
52. **BEEXTCMD1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0
53. **BEEXTCMD10**: external command CHANGE_MAX_SVC_CHECK_ATTEMPTS on bbdo2.0
54. **BEEXTCMD11**: external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo3.0
55. **BEEXTCMD12**: external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo2.0
56. **BEEXTCMD13**: external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo3.0
57. **BEEXTCMD14**: external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo2.0
58. **BEEXTCMD15**: external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo3.0
59. **BEEXTCMD16**: external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo2.0
60. **BEEXTCMD17**: external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo3.0
61. **BEEXTCMD18**: external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo2.0
62. **BEEXTCMD19**: external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo3.0
63. **BEEXTCMD2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0
64. **BEEXTCMD20**: external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo2.0
65. **BEEXTCMD21**: external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS and ENABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo3.0
66. **BEEXTCMD22**: external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS and ENABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo2.0
67. **BEEXTCMD23**: 
     * **GIVEN** Engine and broker configured with BBDO3
     * **WHEN** the external command DISABLE_HOST_CHECK on host_1 is executed
     * **THEN** the host_1 host checks should be disabled
     * **WHEN** the external command ENABLE_HOST_CHECK on host_1 is executed
     * **THEN** the host_1 host checks should be enabled
68. **BEEXTCMD24**: external command DISABLE_HOST_CHECK and ENABLE_HOST_CHECK on bbdo2.0
69. **BEEXTCMD25**: external command DISABLE_HOST_EVENT_HANDLER and ENABLE_HOST_EVENT_HANDLER on bbdo3.0
70. **BEEXTCMD26**: external command DISABLE_HOST_EVENT_HANDLER and ENABLE_HOST_EVENT_HANDLER on bbdo2.0
71. **BEEXTCMD27**: external command DISABLE_HOST_FLAP_DETECTION and ENABLE_HOST_FLAP_DETECTION on bbdo3.0
72. **BEEXTCMD28**: external command DISABLE_HOST_FLAP_DETECTION and ENABLE_HOST_FLAP_DETECTION on bbdo2.0
73. **BEEXTCMD29**: external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo3.0
74. **BEEXTCMD3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0
75. **BEEXTCMD30**: external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo2.0
76. **BEEXTCMD31**: external command DISABLE_HOST_SVC_CHECKS and ENABLE_HOST_SVC_CHECKS on bbdo3.0
77. **BEEXTCMD32**: external command DISABLE_HOST_SVC_CHECKS and ENABLE_HOST_SVC_CHECKS on bbdo2.0
78. **BEEXTCMD33**: external command DISABLE_HOST_SVC_NOTIFICATIONS and ENABLE_HOST_SVC_NOTIFICATIONS on bbdo3.0
79. **BEEXTCMD34**: external command DISABLE_HOST_SVC_NOTIFICATIONS and ENABLE_HOST_SVC_NOTIFICATIONS on bbdo2.0
80. **BEEXTCMD35**: external command DISABLE_PASSIVE_HOST_CHECKS and ENABLE_PASSIVE_HOST_CHECKS on bbdo3.0
81. **BEEXTCMD36**: external command DISABLE_PASSIVE_HOST_CHECKS and ENABLE_PASSIVE_HOST_CHECKS on bbdo2.0
82. **BEEXTCMD37**: external command DISABLE_PASSIVE_SVC_CHECKS and ENABLE_PASSIVE_SVC_CHECKS on bbdo3.0
83. **BEEXTCMD38**: external command DISABLE_PASSIVE_SVC_CHECKS and ENABLE_PASSIVE_SVC_CHECKS on bbdo2.0
84. **BEEXTCMD39**: external command START_OBSESSING_OVER_HOST and STOP_OBSESSING_OVER_HOST on bbdo3.0
85. **BEEXTCMD4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0
86. **BEEXTCMD40**: external command START_OBSESSING_OVER_HOST and STOP_OBSESSING_OVER_HOST on bbdo2.0
87. **BEEXTCMD41**: external command START_OBSESSING_OVER_SVC and STOP_OBSESSING_OVER_SVC on bbdo3.0
88. **BEEXTCMD42**: external command START_OBSESSING_OVER_SVC and STOP_OBSESSING_OVER_SVC on bbdo2.0
89. **BEEXTCMD5**: external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo3.0
90. **BEEXTCMD6**: external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo2.0
91. **BEEXTCMD7**: external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo3.0
92. **BEEXTCMD8**: external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo2.0
93. **BEEXTCMD9**: external command CHANGE_MAX_SVC_CHECK_ATTEMPTS with bbdo3.0
94. **BEEXTCMD_COMPRESS_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and compressed grpc
95. **BEEXTCMD_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and grpc
96. **BEEXTCMD_GRPC2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 and grpc
97. **BEEXTCMD_GRPC3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 and grpc
98. **BEEXTCMD_GRPC4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 and grpc
99. **BEEXTCMD_REVERSE_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and reversed gRPC
100. **BEEXTCMD_REVERSE_GRPC2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 and grpc reversed
101. **BEEXTCMD_REVERSE_GRPC3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 and grpc reversed
102. **BEEXTCMD_REVERSE_GRPC4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 and grpc reversed
103. **BEHOSTCHECK**: 
     * **GIVEN** Engine and Broker configured to work with BBDO 3
     * **WHEN** a schedule forced host check command on host host_1 is launched
     * **THEN** the result appears in the centreon_storage resources table
104. **BEHS1**: store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
105. **BEINSTANCE**: Instance to bdd
106. **BEINSTANCESTATUS**: Instance status to bdd
107. **BENCH_100000_REVERSE_SERVICE_STATUS_TRACES_WITHOUT_SQL**: Broker is configured without SQL output. The connection between Engine and Broker is reversed. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times. Logs are in trace level.
108. **BENCH_100000_REVERSE_SERVICE_STATUS_WITHOUT_SQL**: Broker is configured without SQL output. The connection between Engine and Broker is reversed. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times.
109. **BENCH_100000_SERVICE_STATUS_TRACES_WITHOUT_SQL**: Broker is configured without SQL output. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times. Logs are in trace level.
110. **BENCH_100000_SERVICE_STATUS_WITHOUT_SQL**: Broker is configured without SQL output. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times.
111. **BENCH_10000STATUS**: external command CHECK_SERVICE_RESULT 1000 times
112. **BENCH_10000STATUS_TRACES**: external command CHECK_SERVICE_RESULT ${nb_checks} times
113. **BENCH_1000STATUS**: external command CHECK_SERVICE_RESULT 1000 times
114. **BENCH_1000STATUS_100ENGINE**: external command CHECK_SERVICE_RESULT 100 times    with 100 pollers with 20 services
115. **BENCH_1000STATUS_100ENGINE_2**: external command CHECK_SERVICE_RESULT 100 times    with 100 pollers with 20 services
116. **BENCH_1000STATUS_100ENGINE_3**: external command CHECK_SERVICE_RESULT 100 times    with 100 pollers with 20 services
117. **BENCH_1000STATUS_TRACES**: external command CHECK_SERVICE_RESULT ${nb_checks} times
118. **BENCH_300000_REVERSE_SERVICE_STATUS_TRACES_WITHOUT_SQL**: Broker is configured without SQL output. The connection between Engine and Broker is reversed. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times. Logs are in trace level.
119. **BENCH_300000_REVERSE_SERVICE_STATUS_WITHOUT_SQL**: Broker is configured without SQL output. The connection between Engine and Broker is reversed. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times.
120. **BENCH_300000_SERVICE_STATUS_TRACES_WITHOUT_SQL**: Broker is configured without SQL output. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times. Logs are in trace level.
121. **BENCH_300000_SERVICE_STATUS_WITHOUT_SQL**: Broker is configured without SQL output. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times.
122. **BENCV**: Engine is configured with hosts/services. The first host has no customvariable. Then we add a customvariable to the first host and we reload engine. Then the host should have this new customvariable defined and centengine should not crash.
123. **BENHG1**: 
     * **GIVEN** a Centreon platform with 3 Engine instances
     * **AND** Broker is configured with RRD, central and module outputs
     * **AND** the central broker has 5 database connections
     * **WHEN** I create a host group containing 3 hosts
     * **AND** I reload both Broker and Engine configurations
     * **THEN** the membership of all 3 hosts to the host group should be logged
     * **AND** all membership entries should appear within 45 seconds
124. **BENHG4**: 
     * **GIVEN** a platform with 3 Engine instances and unified_sql output with 5 connections
     * **AND** detailed logging is enabled on module0 (neb debug, core and processing error)
     * **WHEN** I create host group 1 with 3 hosts and reload configurations
     * **THEN** at least 2 host memberships should be logged within 45 seconds
     * **WHEN** I rename host group 1 to "hostgroup_test" and reload configurations
     * **THEN** the hostgroup name should be updated in database within 60 seconds
125. **BENHGU1**: 
     * **GIVEN** a Centreon platform with 3 Engine instances
     * **AND** Broker is configured with RRD, central and module outputs
     * **AND** Broker uses unified_sql output for database operations
     * **AND** SQL logging is enabled at info level
     * **AND** the unified_sql output has 5 database connections
     * **WHEN** I create a host group containing 3 hosts
     * **AND** I reload both Broker and Engine configurations
     * **THEN** the membership of all 3 hosts to the host group should be logged
     * **AND** all membership entries should appear within 45 seconds
126. **BENHGU2**: 
     * **GIVEN** a platform with 3 Engine instances and unified_sql output with 5 connections
     * **AND** BBDO3 protocol is enabled
     * **WHEN** I create a host group with 3 hosts and reload configurations
     * **THEN** at least 2 host memberships should be logged within 45 seconds
127. **BENHGU3**: 
     * **GIVEN** a platform with 4 Engine instances and unified_sql output with 5 connections
     * **AND** BBDO3 protocol is enabled with SQL debug logging
     * **WHEN** I create host group 1 across 4 pollers with 3 hosts each and reload
     * **THEN** host group 1 should contain 12 host members within 30 seconds
     * **WHEN** I remove the hostgroups configuration from poller 0 and reload
     * **THEN** host group 1 should contain only 9 host members within 30 seconds
128. **BENHGU4_BBDO2**: 
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
129. **BENHGU4_BBDO3**: 
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
130. **BENSG1**: **SCENARIO:** Service group creation and synchronization across multiple pollers

     * **GIVEN** 3 Engine pollers and Broker are started in non-centralized mode
     * **AND** the unified SQL output is configured with 5 database connections
     * **WHEN** a service group is created on poller 0 with services service_1, service_2, service_3 from host_1
     * **AND** servicegroups.cfg is added to poller 0 configuration
     * **AND** Broker and Engine are reloaded
     * **THEN** the central broker log should confirm that all 3 services are members of service group 1 on instance 1
131. **BENSGU1**: New service group with several pollers and connections to DB with broker configured with unified_sql
132. **BENSGU2**: New service group with several pollers and connections to DB with broker configured with unified_sql
133. **BENSGU3_BBDO2**: New service group with several pollers and connections to DB with broker and rename this servicegroup
134. **BENSGU3_BBDO3**: New service group with several pollers and connections to DB with broker and rename this servicegroup
135. **BENSVC1**: New services with several pollers
136. **BEOTEL_CENTREON_AGENT_CEIP**: **SCENARIO:** Agent and "centreon_storage.agent_information" Statistics

     * **GIVEN** Engine connected to Broker
     * **WHEN** an agent connects to Engine
     * **THEN** a message is sent to Broker that results in a new row in the "centreon_storage.agent_information" table.
137. **BEOTEL_CENTREON_AGENT_CHECK_COUNTER**: 
     * **GIVEN** an agent with counter check, we expect to get the correct status for the centagent process running on windows host
138. **BEOTEL_CENTREON_AGENT_CHECK_DIFFERENT_INTERVAL**: 
     * **GIVEN** a Centreon Engine with OpenTelemetry server module configured
     * **AND** an OTEL connector using centreon_agent processor with 5s export period
     * **AND** 3 passive services configured with different check intervals (1, 2, 3 minutes)
     * **AND** interval_length is set to 10 seconds
     * **WHEN** the Engine, Broker and Agent are started
     * **THEN** service_1 should execute checks every 10 seconds (1*10) with 5s tolerance
     * **AND** service_2 should execute checks every 20 seconds (2*10) with 5s tolerance
     * **AND** service_3 should execute checks every 30 seconds (3*10) with 5s tolerance
     * **AND** all check intervals should be verified within 80 seconds
139. **BEOTEL_CENTREON_AGENT_CHECK_EVENTLOG**: 
     * **GIVEN** an agent with eventlog check, we expect status, output and metrics
140. **BEOTEL_CENTREON_AGENT_CHECK_FILES**: 
     * **GIVEN** an agent with file check, we expect to get the correct status for files under monitoring on the Windows host
141. **BEOTEL_CENTREON_AGENT_CHECK_HEALTH**: agent check health and we expect to get it in check result
142. **BEOTEL_CENTREON_AGENT_CHECK_HOST**: 
     * **GIVEN** an agent host checked by centagent, we set a first output to check command,
     modify it, reload engine and expect the new output in resource table
143. **BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED**: agent check host with encrypted connection and we expect to get it in check result
144. **BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED_ENCRYPTED_CREDENTIALS**: 
     * **GIVEN** an agent host checked by centagent over an encrypted connection,
     Engine use credentials encryption and send encrypted commands
     we set a first output to check command,
     modify it, reload engine and expect the new output in resource table
145. **BEOTEL_CENTREON_AGENT_CHECK_HOST_NO_ENCRYPTED_CREDENTIALS**: 
     * **GIVEN** an agent host checked by centagent over a non encrypted connection,
     Engine use credentials encryption, but send no encrypted commands
     we set a first output to check command,
     modify it, reload engine and expect the new output in resource table
146. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_CPU**: agent check service with native check cpu and we expect to get it in check result
147. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_MEMORY**: agent check service with native check memory and we expect to get it in check result
148. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_SERVICE**: agent check service with native check service and we expect to get it in check result
149. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_STORAGE**: agent check service with native check storage and we expect to get it in check result
150. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_UPTIME**: agent check service with native check uptime and we expect to get it in check result
151. **BEOTEL_CENTREON_AGENT_CHECK_PROCESS**: 
     * **GIVEN** an agent with eventlog check, we expect to get the correct status for thr centagent process running on windows host
152. **BEOTEL_CENTREON_AGENT_CHECK_SERVICE**: agent check service and we expect to get it in check result
153. **BEOTEL_CENTREON_AGENT_CHECK_TASKSCHEDULER**: 
     * **GIVEN** an agent with task scheduler check, we expect to get the correct status for the centagent process running on windows host
154. **BEOTEL_CENTREON_AGENT_LINUX_NO_DEFUNCT_PROCESS**: agent check host and we expect to get it in check result
155. **BEOTEL_CENTREON_AGENT_NO_TRUSTED_TOKEN**: 
     * **GIVEN** the Centreon Engine is configured with OpenTelemetry server with encryption enabled with no trusted_token
     * **WHEN** the Centreon Agent attempts to connect with tls
     * **THEN** the connection should be accepted
156. **BEOTEL_CENTREON_AGENT_TOKEN**: 
     * **GIVEN** the Centreon Engine is configured with OpenTelemetry server with encryption enabled
     * **WHEN** the Centreon Agent attempts to connect using an valid JWT token
     * **THEN** the connection should be accepted
     * **AND** the log should confirm that the token is valid
157. **BEOTEL_CENTREON_AGENT_TOKEN_AGENT_TELEGRAPH**: 
     * **GIVEN** an OpenTelemetry server is configured with token-based connection
     * **AND** the Centreon Agent is configured with a valid token
     * **WHEN** the agent attempts to connect to the server
     * **THEN** the connection should be successful
     * **AND** the log should confirm that the token is valid
     * **AND** Telegraf should connect and send data to the engine
158. **BEOTEL_CENTREON_AGENT_TOKEN_AGENT_TELEGRAPH_2**: 
     * **GIVEN** an OpenTelemetry server is configured with token-based connection
     * **AND** the Centreon Agent is configured with a valid token that will expire
     * **WHEN** the agent attempts to connect to the server
     * **THEN** the connection should be successful
     * **AND** the log should confirm that the token is valid
     * **AND** Telegraf should connect and send data to the engine
159. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED**: 
     * **GIVEN** the OpenTelemetry server is configured with encryption enabled
     * **AND** the server uses a public certificate and private key for secure communication
     * **WHEN** the Centreon Agent attempts to connect using an expired JWT token
     * **THEN** the connection should be refused
     * **AND** the log should contain the message "Token is expired"
160. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED_WHILE_RUNNING**: 
     * **GIVEN** the OpenTelemetry server is configured with encryption enabled
     * **AND** the server uses a public certificate and private key for secure communication
     * **WHEN** the Centreon Agent attempts to connect using an JWT token valid
     * **THEN** the connection should be accepted
     * **WHEN** the token expires
     * **THEN** the connection should be refused
     * **AND** the log should contain the message "Token is expired"
161. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED_WHILE_RUNNING_REVERSE**: 
     * **GIVEN** the Centreon Engine is configured as client with token and the agent as server with encryption enables
     * **WHEN** the Centreon engine attempts to connect using an valid JWT token
     * **THEN** the connection should be accepted
     * **WHEN** the token expires
     * **THEN** the connection should be refused
     * **AND** the log should contain the message "Token is expired"
162. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRE_REVERSE**: 
     * **GIVEN** the Centreon Engine is configured as client with token and the agent as server with encryption enables
     * **WHEN** the Centreon engine attempts to connect using an valid JWT token but expired
     * **THEN** the connection should be refused
     * **AND** the log should confirm that the token is expired
163. **BEOTEL_CENTREON_AGENT_TOKEN_MISSING_HEADER**: 
     * **GIVEN** the Centreon Engine is configured with OpenTelemetry server with encryption enabled
     * **WHEN** the Centreon Agent attempts to connect without a JWT token
     * **THEN** the connection should be refused
     * **AND** the log should contain the message "UNAUTHENTICATED: No authorization header"
164. **BEOTEL_CENTREON_AGENT_TOKEN_REVERSE**: 
     * **GIVEN** the Centreon Engine is configured as client with token and the agent as server with encryption enables
     * **WHEN** the Centreon engine attempts to connect using an valid JWT token
     * **THEN** the connection should be accepted
     * **AND** the log should confirm that the token is valid
165. **BEOTEL_CENTREON_AGENT_TOKEN_UNTRUSTED**: 
     * **GIVEN** the OpenTelemetry server is configured with encryption enabled
     * **AND** the server uses a public certificate and private key for secure communication
     * **WHEN** the Centreon Agent attempts to connect using an invalid JWT token
     * **THEN** the connection should be refused
     * **AND** the log should contain the message "Token is not trusted"
166. **BEOTEL_CENTREON_AGENT_TOKEN_UNTRUSTED_REVERSE**: 
     * **GIVEN** the Centreon Engine is configured as client with token and the agent as server with encryption enables
     * **WHEN** the Centreon engine attempts to connect using an invalid JWT token
     * **THEN** the connection should be refused
     * **AND** the log should confirm that the token is not trusted
167. **BEOTEL_CENTREON_AGENT_WHITE_LIST**: **SCENARIO:** Enforcing command whitelist for agent checks

     * **GIVEN** a whitelist file is created with allowed commands for host_1
     * **AND** the engine, broker, and agent are configured and started
     * **WHEN** a check command matching the whitelist is executed for host_1
     * **THEN** the check result is accepted and stored in the resources table
     * **WHEN** a check command not matching the whitelist is configured for host_1 and engine is reloaded
     * **THEN** the command is rejected and a "command not allowed by whitelist" message appears in the log
168. **BEOTEL_INVALID_CHECK_COMMANDS_AND_ARGUMENTS**: 
     * **GIVEN** the agent is configured with native checks for services
     * **AND** the OpenTelemetry server module is added
     * **AND** services are configured with incorrect check commands and arguments
     * **WHEN** the broker, engine, and agent are started
     * **THEN** the resources table should be updated with the correct status
     * **AND** appropriate error messages should be generated for invalid checks
169. **BEOTEL_REVERSE_CENTREON_AGENT_CHECK_HOST**: agent check host with reversed connection and we expect to get it in check result
170. **BEOTEL_REVERSE_CENTREON_AGENT_CHECK_HOST_CRYPTED**: agent check host with encrypted reversed connection and we expect to get it in check result
171. **BEOTEL_REVERSE_CENTREON_AGENT_CHECK_SERVICE**: agent check service with reversed connection and we expect to get it in check result
172. **BEOTEL_SERVE_TELEGRAF_CONFIGURATION_CRYPTED**: we configure engine with a telegraf conf server and we check telegraf conf file
173. **BEOTEL_SERVE_TELEGRAF_CONFIGURATION_NO_CRYPTED**: we configure engine with a telegraf conf server and we check telegraf conf file
174. **BEOTEL_SERVE_TELEGRAF_CONFIGURATION_NO_CRYPTED_1**: **SCENARIO:** Serve telegraf configuration with a complex whitelist

     * **GIVEN** the engine is configured with a telegraf conf server and a complex whitelist
     * **WHEN** I request the telegraf conf file for host_1
     * **THEN** I should receive the expected telegraf configuration for host_1
     * **AND** service_3 should be blacklisted and unavailable for host_1
     * **WHEN** I request the telegraf conf file for host_2
     * **THEN** I should receive the expected telegraf configuration for host_2
     * **AND** service_5 should be blacklisted and unavailable for host_2
175. **BEOTEL_TELEGRAF_CHECK_HOST**: we send nagios telegraf formatted data and we expect to get it in check result
176. **BEOTEL_TELEGRAF_CHECK_SERVICE**: **SCENARIO:** Handling of OK and CRITICAL check results from Telegraf input

     * **GIVEN** the OpenTelemetry server is ready
     * **WHEN** I send a Telegraf-formatted check result with status "OK" to the Engine
     * **THEN** the result should be stored in the Centreon Broker storage database with status "OK"
     * **WHEN** I send a Telegraf-formatted check result with status "CRITICAL" to the Engine
     * **THEN** the result should be stored in the Centreon Broker storage database with status "CRITICAL" and state type "SOFT"
     * **WHEN** I send a Telegraf-formatted check result with status "CRITICAL" to the Engine
     * **THEN** the result should be stored in the Centreon Broker storage database with status "CRITICAL" and state type "SOFT"
     * **WHEN** I send a Telegraf-formatted check result with status "CRITICAL" to the Engine
     * **THEN** the result should be stored in the Centreon Broker storage database with status "CRITICAL" and state type "HARD"
177. **BEPBBEE1**: central-module configured with bbdo_version 3.0 but not others. Unable to establish connection.
178. **BEPBBEE3**: bbdo_version 3 generates new bbdo protobuf service status messages.
179. **BEPBBEE4**: bbdo_version 3 generates new bbdo protobuf host status messages.
180. **BEPBBEE5**: bbdo_version 3 generates new bbdo protobuf service messages.
181. **BEPBCVS**: bbdo_version 3 communication of custom variables.
182. **BEPBHostParent**: bbdo_version 3 communication of host parent relations
183. **BEPBINST_CONF**: bbdo_version 3 communication of instance configuration.
184. **BEPBRI1**: bbdo_version 3 use pb_resource new bbdo protobuf ResponsiveInstance message.
185. **BERD1**: **SCENARIO:** Starting/stopping Broker does not create duplicated events.

     * **GIVEN**  the broker configuration central  is set to Lua output test-doubles-c.lua
     * **AND** the broker configuration module0 is set to with Lua output test-doubles.lua
     * **WHEN** the broker and engine are started
     * **THEN** the Lua virtual machine should be initialized in both broker and engine logs
     * **AND** the engine and broker should be connected
     * **WHEN** the broker is kindly stopped and cache is cleared
     * **AND** the broker is restarted
     * **AND** the engine is stopped and broker is kindly stopped
     * **THEN** the contents of /tmp/lua-engine.log and /tmp/lua.log should match
     * **AND** there should be no duplicate events in the logs
186. **BERD2**: **SCENARIO:** Starting/stopping Engine does not create duplicated events.

     * **GIVEN**  the broker configuration central  is set to Lua output test-doubles-c.lua
     * **AND** the broker configuration module0 is set to with Lua output test-doubles.lua
     * **WHEN** the broker and engine are started
     * **THEN** the Lua virtual machine should be initialized in both broker and engine logs
     * **AND** the engine and broker should be connected
     * **WHEN** the engine is stopped
     * **AND** the engine is restarted
     * **AND** the engine is stopped and broker is kindly stopped
     * **THEN** the contents of /tmp/lua-engine.log and /tmp/lua.log should match
     * **AND** there should be no duplicate events in the logs
187. **BERDUC1**: **SCENARIO:** Starting/stopping Broker does not create duplicated events in usual cases with unified_sql and BBDO 3.0

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
188. **BERDUC2**: **SCENARIO:** Starting/stopping Engine does not create duplicated events in usual cases with unified_sql and BBDO 3.0

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
189. **BERDUCA300**: **SCENARIO:** When the engine is stopped, it should emit a stop event and receive an ack event with events to clean from broker.

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
190. **BERDUCA301**: **SCENARIO:** When the engine is stopped, it should emit a stop event and receive an ack event with events to clean from broker with bbdo 3.0.1.

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
199. **BESS2**: **SCENARIO:** Start and stop Broker/Engine with Broker started first and Engine stopped first

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
203. **BESS6_GRPC**: **SCENARIO:** Verify Broker and Engine start and establish connections

     * **GIVEN** the Central Broker, RRD Broker, and Central Engine are started
     * **WHEN** we check the connection between them
     * **THEN** the connection should be well established
     * **AND** the central broker should have two peers connected: the central engine and the RRD broker
     * **AND** the RRD broker should correctly recognize its peer as the Central Broker
204. **BESS6_TCP**: **SCENARIO:** Verify Broker and Engine start and establish connections

     * **GIVEN** the Central Broker, RRD Broker, and Central Engine are started
     * **WHEN** we check the connection between them
     * **THEN** the connection should be well established
     * **AND** the central broker should have two peers connected: the central engine and the RRD broker
     * **AND** the RRD broker should correctly recognize its peer as the Central Broker
205. **BESSBQ1**: A very bad queue file is written for broker. Broker and Engine are then started, Broker must read the file raising an error because of that file and then get data sent by Engine.
206. **BESSCTO**: **SCENARIO:** Service commands time out due to missing Perl Connector

     * **GIVEN** the Engine is configured as usual but without the Perl Connector
     * **WHEN** the Engine executes its service commands
     * **THEN** the commands take too long and reach the timeout
     * **AND** the Engine starts and stops two times as a result
207. **BESSCTOWC**: **SCENARIO:** Service commands time out due to missing Perl Connector

     * **GIVEN** the Engine is configured as usual with some commands using the Perl Connector
     * **WHEN** the Engine executes its service commands
     * **THEN** the commands take too long and reach the timeout
     * **AND** the Engine starts and stops two times as a result
208. **BESSG**: **SCENARIO:** Broker handles connection and disconnection with Engine

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
222. **BESS_RELOAD_OUTPUT_ADD**: **SCENARIO:** Adding an output to broker config during a reload is ignored

     * **GIVEN** Broker and Engine are started with their standard configuration
     * **WHEN** a new output is appended to the broker configuration file
     * **AND** broker is reloaded
     * **THEN** an error message is logged stating the output cannot be added at runtime
     * **AND** the new output does not appear in the broker stats
223. **BESS_RELOAD_OUTPUT_REMOVE**: **SCENARIO:** Removing an output from broker config during a reload is ignored

     * **GIVEN** Broker and Engine are started with their standard configuration
     * **WHEN** the RRD output is removed from the broker configuration file
     * **AND** broker is reloaded
     * **THEN** an error message is logged stating the output cannot be removed at runtime
     * **AND** the RRD output is still present in broker stats
224. **BETAG1**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Broker is started before.
225. **BETAG2**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
226. **BEUTAG1**: Engine is configured with some tags. When broker receives them through unified_sql stream, it stores them in the centreon_storage.tags table. Broker is started before.
227. **BEUTAG10**: some services are configured with tags on two pollers. Then tags are removed from some of them and in centreon_storage, we can observe resources_tags table updated.
228. **BEUTAG11**: **SCENARIO:** Updating resource tags after changing several tags

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
241. **BE_FLAPPING_HOST_RESOURCE**: With BBDO 3, flapping detection must be set in hosts and resources tables.
242. **BE_FLAPPING_SERVICE_RESOURCE**: With BBDO 3, flapping detection must be set in services and resources tables.
243. **BE_NOTIF_OVERFLOW**: bbdo 2.0 notification number =40000. make an overflow => notification_number null in db
244. **BE_TIME_NULL_SERVICE_RESOURCE**: With BBDO 3, notification_interval time must be set to NULL on 0 in services, hosts and resources tables.
245. **BRCS1**: Broker reverse connection stopped
246. **BRCTS1**: Broker reverse connection too slow
247. **BRCTSMN**: 
     * **GIVEN** Broker, Engine configured as usual
     * **AND** map also connected to Broker with a filter allowing only 'neb' category
     * **WHEN** Engine sends pb_service, pb_host, pb_service_status and pb_host_status
     * **THEN** map receives correctly them.
248. **BRCTSMNS**: 
     * **GIVEN** Broker, Engine configured as usual
     * **AND** map also connected to Broker with a filter allowing 'neb' and 'storage' categories
     * **WHEN** Engine sends pb_service, pb_host, pb_service_status, pb_host_status and metrics
     * **THEN** Map receives correctly them.
249. **BRGC1**: Broker good reverse connection
250. **BRRDCDDID1**: RRD metrics deletion from index ids with rrdcached.
251. **BRRDCDDIDDB1**: RRD metrics deletion from index ids with a query in centreon_storage with rrdcached.
252. **BRRDCDDIDU1**: RRD metrics deletion from index ids with unified sql output with rrdcached.
253. **BRRDCDDM1**: RRD metrics deletion from metric ids with rrdcached.
254. **BRRDCDDMDB1**: RRD metrics deletion from metric ids with a query in centreon_storage and rrdcached.
255. **BRRDCDDMID1**: RRD deletion of non existing metrics and indexes with rrdcached
256. **BRRDCDDMIDU1**: RRD deletion of non existing metrics and indexes with rrdcached
257. **BRRDCDDMU1**: RRD metric deletion on table metric with unified sql output with rrdcached
258. **BRRDCDRB1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with storage/sql sql output and rrdcached.
259. **BRRDCDRBDB1**: RRD metric rebuild with a query in centreon_storage and unified sql with rrdcached
260. **BRRDCDRBU1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with unified_sql output and rrdcached.
261. **BRRDCDRBUDB1**: RRD metric rebuild with a query in centreon_storage and unified sql with rrdcached
262. **BRRDDID1**: RRD metrics deletion from index ids.
263. **BRRDDIDDB1**: RRD metrics deletion from index ids with a query in centreon_storage.
264. **BRRDDIDU1**: RRD metrics deletion from index ids with unified sql output.
265. **BRRDDM1**: RRD metrics deletion from metric ids.
266. **BRRDDMDB1**: RRD metrics deletion from metric ids with a query in centreon_storage.
267. **BRRDDMID1**: RRD deletion of non existing metrics and indexes
268. **BRRDDMIDU1**: RRD deletion of non existing metrics and indexes
269. **BRRDDMU1**: RRD metric deletion on table metric with unified sql output
270. **BRRDRBDB1**: RRD metric rebuild with a query in centreon_storage and unified sql
271. **BRRDRBUDB1**: RRD metric rebuild with a query in centreon_storage and unified sql
272. **BRRDRM1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with storage/sql sql output.
273. **BRRDRMU1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with unified_sql output.
274. **BRRDSTATUS**: We are working with BBDO3. This test checks status are correctly handled independently from their value.
275. **BRRDSTATUSRETENTION**: We are working with BBDO3. This test checks status are not sent twice after Engine reload.
276. **BRRDUPLICATE**: RRD metric rebuild with a query in centreon_storage and unified sql with duplicate rows in database
277. **BRRDWM1**: We are working with BBDO3. This test checks protobuf metrics and status are sent to cbd RRD.
278. **CBD_RELOAD_AND_FILTERS**: We start engine/broker with a classical configuration. All is up and running. Some filters are added to the rrd output and cbd is reloaded. All is still up and running but some events are rejected. Then all is newly set as filter and all events are sent to rrd broker.
279. **CBD_RELOAD_AND_FILTERS_WITH_OPR**: We start engine/broker with an almost classical configuration, just the connection between cbd central and cbd rrd is reversed with one peer retention. All is up and running. Some filters are added to the rrd output and cbd is reloaded. All is still up and running but some events are rejected. Then all is newly set as filter and all events are sent to rrd broker.
280. **DTIM**: New services with several pollers are created. Then downtimes are set on all configured hosts. This action results on 5250 downtimes if we also count impacted services. Then all these downtimes are removed. This test is done with BBDO 3.0.1
281. **EBBM1**: A service status contains metrics that do not fit in a float number.
282. **EBBPS1**: 1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table resources.
283. **EBBPS2**: 1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table services.
284. **EBDP1**: Four new pollers are started and then we remove Poller3.
285. **EBDP2**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
286. **EBDP3**: Three new pollers are started, then they are killed. It is still possible to remove Poller2 if removed from the configuration.
287. **EBDP4**: Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by Broker.
288. **EBDP5**: Four new pollers are started and then we remove Poller3.
289. **EBDP6**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
290. **EBDP7**: Three new pollers are started, then they are killed. It is still possible to remove Poller2 if removed from the configuration.
291. **EBDP8**: Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by broker.
292. **EBDP_GRPC2**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
293. **EBMSSM**: 1000 services are configured with 100 metrics each. The rrd output is removed from the broker configuration. GetSqlManagerStats is called to measure writes into data_bin.
294. **EBMSSMDBD**: 1000 services are configured with 100 metrics each. The rrd output is removed from the broker configuration. While metrics are written in the database, we stop the database and then restart it. Broker must recover its connection to the database and continue to write metrics.
295. **EBMSSMPART**: **SCENARIO:** Broker continues writing metrics after partition recreation

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
296. **EBPN0**: Verify if child is in queue when parent is down.
297. **EBPN1**: verify relation parent child when delete parent.
298. **EBPN2**: verify relation parent child when delete child.
299. **EBPS2**: 1000 services are configured with 20 metrics each. The rrd output is removed from the broker configuration to avoid to write too many rrd files. While metrics are written in bulk, the database is stopped. This must not crash broker.
300. **EBSAU2**: New services with action_url with more than 2000 characters
301. **EBSN3**: New services with notes with more than 500 characters
302. **EBSN4**: New hosts with No Alias / Alias and have A Template
303. **EBSNU1**: New services with notes_url with more than 2000 characters
304. **ENRSCHE1**: Verify that next check of a rescheduled host is made at last_check + interval_check
305. **FILTER_ON_LUA_EVENT**: stream connector with a bad configured filter generate a log error message
306. **GRPC_CLOUD_FAILURE**: simulate a broker failure in cloud environment, we provide a muted grpc server and there must remain only one grpc connection. Then we start broker and connection must be ok
307. **GRPC_RECONNECT**: We restart broker and engine must reconnect to it and send data
308. **LCDNU**: the lua cache updates correctly service cache.
309. **LCDNUH**: the lua cache updates correctly host cache
310. **LOGV2DB2**: log-v2 disabled old log disabled check broker sink
311. **LOGV2DF2**: log-v2 disabled old log disabled check logfile sink
312. **LOGV2EB1**: Checking broker sink when log-v2 is enabled and legacy logs are disabled.
313. **LOGV2EBU1**: Checking broker sink when log-v2 is enabled and legacy logs are disabled with bbdo3.
314. **LOGV2EF1**: log-v2 enabled    old log disabled check logfile sink
315. **LUA_CACHE_SAVE_BBDO3**: 
     * **GIVEN** a engine broker configured in bbdo2, we check that services and hosts are stored in bbdo3 format in cache
     To do that we compare host and service event with lua cache
316. **MOVE_HOST_OF_HOSTGROUP_TO_ANOTHER_POLLER**: **SCENARIO:** Moving hosts between pollers without losing hostgroup tag

     * **GIVEN** two pollers each with two hosts
     * **AND** all hosts belong to the same hostgroup
     * **WHEN** I move two hosts from one poller to the other
     * **THEN** the hostgroup tag of the moved hosts is not erased
317. **NON_TLS_CONNECTION_WARNING**: 
     * **GIVEN** an agent starts a non-TLS connection,
     we expect to get a warning message.
318. **NON_TLS_CONNECTION_WARNING_ENCRYPTED**: 
     * **GIVEN** agent with encrypted connection, we expect no warning message.
319. **NON_TLS_CONNECTION_WARNING_FULL**: 
     * **GIVEN** an agent starts a non-TLS connection,
     we expect to get a warning message.
     After 1 hour, we expect to get a warning message about the connection time expired
     * **AND** the connection killed.
320. **NON_TLS_CONNECTION_WARNING_FULL_REVERSED**: 
     * **GIVEN** an agent starts a non-TLS connection reverse,
     we expect to get a warning message.
     After 1 hour, we expect to get a warning message about the connection time expired
     * **AND** the connection killed.
321. **NON_TLS_CONNECTION_WARNING_REVERSED**: 
     * **GIVEN** an agent starts a non-TLS connection reversed,
     we expect to get a warning message.
322. **NON_TLS_CONNECTION_WARNING_REVERSED_ENCRYPTED**: 
     * **GIVEN** agent with encrypted reversed connection, we expect no warning message.
323. **NO_FILTER_NO_ERROR**: no filter configured => no filter error.
324. **RENAME_PARENT**: 
     * **GIVEN** an host with a parent host. We rename the parent host and check if the child host is still linked to the parent.
     Engine mustn't crash and log an error on reload.
325. **RLCode**: Test if reloading LUA code in a stream connector applies the changes
326. **RRD1**: RRD metric rebuild asked with gRPC API. Three non existing indexes IDs are selected then an error message is sent. This is done with unified_sql output.
327. **SDER**: The check attempts and the max check attempts of (host_1,service_1) are changed to 280 thanks to the retention.dat file. Then Engine and Broker are started and Broker should write these values in the services and resources tables. We only test the services table because we need a resources table that allows bigger numbers for these two attributes. But we see that Broker doesn't crash anymore.
328. **SEVERAL_FILTERS_ON_LUA_EVENT**: Two stream connectors with different filters are configured.
329. **STORAGE_ON_LUA**: The category 'storage' is applied on the stream connector. Only events of this category should be sent to this stream.
330. **STUPID_FILTER**: Unified SQL is configured with only the bbdo category as filter. An error is raised by broker and broker should run correctly.
331. **Service_increased_huge_check_interval**: **SCENARIO:** New services with huge check interval at creation time.

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
332. **Services_and_bulks_1**: One service is configured with one metric with a name of 150 to 1021 characters.
333. **Services_and_bulks_2**: One service is configured with one metric with a name of 150 to 1021 characters.
334. **Start_Stop_Broker_Engine_1**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
335. **Start_Stop_Broker_Engine_2**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
336. **Start_Stop_Engine_Broker_1**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
337. **Start_Stop_Engine_Broker_2**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
338. **UNIFIED_SQL_FILTER**: With bbdo version 3.0.1, we watch events written or rejected in unified_sql
339. **VICT_ONE_CHECK_METRIC**: victoria metrics metric output
340. **VICT_ONE_CHECK_METRIC_AFTER_FAILURE**: victoria metrics metric output after victoria shutdown
341. **VICT_ONE_CHECK_STATUS**: victoria metrics status output
342. **Whitelist_Directory_NotReadable**: 
     * **GIVEN** a centengine started by centreon-engine user, whitelist directories are not readable and centengine must log an error
343. **Whitelist_Directory_Rights**: log if /etc/centreon-engine-whitelist has not mandatory rights or owner
344. **Whitelist_Empty_Directory**: log if /etc/centreon-engine-whitelist is empty
345. **Whitelist_Host**: Test on allowed and forbidden commands for hosts
346. **Whitelist_No_Whitelist_Directory**: log if /etc/centreon-engine-whitelist doesn't exist
347. **Whitelist_NotReadable**: 
     * **GIVEN** a centengine started by centreon-engine user, whitelist files are not readable and centengine must log an error
348. **Whitelist_Perl_Connector**: test allowed and forbidden commands for services
349. **Whitelist_Service**: test allowed and forbidden commands for services
350. **Whitelist_Service_EH**: test allowed and forbidden event handler for services
351. **first_notif_delay_equal_retry_interval**: **SCENARIO:** first notification delay equal to the retry interval

     * **GIVEN** a service whose first_notification_delay equals its retry interval
     * **WHEN** the service enters a CRITICAL HARD state
     * **THEN** the CRITICAL notification is sent after the delay
352. **first_notif_delay_gt_retry_interval**: **SCENARIO:** first notification delay greater than the retry interval

     * **GIVEN** a service whose first_notification_delay is greater than its retry interval
     * **WHEN** the service enters a CRITICAL HARD state
     * **THEN** the CRITICAL notification is sent after the delay
353. **first_notif_delay_lt_retry_interval**: **SCENARIO:** first notification delay smaller than the retry interval

     * **GIVEN** a service whose first_notification_delay is smaller than its retry interval
     * **WHEN** the service enters a CRITICAL HARD state
     * **THEN** the CRITICAL notification is sent after the delay
354. **flapping_notif**: **SCENARIO:** a flapping service triggers a FLAPPINGSTART notification

     * **GIVEN** a service with flap detection and flapping notifications enabled
     * **WHEN** the service state oscillates enough to start flapping
     * **THEN** a FLAPPINGSTART notification is sent
355. **forced_notif**: **SCENARIO:** a forced custom notification bypasses the notification-enabled check

     * **GIVEN** a host with notifications disabled
     * **WHEN** a forced custom notification is sent for the host
     * **THEN** the notification is sent despite notifications being disabled
356. **host_down_alert**: **SCENARIO:** a host going down raises a host alert

     * **GIVEN** a host configured with notifications
     * **WHEN** the host enters a DOWN HARD state
     * **THEN** a HOST ALERT is logged
357. **host_down_notif**: **SCENARIO:** a host going down triggers a DOWN notification

     * **GIVEN** a host configured with notifications
     * **WHEN** the host enters a DOWN HARD state
     * **THEN** a DOWN notification is sent to its contact
358. **host_notif_dependency**: **SCENARIO:** a host notification is suppressed by a host dependency

     * **GIVEN** a host depending on another host that has already been notified
     * **WHEN** the dependent host enters a DOWN state
     * **THEN** it sends no notification because its dependency already notified
359. **host_rec_notif**: **SCENARIO:** a host recovery triggers a recovery notification

     * **GIVEN** a host that sent a DOWN notification
     * **WHEN** the host returns to an UP HARD state
     * **THEN** a RECOVERY notification is sent
360. **host_rec_notif_with_downtime**: **SCENARIO:** host notifications across a downtime episode until recovery

     * **GIVEN** a DOWN host for which a downtime is scheduled
     * **WHEN** the downtime suppresses notifications, then is removed while the host is still DOWN, and the host later returns to UP
     * **THEN** no notification is sent during the downtime, a DOWN notification is sent once it is removed, and a RECOVERY notification is sent on recovery
361. **hostgrp_notif_dependency**: **SCENARIO:** host notifications are suppressed by a hostgroup dependency

     * **GIVEN** a hostgroup depending on another hostgroup that has already been notified
     * **WHEN** a host of the dependent hostgroup enters a DOWN state
     * **THEN** it sends no notification because its dependency already notified
362. **metric_mapping**: Check if metric name exists using a stream connector
363. **rec_notif_outside_tp_not_sent**: **SCENARIO:** a recovery is not sent outside the notification period when the flag is off

     * **GIVEN** a CRITICAL service inside its notification period
     * **WHEN** the service recovers outside its notification period and send_recovery_notifications_anyway is off
     * **THEN** the CRITICAL notification is sent but no RECOVERY notification is sent
364. **rec_notif_outside_tp_sent_when_enabled**: **SCENARIO:** a recovery is sent outside the notification period when the flag is on

     * **GIVEN** a CRITICAL service inside its notification period
     * **WHEN** the service recovers outside its notification period and send_recovery_notifications_anyway is on
     * **THEN** both the CRITICAL and the RECOVERY notifications are sent
365. **srv_crit_notif**: **SCENARIO:** a critical service triggers a notification

     * **GIVEN** a service configured with notifications enabled
     * **WHEN** the service enters a non-OK HARD state
     * **THEN** a CRITICAL notification is sent to its contact
366. **srv_notif_allowed_by_wlist**: **SCENARIO:** a service notification command allowed by the whitelist is executed

     * **GIVEN** a service in a non-OK HARD state and a whitelist allowing its notification command
     * **WHEN** the notification is triggered
     * **THEN** the notification command is executed
367. **srv_notif_blocked_by_wlist**: **SCENARIO:** a service notification command blocked by the whitelist is not executed

     * **GIVEN** a service in a non-OK HARD state and a whitelist not allowing its notification command
     * **WHEN** the notification is triggered
     * **THEN** the notification command is rejected by the whitelist
368. **srv_notif_dependency**: **SCENARIO:** a service notification is suppressed by a service dependency

     * **GIVEN** a service depending on another service that has already been notified
     * **WHEN** the dependent service enters a CRITICAL state
     * **THEN** it sends no notification because its dependency already notified
369. **srv_notif_escalations**: **SCENARIO:** service notifications follow the configured escalations

     * **GIVEN** services with notification escalations to different contact groups
     * **WHEN** the services stay CRITICAL across successive notifications
     * **THEN** each escalation level notifies its configured contact group in turn
370. **srv_notif_multiple_commands**: **SCENARIO:** a contact with several notification commands runs them all

     * **GIVEN** a contact configured with two service notification commands
     * **WHEN** a service enters a CRITICAL HARD state
     * **THEN** a notification is sent through each configured command
371. **srv_notif_routed_to_correct_ctct**: **SCENARIO:** each service notification is routed to its own contact

     * **GIVEN** two services each assigned to a different contact
     * **WHEN** both services enter a CRITICAL HARD state
     * **THEN** each contact receives the notification for its own service
372. **srv_notif_suppr_by_empty_tp**: **SCENARIO:** an empty notification period suppresses notifications

     * **GIVEN** a CRITICAL service whose notification period is then set to none
     * **WHEN** the service state changes
     * **THEN** no notification is sent because the notifier is out of its notification period
373. **srv_notif_suppr_during_downtime**: **SCENARIO:** notifications are suppressed while a service is in downtime

     * **GIVEN** a service with a scheduled downtime
     * **WHEN** the service enters a CRITICAL state during the downtime
     * **THEN** no notification is sent during the downtime
     * **AND** once the downtime is removed the CRITICAL and then RECOVERY notifications are sent
374. **srv_notif_then_rm_and_reload**: **SCENARIO:** removing a notified service that is in downtime and reloading does not crash Engine

     * **GIVEN** a service in a non-OK HARD state that has sent a notification and is under a scheduled downtime
     * **WHEN** the service is removed from the configuration and Engine and Broker are reloaded
     * **THEN** Engine keeps running without crashing
375. **srv_rec_notif**: **SCENARIO:** a service recovery triggers a recovery notification

     * **GIVEN** a service that sent a CRITICAL notification
     * **WHEN** the service returns to an OK HARD state
     * **THEN** a RECOVERY notification is sent
376. **srv_rec_notif_after_ack**: **SCENARIO:** a recovery notification is sent after an acknowledged problem recovers

     * **GIVEN** a CRITICAL service that has been acknowledged
     * **WHEN** the service returns to an OK HARD state
     * **THEN** a RECOVERY notification is sent
377. **srv_state_alerts_soft_and_hard**: **SCENARIO:** successive state changes raise SOFT then HARD service alerts

     * **GIVEN** a service configured with several check attempts
     * **WHEN** the service stays CRITICAL over successive checks
     * **THEN** SOFT 1, SOFT 2 and HARD 3 service alerts are logged
378. **srvgrp_notif_dependency**: **SCENARIO:** service notifications are suppressed by a servicegroup dependency

     * **GIVEN** a servicegroup depending on another servicegroup that has already been notified
     * **WHEN** a service of the dependent servicegroup enters a CRITICAL state
     * **THEN** it sends no notification because its dependency already notified

### Ccc
1. **BECCC1**: ccc without port fails with an error message
2. **BECCC2**: ccc with -p 51001 connects to central cbd gRPC server.
3. **BECCC3**: ccc with -p 50001 connects to centengine gRPC server.
4. **BECCC4**: ccc with -p 51001 -l returns the available functions from Broker gRPC server
5. **BECCC5**: ccc with -p 51001 -l GetVersion returns an error because we can't execute a command with -l.
6. **BECCC6**: ccc with -p 51001 GetVersion{} calls the GetVersion command
7. **BECCC7**: ccc with -p 51001 GetVersion{"idx":1} returns an error because the input message is wrong.
8. **BECCC8**: ccc with -p 50001 EnableServiceNotifications{"names":{"host_name": "host_1", "service_name": "service_1"}} works and returns an empty message.

### Centralized/configuration
1. **BECNHG1**: **SCENARIO:** Host group synchronization across 3 pollers in centralized configuration

     * **GIVEN** a centralized engine with 3 pollers
     * **AND** broker is configured with RRD, central module, and SQL debug logging
     * **AND** database connections are set to 5 for both SQL and perfdata outputs
     * **WHEN** I start the broker and engine with new generation
     * **AND** I add a host group containing 3 hosts (host_1, host_2, host_3)
     * **AND** I notify broker of the engine configuration change
     * **THEN** the logs should confirm membership of all 3 hosts to the host group
     * **AND** each host should be properly associated with host group 1 on instance 1
2. **BECNHG3**: **SCENARIO:** Host group synchronization across 4 pollers in centralized configuration

     * **GIVEN** 4 pollers and Broker are started in centralized mode
     * **WHEN** hostgroup_1 is added with 3 hosts per poller (12 total)
     * **THEN** Broker receives all 12 hosts as hostgroup_1 members
     * **WHEN** hostgroup configuration files are removed sequentially from each poller
     * **THEN** Broker progressively removes corresponding hosts from database
     * **AND** hostgroup_1 membership decreases from 12 → 9 → 6 → 3 → 0
3. **BECNHG4**: **SCENARIO:** Host group rename synchronization in centralized configuration

     * **GIVEN** 3 pollers and Broker are started in centralized mode
     * **WHEN** hostgroup_1 is created on poller 1 with hosts: host_1, host_2, host_3
     * **THEN** Broker receives hostgroup_1 with its 3 members
     * **WHEN** hostgroup_1 is renamed to hostgroup_test
     * **THEN** Broker updates the hostgroup name to hostgroup_test in database
     * **AND** the same 3 hosts remain as members of hostgroup_test
4. **BECNHG5**: **SCENARIO:** Host group removal and recreation with same hosts in centralized configuration

     * **GIVEN** 3 pollers and Broker are started in centralized mode
     * **WHEN** hostgroup_1 is created on each poller with different hosts
     * **THEN** Broker receives all hostgroups with their respective members
     * **WHEN** hostgroup_1 is removed from poller 1 and hostgroup_2 is created with the same hosts
     * **THEN** Broker updates the database to reflect the changes
     * **AND** hostgroup_2 contains the 3 hosts from poller 1
     * **AND** hostgroup_1 still contains the 6 hosts from pollers 2 and 3
5. **BECNSG1**: **SCENARIO:** Service group creation and synchronization in centralized configuration

     * **GIVEN** 3 pollers and Broker are started in centralized mode
     * **WHEN** a service group is created on poller 1 with 3 services from host_1
     * **THEN** Broker receives the service group configuration
     * **AND** the 3 services are registered as members of the service group in logs
6. **BECNSG2**: **FEATURE:** Service Groups Management with Unified SQL Database

     **SCENARIO:** Create 4 service groups (3 services each) across 4 pollers, then progressively
     remove servicegroups.cfg files to validate database consistency.
     Given: 4 Engine pollers + central Broker with unified SQL + BBDO3 + debug logs
     When: Create service groups and add servicegroups.cfg to each poller
     Then: Database should show 12 associations in services_servicegroups table
     When: Remove servicegroups.cfg from pollers sequentially
     Then: Associations should decrease by 3 for each removal (12→9→6→3→0)
     Validates: Service group associations are correctly maintained during config changes
7. **BECNSG3**: Test about lua cache. But the centralized configuration currently breaks the broker cache.
8. **BECNSVC1**: 
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
9. **BECNTAG1**: **FEATURE:** Tag associations in Broker gRPC cache with centralized configuration

     **BACKGROUND:**

     * **GIVEN** 4 pollers are configured with 5 hosts each (20 total) and 20 services per host (400 total)
     * **AND** 4 tags are defined on every poller, one of each TagType:
     tag1 (id=1, SERVICEGROUP=0), tag2 (id=1, HOSTGROUP=1),
     tag3 (id=1, SERVICECATEGORY=2), tag4 (id=1, HOSTCATEGORY=3)
     * **AND** Broker and Engine are started in centralized (BBDO3) mode

     **SCENARIO:** Phase 1 - All pollers assign tags

     * **GIVEN** all 4 pollers assign group_tags and category_tags to every host and service
     * **WHEN** Broker and Engine are started and synchronized
     * **THEN** the broker gRPC cache returns exactly the 20 expected hosts with HOSTGROUP tag 'tag2'
     * **AND** the broker gRPC cache returns exactly the 20 expected hosts with HOSTCATEGORY tag 'tag4'
     * **AND** the broker gRPC cache returns 400 services with SERVICEGROUP tag 'tag1', all on the 20 expected hosts
     * **AND** the broker gRPC cache returns 400 services with SERVICECATEGORY tag 'tag3', all on the 20 expected hosts

     **SCENARIO:** Phase 2 - Tags removed from poller 3

     * **GIVEN** the initial state has 20 tagged hosts and 400 tagged services
     * **WHEN** group_tags and category_tags are removed from poller 3 and broker is notified
     * **THEN** the broker gRPC cache returns exactly hosts from pollers 0-2 with HOSTGROUP tag 'tag2'
     * **AND** the broker gRPC cache returns exactly hosts from pollers 0-2 with HOSTCATEGORY tag 'tag4'
     * **AND** the broker gRPC cache returns 300 services with SERVICEGROUP tag 'tag1', all on hosts from pollers 0-2
     * **AND** the broker gRPC cache returns 300 services with SERVICECATEGORY tag 'tag3', all on hosts from pollers 0-2

     **SCENARIO:** Phase 3 - Tags removed from poller 2

     * **GIVEN** poller 3 tags have already been removed
     * **WHEN** group_tags and category_tags are removed from poller 2 and broker is notified
     * **THEN** the broker gRPC cache returns exactly hosts from pollers 0-1 with HOSTGROUP tag 'tag2'
     * **AND** the broker gRPC cache returns exactly hosts from pollers 0-1 with HOSTCATEGORY tag 'tag4'
     * **AND** the broker gRPC cache returns 200 services with SERVICEGROUP tag 'tag1', all on hosts from pollers 0-1
     * **AND** the broker gRPC cache returns 200 services with SERVICECATEGORY tag 'tag3', all on hosts from pollers 0-1

     **SCENARIO:** Phase 4 - Tags removed from all remaining pollers

     * **GIVEN** pollers 2 and 3 tags have already been removed
     * **WHEN** group_tags and category_tags are removed from pollers 0 and 1 and broker is notified
     * **THEN** the broker gRPC cache returns 0 hosts with HOSTGROUP tag 'tag2'
     * **AND** the broker gRPC cache returns 0 hosts with HOSTCATEGORY tag 'tag4'
     * **AND** the broker gRPC cache returns 0 services with SERVICEGROUP tag 'tag1'
     * **AND** the broker gRPC cache returns 0 services with SERVICECATEGORY tag 'tag3'

     **SCENARIO:** Phase 5 - Tag cache is empty (no orphan tags)

     * **GIVEN** all tags have been removed from all pollers
     * **WHEN** GetTags gRPC is called
     * **THEN** the broker tag cache returns an empty list (no orphan tags remain)
10. **BECNTAG2**: **FEATURE:** Tag rename is reflected in the Broker gRPC cache

     **BACKGROUND:**

     * **GIVEN** 4 pollers configured with 5 hosts each (20 total) and 20 services per host
     * **AND** 4 tags (id=1, one per TagType) initially named tag1..tag4 on every poller
     * **AND** tags assigned to all hosts and services

     **SCENARIO:** Tag names are updated in the broker cache after rename on all pollers

     * **GIVEN** the initial state has 20 tagged hosts and 400 tagged services
     * **WHEN** all 4 pollers rename their tags to tag11..tag14 (same ids, new names)
     * **THEN** broker GetTags returns exactly the 4 entries with the new names
     * **AND** GetHostsByTag with the new HOSTGROUP name returns all 20 hosts
     * **AND** GetServicesByTag with the new SERVICEGROUP name returns all 400 services
11. **BECNTAG3**: **FEATURE:** GetTags gRPC returns correct content while tags are active

     **BACKGROUND:**

     * **GIVEN** 4 pollers configured with 5 hosts each (20 total) and 20 services per host
     * **AND** 4 tags (id=1, one per TagType) named tag1..tag4 on every poller
     * **AND** tags assigned to all hosts and services

     **SCENARIO:** GetTags returns 4 entries with the correct names while tags are active
     * **WHEN** Broker and Engine are started and synchronized
     * **THEN** GetTags returns exactly 4 entries
     * **AND** the entry names are exactly {tag1, tag2, tag3, tag4}
12. **BECNTAG4**: **FEATURE:** Broker cache is repopulated after broker restart with tags active

     **BACKGROUND:**

     * **GIVEN** 4 pollers configured with 5 hosts each (20 total) and 20 services per host
     * **AND** 4 tags (id=1, one per TagType) named tag1..tag4 assigned to all hosts/services

     **SCENARIO:** After broker restart, GetTags and GetHostsByTag return correct data

     * **GIVEN** broker and engine are started and synchronized
     * **AND** GetTags returns 4 entries before broker stops
     * **WHEN** broker is stopped and restarted (engine keeps running)
     * **THEN** GetTags returns the same 4 entries after restart
     * **AND** GetHostsByTag returns all 20 expected hosts
     * **AND** GetServicesByTag returns all 400 expected services
13. **BECPN0**: **FEATURE:** Parent-Child Host Dependency Management
     As a monitoring administrator
     I want child host checks to be queued when parent hosts are down
     So that unnecessary checks are avoided
14. **BECPN1**: **FEATURE:** Parent Host Deletion Management
     As a monitoring administrator
     I want parent-child relationships to be cleaned up when parent hosts are deleted
     So that orphaned relationships don't exist in the system

     **SCENARIO:** Parent-child relationship cleanup on parent deletion

     * **GIVEN** host_1 is configured as parent of host_2
     * **AND** the monitoring system is running
     * **AND** the parent-child relationship exists in the database
     * **WHEN** I delete host_1 from the configuration
     * **AND** I notify Broker about that change in the engine configuration
     * **THEN** host_2 should have no parent hosts
     * **AND** the parent-child relationship should be removed from the database
15. **BECPN2**: **FEATURE:** Child Host Deletion Management
     As a monitoring administrator
     I want parent-child relationships to be cleaned up when child hosts are deleted
     So that orphaned relationships don't exist in the system

     **SCENARIO:** Parent-child relationship cleanup on child deletion

     * **GIVEN** host_1 is configured as parent of host_2
     * **AND** the monitoring system is running
     * **AND** the parent-child relationship exists in the database
     * **WHEN** I delete host_2 from the configuration
     * **AND** I notify Broker of a change in the engine configuration
     * **THEN** host_1 should have no child hosts
     * **AND** the parent-child relationship should be removed from the database
16. **BECSS1**: **SCENARIO:** Broker sends configuration to engine in new generation

     * **GIVEN** an engine configuration is provided to the broker
     * **AND** the broker and engine are started in new generation (broker first)
     * **AND** the protocol is bbdo3
     * **WHEN** the broker detects the configuration for the engine
     * **THEN** the broker sends the configuration to the engine
     * **THEN** both broker and engine are stopped (engine first)
17. **BECSS2**: **SCENARIO:** Broker sends configuration to engine in new generation

     * **GIVEN** an engine configuration is provided to the broker
     * **AND** the broker and engine are started in new generation (broker first)
     * **AND** the protocol is bbdo3
     * **WHEN** the broker detects the configuration for the engine
     * **THEN** the broker sends the configuration to the engine
     * **THEN** both broker and engine are stopped (engine first)
18. **BECSS3**: **SCENARIO:** Broker sends configuration to engine in new generation

     * **GIVEN** an engine configuration is provided to the broker
     * **AND** the broker and engine are started in new generation (engine first)
     * **AND** the protocol is bbdo3
     * **WHEN** the broker detects the configuration for the engine
     * **THEN** the broker sends the configuration to the engine
     * **THEN** both broker and engine are stopped (engine first)
19. **BECSS4**: **SCENARIO:** Broker sends configuration to engine in new generation

     * **GIVEN** an engine configuration is provided to the broker
     * **AND** the broker and engine are started in new generation (engine first)
     * **AND** the protocol is bbdo3
     * **WHEN** the broker detects the configuration for the engine
     * **THEN** the broker sends the configuration to the engine
     * **THEN** both broker and engine are stopped (broker first)
20. **BECSSBQ1**: A very bad queue file is written for broker. Broker and Engine are then started, Broker must read the file raising an error because of that file and then get data sent by Engine.
21. **BECSS_CRYPTED_GRPC1**: **SCENARIO:** Repeated start/stop cycles with gRPC and mutual TLS in centralized configuration mode

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
22. **BECSS_CRYPTED_GRPC2**: Start-Stop grpc version Broker/Engine only server crypted
23. **BECSS_CRYPTED_REVERSED_GRPC1**: Start-Stop grpc version Broker/Engine - well configured
24. **BECSS_CRYPTED_REVERSED_GRPC2**: Start-Stop grpc version Broker/Engine only engine server crypted
25. **BECSS_CRYPTED_REVERSED_GRPC3**: Start-Stop grpc version Broker/Engine only engine crypted
26. **BECSS_ENGINE_DELETE_HOST**: once engine and cbd started, stop and restart cbd, delete an host and reload engine, cbd mustn't core
27. **BECSS_GRPC1**: **SCENARIO:** Broker sends configuration to engine in new generation

     * **GIVEN** an engine configuration is provided to the broker
     * **AND** the broker and engine are started in new generation (broker first)
     * **AND** the protocol is bbdo3
     * **WHEN** the broker detects the configuration for the engine
     * **THEN** the broker sends the configuration to the engine
     * **THEN** both broker and engine are stopped (engine first)
28. **BECSS_GRPC2**: **SCENARIO:** Broker sends configuration to engine in new generation

     * **GIVEN** an engine configuration is provided to the broker
     * **AND** the broker and engine are started in new generation (broker first)
     * **AND** the protocol is bbdo3
     * **WHEN** the broker detects the configuration for the engine
     * **THEN** the broker sends the configuration to the engine
     * **THEN** both broker and engine are stopped (engine first)
29. **BECSS_GRPC3**: **SCENARIO:** Broker sends configuration to engine in new generation

     * **GIVEN** an engine configuration is provided to the broker
     * **AND** the broker and engine are started in new generation (engine first)
     * **AND** the protocol is bbdo3
     * **WHEN** the broker detects the configuration for the engine
     * **THEN** the broker sends the configuration to the engine
     * **THEN** both broker and engine are stopped (engine first)
30. **BECSS_GRPC4**: **SCENARIO:** Broker sends configuration to engine in new generation

     * **GIVEN** an engine configuration is provided to the broker
     * **AND** the broker and engine are started in new generation (engine first)
     * **AND** the protocol is bbdo3
     * **WHEN** the broker detects the configuration for the engine
     * **THEN** the broker sends the configuration to the engine
     * **THEN** both broker and engine are stopped (broker first)
31. **BECSS_GRPC_COMPRESS1**: Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped last compression activated
32. **BECTAG1**: **FEATURE:** Tag Management between Engine and Broker
     As a Centreon administrator
     I want to configure tags in Engine
     So that Broker stores them correctly in centreon_storage.tags table

     **BACKGROUND:**

     * **GIVEN** Engine is configured with centralized setup
     * **AND** Broker components (central, rrd, module) are configured
     * **AND** Database logging is enabled with debug/trace level
     * **AND** Retention data is cleared

     **SCENARIO:** Initial tag configuration

     * **GIVEN** Engine is configured with 20 tags
     * **WHEN** Broker and Engine are started
     * **THEN** 20 tags should be added/modified in logs
     * **AND** INSERT statements should be executed in tags table
     * **AND** Configuration file should match database content
     * **AND** Tag IDs should be consistent

     **SCENARIO:** Tag configuration modification

     * **GIVEN** Initial configuration with 20 tags is loaded
     * **WHEN** Configuration is modified to 30 tags
     * **AND** Engine configuration change is notified
     * **THEN** 10 additional tags should be added/modified
     * **AND** Configuration file should still match database content
     * **AND** Tag IDs should remain consistent

     **SCENARIO:** Tag configuration reduction

     * **GIVEN** Configuration with 30 tags is loaded
     * **WHEN** Configuration is reduced to 11 tags starting at ID 50
     * **AND** Engine configuration change is notified
     * **THEN** 11 tags should be present in final configuration
     * **AND** Unused tags should be implicitly removed
     * **AND** Configuration file should match database content
     * **AND** Tag IDs should be consistent with new range
33. **CANO_CFG_SENSITIVITY_SAVED**: 
     * **GIVEN** an anomaly detection service is configured with a specific sensitivity value in configuration
     * **AND** the threshold file contains prediction data with sensitivity parameters
     * **WHEN** the engine and broker are started and then stopped
     * **THEN** the configuration-based sensitivity value should be persisted in the retention data
     because CFG sensitivity parameters are properly saved during retention processing
34. **CANO_DT1**: 
     * **GIVEN** an anomaly detection service is configured with a dependent service relationship
     * **AND** both services are running normally
     * **WHEN** a downtime is scheduled on the dependent service
     * **THEN** the dependent service should enter downtime state
     * **AND** the anomaly detection service should automatically inherit the downtime
     because anomaly detection services inherit downtime from their dependent services
35. **CANO_DT2**: 
     * **GIVEN** an anomaly detection service is configured with a dependent service relationship
     * **AND** both services are running normally
     * **WHEN** a downtime is scheduled on the dependent service
     * **THEN** the anomaly detection service should automatically enter downtime
     * **WHEN** the downtime is deleted from the dependent service
     * **THEN** the anomaly detection service should automatically exit downtime
     because anomaly detection downtime should follow its dependent service downtime state
36. **CANO_DT3**: 
     * **GIVEN** an anomaly detection service is configured with a dependent service relationship
     * **AND** both services are running normally
     * **WHEN** a downtime is scheduled on the dependent service
     * **THEN** the anomaly detection service should automatically enter downtime
     * **WHEN** the downtime is deleted from the anomaly detection service
     * **THEN** the dependent service should remain in its original downtime state
     because deleting downtime on anomaly detection should not affect dependent service downtimes
37. **CANO_DT4**: **SCENARIO:** Removing downtime from service keeps it on anomaly detection

     * **GIVEN** an anomaly detection is attached to a service
     * **AND** a downtime is set on both the service and the anomaly detection
     * **WHEN** the downtime is removed from the service
     * **THEN** the downtime should still be present on the anomaly detection
38. **CANO_EXTCMD_SENSITIVITY_SAVED**: 
     * **GIVEN** an anomaly detection service is configured with threshold data
     * **AND** the service is running with initial sensitivity parameters
     * **WHEN** an external command updates the anomaly sensitivity value
     * **AND** the engine and broker are stopped
     * **THEN** the updated sensitivity value should be persisted in the retention data
     because external command sensitivity changes are properly saved during retention processing
39. **CANO_JSON_SENSITIVITY_NOT_SAVED**: 
     * **GIVEN** an anomaly detection service is configured with threshold data including sensitivity
     * **AND** the threshold file contains prediction data with a specific sensitivity value
     * **WHEN** the engine and broker are started and then stopped
     * **THEN** the sensitivity value should not be persisted in the retention data
     because JSON sensitivity parameters are not saved during retention processing
40. **CANO_NOFILE**: 
     * **GIVEN** an anomaly detection service is configured for metric monitoring
     * **AND** the threshold configuration file is missing from the system
     * **WHEN** the service processes a check result with critical state
     * **THEN** the anomaly detection service must transition to UNKNOWN state
     because it cannot determine thresholds without the configuration file
41. **CANO_OUT_LOWER_THAN_LIMIT**: 
     * **GIVEN** an anomaly detection service is configured with valid threshold data
     * **AND** the threshold file contains lower and upper limits for the metric
     * **WHEN** a service check provides performance data below the lower threshold limit
     * **THEN** the anomaly detection service must transition to CRITICAL state
     because the metric value indicates an anomalous condition requiring attention
42. **CANO_OUT_UPPER_THAN_LIMIT**: 
     * **GIVEN** an anomaly detection service is configured with valid threshold data
     * **AND** the threshold file contains lower and upper limits for the metric
     * **WHEN** a service check provides performance data above the upper threshold limit
     * **THEN** the anomaly detection service must transition to CRITICAL state
     because the metric value indicates an anomalous condition requiring attention
43. **CANO_TOO_OLD_FILE**: 
     * **GIVEN** an anomaly detection service is configured with metric monitoring
     * **AND** a threshold file exists but contains outdated prediction data
     * **WHEN** the service processes a check result with performance data
     * **THEN** the anomaly detection service must transition to UNKNOWN state
     because the threshold data is too old to be reliable for current predictions
44. **CAOUTLU1**: 
     * **GIVEN** an anomaly detection service is configured with valid threshold data using BBDO3 protocol
     * **AND** the threshold file contains lower and upper limits for the metric
     * **WHEN** a service check provides performance data above the upper threshold limit
     * **THEN** the anomaly detection service must transition to CRITICAL state
     * **AND** the resources table should contain SERVICE, HOST and ANOMALY_DETECTION type entries
45. **CBEUDHOSTS**: 
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
46. **CCCRC1**: 
     * **GIVEN** a topology Poller1 -> Relay1 -> central cbd
     * **WHEN** Engine connects to the relay
     * **THEN** the relay sends a ConfigRequest to the central for poller 1
     * **AND** the central logs the receipt of that ConfigRequest.
47. **CCCRC2**: 
     * **GIVEN** a topology Poller1 -> Relay3 -> central cbd
     * **AND** a poller configuration is pre-created before starting the central broker
     * **WHEN** the central processes the configuration and the relay sends a ConfigRequest
     * **THEN** the central sends a non-unknown DiffState to the relay.
48. **CCCRC3**: 
     * **GIVEN** a topology Poller1 -> Relay3 -> central cbd
     * **AND** a poller configuration is pre-created before starting the central broker
     * **WHEN** Engine connects through the relay and the central sends a DiffState
     * **THEN** the relay forwards the DiffState to Engine
     * **AND** the relay forwards the DiffStateAck back to the central.
49. **CCCRC4**: 
     * **GIVEN** a topology Poller1 -> Relay3 -> central cbd
     * **AND** a poller configuration is pre-created before starting central
     * **WHEN** Engine connects and gets the initial config via relay
     * **AND** PHP pushes a new config for poller 1 (5 extra hosts)
     * **THEN** the central sends a new DiffState to the relay
     * **AND** the central receives a new DiffStateAck.
50. **CCCRC5**: 
     * **GIVEN** Engine initially connected to central via Relay3 (poller_id=4)
     * **WHEN** Engine migrates to Relay4 (poller_id=5)
     * **THEN** the central sends ConfigRevoke to Relay3
     * **AND** serves the configuration to Engine via Relay4.
51. **CCCRC6**: 
     * **GIVEN** Engine connected via Relay3 with initial config established
     * **WHEN** the central is stopped cleanly and a new config is pushed during the outage
     * **THEN** after the central restarts, the relay reconnects and the new DiffState
     is forwarded to Engine via the relay, and central receives a new DiffStateAck.
52. **CCCRC7**: 
     * **GIVEN** Engine connected via Relay3 with initial config established
     * **WHEN** GetTopology is called on the central gRPC endpoint
     * **THEN** the response contains Relay3 as a direct broker with poller 1 as its poller.
53. **Centralized_Start_Stop_Broker_Engine_1**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
54. **Centralized_Start_Stop_Broker_Engine_2**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
55. **Centralized_Start_Stop_Engine_Broker_1**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
56. **Centralized_Start_Stop_Engine_Broker_2**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
57. **RENAME_PARENT**: **FEATURE:** Parent Host Rename Management
     As a monitoring administrator
     I want parent-child relationships to be maintained when parent hosts are renamed
     So that dependencies remain intact after configuration changes

     **SCENARIO:** Parent-child relationship maintained on parent rename

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
1. **CCONPERL**: **SCENARIO:** Single host check via Perl Connector in centralized configuration

     * **GIVEN** a centralized engine and broker configuration with the Perl Connector
     * **WHEN** a forced host check is scheduled on host_1
     * **THEN** the check execution result should appear in the engine log file.
2. **CCONPERLM**: **SCENARIO:** Ten host checks via Perl Connector in centralized configuration

     * **GIVEN** a centralized engine and broker configuration with the Perl Connector on ten hosts
     * **WHEN** a forced check is scheduled on each of the ten hosts
     * **THEN** the check execution result for each host should appear in the engine log file.
3. **CONPERL**: The test.pl script is launched using the perl connector. Then we should find its execution in the engine log file.
4. **CONPERLM**: Ten forced checks are scheduled on ten hosts configured with the Perl Connector. The we get the result of each of them.

### Connector ssh
1. **CTest6Hosts**: **SCENARIO:** SSH checks succeed on 6 hosts in centralized configuration

     * **GIVEN** a centralized engine and broker configuration with 6 hosts reachable via SSH
     * **WHEN** forced checks are scheduled on all 6 hosts
     * **THEN** the expected output for each host address should appear in the log.
2. **CTestBadPwd**: **SCENARIO:** SSH check with wrong password fails in centralized configuration

     * **GIVEN** a centralized engine and broker configuration with a wrong SSH password on host_1
     * **WHEN** a forced host check is scheduled
     * **THEN** a connection failure message for the bad password should appear in the log.
3. **CTestBadUser**: **SCENARIO:** SSH check with unknown user fails in centralized configuration

     * **GIVEN** a centralized engine and broker configuration with an unknown SSH user on host_1
     * **WHEN** a forced host check is scheduled
     * **THEN** a connection failure message for the unknown user should appear in the log.
4. **CTestWhiteList**: **SCENARIO:** SSH check blocked then allowed by whitelist in centralized configuration

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
72. **ECI0**: Verify contact inheritance : contact(empty) inherit from template (full), on Start Engine
73. **ECI1**: Verify contact inheritance : contact(full) inherit from template (full) , on Start Engine
74. **ECI2**: Verify contact inheritance : contact(empty) inherit from template (full) , on Reload Engine
75. **ECI3**: Verify contact inheritance : contact(full) inherit from template (full) , on Reload Engine
76. **ECMI0**: Verify command inheritance : command(empty) inherit from template (full) , on Start Engine
77. **ECMI1**: Verify command inheritance : command(full) inherit from template (full) , on Start Engine
78. **ECMI2**: Verify command inheritance : command(empty) inherit from template (full) , on Reload Engine
79. **ECMI3**: Verify command inheritance : command(full) inherit from template (full) , on reload Engine
80. **ECOI0**: Verify connector inheritance : connector(empty) inherit from template (full) , on Start Engine
81. **ECOI1**: Verify connector inheritance : connector(full) inherit from template (full) , on Start Engine
82. **ECOI2**: Verify connector inheritance : connector(empty) inherit from template (full) , on Reload Engine
83. **ECOI3**: Verify connector inheritance : connector(full) inherit from template (full) , on Reload Engine
84. **EESI0**: Verify service escalation : create service escalation for every service in a service group
85. **EESI1**: Verify service escalation  inheritance : escalation(empty) inherit from template (full) , on Start Engine
86. **EESI2**: Verify service escalation  inheritance : escalation(full) inherit from template (full) , on Start Engine
87. **EESI3**: Verify service escalation  inheritance : escalation(empty) inherit from template (full) , on Reload Engine
88. **EESI4**: Verify service escalation  inheritance : escalation(full) inherit from template (full) , on Reload Engine
89. **EESI5**: Verfiy host escalation : create host escalation for every host in the hostgroup
90. **EESI6**: Verify host escalation inheritance : escalation(empty) inherit from template (full) , on Start Engine   
91. **EESI7**: Verify host escalation inheritance : escalation(full) inherit from template (full) , on Start Engine    
92. **EESI8**: Verify host escalation inheritance : escalation(empty) inherit from template (full) , on Reload Engine   
93. **EESI9**: Verify host escalation inheritance : escalation(full) inherit from template (full) , on Reload Engine    
94. **EFHC1**: Engine is configured with hosts and we force check one 5 times with bbdo2
95. **EFHC2**: Engine is configured with hosts and we force check on one 5 times on bbdo2
96. **EFHCU1**: Engine is configured with hosts and we force checks on one 5 times on bbdo3. Bbdo3 has no impact on this behavior. resources table is cleared before starting broker.
97. **EFHCU2**: Engine is configured with hosts and we force checks on one 5 times on bbdo3. Bbdo3 has no impact on this behavior.
98. **EHGI0**: Verify hostgroup inheritance : hostgroup(empty) inherit from template (full) , on Start Engine
99. **EHGI1**: Verify hostgroup inheritance : hostgroup(full) inherit from template (full) , on Start Engine
100. **EHGI2**: Verify hostgroup inheritance : hostgroup(empty) inherit from template (full) , on Reload Engine
101. **EHGI3**: Verify hostgroup inheritance : hostgroup(full) inherit from template (full) , on Reload Engine
102. **EHI0**: Verify inheritance host : host(empty) inherit from template (full) , on Start Engine
103. **EHI1**: Verify inheritance host : host(full) inherit from template (full) , on Start engine
104. **EHI2**: Verify inheritance host : host(empty) inherit from template (full) , on Reload engine
105. **EHI3**: Verify inheritance host : host(full) inherit from template (full) , on engine Reload
106. **EMACROS**: macros ADMINEMAIL and ADMINPAGER are replaced in check outputs
107. **EMACROS_NOTIF**: macros ADMINEMAIL and ADMINPAGER are replaced in notification commands
108. **EMACROS_SEMICOLON**: Macros with a semicolon are used even if they contain a semicolon.
109. **EMTI0**: Verify multiple inheritance host
110. **ENGINE_MANY_CHECKS**: 
     * **GIVEN** a engine with many services and a unique check on each service with it's own env variables
     We expect correct check result in logs and we checks returned args and service macros
111. **EPC1**: Check with perl connector
112. **ERL**: Engine is started and writes logs in centengine.log. Then we remove the log file. The file disappears but Engine is still writing into it. Engine is reloaded and the centengine.log should appear again.
113. **ESGI0**: Verify servicegroup inheritance : servicegroup(empty) inherit from template (full) , on Start Engine
114. **ESGI1**: Verify servicegroup inheritance : servicegroup(empty) inherit from template (full) , on Start Engine
115. **ESGI2**: Verify servicegroup inheritance : servicegroup(empty) inherit from template (full) , on Reload Engine
116. **ESGI3**: Verify servicegroup inheritance : servicegroup(empty) inherit from template (full) , on Reload Engine
117. **ESI0**: Verify inheritance service : Service(empty) inherit from template (full) , on Start Engine
118. **ESI1**: Verify inheritance service : Service(full) inherit from template (full) , on Start Engine
119. **ESI2**: Verify inheritance service : Service(empty) inherit from template (full) , on Reload Engine
120. **ESI3**: Verify inheritance service : Service(full) inherit from template (full) , on Reload Engine
121. **ESS1**: Start-Stop (0s between start/stop) 5 times one instance of engine and no coredump
122. **ESS2**: Start-Stop (300ms between start/stop) 5 times one instance of engine and no coredump
123. **ESS3**: Start-Stop (0s between start/stop) 5 times three instances of engine and no coredump
124. **ESS4**: Start-Stop (300ms between start/stop) 5 times three instances of engine and no coredump
125. **ESSCTO**: **SCENARIO:** Engine services timeout due to missing Perl connector

     * **GIVEN** the Engine is configured as usual without the Perl connector
     * **WHEN** the Engine executes its service commands
     * **THEN** the commands take too long and reach the timeout
     * **AND** the Engine starts and stops two times as a result
126. **ESSCTOWC**: **SCENARIO:** Engine services timeout due to missing Perl connector

     * **GIVEN** the Engine is configured as usual with some command using the Perl connector
     * **WHEN** the Engine executes its service commands
     * **THEN** the commands take too long and reach the timeout
     * **AND** the Engine starts and stops two times as a result
127. **ESSOCWNV**: **SCENARIO:** Engine is started with a valid old configuration (concerning cbmod)

     * **GIVEN** the Engine is configured with a valid old configuration
     * **WHEN** the Engine is started
     * **THEN** the Engine starts correctly
     * **AND** the Engine stops correctly
128. **ESS_STATS**: **SCENARIO:** Reading the stats file after Engine has started

     * **GIVEN** the Engine is started
     * **WHEN** we read the Engine's stats file
     * **THEN** Engine must not crash
129. **EVOCWNV**: **SCENARIO:** The new Engine checks the old configuration (concerning cbmod)

     * **GIVEN** the Engine is configured with a valid old configuration
     * **WHEN** the Engine is started to check the configuration
     * **THEN** the Engine reads it as expected
130. **EXT_CONF1**: Engine configuration is overidden by json conf
131. **EXT_CONF2**: Engine configuration is overidden by json conf after reload
132. **E_FD_LIMIT**: Engine here is started with a low file descriptor limit. The engine should not crash and limit should be set.
133. **E_HOST_DOWN_DISABLE_SERVICE_CHECKS**: host_down_disable_service_checks is set to 1, host down switch all services to UNKNOWN
134. **E_HOST_UNREACHABLE_DISABLE_SERVICE_CHECKS**: host_down_disable_service_checks is set to 1, host unreachable switch all services to UNKNOWN
135. **VERIF**: 
     * **WHEN** centengine is started in verification mode, it does not log in its file.
136. **VERIFY_CONF**: Scenario Verify deprecated engine configuration options are logged as warnings Given the engine and broker are configured with module 1 And the engine configuration is set with deprecated options When the engine is started Then a warning message for 'auto_reschedule_checks' should be logged And a warning message for 'auto_rescheduling_interval' should be logged And a warning message for 'auto_rescheduling_window' should be logged And the engine should be stopped

### Severities
1. **BECSEV1**: **FEATURE:** Severity Management between Engine and Broker
     As a Centreon administrator
     I want to configure severities in Engine
     So that Broker stores them correctly in centreon_storage.severities table

     **BACKGROUND:**

     * **GIVEN** Engine is configured with centralized setup
     * **AND** Broker components (central, rrd, module) are configured
     * **AND** Database logging is enabled with debug/trace level
     * **AND** Retention data is cleared

     **SCENARIO:** Initial severity configuration

     * **GIVEN** Engine is configured with 20 severities
     * **WHEN** Broker and Engine are started
     * **THEN** 20 severities should be added/modified in logs
     * **AND** INSERT statements should be executed in severities table
     * **AND** Configuration file should match database content
     * **AND** Severity IDs should be consistent

     **SCENARIO:** Severity configuration modification

     * **GIVEN** Initial configuration with 20 severities is loaded
     * **WHEN** Configuration is modified to 30 severities
     * **AND** Engine configuration change is notified
     * **THEN** 10 additional severities should be added/modified
     * **AND** Configuration file should still match database content
     * **AND** Severity IDs should remain consistent

     **SCENARIO:** Severity configuration reduction

     * **GIVEN** Configuration with 30 severities is loaded
     * **WHEN** Configuration is reduced to 11 severities starting at ID 50
     * **AND** Engine configuration change is notified
     * **THEN** 11 severities should be present in final configuration
     * **AND** Unused severities should be implicitly removed
     * **AND** Configuration file should match database content
     * **AND** Severity IDs should be consistent with new range
2. **BECSEV2**: **SCENARIO:** Severity db_ids correctly restored after broker restart

     * **GIVEN** broker and engine are started with 20 severities configured on poller 1
     * **AND** services 1 to 4 are linked to severity 11
     * **AND** severities are correctly inserted in DB with non-zero db_ids in broker cache
     * **WHEN** broker is restarted while engine keeps running
     * **AND** the poller reconnects so the config is reprocessed against an already populated DB
     * **THEN** severity db_ids should still be non-zero in broker cache
     * **AND** services should still have correct severity_id in the resources table
3. **BECSEV3**: **SCENARIO:** Severity db_ids correctly restored after broker restart with lost prot files

     * **GIVEN** broker and engine are started with 20 severities configured on poller 1
     * **AND** services 1 to 4 are linked to severity 11
     * **AND** severities are correctly inserted in DB with non-zero db_ids in broker cache
     * **WHEN** broker is restarted after losing its prot files (simulating a fresh broker with existing DB)
     * **AND** engine sends its full configuration back (DiffState unknown path)
     * **THEN** _add_severities_mariadb is called with all-duplicate rows (ON DUPLICATE KEY UPDATE)
     * **AND** LAST_INSERT_ID() returns 0 for all rows, potentially overwriting db_ids in cache with 0
     * **THEN** severity db_ids should still be non-zero in broker cache
     * **AND** services should still have correct severity_id in the resources table
4. **BECSEV4**: **FEATURE:** Severity presence in Broker gRPC cache with centralized configuration

     **BACKGROUND:**

     * **GIVEN** 4 pollers are configured with 5 hosts each (20 total) and 20 services per host
     * **AND** each poller defines 2 severities: id=1/SERVICE/level=1 and id=2/HOST/level=2
     * **AND** severity 1 is assigned to all services, severity 2 to all hosts
     * **AND** Broker and Engine are started in centralized (BBDO3) mode

     **SCENARIO:** Phase 1 — All pollers active
     * **THEN** the broker gRPC cache contains severity (1, SERVICE, level=1)
     * **AND** the broker gRPC cache contains severity (2, HOST, level=2)

     **SCENARIO:** Phase 2 — Severity removed from poller 3
     * **WHEN** severities are removed from poller 3 and broker is notified
     * **THEN** the broker cache STILL contains severity (1, SERVICE) (pollers 0-2 have it)
     * **AND** the broker cache STILL contains severity (2, HOST)

     **SCENARIO:** Phase 3 — Severity removed from poller 2
     * **WHEN** severities are removed from poller 2 and broker is notified
     * **THEN** the broker cache STILL contains severity (1, SERVICE) (pollers 0-1 have it)
     * **AND** the broker cache STILL contains severity (2, HOST)

     **SCENARIO:** Phase 4 — Severity removed from all remaining pollers
     * **WHEN** severities are removed from pollers 0 and 1 and broker is notified
     * **THEN** the broker cache contains 0 severities

     **SCENARIO:** Phase 5 — Severity cache is empty (no orphan entries)
     * **THEN** GetSeverities returns an empty list
5. **BECSEV5**: **FEATURE:** Severity level change is reflected in the Broker gRPC cache

     **BACKGROUND:**

     * **GIVEN** 4 pollers configured with 5 hosts each (20 total) and 20 services per host
     * **AND** each poller defines severity id=1 (SERVICE, level=1) and id=2 (HOST, level=2)
     * **AND** severities assigned to all hosts and services

     **SCENARIO:** Severity levels are updated in the broker cache after modification

     * **GIVEN** the initial state has severity (1, SERVICE, level=1) and (2, HOST, level=2)
     * **WHEN** all 4 pollers update their severities to level=4 (SERVICE) and level=5 (HOST)
     * **THEN** broker GetSeverities returns (1, SERVICE, level=4) and (2, HOST, level=5)
6. **BECSEV6**: **FEATURE:** GetSeverities gRPC returns correct content while severities are active

     **BACKGROUND:**

     * **GIVEN** 4 pollers configured with 5 hosts each (20 total) and 20 services per host
     * **AND** each poller defines severity id=1 (SERVICE, level=1) and id=2 (HOST, level=2)
     * **AND** severities assigned to all hosts and services

     **SCENARIO:** GetSeverities returns 2 entries with correct metadata when all pollers are active
     * **WHEN** Broker and Engine are started and synchronized
     * **THEN** GetSeverities returns exactly 2 entries
     * **AND** severity (1, SERVICE, level=1) is present
     * **AND** severity (2, HOST, level=2) is present
7. **BECSEV7**: **FEATURE:** Broker cache is repopulated after broker restart with severities active

     **BACKGROUND:**

     * **GIVEN** 4 pollers configured with 5 hosts each (20 total) and 20 services per host
     * **AND** each poller defines severity id=1 (SERVICE, level=1) and id=2 (HOST, level=2)
     * **AND** severities assigned to all hosts and services

     **SCENARIO:** After broker restart, GetSeverities returns correct data

     * **GIVEN** broker and engine are started and synchronized
     * **AND** GetSeverities returns 2 entries before broker stops
     * **WHEN** broker is stopped and restarted (engine keeps running)
     * **THEN** GetSeverities returns the same 2 entries after restart
8. **BESEV1**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Broker is started before.
9. **BESEV2**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Engine is started before.
10. **BETUHSEV1**: Hosts have severities provided by templates.
11. **BETUSEV1**: Services have severities provided by templates.
12. **BEUHSEV1**: Four hosts have a severity added. Then we remove the severity from host 1. Then we change severity 10 to severity8 for host 3.
13. **BEUHSEV2**: Seven hosts are configured with a severity on two pollers. Then we remove severities from the first and second hosts of the first poller but only the severity from the first host of the second poller.
14. **BEUSEV1**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Broker is started before.
15. **BEUSEV2**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Engine is started before.
16. **BEUSEV3**: Four services have a severity added. Then we remove the severity from service 1. Then we change severity 11 to severity7 for service 3.
17. **BEUSEV4**: Seven services are configured with a severity on two pollers. Then we remove severities from the first and second services of the first poller but only the severity from the first service of the second poller. Then only severities no more used should be removed from the database.
18. **CBESEV1**: **SCENARIO:** Severities stored in database when Broker starts first (centralized)

     * **GIVEN** Engine is configured with centralized setup and 20 severities
     * **AND** Broker components (central, rrd, module) are configured
     * **AND** retention data is cleared
     * **WHEN** Broker is started before Engine
     * **THEN** severity20 should be of level 5 with icon_id 1
     * **AND** severity1 should be of level 1 with icon_id 5
19. **CBESEV2**: **SCENARIO:** Severities stored in database when Engine starts first (centralized)

     * **GIVEN** Engine is configured with centralized setup and 20 severities
     * **AND** Broker components (central, rrd, module) are configured
     * **AND** retention data is cleared
     * **WHEN** Engine is started before Broker
     * **THEN** severity20 should be of level 5 with icon_id 1
     * **AND** severity1 should be of level 1 with icon_id 5
20. **CBETUHSEV1**: 
     * **GIVEN** hosts on two pollers using templates that define severities
     (template_1: severity 2 on poller 0, severity 6 on poller 1;
     template_2: severity 4 on poller 0, severity 10 on poller 1),
     * **WHEN** the engine and broker are started with centralized configuration,
     * **THEN** host 2 and host 4 should have severity_id=2
     * **AND** host 5 should have severity_id=4
     * **AND** host 31 should have severity_id=6
     * **AND** host 33 should have severity_id=10.
21. **CBETUSEV1**: **SCENARIO:** Service severities inherited from templates via unified SQL (centralized)

     * **GIVEN** Engine is configured with centralized setup across 2 pollers and 20 severities each
     * **AND** service templates with severity assignments are configured
     * **AND** Broker is configured with unified SQL output and BBDO3
     * **WHEN** Engine and Broker are started
     * **THEN** services inheriting template_1 on poller 1 should have severity_id=1
     * **AND** services inheriting template_2 on poller 1 should have severity_id=3
     * **AND** services inheriting template_1 on poller 2 should have severity_id=3
     * **AND** services inheriting template_2 on poller 2 should have severity_id=5
22. **CBEUHSEV1**: 
     * **GIVEN** four hosts with a severity added,
     * **WHEN** we remove the severity from host 1
     * **AND** we change severity 10 to severity 8 for host 3,
     * **THEN** host 2 should still have severity_id=10
     * **AND** host 4 should still have severity_id=10
     * **AND** host 3 should have severity_id=8
     * **AND** host 1 should have no severity.
23. **CBEUHSEV2**: 
     * **GIVEN** seven hosts configured with severities on two pollers,
     * **WHEN** we remove severities from hosts on the first poller
     * **AND** we change host 28's severity from 16 to 14 on the second poller,
     * **THEN** host 26 should still have severity_id=18
     * **AND** host 27 should still have severity_id=18
     * **AND** host 28 should have severity_id=14
     * **AND** hosts 3, 4 and 5 on the first poller should have no severity.
24. **CBEUSEV1**: **SCENARIO:** Severities stored via unified SQL when Broker starts first (centralized)

     * **GIVEN** Engine is configured with centralized setup and 20 severities
     * **AND** Broker is configured with unified SQL output and BBDO3
     * **AND** retention data is cleared
     * **WHEN** Broker is started before Engine
     * **THEN** severity20 should be of level 5 with icon_id 1
     * **AND** severity1 should be of level 1 with icon_id 5
25. **CBEUSEV2**: **SCENARIO:** Severities stored via unified SQL when Engine starts first (centralized)

     * **GIVEN** Engine is configured with centralized setup and 20 severities
     * **AND** Broker is configured with unified SQL output and BBDO3
     * **AND** retention data is cleared
     * **WHEN** Engine is started before Broker
     * **THEN** severity20 should be of level 5 with icon_id 1
     * **AND** severity1 should be of level 1 with icon_id 5
26. **CBEUSEV3**: **SCENARIO:** Service severity removal and change via unified SQL (centralized)

     * **GIVEN** Engine is configured with centralized setup and 20 severities
     * **AND** Broker is configured with unified SQL output and BBDO3
     * **AND** severity 11 is assigned to services 1, 2, 3 and 4
     * **WHEN** Engine and Broker are started
     * **THEN** service (1, 1) should have severity_id=11
     * **WHEN** severity is removed from all services and reassigned (11 to services 2,4 and 7 to service 3)
     * **AND** Engine and Broker are reloaded
     * **THEN** service (1, 3) should have severity_id=7
     * **AND** service (1, 1) should have no severity
27. **CBEUSEV4**: **SCENARIO:** Severity removal across two pollers via unified SQL (centralized)

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
12. **CBAEBC**: **SCENARIO:** AES256 decryption of non-encrypted content returns an error

     * **GIVEN** broker is started in centralized mode
     * **WHEN** AES256 decryption is attempted on content that is not properly encrypted
     * **THEN** broker returns an error indicating the content is not AES256 encrypted
13. **CBAEBS**: **SCENARIO:** AES256 encryption with invalid base64 salt returns an error

     * **GIVEN** broker is started in centralized mode
     * **WHEN** AES256 encryption is attempted with a non-base64 salt
     * **THEN** broker returns an error about illegal base64 characters
14. **CBAEOK**: **SCENARIO:** AES256 encrypt then decrypt returns the original content

     * **GIVEN** broker is started in centralized mode
     * **WHEN** AES256 encryption is applied to content
     * **THEN** the decrypted result matches the original content
15. **CBASV**: **SCENARIO:** Broker with vault configured but vault server down logs an error

     * **GIVEN** broker is started in centralized mode
     * **AND** the vault server is not running
     * **WHEN** broker is configured to retrieve credentials from the vault
     * **THEN** broker logs an error about the inactive http server
16. **CBAV**: **SCENARIO:** Broker retrieves database password from a running vault

     * **GIVEN** broker is started in centralized mode
     * **AND** a vault is running with valid credentials
     * **WHEN** broker is configured to retrieve the database password from the vault
     * **THEN** broker logs that the database password was retrieved from vault
17. **CBWVC1**: **SCENARIO:** Broker with missing vault env file logs an error

     * **GIVEN** broker configured with a wrong vault configuration and no env file
     * **WHEN** broker starts
     * **THEN** broker logs an error that the env file could not be opened
18. **CBWVC2**: **SCENARIO:** Broker with env file missing APP_SECRET logs an error

     * **GIVEN** broker configured with a wrong vault configuration
     * **AND** an env file with invalid content (no APP_SECRET)
     * **WHEN** broker starts
     * **THEN** broker logs an error about missing APP_SECRET
19. **CBWVC3**: **SCENARIO:** Broker with wrong vault file path logs a JSON parse error

     * **GIVEN** broker configured with a strange APP_SECRET and a non-existent vault file
     * **WHEN** broker starts
     * **THEN** broker logs an error about the wrong vault file
20. **CBWVC4**: **SCENARIO:** Broker with malformed vault JSON file logs an error

     * **GIVEN** broker configured with a strange APP_SECRET and a vault file missing required keys
     * **WHEN** broker starts
     * **THEN** broker logs an error about the malformed vault file
21. **CBWVC5**: **SCENARIO:** Broker with non-string salt in vault file logs a type error

     * **GIVEN** broker configured with a strange APP_SECRET and a vault file with numeric salt
     * **WHEN** broker starts
     * **THEN** broker logs an error about the bad encryption type
22. **CBWVC6**: **SCENARIO:** Broker with non-base64 salt in vault file logs an encoding error

     * **GIVEN** broker configured with APP_SECRET and a vault file containing non-base64 salt
     * **WHEN** broker starts
     * **THEN** broker logs an error about the bad base64 encoding


812 tests currently implemented.
