# Centreon Tests

This sub-project contains functional tests for Centreon Broker, Engine and Connectors.
It is based on the [Robot Framework](https://robotframework.org/) with Python functions
we can find in the resources directory. The Python code is formatted using autopep8 and
robot files are formatted using `robottidy --overwrite tests`.

## Getting Started

To get this project, you have to clone centreon-collect.

These tests are executed from the `centreon-tests/robot` folder and uses the [Robot Framework](https://robotframework.org/).

From a Centreon host, you need to install Robot Framework

On AlmaLinux, the following commands should work to initialize your robot tests:

```bash
dnf install "Development Tools" dsniff python3-devel -y

pip3 install -U robotframework \
        robotframework-databaselibrary \
        robotframework-examples pymysql \
        robotframework-requests psutil \
        robotframework-httpctrl boto3 \
        GitPython unqlite py-cpuinfo


pip3 install grpcio grpcio_tools

#you need also to provide opentelemetry proto files at the project root with this command
git clone https://github.com/open-telemetry/opentelemetry-proto.git opentelemetry-proto

#Then you must have something like that:
#root directory/bbdo
#              /broker
#              /engine
#              /opentelemetry-proto
#              /tests
```

We need some perl modules to run the tests, you can install them with the following command:

```bash
dnf install perl-HTTP-Daemon-SSL
dnf install perl-JSON
```

To work with gRPC, we also need to install some python modules.

On rpm based system, we have to install:
```
yum install python3-devel -y
```

On deb based system, we have to install:
```
apt-get install python3-dev
```

And then we can install the required python modules:
```
pip3 install grpcio grpcio_tools
```

Now it should be possible to initialize the tests with the following commands:

```bash
./init-proto.sh
./init-sql.sh
```

Then to run tests, you can use the following commands

```
robot .
```

And it is also possible to execute a specific test, for example:

```
robot broker/sql.robot
```
In order to execute bench tests (broker-engine/bench.robot), you need also to
install py-cpuinfo, cython, unqlite and boto3

pip3 install py-cpuinfo cython unqlite gitpython boto3

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
9. **BABOO_UNKNOWN**: 
      * Given a broker/engine running
      * we add simple boolean rule with 100% impact, we do a check on only one of the two services of the BA, 
      * Then boolean rule "and" stays in unknown state and must not apply impact on BA, Ba must be OK.
10. **BAM_CIRCULAR**: Scenario: A circular BA definition invalidates the BAs of the cycle and nothing else
      * Given a BA "test" of type "worst" on two services
      * And a BA "child1" that is a KPI of "test", and "test" that is a KPI of "child1"
      * And a BA "sane" of type "worst" on service_302, outside of the cycle
      * When the central cbd starts
      * Then the circular definition is reported once in its log, for BA "test"
      * And the comment of BA "test" in mod_bam carries the same message
      * And the error is not reported again: BAM keeps running instead of reconnecting
      * And BA "sane" follows service_302 as if nothing had happened
11. **BAM_CORRUPTED_REPORTING_BA_EVENTS**: 
      * Given broker with bam configured
      * we inject a bad mod_bam_reporting_ba_events record.
      * A ba status changed should set end_time of bad record
12. **BAM_KPI_STATE_SEEDED_FROM_GLOBAL_CACHE**: Scenario: after a Broker restart, BAM seeds the state of its KPI services from the Broker global cache
      * Given a BA of type "worst" on two services, in legacy configuration with BBDO 3
      * And one of its services is CRITICAL, so the BA is CRITICAL
      * When Engine is stopped, then Broker is stopped and BAM's own cache file is deleted
      * And Broker is started again, Engine still stopped so that no service definition is replayed
      * Then once the global cache is ready, BAM seeds its KPI services from it
      * And the BA is still CRITICAL
13. **BAM_RELOAD_ON_CBD_RELOAD**: Scenario: A Broker reload makes BAM read its configuration again
      * Given Broker with BAM configured
      * When Broker is reloaded
      * Then the BAM endpoint is kept, not created again
      * And BAM reads its configuration from the database again
14. **BAWORST**: With bbdo version 3.0.1, a BA of type 'worst' with two services is configured. We also check stats output
15. **BAWORST2**: a worst ba with a boolean kpi and a ba kpi
16. **BAWORST_ACK**: Scenario: Acknowledging a service acknowledges the BA, and removing it unacknowledges the BA
      * Given BBDO version is 3.0.1
      * And a Business Activity of type "worst" is configured with two services
      * When one of the services is acknowledged
      * Then the Business Activity is acknowledged
      * When the acknowledgement is removed from the service
      * Then the Business Activity is no longer acknowledged
      * We also check that we have no bam filter error
17. **BA_BOOL_KPI**: With bbdo version 3.0.1, a BA of type 'worst' with 1 boolean kpi
18. **BA_CHANGED**: Scenario: Replace Service KPI with Boolean Rule KPI in Worst-type BA
      * Given a BA of type "worst" is configured with one service KPI
      * When the service KPI is replaced by a boolean rule KPI
      * And Broker is reloaded
      * Then the BA is correctly updated with the new KPI configuration
19. **BA_DEACTIVATED_SERVICE**: Scenario: A KPI whose service is deactivated is dropped, the activation column saying so
      * Given a non centralized platform, where BAM builds its own host/service mapping from the database
      * And a BA of type "worst" with two service KPIs, service_302 and service_303
      * When service_303 is deactivated -- its row says so and the configuration does not carry it
      * Then BAM loads the mapping from the database
      * And the KPI of service_303 is dropped
      * And the BA still follows service_302
20. **BA_DISABLED**: create a disabled BA with timeperiods and reporting filter don't create error message
21. **BA_IMPACT_2KPI_SERVICES**: With bbdo version 3.0.1, a BA of type 'impact' with 2 serv, ba is critical only if the 2 services are critical
22. **BA_IMPACT_IMPACT**: 
      * Given a Business Activity (BA) of type "impact"
      * And it has two child BAs of type "impact"
      * And the first child has an impact of 90
      * And the second child has an impact of 10
      * When both child BAs are impacting
      * Then the parent BA should be "critical"
      * When both child BAs are not impacting
      * Then the parent BA should be "ok"
23. **BA_RATIO_NUMBER_BA_4_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
24. **BA_RATIO_NUMBER_BA_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 2 services and one ba with 1 service
25. **BA_RATIO_PERCENT_BA_4_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
26. **BA_RATIO_PERCENT_BA_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio percent' with 2 serv an 1 ba with one service
27. **BA_RELOAD_KEEPS_KPI**: Scenario: A reload keeps the KPIs whose configuration did not change
      * Given a BA of type "worst" with one service KPI, service_302
      * And service_302 is CRITICAL, so the BA is CRITICAL
      * When Broker is reloaded, the KPI state in the database having moved but not its configuration
      * Then the KPI is neither removed nor created again
      * And the BA is still CRITICAL
      * And no KPI event was added to the reporting history by the reload
28. **BA_SERVICE_PNAME_AFTER_RELOAD**: Scenario: Verify that the parent_name of a BA service is not erased after a broker reload
      * Given a BA "test" of type "worst" with its service "host_16:service_302"
      * When I start broker and engine
      * Then the BA service "test" should have a status of 0 within 30 seconds
      * When I reload the broker
      * Then the database should still contain a BA service with name "test" and parent_name "_Module_BAM_1"
29. **BEBAMIDT1**: 
      * Given a BA of type 'worst' with one service is configured
      * And The BA is in critical state due to its service
      * When a downtime is set on this service
      * Then an inherited downtime is set to the BA
      * When the downtime is removed from the service
      * Then the inherited downtime is deleted from the BA
30. **BEBAMIDT2**: 
      * Given a BA of type 'worst' with one service is configured
      * And the BA is in critical state due to its service
      * And a downtime is set on this service
      * Then an inherited downtime is set to the BA
      * When Engine is restarted
      * And Broker is restarted
      * Then both downtimes are still present with no duplicates
      * When the downtime is removed from the service
      * Then the inherited downtime is deleted
31. **BEBAMIDT3**: Scenario: An inherited downtime is lifted after a restart of Broker during which the KPI left downtime
      * Given a BA of type "worst" with one service, CRITICAL, so the BA is CRITICAL
      * And a downtime on the service, so the BA is in an inherited downtime
      * When Broker is stopped
      * And the downtime of the service is deleted while Broker is down
      * And Broker is started again
      * Then the inherited downtime of the BA is lifted, although Broker never saw the KPI leave downtime while it ran
      * Whether through the state restored from the database or through the downtime Engine replays on the virtual service
      * And no downtime remains
32. **BEBAMIDTU1**: 
      * Given BBDO version 3.0.1 is running
      * And a BA of type 'worst' with one service is configured
      * And The BA is in critical state due to its service
      * When a downtime is set on this service
      * Then an inherited downtime is set to the BA
      * When the downtime is removed from the service
      * Then the inherited downtime is deleted from the BA
33. **BEBAMIDTU2**: 
      * Given BBDO version 3.0.1 is in use
      * And a 'worst' type BA with one service is configured
      * And The BA is in critical state due to its service
      * When a downtime is set on this service
      * Then an inherited downtime is set to the BA
      * When Engine is restarted
      * And Broker is restarted
      * Then both downtimes are still present with no duplicates
      * When the downtime is removed from the service
      * Then the inherited downtime is deleted
34. **BEBAMIGNDT1**: A BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. A first downtime is cancelled, the BA is still OK, but when the second downtime is cancelled, the BA should be CRITICAL.
35. **BEBAMIGNDT2**: A BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. The first downtime reaches its end, the BA is still OK, but when the second downtime reaches its end, the BA should be CRITICAL.
36. **BEBAMIGNDTU1**: With bbdo version 3.0.1, a BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. A first downtime is cancelled, the BA is still OK, but when the second downtime is cancelled, the BA should be CRITICAL.
37. **BEBAMIGNDTU2**: 
      * Given BBDO version 3.0.1 is configured
      * And a BA of type "worst" with two services is set up
      * And the downtime policy on this BA is "Ignore the indicator in the calculation"
      * And the BA is in a critical state due to the second critical service
      * When two downtimes are applied to the second critical service
      * Then the BA state should be OK due to the policy on indicators
      * When the first downtime reaches its end
      * Then the BA state should still be OK
      * When the second downtime reaches its end
      * Then the BA should be in a critical state
38. **BECBAMBRKIDT1**: 
      * Given BBDO3 / centralized config with notification_mode = broker
      * And a 'worst' BA with one service in critical state
      * And a downtime scheduled on the service via Broker gRPC sets an inherited downtime on the BA
      * When the KPI service recovers (becomes OK) while still under downtime
      * Then BAM removes the inherited downtime from the BA via the Broker downtime_manager
      * (the inherited downtime removal is driven by BAM state recomputation, not by a gRPC delete)
39. **BECBAMBRKIDT2**: 
      * Given BBDO3 / centralized config with notification_mode = broker
      * And a 'worst' BA with one service in critical state
      * And a downtime scheduled on the service via Broker gRPC sets an inherited downtime on the BA
      * When Engine is restarted (Broker stays up and remains the downtime authority)
      * Then both the KPI downtime and the inherited downtime are still present
      * (Engine, being aware that Broker owns downtimes, does not reset the depth on reload)
40. **BECBAMBRKIDT3**: 
      * Given BBDO3 / centralized config with notification_mode = broker
      * And a 'worst' BA with one service in critical state
      * And the BA is in critical state because of its service
      * When a downtime is scheduled on this service via Broker gRPC
      * Then Broker (not Engine) sets an inherited downtime on the BA virtual service
      * When the downtime is removed from the service via Broker gRPC
      * Then the inherited downtime is removed from the BA
41. **BECBAMBRKIDT4**: 
      * Given BBDO3 / centralized config with notification_mode = broker
      * And a 'worst' BA with one service in critical state
      * And a downtime scheduled on the service via Broker gRPC sets an inherited downtime on the BA
      * When Broker is restarted (Engine stays up; Broker is the downtime authority)
      * Then the started downtimes (the KPI downtime and the inherited BA downtime) are
      * re-injected from the Broker cache and the scheduled_downtime_depth is restored to 1
      * (started downtimes survive a Broker restart; depth is re-derived idempotently)
42. **BECBAMIDTU1**: 
      * Given BBDO version 3.0.1 is running with centralized configuration enabled
      * And a BA of type 'worst' with one service is configured
      * And The BA is in critical state due to its service
      * When a downtime is set on this service
      * Then an inherited downtime is set to the BA
      * When the downtime is removed from the service
      * Then the inherited downtime is deleted from the BA
43. **BECBAMIDTU2**: 
      * Given BBDO version 3.0.1 is in use
      * And a 'worst' type BA with one service is configured
      * And The BA is in critical state due to its service
      * When a downtime is set on this service
      * Then an inherited downtime is set to the BA
      * When Engine is restarted
      * And Broker is restarted
      * Then both downtimes are still present with no duplicates
      * When the downtime is removed from the service
      * Then the inherited downtime is deleted
44. **BECBAMIDTU3**: Scenario: An inherited downtime is lifted after a restart of Broker during which the KPI downtime expired, Broker owning the downtimes
      * Given a centralized platform where Broker owns the downtimes
      * And a BA of type "worst" with one service, CRITICAL, so the BA is CRITICAL
      * And a short downtime on the service, so the BA is in an inherited downtime
      * When Broker is stopped long enough for the service downtime to expire
      * And Broker is started again
      * Then the downtime_manager tells the BA its inherited downtime is still there
      * And the BA lifts it, its KPI being out of downtime
      * And no downtime remains
45. **BECBAMIGNDTU1**: With bbdo version 3.0.1, a BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. A first downtime is cancelled, the BA is still OK, but when the second downtime is cancelled, the BA should be CRITICAL.
46. **BECBAMIGNDTU2**: 
      * Given BBDO version 3.0.1 is configured
      * And a BA of type "worst" with two services is set up
      * And the downtime policy on this BA is "Ignore the indicator in the calculation"
      * And the BA is in a critical state due to the second critical service
      * When two downtimes are applied to the second critical service
      * Then the BA state should be OK due to the policy on indicators
      * When the first downtime reaches its end
      * Then the BA state should still be OK
      * When the second downtime reaches its end
      * Then the BA should be in a critical state
47. **BECPB_BA_DURATION_EVENT**: use of pb_ba_duration_event message.
48. **BECPB_DIMENSION_BA_BV_RELATION_EVENT**: bbdo_version 3 use pb_dimension_ba_bv_relation_event message.
49. **BECPB_DIMENSION_BA_EVENT**: bbdo_version 3 use pb_dimension_ba_event message.
50. **BECPB_DIMENSION_BA_TIMEPERIOD_RELATION**: use of pb_dimension_ba_timeperiod_relation message.
51. **BECPB_DIMENSION_BV_EVENT**: bbdo_version 3 use pb_dimension_bv_event message.
52. **BECPB_DIMENSION_KPI_EVENT**: bbdo_version 3 use pb_dimension_kpi_event message.
53. **BECPB_DIMENSION_TIMEPERIOD**: use of pb_dimension_timeperiod message.
54. **BECPB_DIMENSION_TRUNCATE_TABLE**: use of pb_dimension_timeperiod message.
55. **BECPB_KPI_STATUS**: bbdo_version 3 use kpi_status message.
56. **BEPB_BA_DURATION_EVENT**: use of pb_ba_duration_event message.
57. **BEPB_DIMENSION_BA_BV_RELATION_EVENT**: bbdo_version 3 use pb_dimension_ba_bv_relation_event message.
58. **BEPB_DIMENSION_BA_EVENT**: bbdo_version 3 use pb_dimension_ba_event message.
59. **BEPB_DIMENSION_BA_TIMEPERIOD_RELATION**: use of pb_dimension_ba_timeperiod_relation message.
60. **BEPB_DIMENSION_BV_EVENT**: bbdo_version 3 use pb_dimension_bv_event message.
61. **BEPB_DIMENSION_KPI_EVENT**: bbdo_version 3 use pb_dimension_kpi_event message.
62. **BEPB_DIMENSION_TIMEPERIOD**: use of pb_dimension_timeperiod message.
63. **BEPB_DIMENSION_TRUNCATE_TABLE**: use of pb_dimension_timeperiod message.
64. **BEPB_KPI_STATUS**: bbdo_version 3 use kpi_status message.
65. **CBABEST_SERVICE_CRITICAL**: With bbdo version 3.0.1, a BA of type 'best' with 2 serv, ba is critical only if the 2 services are critical
66. **CBABOO**: Scenario: A "worst" BA and an impact BA with an OR boolean rule built on the same 2 services behave identically when a service becomes CRITICAL
      * Given a BA of type "worst" with service_302 and service_303 as KPIs
      * And a BA of type "impact" with a boolean rule "{service_302} IS CRITICAL OR {service_303} IS CRITICAL"
      * When service_302 becomes CRITICAL
      * Then both BAs are CRITICAL
      * When service_302 recovers to OK
      * Then both BAs return to OK
      * And this cycle is repeated 10 times
67. **CBABOOAND**: Scenario: An AND boolean rule evaluates to CRITICAL as soon as one operand is false, even when the other service is UNKNOWN
      * Given a BA of type "impact" with boolean rule "{service_302} IS OK AND {service_303} IS OK"
      * And service_303 is passive and starts UNKNOWN
      * When service_302 becomes CRITICAL
      * Then the BA is CRITICAL (AND short-circuits on the first false operand)
68. **CBABOOCOMPL**: Scenario: A BA with a complex AND/OR boolean rule over 20 services becomes OK only when at least one service in each AND group is OK
      * Given a BA of type "impact" with a rule of 10 AND groups, each requiring at least one of 2 services to be OK
      * When all 20 services are CRITICAL
      * Then the BA is CRITICAL
      * When odd-indexed services are set to OK one by one
      * Then the BA remains CRITICAL until all AND groups have at least one OK service
      * And the BA becomes OK once all AND groups are satisfied
69. **CBABOOCOMPL_RELOAD**: Scenario: A broker reload does not alter a complex boolean rule state
      * Given a BA of type "impact" with a complex AND/OR boolean rule over 20 services
      * And all 20 services are CRITICAL, then odd-indexed services 1-13 are set to OK
      * And the BA is still CRITICAL because even-indexed services remain CRITICAL
      * When broker is reloaded at each remaining step (services 15, 17, 19 set to OK one by one)
      * Then the BA state is identical before and after each broker reload
      * And the BA becomes OK once all AND groups are satisfied
70. **CBABOOCOMPL_RESTART**: Scenario: A broker restart does not alter a complex boolean rule state
      * Given a BA of type "impact" with a complex AND/OR boolean rule over 20 services
      * And all 20 services are CRITICAL, then odd-indexed services 1-13 are set to OK
      * And the BA is still CRITICAL because even-indexed services remain CRITICAL
      * When broker is restarted at each remaining step (services 15, 17, 19 set to OK one by one)
      * Then the BA state is identical before and after each broker restart
      * And the BA becomes OK once all AND groups are satisfied
71. **CBABOODEACTIVATEDSVC**: Scenario: A KPI whose service is deactivated is dropped once the global cache knows the poller
      * Given a centralized platform, where Broker answers the host/service questions from its global cache
      * And a BA of type "worst" with two service KPIs, service_302 and service_303
      * When the configuration is acknowledged, so that the cache holds the poller
      * And service_303 is then deactivated -- its row says so and the export no longer carries it
      * Then the cache no longer holds service_303
      * And on the next start, where the cache is filled from the stored configuration, the KPI of service_303 is dropped
      * And the KPI of service_302 is kept, the BA still following it
72. **CBABOOKPIKINDS**: Scenario: A BA keeps its three kinds of KPI when the host/service ids come from the global cache
      * Given a centralized platform, where Broker answers the host/service questions from its global cache
      * And a child BA of type "worst" built on service_314
      * And a parent BA of type "worst" holding one KPI of each kind: service_303, a boolean rule on service_302 and the child BA
      * When the three services are OK
      * Then the parent BA is OK
      * When service_314 alone becomes CRITICAL
      * Then the parent BA is CRITICAL, which its BA KPI alone can explain
      * When service_302 alone becomes CRITICAL
      * Then the parent BA is CRITICAL, which its boolean KPI alone can explain
      * When service_303 alone becomes CRITICAL
      * Then the parent BA is CRITICAL, which its service KPI alone can explain
73. **CBABOOOR**: Scenario: An OR boolean rule evaluates to CRITICAL as soon as one operand is true, even when the other service is UNKNOWN
      * Given a BA of type "impact" with boolean rule "{service_302} IS CRITICAL OR {service_303} IS CRITICAL"
      * And service_303 is passive and starts UNKNOWN
      * When service_302 becomes CRITICAL
      * Then the BA is CRITICAL (OR short-circuits on the first true operand)
74. **CBABOOORREL**: Scenario: Updating a boolean rule and reloading broker and engine takes effect correctly
      * Given a BA of type "impact" with boolean rule "{service_302} IS OK OR {service_303} IS OK"
      * When service_302 and service_303 are CRITICAL
      * Then the BA is CRITICAL
      * When the boolean rule is updated to "{service_302} IS OK OR {service_304} IS OK" and broker and engine are reloaded
      * And service_304 is OK
      * Then the BA is OK
      * When the boolean rule is restored to "{service_302} IS OK OR {service_303} IS OK" and broker and engine are reloaded
      * And service_302 and service_303 are CRITICAL
      * Then the BA is CRITICAL again
75. **CBAWORST**: Scenario: A BA of type "worst" reacts to KPI state changes and broker stats are valid after reload
      * Given BBDO version is 3.0.1
      * And a Business Activity of type "worst" is configured with two services
      * When all services are OK
      * Then the Business Activity is OK
      * When one service becomes UNKNOWN
      * Then the Business Activity is UNKNOWN
      * When that service becomes WARNING
      * Then the Business Activity is WARNING
      * When another service becomes CRITICAL
      * Then the Business Activity is CRITICAL
      * And broker stats show expected endpoints state
      * When broker and engine are reloaded
      * Then broker stats still show expected endpoints state
      * And the GetBa gRPC command returns a valid digraph output
76. **CBAWORST2**: Scenario: A BA of type "worst" with a boolean KPI and a child BA KPI reacts correctly to state changes
      * Given BBDO version is 3.0.1
      * And a Business Activity of type "worst" is configured with a boolean KPI and a child BA KPI
      * When all KPIs are in an OK state
      * Then the Business Activity is OK
      * When the boolean rule becomes CRITICAL
      * Then the Business Activity is CRITICAL
      * When the child BA also becomes CRITICAL
      * Then the Business Activity is still CRITICAL with both KPIs reported
      * When the boolean rule recovers to OK
      * Then the Business Activity remains CRITICAL due to the child BA KPI
77. **CBAWORST_ACK**: Scenario: Acknowledging a service acknowledges the BA, and removing it unacknowledges the BA
      * Given BBDO version is 3.0.1
      * And a Business Activity of type "worst" is configured with two services
      * When one of the services is acknowledged
      * Then the Business Activity is acknowledged
      * When the acknowledgement is removed from the service
      * Then the Business Activity is no longer acknowledged
78. **CBA_BOOL_KPI**: With bbdo version 3.0.1, a BA of type 'worst' with 1 boolean kpi
79. **CBA_CHANGED**: Scenario: Replace Service KPI with Boolean Rule KPI in Worst-type BA
      * Given a BA of type "worst" is configured with one service KPI
      * When the service KPI is replaced by a boolean rule KPI
      * And Broker is reloaded
      * Then the BA is correctly updated with the new KPI configuration
80. **CBA_CONF_PUSHED_NO_RELOAD**: Scenario: a BA created after Broker started is taken into account when its configuration is pushed, without any Broker reload
      * Given Broker and Engine are started in centralized mode with no BA
      * When a BA of type "worst" on two services is created and the Engine configuration is pushed through the .lck file
      * Then Broker asks the BAM endpoint to reload once the poller configuration is applied to the global cache
      * And the BA becomes OK without any reload of Broker
      * When one of its services becomes CRITICAL
      * Then the BA becomes CRITICAL
81. **CBA_DISABLED**: create a disabled BA with timeperiods and reporting filter don't create error message
82. **CBA_IMPACT_2KPI_SERVICES**: With bbdo version 3.0.1, a BA of type 'impact' with 2 serv, ba is critical only if the 2 services are critical
83. **CBA_IMPACT_IMPACT**: 
      * Given a Business Activity (BA) of type "impact"
      * And it has two child BAs of type "impact"
      * And the first child has an impact of 90
      * And the second child has an impact of 10
      * When both child BAs are impacting
      * Then the parent BA should be "critical"
      * When both child BAs are not impacting
      * Then the parent BA should be "ok"
84. **CBA_KPI_RUNTIME_RESTORED_FROM_HOSTS_SERVICES**: Scenario: after a Broker restart in centralized configuration, the reference output restores the resources runtime from the hosts and services tables into the global cache (store_in_resources is false), and BAM seeds its KPIs from it
      * Given a BA of type "worst" on two services, in centralized configuration
      * And one of its services is CRITICAL, so the BA is CRITICAL
      * When Engine is stopped, then Broker is stopped and BAM's own cache file is deleted
      * And Broker is started again, Engine still stopped so that no status is replayed
      * Then unified_sql, the reference of the global cache, restores the hosts and services runtime from the hosts and services tables into the cache
      * And once the cache is ready, BAM seeds its KPI services from it
      * And the BA is still CRITICAL
85. **CBA_KPI_RUNTIME_RESTORED_FROM_RESOURCES**: Scenario: after a Broker restart in centralized configuration, the reference output restores the resources runtime from the resources table into the global cache, and BAM seeds its KPIs from it
      * Given a BA of type "worst" on two services, in centralized configuration
      * And one of its services is CRITICAL, so the BA is CRITICAL
      * When Engine is stopped, then Broker is stopped and BAM's own cache file is deleted
      * And Broker is started again, Engine still stopped so that no status is replayed
      * Then unified_sql, the reference of the global cache, restores the hosts and services runtime from the resources table into the cache
      * And once the cache is ready, BAM seeds its KPI services from it
      * And the BA is still CRITICAL
86. **CBA_KPI_SERVICE_ADDED_LATER**: Scenario: a KPI whose service does not exist yet becomes active when the service is added to the configuration
      * Given a BA of type "worst" with two KPIs, one on service_314 and one on service_303
      * And service_303 is not in the Engine configuration when Broker starts, so BAM drops that KPI
      * When service_303 is added to the Engine configuration and pushed
      * Then Broker asks the BAM endpoint to reload once the diff is applied to the global cache
      * And a CRITICAL result on service_303 makes the BA CRITICAL, without any reload of Broker
87. **CBA_PROT_LOST_RESTART**: Scenario: a BA survives the loss of the stored poller configuration across a Broker restart
      * Given a BA of type "worst" on two services is OK
      * When Broker is stopped, its stored poller configuration (1.prot) is deleted and Broker is started again
      * Then BAM cannot resolve the KPI services when it opens, since the global cache is empty
      * And Engine sends its configuration back, Broker stores it, feeds the cache and asks the BAM endpoint to reload
      * And the BA reacts to a CRITICAL service without any reload of Broker
88. **CBA_RATIO_NUMBER_BA_4_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
89. **CBA_RATIO_NUMBER_BA_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 2 services and one ba with 1 service
90. **CBA_RATIO_PERCENT_BA_4_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
91. **CBA_RATIO_PERCENT_BA_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio percent' with 2 serv an 1 ba with one service
92. **CBA_SERVICE_PNAME_AFTER_RELOAD**: Scenario: Verify that the parent_name of a BA service is not erased after a broker reload
      * Given a BA "test" of type "worst" with its service "host_16:service_302"
      * When I start broker and engine
      * Then the BA service "test" should have a status of 0 within 30 seconds
      * When I reload the broker
      * Then the database should still contain a BA service with name "test" and parent_name "_Module_BAM_1"

### Broker
1. **BBDO_300_3100**: 
      * Given a central configured in bbdo 3.0.0 and a rrd cbd configured in 3.1.0, rrd cbd should operate in bbdo 3.0.0
      * Then we wait event of type 131074 (bbdo2 ack).
2. **BC1**: Central and RRD brokers are started. Then we check they are correctly connected. RRD broker is stopped. The connection is lost. Then RRD broker is started again. The connection is re-established. Central broker is stopped. The connection is lost. Then Central broker is started again. The connection is re-established.
3. **BCL1**: Starting broker with option '-s foobar' should return an error
4. **BCL2**: Starting broker with option '-s5' should work
5. **BCL3**: Starting broker with options '-D' should work and activate diagnose mode
6. **BCL4**: Starting broker with options '-s2' and '-D' should work.
7. **BCPC1**: Scenario: a valid centralized poller configuration passes the check
      * Given a centralized engine configuration for 1 poller
      * And broker is started so its gRPC server answers
      * When CheckPollerConfig is called on the poller configuration directory
      * Then ok is true and there is no ERROR diagnostic
8. **BCPC10**: Scenario: an object dropped from the configuration under check reads as undefined
      * Given poller 1 whose configuration has been ingested and acknowledged
      * When host_5 is removed from its configuration and a dependency still references it
      * And CheckPollerConfig is called on that directory
      * Then host_5 reads as defined nowhere, not as living on poller 1 itself
9. **BCPC2**: Scenario: a timeperiod excluding a non-existent one makes the check fail
      * Given a centralized engine configuration with an invalid timeperiod (exclude -> unknown)
      * When CheckPollerConfig is called on the poller configuration directory
      * Then ok is false and an ERROR diagnostic names the unresolved exclusion
10. **BCPC3**: Scenario: several invalid timeperiods are all reported (return shape)
      * Given a centralized engine configuration with two invalid timeperiods
      * When CheckPollerConfig is called on the poller configuration directory
      * Then ok is false, the response holds a list of {severity, message} diagnostics,
      * and both missing exclusions are reported as errors
11. **BCPC4**: Scenario: a contact with no host notification commands makes the check fail
      * Given a centralized engine configuration where contact U1 has no host_notification_commands
      * When CheckPollerConfig is called on the poller configuration directory
      * Then ok is false and an ERROR diagnostic reports the missing host notification commands
12. **BCPC5**: Scenario: a contact group with a non-existing member makes the check fail
      * Given a centralized engine configuration with a contact group referencing an undefined contact
      * When CheckPollerConfig is called on the poller configuration directory
      * Then ok is false and an ERROR diagnostic reports the missing contact
13. **BCPC6**: Scenario: a command defined twice under the same name makes the check fail
      * Given a centralized engine configuration with a command defined twice
      * When CheckPollerConfig is called on the poller configuration directory
      * Then ok is false and an ERROR diagnostic names the duplicated command
14. **BCPC7**: Scenario: a host group with a non-existing member makes the check fail
      * Given a centralized engine configuration with a host group referencing an undefined host
      * When CheckPollerConfig is called on the poller configuration directory
      * Then ok is false and an ERROR diagnostic names the missing host
15. **BCPC8**: Scenario: a service group with a non-existing member makes the check fail
      * Given a centralized engine configuration with a service group referencing an undefined service
      * When CheckPollerConfig is called on the poller configuration directory
      * Then ok is false and an ERROR diagnostic names the missing service
16. **BCPC9**: Scenario: a dependency on a host of another poller is named as such
      * Given two pollers whose configurations have been ingested and acknowledged
      * And a host dependency of poller 1 whose dependent host belongs to poller 2
      * When CheckPollerConfig is called on the configuration directory of poller 1
      * Then ok is false, as at ingestion: a dependency does not cross a poller boundary
      * And the ERROR names poller 2 instead of claiming the host is defined nowhere
17. **BDB1**: 
      * Given a broker with a wrong unified_sql db_host
      * When cbd starts
      * Then it should log an error about the connection
      * And it should not crash
18. **BDB2**: 
      * Given a broker with a wrong unified_sql db_password
      * When cbd starts
      * Then it should log an error about access denied
      * And it should not crash
19. **BDB3**: 
      * Given a broker with a correct unified_sql user password
      * When cbd starts
      * Then the connection to the database should be established
20. **BDBM1**: Feature: Broker and Engine Start/Stop with MariaDB Scenario: Start broker and engine, then start MariaDB with different connection counts Given the broker and engine are started When MariaDB is started after them And the broker is configured with connections_count set to 1 and 3 Then the connection to the database should be established for each configured connection
21. **BEDB1**: 
      * Given the broker and engine are started,
      * When MariaDB is started after them,
      * Then the connection to the database should be established
22. **BEDB2**: Feature: SQL Connections via gRPC API Scenario: Start broker and engine, stop MariaDB, then start it again Given the broker and engine are running When MariaDB is stopped and then started again Then the gRPC API should provide information about SQL connections
23. **BEDB3**: Feature: SQL Connections via gRPC API Scenario: Start broker and engine, then stop MariaDB and then start it again Given broker and engine are running When MariaDB is stopped and then started again Then the gRPC API should provide information about SQL connections
24. **BFC1**: Scenario: Start broker with valid and invalid filters on an output
25. **BFC2**: Scenario: Start broker with only invalid filters on an output
26. **BGRPCSS1**: Scenario: Two broker instances with grpc stream start and stop cleanly
      * Given central broker with grpc output and rrd broker with grpc input
      * When both brokers are started and stopped 5 times with 100ms interval in new generation mode
      * Then no coredump occurs and the connection is established each time
27. **BGRPCSS2**: Scenario: Single broker instance with grpc starts and stops 10 times with 300ms interval
      * Given central broker with grpc output
      * When the broker is started and stopped 10 times with 300ms interval in new generation mode
      * Then no coredump occurs
28. **BGRPCSS3**: Scenario: Single broker instance with grpc starts and stops 5 times with 100ms interval
      * Given central broker with grpc output
      * When the broker is started and stopped 5 times with 100ms interval in new generation mode
      * Then no coredump occurs
29. **BGRPCSS4**: Scenario: Single broker instance with grpc starts and stops 10 times with 1s interval
      * Given central broker with grpc output
      * When the broker is started and stopped 10 times with 1s interval in new generation mode
      * Then no coredump occurs
30. **BGRPCSS5**: Scenario: Reversed grpc acceptor with one_peer_retention_mode starts and stops without deadlock
      * Given central broker with grpc output in one_peer_retention_mode with no host configured
      * When the broker is started and stopped 5 times with 1s interval in new generation mode
      * Then no deadlock occurs
31. **BGRPCSSU1**: Scenario: Two broker instances with unified_sql and grpc stream start and stop cleanly
      * Given central broker with unified_sql and grpc output and rrd broker with grpc input
      * When both brokers are started and stopped 5 times with 100ms interval in new generation mode
      * Then no coredump occurs and the connection is established each time
32. **BGRPCSSU2**: Scenario: Single broker instance with unified_sql and grpc starts and stops 10 times with 300ms interval
      * Given central broker with unified_sql and grpc output
      * When the broker is started and stopped 10 times with 300ms interval in new generation mode
      * Then no coredump occurs
33. **BGRPCSSU3**: Scenario: Single broker instance with unified_sql and grpc starts and stops 5 times with 100ms interval
      * Given central broker with unified_sql and grpc output
      * When the broker is started and stopped 5 times with 100ms interval in new generation mode
      * Then no coredump occurs
34. **BGRPCSSU4**: Scenario: Single broker instance with unified_sql and grpc starts and stops 10 times with 1s interval
      * Given central broker with unified_sql and grpc output
      * When the broker is started and stopped 10 times with 1s interval in new generation mode
      * Then no coredump occurs
35. **BGRPCSSU5**: Scenario: Reversed grpc acceptor with unified_sql and one_peer_retention_mode starts and stops without deadlock
      * Given central broker with unified_sql and grpc output in one_peer_retention_mode
      * When the broker is started and stopped 5 times with 1s interval in new generation mode
      * Then no deadlock occurs
36. **BSCSS1**: Start-Stop two instances of broker and no coredump
37. **BSCSS2**: Start/Stop 10 times broker with 300ms interval and no coredump
38. **BSCSS3**: Start-Stop one instance of broker with tcp connection and no coredump
39. **BSCSS4**: Start/Stop 10 times broker with 1sec interval and no coredump
40. **BSCSSC1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is enabled on client side.
41. **BSCSSC2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is disabled on client side.
42. **BSCSSCG1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on client side.
43. **BSCSSCGRR1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on output side. Reversed connection with retention and grpc transport protocol.
44. **BSCSSCGRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on output side. Reversed connection with retention and grpc transport protocol.
45. **BSCSSCRR1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is enabled on client side. Connection reversed with retention.
46. **BSCSSCRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is disabled on client side. Connection reversed with retention.
47. **BSCSSG1**: Start-Stop two instances of broker and no coredump
48. **BSCSSG2**: Start/Stop 10 times broker with 300ms interval and no coredump
49. **BSCSSG3**: Start-Stop one instance of broker with grpc connection and no coredump
50. **BSCSSG4**: Start/Stop 10 times broker with 1sec interval and no coredump
51. **BSCSSGA1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. An authorization token is added on the server. Error messages are raised.
52. **BSCSSGA2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. An authorization token is added on the server and also on the client. All looks ok.
53. **BSCSSGA3**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. The authorization token is retrieved from the vault on the server side.
54. **BSCSSGRR1**: Start-Stop two instances of broker and no coredump, reversed and retention, with transport protocol grpc, start-stop 5 times.
55. **BSCSSK1**: Scenario: Client uses tcp but server expects grpc - connection fails
      * Given central broker is configured with a bbdo_server input using tcp on port 5669
      * And central broker is configured with a bbdo_client output using tcp to port 5670
      * And rrd broker is configured with a bbdo_server input using grpc on port 5670
      * When both brokers are started in new generation mode
      * Then an error is raised on the client side about corrupted data
56. **BSCSSK2**: Start-Stop two instances of broker, server configured with tcp and client with grpc. No connection established and error raised on client side.
57. **BSCSSP1**: Start-Stop two instances of broker and no coredump. The server contains a listen address
58. **BSCSSPRR1**: Start-Stop two instances of broker and no coredump. The server contains a listen address, reversed and retention. centreon-broker-master-rrd is then a failover.
59. **BSCSSR1**: Start-Stop two instances of broker and no coredump. Connection with bbdo_server/bbdo_client and reversed.
60. **BSCSSRR1**: Start-Stop two instances of broker and no coredump. Connection with bbdo_server/bbdo_client, reversed and retention. centreon-broker-master-rrd is then a failover.
61. **BSCSSRR2**: Start/Stop 10 times broker with 300ms interval and no coredump, reversed and retention. centreon-broker-master-rrd is then a failover.
62. **BSCSST1**: Start-Stop two instances of broker and no coredump. Encryption is enabled on client side.
63. **BSCSST2**: Start-Stop two instances of broker and no coredump. Encryption is enabled on client side.
64. **BSCSSTG1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. This is not sufficient, then an error is raised.
65. **BSCSSTG2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. It works with good certificates and keys.
66. **BSCSSTG3**: Start-Stop two instances of broker. The connection cannot be established if the server private key is missing and an error message explains this issue.
67. **BSCSSTGRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. It works with good certificates and keys. Reversed grpc connection with retention.
68. **BSCSSTRR1**: Start-Stop two instances of broker and no coredump. Encryption is enabled. transport protocol is tcp, reversed and retention.
69. **BSCSSTRR2**: Start-Stop two instances of broker and no coredump. Encryption is enabled.
70. **BSS1**: Start-Stop two instances of broker and no coredump
71. **BSS2**: Start/Stop 10 times broker with 300ms interval and no coredump
72. **BSS3**: Start-Stop one instance of broker 5 times and no coredump
73. **BSS4**: Start/Stop 10 times broker with 1sec interval and no coredump
74. **BSS5**: Start-Stop with reversed connection on TCP acceptor with only one instance and no deadlock
75. **BSSU1**: Start-Stop two instances of broker with BBDO3 and no coredump
76. **BSSU2**: Start/Stop 10 times broker (BBDO3) with 300ms interval and no coredump
77. **BSSU3**: Start-Stop one instance of broker (BBDO3) and no coredump
78. **BSSU4**: Start/Stop 10 times broker with 1sec interval and no coredump
79. **BSSU5**: Start-Stop with reversed connection on TCP acceptor with only one instance and no deadlock
80. **CBDBM1**: Scenario: Broker reconnects to MariaDB after startup with configurable connection count
      * Given the broker and engine are started in new generation mode before MariaDB
      * When MariaDB is started after them with connections_count set to 1 then 3
      * Then the broker reconnects with the configured number of connections each time
81. **CBEDB1**: Scenario: Broker connects to MariaDB when the database is started after it
      * Given the broker and engine are started in new generation mode
      * When MariaDB is started after them
      * Then the connection to the database should be established
82. **CBEDB2**: Feature: SQL Connections via gRPC API Scenario: Start broker and engine, stop MariaDB, then start it again Given the broker and engine are running When MariaDB is stopped and then started again Then the gRPC API should provide information about SQL connections
83. **CBEDB3**: Feature: SQL Connections via gRPC API Scenario: Start broker and engine, then stop MariaDB and then start it again Given broker and engine are running When MariaDB is stopped and then started again Then the gRPC API should provide information about SQL connections
84. **CBLBD**: Scenario: Broker starts with default logger levels when no loggers section is configured
      * Given central broker configured without a loggers section
      * When broker is started in new generation mode
      * Then the gRPC API reports the expected default log levels for all loggers
85. **CBLDIS1**: Scenario: Broker starts with core logs disabled - sql logs still produced
      * Given central broker configured with core logs 'disabled' and sql logs at debug level
      * When broker is started in new generation mode
      * Then sql log entries are produced
      * And no core log entries are produced
86. **CBLEC1**: Scenario: Core log level changed live from trace to debug via gRPC API
      * Given central broker started with core logs at trace level in new generation mode
      * When the core log level is changed to debug via the gRPC API
      * Then the gRPC API reports the new core log level as debug
87. **CBLEC2**: Scenario: Setting an invalid log level via gRPC API raises an error
      * Given central broker started with core logs at trace level in new generation mode
      * When the core log level is set to the invalid value 'foo' via the gRPC API
      * Then an error message about the unknown enum value is returned
88. **CBLEC3**: Scenario: Setting log level for a non-existent logger via gRPC API raises an error
      * Given central broker started with core logs at trace level in new generation mode
      * When the log level of the non-existent 'foo' logger is set via the gRPC API
      * Then an error message about the missing logger is returned
89. **CBSCSSK2**: Scenario: Client uses grpc but server expects tcp - connection fails
      * Given central broker is configured with a bbdo_server input using grpc on port 5669
      * And central broker is configured with a bbdo_client output using grpc to port 5670
      * And rrd broker is configured with a bbdo_server input using tcp on port 5670
      * When both brokers are started in new generation mode
      * Then an error is raised on the client side about invalid protocol header
90. **CBSS1**: Scenario: Two broker instances start and stop cleanly 5 times in new generation mode
      * Given central and rrd brokers configured in new generation mode
      * When both brokers are started and stopped 5 times immediately
      * Then no coredump occurs each time
91. **CBSS2**: Scenario: Single broker instance starts and stops 10 times with 300ms interval in new generation mode
      * Given central broker configured in new generation mode
      * When the broker is started and stopped 10 times with 300ms interval
      * Then no coredump occurs
92. **CBSS3**: Scenario: Single broker instance starts and stops 5 times immediately in new generation mode
      * Given central broker configured in new generation mode
      * When the broker is started and stopped 5 times immediately
      * Then no coredump occurs
93. **CBSS4**: Scenario: Single broker instance starts and stops 10 times with 1s interval in new generation mode
      * Given central broker configured in new generation mode
      * When the broker is started and stopped 10 times with 1s interval
      * Then no coredump occurs
94. **CBSS5**: Scenario: Reversed TCP connection with one_peer_retention_mode starts and stops without deadlock in new generation mode
      * Given central broker configured with one_peer_retention_mode and no host on the rrd output
      * When the broker is started and stopped 5 times with 1s interval in new generation mode
      * Then no deadlock occurs
95. **CBSS_CBD**: Scenario: Broker restart with unified_sql preserves non-null service and host states
      * Given broker and engine are started in new generation mode
      * And broker is then restarted
      * When services and hosts are queried from the database for 30 seconds
      * Then no service or host state is null
96. **CBSS_REFERENCE**: Scenario: in centralized configuration, the unified_sql output is the reference of the global cache by default
      * Given central and rrd brokers configured in new generation mode, without any "reference" key on the unified_sql output
      * When the central broker is started
      * Then the global cache logs that central-broker-unified-sql is its reference
      * And the unified_sql stream logs that it is the reference
97. **CBSS_REFERENCE_OPT_OUT**: Scenario: "reference": false on the unified_sql output keeps it from being the reference of the global cache
      * Given central and rrd brokers configured in new generation mode, with "reference": false on the unified_sql output
      * When the central broker is started
      * Then the global cache declares no reference
      * And the unified_sql stream logs that it is not the reference
98. **START_STOP_CBD**: restart cbd with unified_sql services state must not be null after restart

### Broker/database
1. **BDBTEPWD1**: 
      * Given a broker configured with unified_sql and an AES256-encrypted db_password
      * When broker starts with a valid engine-context.json containing the matching app_secret and salt
      * Then broker decrypts the password transparently and establishes a working connection to the database
2. **BDBTEPWD2**: 
      * Given a broker configured with unified_sql and a db_password encrypted with a wrong app_secret
      * When broker starts with an engine-context.json containing a different app_secret and salt
      * Then broker fails to decrypt the password, logs "No usable encrypted password" and does not connect to the database
3. **BDBTEPWD3**: 
      * Given a broker configured with unified_sql and a db_password prefixed with "encrypt::"
      * When broker starts without any engine-context.json file
      * Then broker logs "encrypted password but no decrypt enabled" and does not connect to the database
4. **BDBTSSLC1**: Verify broker establishes TLS connection to database when DB_SSL_ENABLED is true
5. **BDBTSSLC2**: Scenario: TLS without certificate verification on a database without TLS
      * Given a database server that does not support TLS
      * And a broker unified_sql output with db_ssl_enabled=true and db_ssl_verify_cert=false
      * When the broker connects to it over TCP
      * Then it reads the server greeting and refuses the connection before sending any credential
6. **BDBTSSLC3**: Scenario: TLS without certificate verification on a database without TLS, over the unix socket
      * Given a database server that does not support TLS
      * And a broker unified_sql output on localhost with db_ssl_enabled=true and db_ssl_verify_cert=false
      * When the broker connects to it through the unix socket
      * Then it refuses the unencrypted connection instead of silently using it
7. **BDBTSSLC4**: Scenario: TLS options given as JSON booleans, as Centreon Web generates them
      * Given a broker unified_sql output with db_ssl_enabled=true and db_ssl_verify_cert=false as JSON booleans
      * When the broker starts
      * Then it reads both options and configures the database connection with TLS
8. **DEDICATED_DB_CONNECTION_${nb_conn}_${store_in_data_bin}**: count database connection
9. **NetworkDBFail6**: GIVEN a Broker configured with 5 database connections WHEN the network connection to the database (port 3306) is disrupted for 60 seconds THEN Broker should lose database connectivity during the outage AND should resume normal operations after network restoration
10. **NetworkDBFail7**: GIVEN Broker is running with 5 database connections AND Engine is connected to Broker AND database queries are being executed successfully WHEN the network connection on port 3306 is repeatedly disrupted (6 cycles of 10s down / 10s up) THEN Broker should handle the intermittent network failures AND should acknowledge all events once the network is stable
11. **NetworkDBFail8**: GIVEN Broker with unified_sql and 3 database connections WHEN database network is blocked until failure detection THEN Broker should log database errors AND should recover and execute pending statements after network restoration
12. **NetworkDBFailU6**: GIVEN Broker is running with unified_sql and 5 database connections AND Engine is connected to Broker using BBDO3 protocol AND database queries are being executed successfully WHEN the network connection on port 3306 is blocked for 60 seconds THEN database operations should fail during the network outage AND Broker should recover and acknowledge events after network restoration
13. **NetworkDBFailU7**: GIVEN Broker is running with unified_sql and 5 database connections AND Engine is connected to Broker using BBDO3 protocol AND database queries are being executed successfully WHEN the network connection on port 3306 is repeatedly disrupted (6 cycles of 10s down / 10s up) THEN Broker should handle the intermittent network failures AND should acknowledge all events once the network is stable
14. **NetworkDBFailU8**: GIVEN Broker is running with unified_sql, BBDO3 protocol and 3 database connections AND Engine is connected to Broker AND database queries are being executed successfully WHEN the network connection on port 3306 is blocked indefinitely THEN Broker should detect the database failure and log appropriate errors AND WHEN the network is restored THEN Broker should reconnect and successfully execute pending statements
15. **NetworkDbFail1**: network failure test between broker and database (shutting down connection for 100ms)
16. **NetworkDbFail2**: network failure test between broker and database (shutting down connection for 1s)
17. **NetworkDbFail3**: network failure test between broker and database (shutting down connection for 10s)
18. **NetworkDbFail4**: network failure test between broker and database (shutting down connection for 30s)
19. **NetworkDbFail5**: network failure test between broker and database (shutting down connection for 60s)

### Broker/engine
1. **ANO_CFG_SENSITIVITY_SAVED**: cfg sensitivity saved in retention
2. **ANO_DT1**: downtime on dependent service is inherited by ano
3. **ANO_DT2**: 
      * Given a service and its AD,
      * when we delete downtime on dependent service, AD must not be in downtime anymore
4. **ANO_DT3**: delete downtime on anomaly don t delete dependent service one
5. **ANO_DT4**: Scenario: Removing downtime from service keeps it on anomaly detection
      * Given an anomaly detection is attached to a service
      * And a downtime is set on both the service and the anomaly detection
      * When the downtime is removed from the service
      * Then the downtime should still be present on the anomaly detection
6. **ANO_EXTCMD_SENSITIVITY_SAVED**: extcmd sensitivity saved in retention
7. **ANO_JSON_SENSITIVITY_NOT_SAVED**: json sensitivity not saved in retention
8. **ANO_NOFILE**: an anomaly detection without threshold file must be in unknown state
9. **ANO_NOFILE_VERIF_CONFIG_NO_ERROR**: An anomaly detection without threshold file doesn't display error on config check
10. **ANO_OUT_LOWER_THAN_LIMIT**: an anomaly detection with a perfdata lower than lower limit make a critical state
11. **ANO_OUT_UPPER_THAN_LIMIT**: an anomaly detection with a perfdata upper than upper limit make a critical state
12. **ANO_TOO_OLD_FILE**: An anomaly detection with an oldest threshold file must be in unknown state
13. **AOUTLU1**: an anomaly detection with a perfdata upper than upper limit make a critical state with bbdo 3
14. **BAM_STREAM_FILTER**: With bbdo version 3.0.1, a BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. we watch its events
15. **BEACK10**: Scenario: acknowledgements survive a Broker restart (cache persistence).
      * Given a BBDO3 configuration and an acknowledged critical service
      * When Broker is restarted while Engine keeps running
      * Then GetAcknowledgements still lists it (restored from the persisted cache, not re-sent by Engine)
      * When the service recovers
      * Then the restored acknowledgement is closed and leaves the cache.
16. **BEACK2**: Configuration is made with BBDO3. Engine has a critical service. An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to OK. And the acknowledgement in database is deleted.
17. **BEACK4**: Configuration is made with BBDO3. Engine has a critical service. An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The acknowledgement is removed and the comment in the comments table has its deletion_time column updated.
18. **BEACK6**: Configuration is made with BBDO3. Engine has a critical service. An external command is sent to acknowledge it ; the acknowledgement is sticky. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to WARNING. And the acknowledgement in database is still there.
19. **BEACK8**: Engine has a critical service. It is configured with BBDO 3. An external command is sent to acknowledge it ; the acknowledgement is normal. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to WARNING. And the acknowledgement in database is removed (not sticky).
20. **BEACK9**: Scenario: the Broker cache exposes acknowledgements through gRPC.
      * Given a BBDO3 configuration (unified_sql, so the cache is enabled)
      * When a critical service is acknowledged
      * Then the GetAcknowledgements gRPC endpoint lists it in the Broker cache
      * When the service recovers
      * Then the acknowledgement leaves the Broker cache (and is not re-ingested).
21. **BEACKBRK1**: Scenario: Service acknowledgement via Broker gRPC, cleared on recovery
      * Given a BBDO3 platform with notification_mode=broker and a CRITICAL HARD service
      * When the service is acknowledged through the Broker AcknowledgeServiceProblem RPC
      * Then the acknowledgement is stored in the acknowledgements table and in the Broker cache
      * And the resources table shows the service acknowledged, and a logs entry is written
      * When the service comes back OK
      * Then the acknowledgement comment is deleted and the resource is no longer acknowledged
22. **BEACKBRK2**: Scenario: Service acknowledgement removed via Broker gRPC
      * Given a CRITICAL HARD service acknowledged through Broker (notification_mode=broker)
      * When the acknowledgement is removed through the Broker RemoveServiceAcknowledgement RPC
      * Then both the acknowledgements and comments rows get their deletion_time
      * And the resource is no longer acknowledged
23. **BEACKBRK3**: Scenario: Sticky versus normal acknowledgement on a state change (Broker authority)
      * Given a CRITICAL HARD service with a normal acknowledgement set through Broker
      * When the service goes WARNING HARD
      * Then the normal acknowledgement is cleared
      * Given the WARNING service is then acknowledged sticky
      * When the service goes CRITICAL HARD
      * Then the sticky acknowledgement is kept
      * When the service comes back OK
      * Then the sticky acknowledgement is cleared
24. **BEACKBRK4**: Scenario: Host acknowledgement via Broker gRPC survives a Broker restart
      * Given a DOWN HARD host acknowledged through the Broker AcknowledgeHostProblem RPC
      * Then a host acknowledgement entry is written in the logs table
      * When Broker is restarted
      * Then the acknowledgement is still in the Broker cache and the host still acknowledged
      * And a new Engine status does not clear it
      * When Engine is restarted and resends its definitions
      * Then Broker restores the acknowledgement flag on the fresh host definition
      * When the acknowledgement is removed through the Broker RemoveHostAcknowledgement RPC
      * Then the host is no longer acknowledged
25. **BEACKBRK5**: Scenario: Acknowledgement drives the Broker notification decision
      * Given a service with a contact, in notification_mode=broker (BBDO3)
      * When the service goes CRITICAL HARD
      * Then Broker dispatches the CRITICAL notification
      * When the service is acknowledged through Broker with notify set
      * Then Broker dispatches the ACKNOWLEDGEMENT notification
      * And the next CRITICAL result is not notified because the problem is acknowledged
      * When the service comes back OK
      * Then Broker dispatches the RECOVERY notification
26. **BEACK_DT**: Scenario: a downtime must not remove an acknowledgement
      * Given a configuration with BBDO3 and a critical service that is acknowledged
      * When a downtime is scheduled on this service and starts
      * Then the acknowledgement must not be deleted
      * And the acknowledgements, services and resources tables must stay consistent.
27. **BEATOI11**: external command SEND_CUSTOM_HOST_NOTIFICATION with option_number=1 should work
28. **BEATOI12**: external command SEND_CUSTOM_HOST_NOTIFICATION with option_number>7 should fail
29. **BEATOI13**: external command Schedule Service Downtime with duration<0 should fail
30. **BEATOI21**: external command ADD_HOST_COMMENT and DEL_HOST_COMMENT should work
31. **BEATOI22**: external command DEL_HOST_COMMENT with comment_id<0 should fail
32. **BEATOI23**: external command ADD_SVC_COMMENT with persistent=0 should work
33. **BEBDRRD1**: A service is forced checked then a downtime is set on this service via Broker gRPC. The service is forced checked again and the downtime is removed. Then we should not get any error in cbd RRD of kind 'ignored update error in file...'.
34. **BEBDTHOSTFIXED**: Scenario: Host downtime via Broker gRPC (BBDO3)
      * Given a host downtime is scheduled via Broker gRPC
      * Then 21 downtimes appear in the database (1 host + 20 services)
      * When the host downtime is deleted
      * Then the database contains 0 downtimes
35. **BEBDTIM**: New services with several pollers are created. Then downtimes are set on all configured hosts via Broker gRPC. This results in 5250 downtimes (250 hosts × 21). Then all downtimes are removed.
36. **BEBDTMASS1**: Scenario: Mass downtime scheduling via Broker gRPC (BBDO3)
      * Given 3 pollers with 50 hosts and 20 services each
      * When host downtimes are scheduled via Broker gRPC on 50 hosts
      * Then 1050 downtimes appear in the database (1 host + 20 services each)
      * When all host downtimes are deleted via Broker gRPC
      * Then the database contains 0 downtimes
37. **BEBDTSVCFIXED**: Scenario: Single service downtime via Broker gRPC (BBDO3)
      * Given a service downtime is scheduled via Broker gRPC
      * Then 1 downtime appears in the database
      * When the downtime is deleted via Broker gRPC
      * Then the database contains 0 downtimes
38. **BEBDTSVCFIXED_CHECK_DEPTH**: Scenario: Service downtime depth via Broker gRPC (BBDO3)
      * Given a service downtime is scheduled via Broker gRPC
      * Then the service scheduled_downtime_depth is 1 in the database
      * When the downtime is deleted
      * Then the service scheduled_downtime_depth is 0
39. **BEBDTSVCREN**: Scenario: Service downtime survives service rename (Broker gRPC, BBDO3)
      * Given a service downtime is scheduled via Broker gRPC
      * When the service is renamed via Engine config reload
      * Then the downtime is still active (tracked by ID, not name)
      * When the downtime is deleted
      * Then the database contains 0 downtimes
40. **BECC1**: Broker/Engine communication with compression between central and poller
41. **BECMTBRK1**: Scenario: service comment added and deleted by id through Broker
      * Given a BBDO3 platform with notification_mode=broker
      * When a service comment is added through the Broker AddServiceComment RPC
      * Then an active user comment appears in the comments table with the returned internal_id
      * When the comment is deleted through the Broker DeleteComment RPC
      * Then the row gets a deletion_time
42. **BECMTBRK2**: Scenario: all comments of a host and of a service deleted through Broker
      * Given a BBDO3 platform with notification_mode=broker
      * And two host comments and two service comments added through Broker
      * When DeleteAllHostComments is called for the host
      * Then the host has no active comment left and the service still has two
      * When DeleteAllServiceComments is called for the service
      * Then the service has no active comment left
43. **BECMTBRK3**: Scenario: a comment minted by a poller cannot be deleted through Broker
      * Given a BBDO3 platform with notification_mode=broker
      * And a service comment added through the Engine external command (poller-minted id)
      * When DeleteComment is called on Broker with that id
      * Then Broker refuses with FAILED_PRECONDITION and the comment stays active
44. **BECMT_DEL_ALL**: Scenario: bulk deletion of comments
      * Given several comments on a host and on a service
      * When DEL_ALL_HOST_COMMENTS / DEL_ALL_SVC_COMMENTS are sent
      * Then every matching active comment gets a deletion_time
45. **BECMT_DEL_SVC**: Scenario: deleting a service comment by id
      * Given a service comment has been added by external command
      * When a DEL_SVC_COMMENT external command is sent with its internal_id
      * Then the matching row in the "comments" table gets a deletion_time
46. **BECMT_DOWNTIME**: Scenario: a downtime owns a comment
      * Given a fixed downtime is scheduled on a service
      * Then a downtime comment is created in the "comments" table
      * When the downtime is deleted
      * Then the downtime comment gets a deletion_time
47. **BECMT_FLAPPING**: Scenario: a flapping service owns a comment
      * Given flap detection is enabled on a service
      * When the service flaps
      * Then a flapping comment is created in the "comments" table
      * When the service state stabilizes and flapping stops
      * Then the flapping comment gets a deletion_time
48. **BECMT_RETENTION**: Scenario: a persistent comment survives an Engine restart
      * Given a persistent host comment
      * When Engine is restarted (retention preserved)
      * Then the comment is still active in the "comments" table
49. **BECMT_RETENTION_ACK**: Scenario: an acknowledgement comment is still deletable after a restart
      * Given a service is acknowledged (a non-persistent ack comment is created)
      * When Engine is restarted (retention preserved)
      * And the service goes back to OK so the acknowledgement is cleared
      * Then the ack comment is deleted, proving its id survived the restart on the notifier
50. **BECNTBRK1**: Scenario: contact service notifications switch and timeperiod through Broker
      * Given a service notifying contact John_Doe, in notification_mode=broker (BBDO3)
      * When John_Doe's service notifications are disabled through SetContactServiceNotifications
      * Then a CRITICAL HARD result is not notified (Broker filters the contact out)
      * When they are enabled again
      * Then the next CRITICAL result is notified by the poller
      * When John_Doe's service notification period is set to 'none' through SetContactServiceNotificationPeriod
      * Then the next CRITICAL result is not notified (contact outside its period)
      * When it is set back to '24x7'
      * Then the next CRITICAL result is notified
      * And an unknown timeperiod or contact is refused with NOT_FOUND
51. **BECNTBRK2**: Scenario: contactgroup switch through Broker survives a Broker restart and an Engine restart
      * Given a service notifying contactgroup_1 = {John_Doe}, in notification_mode=broker (BBDO3)
      * When SetContactgroupServiceNotifications(disabled) is called on contactgroup_1
      * Then a CRITICAL HARD result is not notified
      * When Broker is restarted, then Engine is restarted (it resends its configuration)
      * Then the switch is still off: the next CRITICAL result is not notified
      * When SetContactgroupServiceNotifications(enabled) is called
      * Then the next CRITICAL result is notified
52. **BECNTBRK3**: Scenario: service notification timeperiod through Broker
      * Given a service notifying John_Doe with notification_period 24x7, in notification_mode=broker (BBDO3)
      * When SetServiceNotificationPeriod(none) is called on service_1
      * Then services.notification_period is 'none' in the database
      * And a CRITICAL HARD result is not notified (service outside its period)
      * When Engine is restarted (it resends the configured period)
      * Then services.notification_period is still 'none': Broker restores it
      * When SetServiceNotificationPeriod(24x7) is called
      * Then services.notification_period is back to '24x7' and the next CRITICAL result is notified
53. **BECT1**: Broker/Engine communication with anonymous TLS between central and poller
54. **BECT2**: Broker/Engine communication with TLS between central and poller with key/cert
55. **BECT3**: Broker/Engine communication with anonymous TLS and ca certificate
56. **BECT4**: Broker/Engine communication with TLS between central and poller with key/cert and hostname forced
57. **BECT_GRPC1**: Broker/Engine communication with GRPC and with anonymous TLS between central and poller
58. **BECT_GRPC2**: Broker/Engine communication with TLS between central and poller with key/cert
59. **BECT_GRPC3**: Broker/Engine communication with anonymous TLS and ca certificate
60. **BECT_GRPC4**: Broker/Engine communication with TLS between central and poller with key/cert and hostname forced
61. **BECUSTOMHOSTVAR**: external command CHANGE_CUSTOM_HOST_VAR on SNMPVERSION
62. **BECUSTOMSVCVAR**: external command CHANGE_CUSTOM_SVC_VAR on CRITICAL
63. **BEDSVCDTDEL**: 
      * Given a configuration with BBDO3 and a unique downtime set on a service
      * When the downtime is finished
      * Then the downtime is well removed
      * And the number of downtimes is 0
64. **BEDTCANCELCLEAN**: 
      * Given a downtime running on a poller that is lost brutally
      * When the poller comes back without its retention
      * Then unified_sql cancels the orphaned downtime AND sets its actual_end_time
      * So that availability reporting does not consider it running forever.
65. **BEDTHOSTFIXED**: A downtime is set on a host, the total number of downtimes is really 21 (1 for the host and 20 for its 20 services) then we delete this downtime and the number is 0.
66. **BEDTHOSTFIXED1**: Scenario: Setting and Removing Downtime on a Host and its Services
      * Given a downtime is set on a host
      * Then the total number of downtimes is 21
      * And this includes 1 for the host and 20 for its services
      * When the downtime is deleted
      * Then the total number of downtimes is 0
67. **BEDTMASS1**: Scenario: Setting and Removing Downtimes on Configured Hosts and Services
      * Given new services with several pollers are created
      * When downtimes are set on all configured hosts
      * Then the total number of downtimes, including impacted services, is 1050
      * And all these downtimes are removed
      * And the test is performed with BBDO 3.0.0
68. **BEDTMASS2**: Scenario: Setting and Removing Downtimes on Configured Hosts and Services
      * Given new services with several pollers are created
      * When downtimes are set on all configured hosts
      * Then the total number of downtimes, including impacted services, is 1050
      * And all these downtimes are removed
      * And the test is performed with BBDO 2.0.0
69. **BEDTRRD1**: A service is forced checked then a downtime is set on this service. The service is forced checked again and the downtime is removed. This test is done with BBDO 3.0.0. Then we should not get any error in cbd RRD of kind 'ignored update error in file...'.
70. **BEDTSVCFIXED**: 
      * Given a unique downtime set on a service
      * When the downtime is removed
      * Then the downtime is well removed
      * And the number of downtimes is 0
71. **BEDTSVCFIXED1**: 
      * Given a configuration with BBDO3 and a unique downtime set on a service
      * When the downtime is removed
      * Then the downtime is well removed
      * And the number of downtimes is 0
72. **BEDTSVCREN1**: 
      * Given a downtime set on a service
      * When the service is renamed
      * Then the downtime is still active on the renamed service
      * When the downtime is removed from the renamed service
      * Then the downtime is well removed
73. **BEDTSVCREN2**: 
      * Given a configuration with BBDO3 and a downtime set on a service
      * When the service is renamed
      * Then the downtime is still active on the renamed service
      * When the downtime is removed from the renamed service
      * Then the downtime is well removed
74. **BEDW**: Scenario: Verify Broker configured with cache_config_directory listens to it
      * Given the Central Broker is started with cache_config_directory set to a specific Directory
      * And the pollers_config_directory is set to its default value: /var/lib/centreon-broker/pollers-configuration.
      * When a file of the form <poller_id>.lck is created in the cache_config_directory
      * Then Broker logs a message telling the file has been created
      * When the corresponding configuration directory doesn't exist
      * Then Broker logs a message telling the directory doesn't exist
75. **BEDWEN**: Scenario: Verify Broker configured with cache_config_directory listens to it
      * Given the Central Broker is started with cache_config_directory set to a specific Directory
      * And the pollers_config_directory is set (default value) to /var/lib/centreon-broker/pollers-configuration.
      * When a file of the form <poller_id>.lck is created in the cache_config_directory
      * Then Broker logs a message telling the file has been created
      * When the corresponding configuration directory doesn't exist
      * Then Broker logs a message telling the directory doesn't exist
76. **BEDWEND**: Scenario: Verify Broker configured with cache_config_directory creates the protobuf serialized configuration
      * Given Central Broker is started with cache_config_directory set to a specific Directory
      * And the pollers_config_directory is set (default value) to /var/lib/centreon-broker/pollers-configuration.
      * And Central Broker has already sent a first configuration to Engine
      * When a new configuration is put into the cache_config_directory
      * Then Engine should be notified about the new configuration by Broker
      * And Engine should update its configuration from a differential configuration
77. **BEDWENF**: Scenario: Verify Broker configured with cache_config_directory creates the protobuf serialized configuration
      * Given the Central Broker is started with cache_config_directory set to a specific Directory
      * And the pollers_config_directory is set (default value) to /var/lib/centreon-broker/pollers-configuration.
      * When the export is announced by a pollers.lck naming the poller whose directory is filled correctly
      * Then Broker logs a message telling the batch has been announced
      * And Broker dumps a file <poller_id>.prot in the pollers_conf directory
78. **BEEXTBRK1**: Scenario: forced checks and passive results reach the poller owning the host
      * Given two pollers in centralized configuration (host_1 on poller 1, host_26 on poller 2)
      * When SCHEDULE_FORCED_SVC_CHECK on host_26/service_501 is sent to Broker
      * Then poller 2 logs the external command and poller 1 does not
      * When SCHEDULE_FORCED_HOST_CHECK on host_1 is sent to Broker
      * Then poller 1 logs the external command
      * When PROCESS_SERVICE_CHECK_RESULT CRITICAL on host_26/service_501 is sent to Broker
      * Then the service becomes CRITICAL in the database
79. **BEEXTBRK2**: Scenario: Broker refuses what it cannot route, synchronously
      * Given one poller in centralized configuration and notification_mode=broker
      * When an unknown command, a command on an unknown host or service, a malformed timestamp are sent
      * Then each is refused with an explicit error and nothing reaches the poller
      * When a hostgroup command is sent, or a global command without poller, or on an unknown poller
      * Then each is refused with an explicit error
      * When a native command is malformed or names an unknown host
      * Then Broker, which executes it itself in notification_mode=broker, refuses it with the conversion or RPC error
80. **BEEXTBRK3**: Scenario: global, process and contact commands find their pollers
      * Given two pollers in centralized configuration
      * When DISABLE_EVENT_HANDLERS is sent to Broker for poller 2
      * Then poller 2 logs the command and poller 1 does not
      * When DISABLE_CONTACT_HOST_NOTIFICATIONS is sent to Broker
      * Then both pollers log it (every poller has its copy of the contacts)
      * When a service command is sent with stale names but Broker knows the ids
      * Then the poller executes it on the right service (names rewritten from the ids)
81. **BEEXTBRK4**: Scenario: in notification_mode=broker, Broker executes the legacy natives itself
      * Given a BBDO3 platform with notification_mode=broker and a CRITICAL HARD service
      * When ACKNOWLEDGE_SVC_PROBLEM is sent as a legacy line to ExecuteExternalCommand
      * Then the acknowledgement is stored as if AcknowledgeServiceProblem had been called
      * When SCHEDULE_SVC_DOWNTIME and then DEL_SVC_DOWNTIME are sent as legacy lines
      * Then the downtime appears and disappears in the database
      * When DISABLE_SVC_NOTIFICATIONS is sent as a legacy line
      * Then the service notifications are disabled in the Broker cache and the database
82. **BEEXTBRK_ADAPTIVE**: Scenario: a check toggle sent to a centralized poller reaches the database
      * Given one poller in centralized configuration, external commands routed by Broker
      * When DISABLE_HOST_CHECK on host_1 and DISABLE_SVC_CHECK on host_1/service_1 are sent to Broker
      * Then the poller applies them and sends the adaptive events back, so the hosts, services and resources tables show the checks disabled
      * When ENABLE_HOST_CHECK and ENABLE_SVC_CHECK are sent
      * Then the tables show the checks enabled again
83. **BEEXTBRK_ADAPTIVE_RESTART**: Scenario: a check toggle survives a restart of Broker and of Engine through the reference database
      * Given one poller in centralized configuration, Broker's unified_sql output being the reference of the global cache
      * When DISABLE_HOST_CHECK and CHANGE_HOST_CHECK_TIMEPERIOD (workhours) on host_1 and DISABLE_SVC_CHECK on host_1/service_1 are sent to Broker
      * Then the hosts and services tables carry the overrides and their modified_attributes bits
      * When Broker is restarted
      * Then unified_sql restores the overrides of one host and one service into the global cache from the hosts/services tables
      * When Engine is restarted, without retention.dat
      * Then the snapshot of its connection brings the overrides back: host_1 and service_1 have their active checks disabled with the MOD_ACTIVE_CHECKS_ENABLED bit, and host_1 checks during workhours with the MOD_CHECK_TIMEPERIOD bit
84. **BEEXTCMD1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 The gRPC iteration also checks that Engine logs the deprecation of its command RPC.
85. **BEEXTCMD10**: external command CHANGE_MAX_SVC_CHECK_ATTEMPTS on bbdo2.0
86. **BEEXTCMD11**: external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo3.0
87. **BEEXTCMD12**: external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo2.0
88. **BEEXTCMD13**: external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo3.0
89. **BEEXTCMD14**: external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo2.0
90. **BEEXTCMD15**: external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo3.0
91. **BEEXTCMD16**: external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo2.0
92. **BEEXTCMD17**: external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo3.0
93. **BEEXTCMD18**: external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo2.0
94. **BEEXTCMD19**: external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo3.0
95. **BEEXTCMD2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0
96. **BEEXTCMD20**: external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo2.0
97. **BEEXTCMD21**: external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS and ENABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo3.0
98. **BEEXTCMD22**: external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS and ENABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo2.0
99. **BEEXTCMD23**: 
      * Given Engine and broker configured with BBDO3
      * When the external command DISABLE_HOST_CHECK on host_1 is executed
      * Then the host_1 host checks should be disabled
      * When the external command ENABLE_HOST_CHECK on host_1 is executed
      * Then the host_1 host checks should be enabled
100. **BEEXTCMD24**: external command DISABLE_HOST_CHECK and ENABLE_HOST_CHECK on bbdo2.0
101. **BEEXTCMD25**: external command DISABLE_HOST_EVENT_HANDLER and ENABLE_HOST_EVENT_HANDLER on bbdo3.0
102. **BEEXTCMD26**: external command DISABLE_HOST_EVENT_HANDLER and ENABLE_HOST_EVENT_HANDLER on bbdo2.0
103. **BEEXTCMD27**: external command DISABLE_HOST_FLAP_DETECTION and ENABLE_HOST_FLAP_DETECTION on bbdo3.0
104. **BEEXTCMD28**: external command DISABLE_HOST_FLAP_DETECTION and ENABLE_HOST_FLAP_DETECTION on bbdo2.0
105. **BEEXTCMD29**: external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo3.0
106. **BEEXTCMD3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0
107. **BEEXTCMD30**: external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo2.0
108. **BEEXTCMD31**: external command DISABLE_HOST_SVC_CHECKS and ENABLE_HOST_SVC_CHECKS on bbdo3.0
109. **BEEXTCMD32**: external command DISABLE_HOST_SVC_CHECKS and ENABLE_HOST_SVC_CHECKS on bbdo2.0
110. **BEEXTCMD33**: external command DISABLE_HOST_SVC_NOTIFICATIONS and ENABLE_HOST_SVC_NOTIFICATIONS on bbdo3.0
111. **BEEXTCMD34**: external command DISABLE_HOST_SVC_NOTIFICATIONS and ENABLE_HOST_SVC_NOTIFICATIONS on bbdo2.0
112. **BEEXTCMD35**: external command DISABLE_PASSIVE_HOST_CHECKS and ENABLE_PASSIVE_HOST_CHECKS on bbdo3.0
113. **BEEXTCMD36**: external command DISABLE_PASSIVE_HOST_CHECKS and ENABLE_PASSIVE_HOST_CHECKS on bbdo2.0
114. **BEEXTCMD37**: external command DISABLE_PASSIVE_SVC_CHECKS and ENABLE_PASSIVE_SVC_CHECKS on bbdo3.0
115. **BEEXTCMD38**: external command DISABLE_PASSIVE_SVC_CHECKS and ENABLE_PASSIVE_SVC_CHECKS on bbdo2.0
116. **BEEXTCMD39**: external command START_OBSESSING_OVER_HOST and STOP_OBSESSING_OVER_HOST on bbdo3.0
117. **BEEXTCMD4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0
118. **BEEXTCMD40**: external command START_OBSESSING_OVER_HOST and STOP_OBSESSING_OVER_HOST on bbdo2.0
119. **BEEXTCMD41**: external command START_OBSESSING_OVER_SVC and STOP_OBSESSING_OVER_SVC on bbdo3.0
120. **BEEXTCMD42**: external command START_OBSESSING_OVER_SVC and STOP_OBSESSING_OVER_SVC on bbdo2.0
121. **BEEXTCMD5**: external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo3.0
122. **BEEXTCMD6**: external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo2.0
123. **BEEXTCMD7**: external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo3.0
124. **BEEXTCMD8**: external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo2.0
125. **BEEXTCMD9**: external command CHANGE_MAX_SVC_CHECK_ATTEMPTS with bbdo3.0
126. **BEEXTCMD_COMPRESS_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and compressed grpc
127. **BEEXTCMD_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and grpc
128. **BEEXTCMD_GRPC2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 and grpc
129. **BEEXTCMD_GRPC3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 and grpc
130. **BEEXTCMD_GRPC4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 and grpc
131. **BEEXTCMD_REVERSE_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and reversed gRPC
132. **BEEXTCMD_REVERSE_GRPC2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 and grpc reversed
133. **BEEXTCMD_REVERSE_GRPC3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 and grpc reversed
134. **BEEXTCMD_REVERSE_GRPC4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 and grpc reversed
135. **BEHOSTCHECK**: 
      * Given Engine and Broker configured to work with BBDO 3
      * When a schedule forced host check command on host host_1 is launched
      * Then the result appears in the centreon_storage resources table
136. **BEHS1**: store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
137. **BEINSTANCE**: Instance to bdd
138. **BEINSTANCESTATUS**: Instance status to bdd
139. **BEMC1**: 
      * When host_1 is moved from engine0 to engine1, reloading engine1 first
      * (so broker cache records instance_id=2 for host_1) and then reloading
      * engine0 (which sends a DELETE with instance_id=1) must not evict host_1
      * from broker cache. The broker log must contain "ignoring stale deletion".
140. **BEMC2**: 
      * When service_1 on host_1 is moved from engine0 to engine1, reloading
      * engine1 first (broker records instance_id=2) and then reloading engine0
      * (DELETE with instance_id=1) must not evict service_1 from broker cache.
141. **BENCR1**: 
      * Given an engine configured with hosts and services with check commands,
      * when I change the check commands in the configuration and reload the engine,
142. **BENCV**: Engine is configured with hosts/services. The first host has no customvariable. Then we add a customvariable to the first host and we reload engine. Then the host should have this new customvariable defined and centengine should not crash.
143. **BENHG1**: GIVEN a Centreon platform with 3 Engine instances AND Broker is configured with RRD, central and module outputs AND the central broker has 5 database connections WHEN I create a host group containing 3 hosts AND I reload both Broker and Engine configurations THEN the membership of all 3 hosts to the host group should be logged AND all membership entries should appear within 45 seconds
144. **BENHG4**: GIVEN a platform with 3 Engine instances and unified_sql output with 5 connections AND detailed logging is enabled on module0 (neb debug, core and processing error) WHEN I create host group 1 with 3 hosts and reload configurations THEN at least 2 host memberships should be logged within 45 seconds WHEN I rename host group 1 to "hostgroup_test" and reload configurations THEN the hostgroup name should be updated in database within 60 seconds
145. **BENHGU1**: GIVEN a Centreon platform with 3 Engine instances AND Broker is configured with RRD, central and module outputs AND Broker uses unified_sql output for database operations AND SQL logging is enabled at info level AND the unified_sql output has 5 database connections WHEN I create a host group containing 3 hosts AND I reload both Broker and Engine configurations THEN the membership of all 3 hosts to the host group should be logged AND all membership entries should appear within 45 seconds
146. **BENHGU2**: GIVEN a platform with 3 Engine instances and unified_sql output with 5 connections AND BBDO3 protocol is enabled WHEN I create a host group with 3 hosts and reload configurations THEN at least 2 host memberships should be logged within 45 seconds
147. **BENHGU3**: GIVEN a platform with 4 Engine instances and unified_sql output with 5 connections AND BBDO3 protocol is enabled with SQL debug logging WHEN I create host group 1 across 4 pollers with 3 hosts each and reload THEN host group 1 should contain 12 host members within 30 seconds WHEN I remove the hostgroups configuration from poller 0 and reload THEN host group 1 should contain only 9 host members within 30 seconds
148. **BENHGU4_${test_label}**: GIVEN a platform with 3 Engine instances and unified_sql output with 5 connections AND detailed trace/debug logging is enabled (sql, lua, core) AND a Lua output dumps host groups to /tmp/lua-engine.log AND BBDO protocol version is configured based on test parameter WHEN I create host group 1 with 3 hosts and reload configurations THEN all 3 host memberships should be logged and stored in database within 60 seconds AND the hostgroup should appear in the Lua output file WHEN I rename host group 1 to "hostgroup_test" and reload configurations THEN the hostgroup name should be updated in database within 60 seconds AND the renamed hostgroup should appear in the Lua output file WHEN I remove the host group configuration and reload THEN the hostgroup should be deleted from database within 60 seconds AND no hostgroup should appear in the Lua output file after 10 seconds
149. **BENOTBRK1_${transport}**: Scenario: service notifications disabled then enabled through Broker
      * Given a service with a contact, in notification_mode=broker (BBDO3)
      * And Engine and Broker linked over the given transport, tcp or grpc
      * When its notifications are disabled through SetServiceNotifications
      * Then resources.notifications_enabled drops to 0
      * And a CRITICAL HARD result is not notified (Broker logs the disabled reason)
      * When its notifications are enabled again
      * Then the next CRITICAL result is notified by the poller
      * And the notification execution order reaches the poller whatever the transport
150. **BENOTBRK2**: Scenario: host and services switch through Broker survives Broker and Engine restarts
      * Given a BBDO3 platform with notification_mode=broker
      * When SetHostNotifications(disabled, HOST_AND_SERVICES) is called on host_1
      * Then host_1 and its 20 services have notify=0 in the database
      * When Broker is restarted, then Engine is restarted
      * Then the switches are still off: Broker restores them on the definitions Engine resends
      * When SetHostNotifications(enabled, HOST) is called on host_1
      * Then only the host is back to notify=1, the services stay off
151. **BENOTBRK3**: Scenario: propagation to child hosts through Broker
      * Given host_2 and host_3 children of host_1, host_4 child of host_3 (BBDO3, notification_mode=broker)
      * When SetHostNotifications(disabled, HOST_AND_CHILDREN) is called on host_1
      * Then host_1, host_2, host_3 and host_4 have notifications_enabled=0 and their services are untouched
      * When SetHostNotifications(enabled, BEYOND_HOST) is called on host_1
      * Then host_2, host_3, host_4 and their services are enabled while host_1 stays disabled
152. **BENOTSET1**: Scenario: service notification number set through Broker
      * Given a service with a contact, in notification_mode=broker (BBDO3)
      * When the service goes CRITICAL HARD and is notified
      * Then services.notification_number is 1
      * When SetServiceNotificationNumber(5) is called on Broker
      * Then services.notification_number becomes 5 (the manager publishes it)
      * And a new CRITICAL result after the interval is notified with number 6
153. **BENOTSET2**: Scenario: custom notifications through Broker, plain and forced
      * Given a service with a contact, in notification_mode=broker (BBDO3)
      * When SendCustomServiceNotification is called on Broker
      * Then the poller executes a CUSTOM notification with the given author and comment
      * When the service notifications are disabled through Broker and a custom notification is sent
      * Then it is refused by the decision
      * When the same custom notification is sent with forced set
      * Then it bypasses the switch and is executed
154. **BENSG1**: Scenario: Service group creation and synchronization across multiple pollers
      * Given 3 Engine pollers and Broker are started in non-centralized mode
      * And the unified SQL output is configured with 5 database connections
      * When a service group is created on poller 0 with services service_1, service_2, service_3 from host_1
      * And servicegroups.cfg is added to poller 0 configuration
      * And Broker and Engine are reloaded
      * Then the central broker log should confirm that all 3 services are members of service group 1 on instance 1
155. **BENSGU1**: New service group with several pollers and connections to DB with broker configured with unified_sql
156. **BENSGU2**: New service group with several pollers and connections to DB with broker configured with unified_sql
157. **BENSGU3_${test_label}**: New service group with several pollers and connections to DB with broker and rename this servicegroup
158. **BENSVC1**: New services with several pollers
159. **BEOTEL_CENTREON_AGENT_ACCENTED_PERFDATA**: Scenario: plugin output and perfdata label with accented characters (SSL certificate subject)
      * Given an agent that executes two plugins printing an SSL certificate check result
      * whose output and perfdata label contain ô, é and à
      * And service_1 plugin prints it in UTF-8
      * And service_2 plugin prints it in ISO-8859-1 (not valid UTF-8)
      * When Broker, Engine and Agent are started
      * Then the OTel messages are serialized without error and both services reach HARD OK
      * And their outputs and metric names are stored in UTF-8 in the database
160. **BEOTEL_CENTREON_AGENT_CEIP**: Scenario: Agent and "centreon_storage.agent_information" Statistics
      * Given Engine connected to Broker
      * When an agent connects to Engine
161. **BEOTEL_CENTREON_AGENT_CHECK_COUNTER**: 
      * Given an agent with counter check, we expect to get the correct status for the centagent process running on windows host
162. **BEOTEL_CENTREON_AGENT_CHECK_DIFFERENT_INTERVAL**: GIVEN a Centreon Engine with OpenTelemetry server module configured AND an OTEL connector using centreon_agent processor with 5s export period AND 3 passive services configured with different check intervals (1, 2, 3 minutes) AND interval_length is set to 10 seconds WHEN the Engine, Broker and Agent are started THEN service_1 should execute checks every 10 seconds (1*10) with 5s tolerance AND service_2 should execute checks every 20 seconds (2*10) with 5s tolerance AND service_3 should execute checks every 30 seconds (3*10) with 5s tolerance AND all check intervals should be verified within 80 seconds
163. **BEOTEL_CENTREON_AGENT_CHECK_EVENTLOG**: 
      * Given an agent with eventlog check, we expect status, output and metrics
164. **BEOTEL_CENTREON_AGENT_CHECK_FILES**: 
      * Given an agent with file check, we expect to get the correct status for files under monitoring on the Windows host
165. **BEOTEL_CENTREON_AGENT_CHECK_HEALTH**: agent check health and we expect to get it in check result
166. **BEOTEL_CENTREON_AGENT_CHECK_HOST**: 
      * Given an agent host checked by centagent, we set a first output to check command,
      * modify it, reload engine and expect the new output in resource table
167. **BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED**: agent check host with encrypted connection and we expect to get it in check result
168. **BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED_ENCRYPTED_CREDENTIALS**: 
      * Given an agent host checked by centagent over an encrypted connection,
      * Engine use credentials encryption and send encrypted commands
      * we set a first output to check command,
      * modify it, reload engine and expect the new output in resource table
169. **BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED_MANY_AGENT**: 
      * Given an engine listening for incomming CMA connections,
      * We connect two agent with the same host_name and engine must not crash
170. **BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED_RENEW_CERT**: 
      * Given an engine with an encrypted otel server with one minute ttl, we expect that otel server
      * will restart after on minute, then we recreate certificates and otel server must restart also
171. **BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED_RENEW_CERT_WITH_DEFAULT_CERT**: 
      * Given an engine with an encrypted otel server with one minute ttl and default engine generated certificates, 
      * we expect that otel server will restart after on minute, then we recreate certificates and otel server must restart also
172. **BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED_SAN_IP**: 
      * Given an engine with an encrypted otel server using a self-signed cert
      * whose CN does not match the hostname but has IP:127.0.0.1 and DNS:<hostname> in SAN,
      * with minute_certificate_ttl triggering cert regeneration,
      * the agent should connect successfully because SANs are copied to the generated cert.
173. **BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED_WITHOUT_CERT**: 
      * Given Engine is configured with a full TLS gRPC server (port 4318) exposing its CA certificate,
      * And the CMA has only a fingerprint (no CA certificate file) and a JWT token,
      * When the CMA starts and attempts a full TLS connection to Engine (which fails — no CA yet),
      * Then the CMA falls back to an insecure TLS connection to retrieve the CA certificate from Engine,
      * And Engine logs a security entry confirming the CA certificate was delivered,
      * And the CMA validates the retrieved CA certificate against the configured fingerprint,
      * And the CMA logs a security entry confirming the CA bootstrap succeeded,
      * And the CMA stores the CA certificate and reconnects using full TLS with the JWT token,
      * Then the host check result is reported successfully in the resources table.
174. **BEOTEL_CENTREON_AGENT_CHECK_HOST_NO_ENCRYPTED_CREDENTIALS**: 
      * Given an agent host checked by centagent over a non encrypted connection,
      * Engine use credentials encryption, but send no encrypted commands
      * we set a first output to check command,
      * modify it, reload engine and expect the new output in resource table
175. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_CPU**: agent check service with native check cpu and we expect to get it in check result
176. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_CUSTOM**: Scenario: Native custom check echoes provided arguments
      * Given Centreon Engine is configured with an OpenTelemetry server module and OTEL connector
      * And service_1 is set as a passive service with defined check and retry intervals
      * And the check command "otel_check" runs the native custom check "check_echo" with ARG1 "user" and ARG2 "hello"
      * When Broker, Engine and Agent are started and the OTEL server becomes ready
      * Then service_1 must reach HARD OK state within 120 seconds
      * And the service output must be "hello user from custom check"
177. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_MEMORY**: agent check service with native check memory and we expect to get it in check result
178. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_SERVICE**: agent check service with native check service and we expect to get it in check result
179. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_SERVICE_AUTO**: 
      * Given a window agent with native service check, we check cma service with different configurations
180. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_SERVICE_FILTER_STATE**: 
      * Given a windows agent with native service check, 
      * we filter on CentreonMonitoringAgent service and use warning-state and critical-state to trigger status changes
181. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_STORAGE**: agent check service with native check storage and we expect to get it in check result, with the <drive>:#storage.space.free.bytes, usage.bytes and usage.percentage perfdata
182. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_STORAGE_FILTER_FS**: 
      * Given a windows agent with native check storage,
      * when filter-fs or exclude-fs refuses a drive, then this drive is refused before any system call
      * on it (no GetVolumeInformation or GetDiskFreeSpaceEx), so no error is logged even for drives
      * without media. filter-fs and exclude-fs are case insensitive.
      * With filter-fs c:.*, the count perfdata is 1 and C:#storage.space.usage.percentage is sent.
183. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_STORAGE_FILTER_FS_MACRO**: 
      * Given a windows agent with native check storage whose filter-fs and exclude-fs
      * come from FILTERFS and EXCLUDEFS service macros,
      * when backslashes are escaped once or twice (legacy form) in these macros,
      * then both forms give the same result: C: is checked with FILTERFS and refused with EXCLUDEFS.
      * A FILTERFS that matches no drive still refuses every drive.
      * C:#storage.space.usage.percentage is also expected with each FILTERFS form.
184. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_STORAGE_FILTER_STORAGE_TYPE**: 
      * Given a windows agent with native check storage,
      * when filter-storage-type contains a storage type (hrstoragefixeddisk, whatever its case)
      * or a file system format (hrfsntfs, other|ntfs), then C: drive is checked and service is OK.
      * When filter-storage-type doesn't match any drive, then service is CRITICAL with
      * 'No storage found (filters issue)'.
      * Between each filter, a filter that refuses all drives is applied in order to be sure that
      * OK state comes from the new filter.
      * C:#storage.space.usage.percentage is also expected for each filter.
185. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_UPTIME**: agent check service with native check uptime and we expect to get it in check result, with the last reboot time in ISO 8601 UTC in the output
186. **BEOTEL_CENTREON_AGENT_CHECK_PROCESS**: 
      * Given an agent with eventlog check, we expect to get the correct status for thr centagent process running on windows host
187. **BEOTEL_CENTREON_AGENT_CHECK_SERVICE**: 
      * Given an Engine configured with an OpenTelemetry server module 
188. **BEOTEL_CENTREON_AGENT_CHECK_SERVICE_FRESHNESS**: 
      * Given Centreon Engine is configured with an OpenTelemetry server module
189. **BEOTEL_CENTREON_AGENT_CHECK_SERVICE_TIMEPERIODS**: CMA timeperiod gating: services with always-active periods are checked, services outside their period are deferred with the correct next-open offset. service_1 -> 24x7           always active (built-in) service_4 -> opens_in_1h    inactive now, opens in ~1 hour
190. **BEOTEL_CENTREON_AGENT_CHECK_TASKSCHEDULER**: 
      * Given an agent with task scheduler check, we expect to get the correct status for the centagent process running on windows host
191. **BEOTEL_CENTREON_AGENT_CMD_DATABASE**: 
      * Given an Engine configured 
192. **BEOTEL_CENTREON_AGENT_FORCE_CHECK**: 
      * Given an agent host with 20 services checked by centagent, we force check to 10nth service and then to the host and 
      * we expect correct status in bdd
193. **BEOTEL_CENTREON_AGENT_INSECURE**: 
      * Given the Centreon Engine is configured with OpenTelemetry server with encryption enabled 
      * When the Centreon Agent connects using a certificate with a mismatched hostname and the insecure option enabled
      * Then the connection succeeds
      * And the engine log confirms the token was accepted
194. **BEOTEL_CENTREON_AGENT_LINUX_NO_DEFUNCT_PROCESS**: agent check host and we expect to get it in check result
195. **BEOTEL_CENTREON_AGENT_NO_TRUSTED_TOKEN**: Scenario: Encrypted connection rejected with untrusted token
      * Given Centreon Engine is configured with an encrypted OpenTelemetry server on port 4318 and an empty trusted token list
      * And host_1 is configured with a passive check using otel_check_icmp
      * And Centreon Agent is configured with a JWT token and the server certificate
      * When Broker, Engine and Agent are started
      * Then the encrypted OTEL server starts listening
      * And the agent connection is refused
      * And host_1 resource status is not updated in the resources table
      * And the engine log contains 'UNAUTHENTICATED : Token is not trusted'
      * Scenario: Unencrypted connection rejected with untrusted token
      * When the Engine is reloaded and the Agent restarted
      * Then the unencrypted OTEL server starts listening
      * And the agent connection is refused
      * And host_1 resource status is not updated in the resources table
      * And the engine log contains 'UNAUTHENTICATED : Token is not trusted'
196. **BEOTEL_CENTREON_AGENT_TLS_BAD_CERT**: 
      * Given the Centreon Engine is configured with OpenTelemetry server with encryption enabled 
      * When the Centreon Agent connects using a certificate with a mismatched hostname and the full option enabled
      * Then the connection is refused
      * And the engine log confirms the certificate hostname mismatch
197. **BEOTEL_CENTREON_AGENT_TOKEN**: Scenario: Validate agent connection with valid JWT token
      * Given the OpenTelemetry server is configured with encryption enabled and the server trusts a JWT token
      * And the Centreon Agent is configured to use that valid token and the server certificate
      * When the broker, engine and agent are started
      * Then the engine should expose an encrypted OTEL listener
      * And the engine log should report that the token is valid
      * When the agent is reconfigured to use an unencrypted connection and the engine is reloaded
      * And the agent is restarted
      * Then the engine should expose an unencrypted OTEL listener
      * And the engine log should still report that the token is valid
198. **BEOTEL_CENTREON_AGENT_TOKEN_AGENT_TELEGRAPH**: 
      * Given an OpenTelemetry server is configured with token-based connection
199. **BEOTEL_CENTREON_AGENT_TOKEN_AGENT_TELEGRAPH_2**: 
      * Given an OpenTelemetry server is configured with token-based connection
200. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED**: Scenario: Encrypted connection with expired token
201. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED_WHILE_RUNNING**: Scenario: Token expires during encrypted connection
      * Given Centreon Engine is configured with an encrypted OTEL server on port 4318
      * And Centreon Agent is configured with a valid JWT token (20s lifetime) and the server certificate
      * When Broker, Engine and Agent are started
      * Then the engine log reports the encrypted server is listening
      * And the engine log reports "Token is valid"
      * When the token lifetime elapses
      * Then the engine log reports "UNAUTHENTICATED : Token expired"
      * Scenario: Token expires during unencrypted connection after reconfiguration
      * Given a new valid JWT token (20s lifetime) is created and added to the OTEL server
      * And Engine and Agent are reconfigured to use an unencrypted OTEL server with the new token
      * When the Engine is reloaded and the Agent restarted
      * Then the engine log reports the unencrypted server is listening
      * And the engine log reports "Token is valid"
      * When the token lifetime elapses
      * Then the engine log reports "UNAUTHENTICATED : Token expired"
202. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED_WHILE_RUNNING_REVERSE**: Scenario: Token expires during encrypted reverse connection
      * Given Centreon Engine is configured to initiate a reverse encrypted OTEL connection to the Agent using a valid JWT token (10s lifetime)
      * When Broker, Engine and Agent are started
      * Then the Agent starts its encrypted reverse OTEL server and the Engine connects (init log appears)
      * And the Agent log reports "Token is valid"
      * When the token lifetime elapses
      * Then the Engine log reports "client::OnDone(Token expired)"
      * Scenario: Token expires during unencrypted reverse connection after reconfiguration
      * Given the Engine and Agent are reconfigured to use an unencrypted reverse OTEL connection with a new valid JWT token (10s lifetime)
      * When the Engine configuration is reloaded and the Agent restarted
      * Then the Agent starts its unencrypted reverse OTEL server and the Engine connects (init log appears)
      * And the Agent log reports "Token is valid"
      * When the new token lifetime elapses
      * Then the Engine log reports "client::OnDone(Token expired)"
203. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRE_REVERSE**: Scenario: Encrypted reversed connection with expired token
      * Given Centreon Engine is configured as a client using a reversed encrypted OTEL connection to the Agent on port 4321
      * And the Agent reverse OTEL server trusts a JWT token that is already expired
      * When Broker, Engine and Agent are started
      * Then the Agent starts its encrypted reversed OTEL server (server listening appears in the agent log)
      * And the Engine connection is refused because the token is expired client::OnDone(Token expired appears in engine log)
      * Scenario: Unencrypted reversed connection with expired token
      * Given encryption is disabled for the reverse connection while still using the same expired token
      * When the Agent is stopped, the Engine configuration is reloaded, and the Agent is restarted
      * Then the Agent starts its unencrypted reversed OTEL server (server listening appears in the agent log)
      * And the Engine connection is refused because the token is expired client::OnDone(Token expired appears in engine log)
204. **BEOTEL_CENTREON_AGENT_TOKEN_MISSING_HEADER**: Scenario: Missing JWT header over encrypted OTEL connection
      * Given Centreon Engine is configured with an encrypted OTEL server (port 4318) and a trusted token list
      * And Centreon Agent is configured to connect without a JWT token (no Authorization header)
      * When Broker, Engine and Agent are started
      * Then the encrypted OTEL server is listening
      * And the agent connection is rejected
      * And the engine log contains "UNAUTHENTICATED: No authorization header"
      * Scenario: Missing JWT header over unencrypted OTEL connection
      * Given Engine and Agent are reconfigured to use an unencrypted OTEL connection still without a JWT token
      * When the Engine is reloaded and the Agent restarted
      * Then the unencrypted OTEL server is listening
      * And the agent connection is rejected
      * And the engine log contains "UNAUTHENTICATED: No authorization header"
205. **BEOTEL_CENTREON_AGENT_TOKEN_REVERSE**: Scenario: Encrypted reversed connection with valid token
      * Given Centreon Engine is configured as a client with a reverse encrypted connection to the Agent server on port 4321 and the Agent trusts a valid JWT token
      * When Broker, Engine and Agent are started
      * Then the Agent starts an encrypted reversed OTEL server
      * And the Engine connects to the Agent (init log appears)
      * And the Agent log reports "Token is valid"
      * Scenario: Unencrypted reversed connection with valid token after reconfiguration
      * Given encryption is disabled for the reverse connection while keeping the same trusted JWT token
      * When the Agent is stopped, the Engine configuration is reloaded, and the Agent is restarted
      * Then the Agent starts an unencrypted reversed OTEL server
      * And the Engine reconnects to the Agent (init log appears)
      * And the Agent log reports "Token is valid"
206. **BEOTEL_CENTREON_AGENT_TOKEN_UNTRUSTED**: Scenario: Encrypted connection with untrusted token
      * Given Centreon Engine is configured with an encrypted OpenTelemetry server
      * And the server trusts a JWT token different from the one configured on the Centreon Agent
      * When Broker, Engine and Agent are started
      * Then the encrypted OTEL server starts listening
      * And the agent connection is refused
      * And the engine log contains "Token is not trusted"
      * Scenario: Unencrypted connection with untrusted token
      * Given Engine and Agent are reconfigured to use an unencrypted OTEL connection with the same untrusted agent token
      * When the Engine is reloaded and the Agent restarted
      * Then the unencrypted OTEL server starts listening
      * And the agent connection is refused
      * And the engine log contains "Token is not trusted"
207. **BEOTEL_CENTREON_AGENT_TOKEN_UNTRUSTED_REVERSE**: Scenario: Encrypted reversed connection refused with untrusted token
      * Given Centreon Engine is configured as a client using a reverse encrypted OTEL connection to the Agent on port 4321
      * And the Agent reverse OTEL server trusts token1
      * And the Engine is configured to connect with a different (untrusted) token2
      * When Broker, Engine and Agent are started
      * Then the Agent starts its encrypted reversed server (listening is logged)
      * And the Engine connection is refused
      * And the engine log contains "client::OnDone(Token not trusted)"
      * Scenario: Unencrypted reversed connection refused with untrusted token after reconfiguration
      * Given encryption is disabled for the reverse connection while the Agent still trusts token1 and the Engine still uses token2
      * When the Agent is stopped, the Engine configuration reloaded, and the Agent restarted
      * Then the Agent starts its unencrypted reversed server (listening is logged)
      * And the Engine connection is refused
      * And the engine log contains "client::OnDone(Token not trusted)"
208. **BEOTEL_CENTREON_AGENT_UNKNOWN_HOST**: 
      * Given an engine configured, a broker with event_script output
      * We connect a CMA with an hostname unknown from engine configuration, 
      * we expect that broker will call the script with the new agent infos
      * Then we add unknown host to engine config and we wait for a check result
209. **BEOTEL_CENTREON_AGENT_WHITE_LIST**: Scenario: Enforcing command whitelist for agent checks
      * Given a whitelist file is created with allowed commands for host_1
      * And the engine, broker, and agent are configured and started
      * When a check command matching the whitelist is executed for host_1
      * Then the check result is accepted and stored in the resources table
      * When a check command not matching the whitelist is configured for host_1 and engine is reloaded
      * Then the command is rejected and a "command not allowed by whitelist" message appears in the log
210. **BEOTEL_CENTREON_AGENT_WHITE_LIST_FORCE_CHECK_HOST**: Scenario: forced checks of a CMA host use the CMA whitelist
      * Given a whitelist file where the cma-whitelist accepts the CMA check command of host_1
      * And the engine whitelist does not accept this command
      * And the engine, broker, and agent are configured and started
      * When the agent executes the check of host_1
      * Then the check result is accepted and stored in the resources table
      * When a forced check is scheduled on host_1
      * Then no whitelist error appears in the engine log
      * And the check result is stored in the resources table
211. **BEOTEL_CENTREON_AGENT_WHITE_LIST_FORCE_CHECK_SERVICE**: Scenario: forced checks of a CMA service use the CMA whitelist
      * Given a whitelist file where the cma-whitelist accepts the CMA check command of host_1/service_1
      * And the engine whitelist does not accept this command
      * And the engine, broker, and agent are configured and started
      * When the agent executes the check of service_1
      * Then the check result is accepted and stored in the resources table
      * When a forced check is scheduled on service_1
      * Then no whitelist error appears in the engine log
      * And the check result is stored in the resources table
212. **BEOTEL_INVALID_CHECK_COMMANDS_AND_ARGUMENTS**: 
      * Given the agent is configured with native checks for services
      * And the OpenTelemetry server module is added
      * And services are configured with incorrect check commands and arguments
      * When the broker, engine, and agent are started
      * Then the resources table should be updated with the correct status
      * And appropriate error messages should be generated for invalid checks
213. **BEOTEL_REVERSE_CENTREON_AGENT_CHECK_HOST**: agent check host with reversed connection and we expect to get it in check result
214. **BEOTEL_REVERSE_CENTREON_AGENT_CHECK_HOST_CRYPTED**: agent check host with encrypted reversed connection and we expect to get it in check result
215. **BEOTEL_REVERSE_CENTREON_AGENT_CHECK_SERVICE**: agent check service with reversed connection and we expect to get it in check result
216. **BEOTEL_SERVE_TELEGRAF_CONFIGURATION_CRYPTED**: we configure engine with a telegraf conf server and we check telegraf conf file
217. **BEOTEL_SERVE_TELEGRAF_CONFIGURATION_NO_CRYPTED**: we configure engine with a telegraf conf server and we check telegraf conf file
218. **BEOTEL_SERVE_TELEGRAF_CONFIGURATION_NO_CRYPTED_1**: Scenario: Serve telegraf configuration with a complex whitelist
      * Given the engine is configured with a telegraf conf server and a complex whitelist
      * When I request the telegraf conf file for host_1
      * Then I should receive the expected telegraf configuration for host_1
      * And service_3 should be blacklisted and unavailable for host_1
      * When I request the telegraf conf file for host_2
      * Then I should receive the expected telegraf configuration for host_2
      * And service_5 should be blacklisted and unavailable for host_2
219. **BEOTEL_TELEGRAF_CHECK_HOST**: we send nagios telegraf formatted data and we expect to get it in check result
220. **BEOTEL_TELEGRAF_CHECK_SERVICE**: Scenario: Handling of OK and CRITICAL check results from Telegraf input
      * Given the OpenTelemetry server is ready
      * When I send a Telegraf-formatted check result with status "OK" to the Engine
      * Then the result should be stored in the Centreon Broker storage database with status "OK"
      * When I send a Telegraf-formatted check result with status "CRITICAL" to the Engine
      * Then the result should be stored in the Centreon Broker storage database with status "CRITICAL" and state type "SOFT"
      * When I send a Telegraf-formatted check result with status "CRITICAL" to the Engine
      * Then the result should be stored in the Centreon Broker storage database with status "CRITICAL" and state type "SOFT"
      * When I send a Telegraf-formatted check result with status "CRITICAL" to the Engine
      * Then the result should be stored in the Centreon Broker storage database with status "CRITICAL" and state type "HARD"
221. **BEPBBEE1**: central-module configured with bbdo_version 3.0 but not others. Unable to establish connection.
222. **BEPBBEE3**: bbdo_version 3 generates new bbdo protobuf service status messages.
223. **BEPBBEE4**: bbdo_version 3 generates new bbdo protobuf host status messages.
224. **BEPBBEE5**: bbdo_version 3 generates new bbdo protobuf service messages.
225. **BEPBCVS**: bbdo_version 3 communication of custom variables.
226. **BEPBHostParent**: bbdo_version 3 communication of host parent relations
227. **BEPBINST_CONF**: bbdo_version 3 communication of instance configuration.
228. **BEPBRI1**: bbdo_version 3 use pb_resource new bbdo protobuf ResponsiveInstance message.
229. **BEPBUID1**: uid > 2^32 in broker module config overrides poller_id. The instances, hosts and resources tables must store this 64-bit value.
230. **BEPHG1**: Scenario: Hostgroups added then removed one by one stay consistent between the database and the broker cache
      * Given a central broker, a rrd broker and 5 engine instances in centralized mode
      * With 50 hosts each (250 hosts total, numbered 1 to 250) and 20 services per host
      * When broker and engines are started and the initial configuration is applied
      * And hostgroups hg1 to hg5 are added one by one:
      * hg1 contains all 250 hosts,
      * hg2 contains the 125 hosts with even IDs,
      * hg3 contains the 83 hosts with IDs divisible by 3,
      * hg4 contains the 62 hosts with IDs divisible by 4,
      * hg5 contains the 50 hosts with IDs divisible by 5
      * Then after each addition the database and the broker cache are consistent:
      * the hosts_hostgroups table has the expected number of entries,
      * the hostgroup name in the cache matches hostgroup_<id>,
      * and the member count in the cache matches the database
      * And when hostgroups hg1 to hg5 are removed one by one
      * Then after each removal the database shows zero entries for that group
      * And the broker cache no longer reports any members for that group
231. **BEPOLBRK1**: Scenario: poller notifications switch through Broker survives restarts
      * Given a service notifying John_Doe on poller 1, in notification_mode=broker (BBDO3)
      * When the poller notifications are disabled through SetPollerNotifications
      * Then a CRITICAL HARD result is not notified (Broker logs that notifications are disabled)
      * When Broker is restarted, then Engine is restarted (it resends its configuration with enable_notifications=1)
      * Then the switch is still off: the next CRITICAL result is not notified
      * When the poller notifications are enabled again
      * Then the next CRITICAL result is notified by the poller
      * And an unknown poller is refused with NOT_FOUND
232. **BEPOLLERTZ**: Scenario: GetPollers reports each poller's own local timezone
      * Given three centralized Engine pollers started with distinct TZ environment variables (Europe/Paris, America/New_York, Asia/Shanghai)
      * When they connect to the central Broker in BBDO3 centralized configuration mode and advertise their local timezone in the Welcome message
      * Then the GetPollers gRPC method returns the three pollers, each with the timezone it advertised
233. **BEPSG1**: Scenario: Servicegroups added then removed one by one stay consistent between the database and the broker cache
      * Given a central broker, a rrd broker and 5 engine instances in centralized mode
      * With 50 hosts each (250 hosts total, numbered 1 to 250) and 20 services per host
      * (5000 services total, numbered 1 to 5000)
      * When broker and engines are started and the initial configuration is applied
      * And servicegroups sg1 to sg5 are added one by one:
      * sg1 contains all 5000 services,
      * sg2 contains the 2500 services with even IDs,
      * sg3 contains the 1666 services with IDs divisible by 3,
      * sg4 contains the 1250 services with IDs divisible by 4,
      * sg5 contains the 1000 services with IDs divisible by 5
      * Then after each addition the database and the broker cache are consistent:
      * the services_servicegroups table has the expected number of entries,
      * the servicegroup name in the cache matches servicegroup_<id>,
      * and the member count in the cache matches the database
      * And when servicegroups sg1 to sg5 are removed one by one
      * Then after each removal the database shows zero entries for that group
      * And the broker cache no longer reports any members for that group
234. **BERD1**: Scenario: Starting/stopping Broker does not create duplicated events.
      * Given  the broker configuration central  is set to Lua output test-doubles-c.lua
      * And the broker configuration module0 is set to with Lua output test-doubles.lua
      * When the broker and engine are started
      * Then the Lua virtual machine should be initialized in both broker and engine logs
      * And the engine and broker should be connected
      * When the broker is kindly stopped and cache is cleared
      * And the broker is restarted
      * And the engine is stopped and broker is kindly stopped
      * Then the contents of /tmp/lua-engine.log and /tmp/lua.log should match
      * And there should be no duplicate events in the logs
235. **BERD2**: Scenario: Starting/stopping Engine does not create duplicated events.
      * Given  the broker configuration central  is set to Lua output test-doubles-c.lua
      * And the broker configuration module0 is set to with Lua output test-doubles.lua
      * When the broker and engine are started
      * Then the Lua virtual machine should be initialized in both broker and engine logs
      * And the engine and broker should be connected
      * When the engine is stopped
      * And the engine is restarted
      * And the engine is stopped and broker is kindly stopped
      * Then the contents of /tmp/lua-engine.log and /tmp/lua.log should match
      * And there should be no duplicate events in the logs
236. **BERDUC1**: Scenario: Starting/stopping Broker does not create duplicated events in usual cases with unified_sql and BBDO 3.0
      * Given the broker configuration central is set to Lua output test-doubles-c.lua
      * And the broker configuration module0 is set to Lua output test-doubles.lua
      * When the broker and engine are started
      * Then the Lua virtual machine should be initialized in both broker and engine logs
      * And the engine and broker should be connected
      * When the broker is kindly stopped
      * And the cache is cleared
      * And the broker is restarted
      * And the engine is stopped and broker is kindly stopped again
      * Then there should be no duplicate events in the logs
237. **BERDUC2**: Scenario: Starting/stopping Engine does not create duplicated events in usual cases with unified_sql and BBDO 3.0
      * Given the broker configuration central is set to Lua output test-doubles-c.lua
      * And the broker configuration module0 is set to Lua output test-doubles.lua
      * When the broker and engine are started
      * Then the Lua virtual machine should be initialized in both broker and engine logs
      * And the engine and broker should be connected
      * When the engine is stopped
      * And the cache is cleared
      * And the engine is restarted
      * And the engine is stopped and broker is kindly stopped
      * Then there should be no duplicate events in the logs
238. **BERDUCA300**: Scenario: When the engine is stopped, it should emit a stop event and receive an ack event with events to clean from broker.
      * Given the broker configuration central is set to Lua output test-doubles-c.lua
      * And the broker configuration module0 is set to Lua output test-doubles.lua
      * When the broker and engine are started
      * Then the Lua virtual machine should be initialized in both broker and engine logs
      * And the engine and broker should be connected
      * When the engine is stopped
      * Then the engine should emit a stop event
      * And the broker should receive the stop event
      * And the broker should send an ack for handled events
      * And the engine should receive the ack for handled events from the broker
239. **BERDUCA301**: Scenario: When the engine is stopped, it should emit a stop event and receive an ack event with events to clean from broker with bbdo 3.0.1.
      * Given the broker configuration central is set to Lua output test-doubles-c.lua
      * And the broker configuration module0 is set to Lua output test-doubles.lua
      * When the broker and engine are started
      * Then the Lua virtual machine should be initialized in both broker and engine logs
      * And the engine and broker should be connected
      * When the engine is stopped
      * Then the engine should emit a stop event
      * And the broker should receive the stop event
      * And the broker should send an ack for handled events
      * And the engine should receive the ack for handled events from the broker
240. **BERES1**: store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
241. **BERRDREC1**: RRD retention startup merge — metric.
242. **BERRDREC2**: RRD retention startup merge — status.
243. **BESAU2**: New hosts with action_url with more than 2000 characters
244. **BESERVCHECK**: external command CHECK_SERVICE_RESULT
245. **BESN3**: New hosts with notes with more than 500 characters
246. **BESNU1**: New hosts with notes_url with more than 2000 characters
247. **BESS1**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
248. **BESS2**: Scenario: Start and stop Broker/Engine with Broker started first and Engine stopped first
      * Given the Broker is started before the Engine and both use BBDO 3
      * When the Engine is started after the Broker
      * Then the connection between Engine and Broker should be established
      * And the poller should be visible in the database
      * When the Engine is stopped before the Broker
      * Then the poller should be disabled and not visible in the database
      * And neither Broker nor Engine should crash
249. **BESS3**: Start-Stop Broker/Engine - Engine started first - Engine stopped first
250. **BESS4**: Start-Stop Broker/Engine - Engine started first - Broker stopped first
251. **BESS5**: Start-Stop Broker/engine - Engine debug level is set to all, it should not hang
252. **BESS6_${label}**: Scenario: Verify Broker and Engine start and establish connections
      * Given the Central Broker, RRD Broker, and Central Engine are started
      * When we check the connection between them
      * Then the connection should be well established
      * And the central broker should have two peers connected: the central engine and the RRD broker
      * And the RRD broker should correctly recognize its peer as the Central Broker
253. **BESSBQ1**: A very bad queue file is written for broker. Broker and Engine are then started, Broker must read the file raising an error because of that file and then get data sent by Engine.
254. **BESSCTO**: Scenario: Service commands time out due to missing Perl Connector
      * Given the Engine is configured as usual but without the Perl Connector
      * When the Engine executes its service commands
      * Then the commands take too long and reach the timeout
      * And the Engine starts and stops two times as a result
255. **BESSCTOWC**: Scenario: Service commands time out due to missing Perl Connector
      * Given the Engine is configured as usual with some commands using the Perl Connector
      * When the Engine executes its service commands
      * Then the commands take too long and reach the timeout
      * And the Engine starts and stops two times as a result
256. **BESSG**: Scenario: Broker handles connection and disconnection with Engine
      * Given Broker is configured with only one output that is Graphite
      * When the Engine starts and connects to the Broker
      * Then the Broker must be able to handle the connection
      * When the Engine stops
      * Then the Broker must be able to handle the disconnection
257. **BESS_CRYPTED_GRPC1**: Start-Stop grpc version Broker/Engine - well configured
258. **BESS_CRYPTED_GRPC2**: Start-Stop grpc version Broker/Engine only server crypted
259. **BESS_CRYPTED_GRPC3**: Start-Stop grpc version Broker/Engine only engine crypted
260. **BESS_CRYPTED_REVERSED_GRPC1**: Start-Stop grpc version Broker/Engine - well configured
261. **BESS_CRYPTED_REVERSED_GRPC2**: Start-Stop grpc version Broker/Engine only engine server crypted
262. **BESS_CRYPTED_REVERSED_GRPC3**: Start-Stop grpc version Broker/Engine only engine crypted
263. **BESS_ENGINE_DELETE_HOST**: once engine and cbd started, stop and restart cbd, delete an host and reload engine, cbd mustn't core
264. **BESS_GRPC1**: Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped first
265. **BESS_GRPC2**: Start-Stop grpc version Broker/Engine - Broker started first - Engine stopped first
266. **BESS_GRPC3**: Start-Stop grpc version Broker/Engine - Engine started first - Engine stopped first
267. **BESS_GRPC4**: Start-Stop grpc version Broker/Engine - Engine started first - Broker stopped first
268. **BESS_GRPC5**: Start-Stop grpc version Broker/engine - Engine debug level is set to all, it should not hang
269. **BESS_GRPC_COMPRESS1**: Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped last compression activated
270. **BESS_RELOAD_OUTPUT_ADD**: Scenario: Adding an output to broker config during a reload is ignored
      * Given Broker and Engine are started with their standard configuration
      * When a new output is appended to the broker configuration file
      * And broker is reloaded
      * Then an error message is logged stating the output cannot be added at runtime
      * And the new output does not appear in the broker stats
271. **BESS_RELOAD_OUTPUT_REMOVE**: Scenario: Removing an output from broker config during a reload is ignored
      * Given Broker and Engine are started with their standard configuration
      * When the RRD output is removed from the broker configuration file
      * And broker is reloaded
      * Then an error message is logged stating the output cannot be removed at runtime
      * And the RRD output is still present in broker stats
272. **BETAG1**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Broker is started before.
273. **BETAG2**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
274. **BEUTAG1**: Engine is configured with some tags. When broker receives them through unified_sql stream, it stores them in the centreon_storage.tags table. Broker is started before.
275. **BEUTAG10**: some services are configured with tags on two pollers. Then tags are removed from some of them and in centreon_storage, we can observe resources_tags table updated.
276. **BEUTAG11**: Scenario: Updating resource tags after changing several tags
      * Given some services are configured with tags on two pollers
      * Then the resources_tags table contains them
      * When several tags are changed
      * Then the resources_tags table is updated
277. **BEUTAG12**: Engine is configured with some tags. Group tags tag2, tag6 are set to hosts 1 and 2. Category tags tag4 and tag8 are added to hosts 2, 3, 4. The resources and resources_tags tables are well filled. The tag6 and tag8 are removed and resources_tags is also well updated.
278. **BEUTAG2**: Engine is configured with some tags. A new service is added with a tag. Broker should make the relations.
279. **BEUTAG3**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
280. **BEUTAG4**: Engine is configured with some tags. Group tags tag9, tag13 are set to services 1 and 3. Category tags tag3 and tag11 are added to services 1, 3, 5 and 6. The centreon_storage.resources and resources_tags tables are well filled.
281. **BEUTAG5**: Engine is configured with some tags. Group tags tag2, tag6 are set to hosts 1 and 2. Category tags tag4 and tag8 are added to hosts 2, 3, 4. The resources and resources_tags tables are well filled.
282. **BEUTAG6**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.resources_tags table. Engine is started before.
283. **BEUTAG7**: Some services are configured with tags on two pollers. Then tags configuration is modified.
284. **BEUTAG8**: Services have tags provided by templates.
285. **BEUTAG9**: hosts have tags provided by templates.
286. **BEUTAG_REMOVE_HOST_FROM_HOSTGROUP**: remove a host from hostgroup, reload, insert 2 host in the hostgroup must not make sql error
287. **BE_BACKSLASH_CHECK_RESULT**: external command PROCESS_SERVICE_CHECK_RESULT with \:
288. **BE_DEFAULT_NOTIFICATION_INTERVAL_IS_ZERO_SERVICE_RESOURCE**: default notification_interval must be set to NULL in services, hosts and resources tables.
289. **BE_FLAPPING_GLOBAL_ADAPTIVE**: Scenario: the program wide flap detection commands update the flapping flag of every object they touch
      * Given a passive host that flaps and a passive service of ANOTHER host that flaps, with their flapping flag set in the "hosts", "services" and "resources" tables
      * When flap detection is disabled program wide with DISABLE_FLAP_DETECTION
      * Then the flapping flag of both objects is cleared
      * When flap detection is enabled program wide with ENABLE_FLAP_DETECTION
      * Then the flapping flag of both objects is set back, each carried by its own adaptive status
      * And no check result was needed for that
290. **BE_FLAPPING_HOST_ADAPTIVE**: Scenario: re-enabling flap detection on a host updates the flapping flag without waiting for a check
      * Given a passive host that flaps, with its flapping flag set in the "hosts" and "resources" tables
      * When flap detection is disabled on that host
      * Then the flapping flag is cleared in both tables
      * When flap detection is enabled again on that host
      * Then the flapping flag is set back in both tables, carried by an adaptive host status
      * And no check result was needed for that
291. **BE_FLAPPING_HOST_RESOURCE**: With BBDO 3, flapping detection must be set in hosts and resources tables.
292. **BE_FLAPPING_SERVICE_ADAPTIVE**: Scenario: re-enabling flap detection on a service updates the flapping flag without waiting for a check
      * Given a passive service that flaps, with its flapping flag set in the "services" and "resources" tables
      * When flap detection is disabled on that service
      * Then the flapping flag is cleared in both tables
      * When flap detection is enabled again on that service
      * Then the flapping flag is set back in both tables, carried by an adaptive service status
      * And no check result was needed for that
293. **BE_FLAPPING_SERVICE_RESOURCE**: With BBDO 3, flapping detection must be set in services and resources tables.
294. **BE_FLAPPING_SERVICE_STOP_NO_EXTRA_CHECK**: Scenario: the end of a flapping is published by the check result that caused it
      * Given a passive service that flaps
      * When stable check results are sent until Engine logs the end of the flapping
      * And no further check result is sent
      * Then the flapping flag is cleared in the "services" and "resources" tables
      * And it did not wait for one more check result to get there
295. **BE_NOTIF_OVERFLOW**: bbdo 2.0 notification number =40000. make an overflow => notification_number null in db
296. **BE_TIME_NULL_SERVICE_RESOURCE**: With BBDO 3, notification_interval time must be set to NULL on 0 in services, hosts and resources tables.
297. **BRCS1**: Broker reverse connection stopped
298. **BRCTS1**: Broker reverse connection too slow
299. **BRCTSMN**: 
      * Given Broker, Engine configured as usual
      * And map also connected to Broker with a filter allowing only 'neb' category
      * When Engine sends pb_service, pb_host, pb_service_status and pb_host_status
      * Then map receives correctly them.
300. **BRCTSMNS**: 
      * Given Broker, Engine configured as usual
      * And map also connected to Broker with a filter allowing 'neb' and 'storage' categories
      * When Engine sends pb_service, pb_host, pb_service_status, pb_host_status and metrics
      * Then Map receives correctly them.
301. **BRGC1**: Broker good reverse connection
302. **BROKER_CACHE_HOST_NOTIF_DEPENDENCY**: Scenario: the broker cache evaluates a host notification dependency, queried through gRPC
      * Given host_2 depending on host_1 (notification failing on DOWN) fed to the broker cache in BBDO3
      * When the master host_1 is UP then DOWN HARD
      * Then NotificationAuthorizedByDependencies returns True then False for host_2, and always True for the independent host_3
303. **BROKER_CACHE_SVC_NOTIF_DEPENDENCY**: Scenario: the broker cache evaluates a service notification dependency, queried through gRPC
      * Given service_2 (host_2) depending on service_1 (host_1), notification failing on CRITICAL, fed to the broker cache in BBDO3
      * When the master service_1 is CRITICAL HARD then OK HARD
      * Then NotificationAuthorizedByDependencies returns False then True for (host_2, service_2)
304. **BROKER_LUA_ENCRYPTION**: 
      * Given an engine with configured encryption, and key and salt, 
      * cbmod and broker use a lua script that use encrypted credentials
      * we give it an encrypted macro that contains path to a lua output files and 
      * we expect that this file will becreated by lua.
305. **BRRDCDDID1**: Scenario: RRD metrics deletion from index ids with rrdcached
      * Given Broker is configured with an rrd output using the rrdcached socket
      * And 3 metrics exist in the storage database
      * When Broker and Engine are started and connected
      * And a remove graphs request is sent for 2 indexes
      * Then Broker logs that these indexes are erased from the database
      * And the index status rrd files are removed from disk
      * And the metrics rrd files matching these indexes are removed from disk
306. **BRRDCDDIDDB1**: RRD metrics deletion from index ids with a query in centreon_storage with rrdcached.
307. **BRRDCDDIDU1**: RRD metrics deletion from index ids with unified sql output with rrdcached.
308. **BRRDCDDM1**: Scenario: RRD metrics deletion from metric ids with rrdcached
      * Given Broker is configured with an rrd output using the rrdcached socket
      * And 3 metrics exist in the storage database
      * When Broker and Engine are started and connected
      * And a remove graphs request is sent for these 3 metrics
      * Then Broker logs that the metrics are erased from the database
      * And the 3 corresponding rrd files are removed from disk
309. **BRRDCDDMDB1**: RRD metrics deletion from metric ids with a query in centreon_storage and rrdcached.
310. **BRRDCDDMID1**: Scenario: RRD deletion of non existing metrics and indexes with rrdcached
      * Given Broker is configured with an rrd output using the rrdcached socket
      * When Broker and Engine are started and connected
      * And a remove graphs request is sent for indexes and metrics that do not exist
      * Then Broker logs that these indexes and metrics do not appear in the storage database
311. **BRRDCDDMIDU1**: RRD deletion of non existing metrics and indexes with rrdcached
312. **BRRDCDDMU1**: Scenario: RRD metric deletion from metric ids with unified_sql output and rrdcached
      * Given Broker is configured with a unified_sql output and an rrd output using the rrdcached socket
      * And 3 metrics exist in the storage database
      * When Broker and Engine are started and connected
      * And a remove graphs request is sent for these 3 metrics
      * Then Broker logs that the metrics are erased from the database
      * And the 3 corresponding rrd files are removed from disk
313. **BRRDCDRB1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with storage/sql sql output and rrdcached.
314. **BRRDCDRBDB1**: RRD metric rebuild with a query in centreon_storage and unified sql with rrdcached
315. **BRRDCDRBU1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with unified_sql output and rrdcached.
316. **BRRDCDRBUDB1**: RRD metric rebuild with a query in centreon_storage and unified sql with rrdcached
317. **BRRDDIDDB1**: RRD metrics deletion from index ids with a query in centreon_storage.
318. **BRRDDIDU1**: RRD metrics deletion from index ids with unified sql output.
319. **BRRDDMDB1**: RRD metrics deletion from metric ids with a query in centreon_storage.
320. **BRRDDMIDU1**: RRD deletion of non existing metrics and indexes
321. **BRRDDMU1**: RRD metric deletion on table metric with unified sql output
322. **BRRDRBDB1**: RRD metric rebuild with a query in centreon_storage and unified sql
323. **BRRDRBUDB1**: Scenario: RRD metric rebuild triggered from the storage database with unified_sql output
      * Given Broker is configured with a unified_sql output and BBDO3
      * And 3 metrics exist in the storage database
      * When Broker and Engine are started and connected
      * And 3 indexes to rebuild are collected (forcing service checks if needed)
      * And these indexes are flagged to rebuild in the storage database and Broker is reloaded
      * Then RRD starts, rebuilds and finishes rebuilding the matching metrics
      * And the rebuilt rrd metric files hold the expected average value
324. **BRRDRMU1**: Scenario: RRD metric rebuild through the gRPC API with unified_sql output
      * Given Broker is configured with a unified_sql output and BBDO3
      * And 3 metrics exist in the storage database
      * When Broker and Engine are started and connected
      * And 3 indexes to rebuild are collected (forcing service checks if needed)
      * And a rebuild request is sent for these indexes through the gRPC API
      * Then Central sends the metrics to rebuild and RRD starts, rebuilds and finishes them
      * And the rebuilt rrd metric files hold the expected average value and RW group permission
      * And the rebuilt rrd status files hold the expected average value
325. **BRRDSTATUS**: We are working with BBDO3. This test checks status are correctly handled independently from their value.
326. **BRRDSTATUSRETENTION**: We are working with BBDO3. This test checks status are not sent twice after Engine reload.
327. **BRRDUPLICATE**: RRD metric rebuild with a query in centreon_storage and unified sql with duplicate rows in database
328. **BRRDWM1**: We are working with BBDO3. This test checks protobuf metrics and status are sent to cbd RRD.
329. **BRRD_RRDLEN**: 
      * Given an RRD with default length 15552000s, 180 days,
      * We change length to 86400s, 1 day, we rebuild metrics and we expect to find 289 rows in metrics
330. **BRTCPWQF**: 
      * Given Broker configured with a map output and a small event queue
      * When a map client connects with a tiny receive buffer and stops reading
      * Then Broker logs "write queue full => remove oldest event"
331. **CBD_RELOAD_AND_FILTERS**: We start engine/broker with a classical configuration. All is up and running. Some filters are added to the rrd output and cbd is reloaded. All is still up and running but some events are rejected. Then all is newly set as filter and all events are sent to rrd broker.
332. **CBD_RELOAD_AND_FILTERS_WITH_OPR**: We start engine/broker with an almost classical configuration, just the connection between cbd central and cbd rrd is reversed with one peer retention. All is up and running. Some filters are added to the rrd output and cbd is reloaded. All is still up and running but some events are rejected. Then all is newly set as filter and all events are sent to rrd broker.
333. **CUSTOM_MACRO_RECURSIVE_RESOLUTION**: Custom macros can reference other macros and get them resolved: - host custom containing host standard ($HOSTADDRESS$) - host custom containing another host custom ($_HOSTINNER$) - host custom containing resource macro ($USER1$) - service custom containing service standard ($SERVICEDESC$) - service custom containing host standard + host custom + service custom - service custom containing $USER1$ + $HOSTADDRESS$ - recursion stops at depth 2 (3-level chain leaves innermost unresolved)
334. **CUSTOM_MACRO_RECURSIVE_RESOLUTION_WITH_ONE_DOLLAR**: Custom macros can reference other macros and get them resolved: - host custom containing host standard ($HOSTADDRESS$) - host custom containing another host custom ($_HOSTINNER$) - host custom containing resource macro ($USER1$) - service custom containing service standard ($SERVICEDESC$) - service custom containing host standard + host custom + service custom - service custom containing $USER1$ + $HOSTADDRESS$ - recursion stops at depth 2 (3-level chain leaves innermost unresolved)
335. **CUSTOM_MACRO_RESOLUTION_WITH_FALSE_MACRO**: 
      * Given engine is configured with host and service custom macros whose values contain dollar signs
      * When a service check runs using old-style $MACRO$ syntax with a non-existent macro at the end of the command
      * Then all macros are resolved correctly, dollar signs inside values are preserved, and the non-existent macro expands to empty
336. **CUSTOM_MACRO_RESOLUTION_WITH_NEW_STYLE_MACRO**: 
      * Given engine is configured with host and service custom macros, one value embedding a new-style non-existent macro {{$TITI$}}
      * When a service check runs using new-style {{$MACRO$}} syntax with a non-existent macro at the end of the command
      * Then all macros are resolved correctly, dollar signs inside values are preserved, and non-existent macros expand to empty
337. **CUSTOM_MACRO_RESOLUTION_WITH_ONE_DOLLAR**: Custom macros with one dollar can be resolved an dollar is not removed
338. **DTIM**: New services with several pollers are created. Then downtimes are set on all configured hosts. This action results on 5250 downtimes if we also count impacted services. Then all these downtimes are removed. This test is done with BBDO 3.0.1
339. **DT_LOG_BROKER**: notification_mode=broker: a host+service downtime handled by Broker (via gRPC) writes the SAME DOWNTIME ALERT STARTED and CANCELLED lines to the storage logs table as Engine does (see DT_LOG_ENGINE in downtimes.robot).
340. **DT_LOG_ENGINE**: Reference (notification_mode=engine): a host+service downtime writes the DOWNTIME ALERT STARTED and CANCELLED lines to the storage logs table (the GUI monitoring log).
341. **DT_WITH_QUOTES**: 
      * Given a centreon running, we set a downtime with quotes
      * Then we expect to get these quotes in bdd
342. **EBBM1**: A service status contains metrics that do not fit in a float number.
343. **EBBM2**: Scenario: an output carrying more metrics than the cap allows is truncated
      * Given a central broker whose unified_sql output carries max_perfdata=3
      * And its perfdata logger low enough to let a warning through
      * When a passive result carrying the five metrics m0 to m4 is submitted
      * Then the central log announces the truncation to three of about five
      * And the service carries m0, m1 and m2 in the metrics table
      * And m4 never reaches it
344. **EBBM3**: Scenario: without the option, every metric of the output is kept
      * Given a central broker whose unified_sql output carries no max_perfdata
      * When a passive result carrying the five metrics m0 to m4 is submitted
      * Then the service carries all five of them
      * And nothing is said about any truncation
345. **EBBPS1**: 1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table resources.
346. **EBBPS2**: 1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table services.
347. **EBDP1**: Four new pollers are started and then we remove Poller3.
348. **EBDP2**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
349. **EBDP3**: Three new pollers are started, then they are killed. It is still possible to remove Poller2 if removed from the configuration.
350. **EBDP4**: Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by Broker.
351. **EBDP5**: Four new pollers are started and then we remove Poller3.
352. **EBDP6**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
353. **EBDP7**: Three new pollers are started, then they are killed. It is still possible to remove Poller2 if removed from the configuration.
354. **EBDP8**: Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by broker.
355. **EBDP_CENTRALIZED_PROT**: Scenario: removing a poller also removes the configuration Broker stores for it
      * Given three pollers in centralized mode, each with its configuration acknowledged
      * And Broker therefore holds a <id>.prot for each of them
      * When every Engine is stopped and Poller3 is removed through the gRPC command
      * Then 3.prot is gone, so a later start cannot load a poller that left the platform
      * And the configurations of the two remaining pollers are untouched
356. **EBDP_GRPC2**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
357. **EBMSSM**: 1000 services are configured with 100 metrics each. The rrd output is removed from the broker configuration. GetSqlManagerStats is called to measure writes into data_bin.
358. **EBMSSMDBD**: 1000 services are configured with 100 metrics each. The rrd output is removed from the broker configuration. While metrics are written in the database, we stop the database and then restart it. Broker must recover its connection to the database and continue to write metrics.
359. **EBMSSMPART**: Scenario: Broker continues writing metrics after partition recreation
      * Given 1000 services are configured with 100 metrics each
      * And the rrd output is removed from the broker configuration
      * And the data_bin table is configured with two partitions "p1" and "p2"
      * And "p1" contains old data
      * And "p2" contains current data
      * When metrics are being written in the database
      * And the "p2" partition is removed
      * And the "p2" partition is recreated
      * Then the broker must recover its connection to the database
      * And it must continue writing metrics
      * When a last service check is forced
      * Then its metrics must be written in the database
360. **EBNHG5**: Scenario: Host group creation and membership updates with unified SQL and BBDO3
      * And a host group 1 is defined with 7 members across instances (hosts host_1 to host_7)
      * When the broker and engine start and the engine becomes ready
      * And the engine configuration file hostgroups.cfg is added and the engine is reloaded
      * Then the system reports 7 relations between hostgroup 1 and its hosts
      * When host host_1 is removed and hostgroup_1 members are updated to exclude host_1
      * And the engine is reloaded
      * Then the system reports 6 relations between hostgroup 1 and its hosts
361. **EBNHG6**: Scenario: Two pollers are connected, hostgroup 1 is defined on poller 0 with host_1 and
      * host_2 as members. Both hosts are then moved from poller 0's configuration to poller 1's
      * configuration, poller 1 is reloaded first, then poller 0. The hostgroup and its host
      * memberships must not disappear from the database.
362. **EBNSG2**: Scenario: Two pollers are connected, servicegroup 1 is defined on poller 0 with
      * host_1/service_1 and host_2/service_2 as members. Both hosts and their services are then
      * moved from poller 0's configuration to poller 1's configuration, poller 1 is reloaded first,
      * then poller 0. The servicegroup and its service memberships must not disappear from the
      * database.
363. **EBPN0**: Verify if child is in queue when parent is down.
364. **EBPN1**: verify relation parent child when delete parent.
365. **EBPN2**: verify relation parent child when delete child.
366. **EBPS2**: 1000 services are configured with 20 metrics each. The rrd output is removed from the broker configuration to avoid to write too many rrd files. While metrics are written in bulk, the database is stopped. This must not crash broker.
367. **EBSAU2**: New services with action_url with more than 2000 characters
368. **EBSG_1**: Scenario: Service group creation and membership updates with unified SQL and BBDO3
      * And a service group 1 is defined with 7 members across hosts (including host_1 services 1-5 and host_2 services 6-7)
      * When the broker and engine start and the engine becomes ready
      * And the engine configuration file servicegroups.cfg is added and the engine is reloaded
      * Then the system reports 7 relations between servicegroup 1 and its services
      * When service host_1/service_1 is removed and servicegroup_1 members are updated to exclude service_1
      * And the engine is reloaded
      * Then the system reports 6 relations between servicegroup 1 and its services
369. **EBSN3**: New services with notes with more than 500 characters
370. **EBSN4**: New hosts with No Alias / Alias and have A Template
371. **EBSNU1**: New services with notes_url with more than 2000 characters
372. **ENGINE_ENCRYPTION_BAD_CONF**: 
      * Given an engine with configured encryption, but without key and salt, 
      * we give him several macros and we expect to retrieve them in logs without decrypt.
      * As engine is not ready to receive encrypted macros, is_encryption_ready must be equal to 0 in db
373. **ENGINE_ENCRYPTION_GOOD_CONF**: 
      * Given an engine with configured encryption, and key and salt, 
      * we give him several macros and we expect to retrieve them in logs without decrypt.
      * As engine is ready to receive encrypted macros, is_encryption_ready must be equal to 1 in db
374. **ENRSCHE1**: Verify that next check of a rescheduled host is made at last_check + interval_check
375. **EVENT_SCRIPT_BASIC_SERVICE_CHECK**: 
      * Given an event_script output configured in broker
      * When a service check result is processed
      * Then the event is passed to the script as JSON
376. **FILTER_ON_LUA_EVENT**: stream connector with a bad configured filter generate a log error message
377. **FIRST_NOTIF_DELAY_EQUAL_RETRY_INTERVAL**: Scenario: first notification delay equal to the retry interval
      * Given a service whose first_notification_delay equals its retry interval
      * When the service enters a CRITICAL HARD state
      * Then the CRITICAL notification is sent after the delay
378. **FIRST_NOTIF_DELAY_GT_RETRY_INTERVAL**: Scenario: first notification delay greater than the retry interval
      * Given a service whose first_notification_delay is greater than its retry interval
      * When the service enters a CRITICAL HARD state
      * Then the CRITICAL notification is sent after the delay
379. **FIRST_NOTIF_DELAY_LT_RETRY_INTERVAL**: Scenario: first notification delay smaller than the retry interval
      * Given a service whose first_notification_delay is smaller than its retry interval
      * When the service enters a CRITICAL HARD state
      * Then the CRITICAL notification is sent after the delay
380. **FLAPPING_NOTIF**: Scenario: a flapping service triggers a FLAPPINGSTART notification
      * Given a service with flap detection and flapping notifications enabled
      * When the service state oscillates enough to start flapping
      * Then a FLAPPINGSTART notification is sent
381. **FORCED_NOTIF**: Scenario: a forced custom notification bypasses the notification-enabled check
      * Given a host with notifications disabled
      * When a forced custom notification is sent for the host
      * Then the notification is sent despite notifications being disabled
382. **GRAPHITE_BROKER_RESTART_TEST**: 
      * Given a central broker configured with a Graphite output (same config as GRAPHITE_FORMAT_TEST)
      * And engine already running
      * When broker is started, a service check result is processed for host_16 / service_314,
      * the TCP server receives a graphite frame, then broker is killed with SIGKILL (repeated 10 times)
      * Then each cycle produces a valid graphite metric or status frame on the TCP server
      * The goal is to test cache resilience to broker crash
383. **GRAPHITE_FORMAT_TEST**: 
      * Given a central broker configured with a Graphite output using a custom metric and status path format
      * And an engine with host_16 belonging to host groups 1 and 2, service_314 belonging to service groups 4 and 5
      * And host_16 tagged with host group tag 2 and host category tag 2
      * And service_314 tagged with service group tag 4 and service category tags 4 and 5
      * And a TCP server listening on port 8000 acting as the Graphite endpoint
      * When a service check result is processed for host_16 / service_314 with perfdata metric_taratata=80%;50;75;5;99
      * Then the TCP server receives a metric message whose path contains all expanded macros:
      * instance, host id, host name, service id, service name, index id, perfdata name, max, min,
      * host groups, service groups, host/service tag names and ids
      * And the metric value is 80 with the correct timestamp
      * And the TCP server receives a status message whose path contains all the same context macros
      * And both messages include the expected Basic authentication header
384. **GRPC_CLOUD_FAILURE**: simulate a broker failure in cloud environment, we provide a muted grpc server and there must remain only one grpc connection. Then we start broker and connection must be ok
385. **GRPC_RECONNECT**: We restart broker and engine must reconnect to it and send data
386. **HOSTGRP_NOTIF_DEPENDENCY**: Scenario: host notifications are suppressed by a hostgroup dependency
      * Given a hostgroup depending on another hostgroup that has already been notified
      * When a host of the dependent hostgroup enters a DOWN state
      * Then it sends no notification because its dependency already notified
387. **HOST_DOWN_ALERT**: Scenario: a host going down raises a host alert
      * Given a host configured with notifications
      * When the host enters a DOWN HARD state
      * Then a HOST ALERT is logged
388. **HOST_DOWN_NOTIF**: Scenario: a host going down triggers a DOWN notification
      * Given a host configured with notifications
      * When the host enters a DOWN HARD state
      * Then a DOWN notification is sent to its contact
389. **HOST_DOWN_NOTIF_BROKER**: Scenario: in notification_mode=broker, Broker decides a HOST notification and dispatches its execution to the poller
      * Given a host with a contact, in notification_mode=broker (BBDO3)
      * When the host enters a DOWN HARD state
      * Then Broker dispatches the host notification execution to the supervising poller
      * And the poller runs the contact notification command (no local Engine decision)
390. **HOST_NOTIF_DEPENDENCY**: Scenario: a host notification is suppressed by a host dependency
      * Given a host depending on another host that has already been notified
      * When the dependent host enters a DOWN state
      * Then it sends no notification because its dependency already notified
391. **HOST_REC_NOTIF**: Scenario: a host recovery triggers a recovery notification
      * Given a host that sent a DOWN notification
      * When the host returns to an UP HARD state
      * Then a RECOVERY notification is sent
392. **HOST_REC_NOTIF_WITH_DT**: Scenario: host notifications across a downtime episode until recovery
      * Given a DOWN host for which a downtime is scheduled
      * When the downtime suppresses notifications, then is removed while the host is still DOWN, and the host later returns to UP
      * Then no notification is sent during the downtime, a DOWN notification is sent once it is removed, and a RECOVERY notification is sent on recovery
393. **HUGE_CONF**: 
      * Given a broker with 3 pollers, we wait that all engine are ready,
      * Then once all data are saved in db, we stop broker, we start broker and we test cache content with a lua script. 
      * We also check that we don't have cache error in broker logs
394. **INFLUXDB_FORMAT_TEST**: 
      * Given a central broker configured with an InfluxDB output using the InfluxDB line protocol
      * And an engine with host_16 belonging to host groups 1 and 2, service_314 belonging to service groups 4 and 5
      * And host_16 tagged with host group tag 2 and host category tag 2
      * And service_314 tagged with service group tag 4 and service category tags 4 and 5
      * And a TCP server listening on port 8086 acting as the InfluxDB endpoint
      * When a service check result is processed for host_16 / service_314 with perfdata metric_taratata=80%;50;75;5;99
      * Then the TCP server receives an HTTP POST to /write containing a metric line in InfluxDB line protocol
      * with measurement centreon_metric, tags host/service/instance/metric and all group/tag fields expanded
      * And the metric value is 80 with the correct timestamp
      * And a second HTTP POST is received containing a status line with measurement centreon_status
      * with value 0 and the same context fields expanded
      * And each HTTP POST is answered with HTTP/1.0 204 No Content to acknowledge the write
395. **LCDNU**: the lua cache updates correctly service cache.
396. **LCDNUH**: the lua cache updates correctly host cache
397. **LOGV2DB2**: log-v2 disabled old log disabled check broker sink
398. **LOGV2DF2**: log-v2 disabled old log disabled check logfile sink
399. **LOGV2EB1**: Checking broker sink when log-v2 is enabled and legacy logs are disabled.
400. **LOGV2EBU1**: Checking broker sink when log-v2 is enabled and legacy logs are disabled with bbdo3.
401. **LOGV2EF1**: log-v2 enabled    old log disabled check logfile sink
402. **LUA_CACHE_SAVE_BBDO3**: 
      * Given a engine broker configured in bbdo2, we check that services and hosts are stored in bbdo3 format in cache
      * To do that we compare host and service event with lua cache
403. **LUA_CACHE_SAVE_HOST_GROUP**: Scenario: the Lua broker_cache follows the host group changes of the pollers
      * Given two pollers and a Lua output that dumps the host groups of each host
      * And host group 1 holds host_1 and host_2 on poller 0
      * When host group 2 is added with host_2, then removed
      * And host_1 leaves host group 1
      * And host group 2 spans both pollers (host_2 on poller 0, host_3 and host_4 on poller 1)
      * And host group 1 is removed, then host_4 leaves host group 2
      * Then after each Engine reload the Lua cache gives each host exactly its host groups
404. **LUA_CACHE_SAVE_SERVICE_GROUP**: Scenario: the Lua broker_cache follows the service group changes of the pollers
      * Given two pollers and a Lua output that dumps the service groups of each service
      * And service group 1 holds service_1 and service_2 of host_1 on poller 0
      * When service group 2 is added with service_2, then removed
      * And service_1 leaves service group 1
      * And service group 2 spans both pollers (service_2 on poller 0, service_3 and service_4 on poller 1)
      * And service group 1 is removed, then service_4 leaves service group 2
      * And every service group is removed
      * Then after each Engine reload the Lua cache gives each service exactly its service groups
405. **LUA_FAILURE**: 
      * Given a engine broker configured with a SC, we check write intervals in case of lua write returns nothing
406. **MOVE_HOST_OF_HOSTGROUP_TO_ANOTHER_POLLER**: Scenario: Moving hosts between pollers without losing hostgroup tag
      * Given two pollers each with two hosts
      * And all hosts belong to the same hostgroup
      * When I move two hosts from one poller to the other
      * Then the hostgroup tag of the moved hosts is not erased
407. **NON_TLS_CONNECTION_WARNING**: 
      * Given an agent starts a non-TLS connection,
      * we expect to get a warning message.
408. **NON_TLS_CONNECTION_WARNING_ENCRYPTED**: 
      * Given agent with encrypted connection, we expect no warning message.
409. **NON_TLS_CONNECTION_WARNING_FULL**: 
      * Given an agent starts a non-TLS connection,
      * we expect to get a warning message.
      * After 1 hour, we expect to get a warning message about the connection time expired
      * and the connection killed.
410. **NON_TLS_CONNECTION_WARNING_FULL_REVERSED**: 
      * Given an agent starts a non-TLS connection reverse,
      * we expect to get a warning message.
      * After 1 hour, we expect to get a warning message about the connection time expired
      * and the connection killed.
411. **NON_TLS_CONNECTION_WARNING_REVERSED**: 
      * Given an agent starts a non-TLS connection reversed,
      * we expect to get a warning message.
412. **NON_TLS_CONNECTION_WARNING_REVERSED_ENCRYPTED**: 
      * Given agent with encrypted reversed connection, we expect no warning message.
413. **NO_ENGINE_ENCRYPTION**: 
      * Given an engine without configured encryption, we give him several macros and we expect to retrieve them in logs.
      * As engine is not ready to receive encrypted macros, is_encryption_ready must be equal to 0 in db
414. **NO_FILTER_NO_ERROR**: no filter configured => no filter error.
415. **OTLP_AGENT_HOST_METADATA**: Scenario: CMA host information becomes host and os resource attributes
      * Given poller 10 owns the host and link-local filtering is left disabled
      * And exports carry no host.id
      * When an AgentHostInfo event is received for the host
      * Then exports carry typed host.* and os.* resource attributes
      * And host.ip keeps the IPv4 and IPv6 link-local addresses
      * When a later AgentHostInfo event is empty
      * Then all these attributes are removed
416. **OTLP_AGENT_HOST_METADATA_PERSISTENCE**: Scenario: Broker restores CMA host information from the OTLP output cache after a crash
      * Given poller 10 owns the host
      * And an AgentHostInfo event gave it host.id robot-machine
      * And Broker saved it in the OTLP output cache
      * When Broker is killed and restarted, and the test peer sends the host again but no AgentHostInfo
      * Then exports still carry host.id robot-machine
417. **OTLP_BBDO2_MACRO_CONVERSION**: Scenario: Broker converts BBDO2 custom variable payloads
      * Given poller 10 owns the host
      * When BBDO2 configuration events set name "legacy" and namespace "initial"
      * Then exports carry legacy/initial
      * When a BBDO2 status event changes the namespace to "runtime"
      * Then exports carry legacy/runtime
      * When a disabled BBDO2 configuration event removes the name
      * Then exports carry centreon-broker/runtime
418. **OTLP_BROKER_RESTART**: Scenario: A runtime identity survives a Broker restart
      * Given host_1 is configured with name "configured"
      * When the name is changed to "runtime" at runtime
      * Then exports carry runtime/centreon
      * When Broker is restarted while Engine keeps running
      * Then exports still carry runtime/centreon
419. **OTLP_CACHE_PERSISTENCE**: Scenario: Broker restores both macros from its persisted cache
      * Given poller 10 owns the host
      * And it sends name "persisted" as configuration and namespace "runtime" as status
      * And exports carry persisted/runtime
      * When Broker is restarted and the test peer reconnects without replaying any event
      * Then exports still carry persisted/runtime
420. **OTLP_DEFAULT_IDENTITY**: Scenario: Without OTel macros, the service identity names Broker
      * Given host_1 defines neither _OTEL_SERVICE_NAME nor _OTEL_SERVICE_NAMESPACE
      * When a passive check result of service_1 is exported
      * Then its resource carries service.name centreon-broker and service.namespace centreon
      * And its resource carries a non-empty service.version
      * And centreon.service.description stays on the datapoint, not on the resource
421. **OTLP_ENGINE_RESTART_REMOVAL**: Scenario: Macros removed while Engine is stopped do not survive its restart
      * Given host_1 is configured with name and namespace "removed"
      * And exports carry removed/removed
      * When Engine is stopped, both macros are removed from its configuration and Engine is started
      * Then its startup dump restores centreon-broker/centreon
422. **OTLP_EXCLUDE_LINK_LOCAL**: Scenario: The opt-in filter removes link-local addresses from host.ip
      * Given host_ip_exclude_link_local is enabled on the OTLP output
      * When an AgentHostInfo event lists link-local and other IPv4 and IPv6 addresses
      * Then host.ip only keeps 192.0.2.10 and 2001:db8::1
423. **OTLP_HOST_AND_INSTANCE_CLEANUP**: Scenario: Owner poller startup and host deletion clear the host identity
      * Given poller 10 owns the host and sends name "configured"
      * When poller 20 starts
      * Then exports still carry configured/centreon
      * When poller 10 starts
      * Then the identity falls back to centreon-broker/centreon
      * When poller 10 sends name "deleted"
      * Then exports carry deleted/centreon
      * When poller 10 deletes the host
      * And a status update arrives before the host is created again
      * Then exports carry centreon-broker/centreon
424. **OTLP_HOST_MACROS**: Scenario: Host macros override the service identity of each host independently
      * Given host_1 defines _OTEL_SERVICE_NAME, _OTEL_SERVICE_NAMESPACE and a _SECRET macro
      * And host_2 only defines _OTEL_SERVICE_NAMESPACE
      * And service_1 defines its own _OTEL_SERVICE_NAME
      * When passive check results of both hosts are exported
      * Then host_1 carries payments/shop without service.version
      * And host_2 carries centreon-broker/production
      * And no resource carries the secret macro
      * When the service_1 macro is changed at runtime
      * Then host_1 still carries payments/shop
425. **OTLP_MAPPING_RELOAD**: Scenario: The JSON metric mapping is applied and reloaded live
      * Given a mapping file renames robot_cpu to system.cpu.utilization with a 0.01 scale
      * When a check result with robot_cpu=25% is received
      * Then the exported point is system.cpu.utilization=0.25 with unit 1 and cpu.mode user
      * When the mapping file is rewritten to rename robot_cpu to robot.cpu.percent
      * Then Broker reloads it and the next point is robot.cpu.percent=26 with unit %
      * When the mapping file is replaced by invalid JSON
      * Then Broker keeps the previous mapping and the next point is robot.cpu.percent=27
426. **OTLP_MIGRATION_WITHOUT_MACROS**: Scenario: Moving a host to a poller without macros clears its old identity
      * Given host_1 is monitored by poller 0 with name and namespace "old"
      * When host_1 moves to poller 1 without OTel macros and poller 1 is reloaded
      * Then exports through poller 1 carry centreon-broker/centreon
      * When poller 0, not reloaded yet, sends a "stale" runtime name for host_1
      * Then exports through poller 0 still carry centreon-broker/centreon
      * When poller 0 is reloaded
      * Then exports through poller 1 still carry centreon-broker/centreon
427. **OTLP_OLDER_PROTOBUF_SENDERS**: Scenario: Macro events without instance_id from older senders are still accepted
      * Given a BBDO test peer whose transport source_id is not a poller ID
      * And the host moved from poller 10 to poller 20
      * When lowercase macro events without instance_id are received
      * Then exports carry legacy/compatibility
428. **OTLP_POLLER_MIGRATION**: Scenario: Delayed updates from the old poller cannot overwrite a moved host identity
      * Given host_1 is monitored by poller 0 with name and namespace "old"
      * When host_1 moves to poller 1 with name and namespace "new" and poller 1 is reloaded
      * Then exports through poller 1 carry new/new
      * When poller 0, not reloaded yet, sends "stale" runtime updates for host_1
      * Then exports through poller 0 still carry new/new
      * When poller 0 is reloaded
      * Then exports through poller 1 still carry new/new
429. **OTLP_RELOAD_AND_REMOVAL**: Scenario: Configuration reloads update and then remove host macros
      * Given host_1 is configured with name "initial" and namespace "shop"
      * When the name is changed in the configuration and Engine is reloaded
      * Then the next export carries changed/shop
      * When the updated name is removed from the configuration and Engine is reloaded
      * Then the removal reaches Broker, since the update kept is_sent
      * And service.name falls back to centreon-broker
      * When the namespace is removed too and Engine is reloaded
      * Then the identity falls back to centreon-broker/centreon
430. **OTLP_RUNTIME_MACROS**: Scenario: Runtime host macro updates apply to the next exports
      * Given host_1 is configured with name and namespace "initial"
      * When both macros are changed at runtime with surrounding spaces
      * Then the next export carries the trimmed values payments/shop
      * When the name is changed to spaces only
      * Then service.name falls back to centreon-broker and the namespace is kept
      * When the namespace is changed to an empty value
      * Then the identity falls back to centreon-broker/centreon
431. **OTLP_STALE_CONFIGURATION_EVENTS**: Scenario: Macro events are only accepted from the poller owning the host
      * Given a BBDO test peer replaces the pollers on the Broker input
      * When macros arrive before their host, or from a poller not owning it
      * Then they are ignored and the identity stays centreon-broker/centreon
      * When poller 10 owns the host and sends name and namespace "old"
      * Then exports carry old/old
      * When the host moves to poller 20
      * Then the identity falls back to centreon-broker/centreon
      * When poller 20 sends name and namespace "new"
      * And poller 10 sends stale updates and deletions of both macros
      * Then exports still carry new/new
      * When poller 20 removes both macros
      * Then the identity falls back to centreon-broker/centreon
432. **OTLP_TLS_EXPORT**: Scenario: The OTLP output reads its CA certificate from the path in its configuration
      * Given the collector only accepts TLS, with a self-signed certificate for 127.0.0.1
      * And the OTLP output has encryption on and ca_certificate set to the path of that certificate
      * When a check result is received
      * Then it is exported to the collector over TLS
433. **OTLP_TLS_MISSING_FILE**: Scenario: A TLS file that cannot be read is reported with the parameter that names it
      * Given the OTLP output has encryption on and ca_certificate set to a path that does not exist
      * When Broker starts
      * Then Broker stops with an error
      * And it logs that it cannot read ca_certificate for the robot-otlp endpoint
434. **REC_NOTIF_OUTSIDE_TP_NOT_SENT**: Scenario: a recovery is not sent outside the notification period when the flag is off
      * Given a CRITICAL service inside its notification period
      * When the service recovers outside its notification period and send_recovery_notifications_anyway is off
      * Then the CRITICAL notification is sent but no RECOVERY notification is sent
435. **REC_NOTIF_OUTSIDE_TP_SENT_WHEN_ENABLED**: Scenario: a recovery is sent outside the notification period when the flag is on
      * Given a CRITICAL service inside its notification period
      * When the service recovers outside its notification period and send_recovery_notifications_anyway is on
      * Then both the CRITICAL and the RECOVERY notifications are sent
436. **RENAME_PARENT**: 
      * Given an host with a parent host. We rename the parent host and check if the child host is still linked to the parent.
      * Engine mustn't crash and log an error on reload.
437. **RLCode**: Test if reloading LUA code in a stream connector applies the changes
438. **RRD1**: RRD metric rebuild asked with gRPC API. Three non existing indexes IDs are selected then an error message is sent. This is done with unified_sql output.
439. **SDER**: The check attempts and the max check attempts of (host_1,service_1) are changed to 280 thanks to the retention.dat file. Then Engine and Broker are started and Broker should write these values in the services and resources tables. We only test the services table because we need a resources table that allows bigger numbers for these two attributes. But we see that Broker doesn't crash anymore.
440. **SEVERAL_FILTERS_ON_LUA_EVENT**: Two stream connectors with different filters are configured.
441. **SRVGRP_NOTIF_DEPENDENCY**: Scenario: service notifications are suppressed by a servicegroup dependency
      * Given a servicegroup depending on another servicegroup that has already been notified
      * When a service of the dependent servicegroup enters a CRITICAL state
      * Then it sends no notification because its dependency already notified
442. **SRV_CRIT_NOTIF**: Scenario: a critical service triggers a notification
      * Given a service configured with notifications enabled
      * When the service enters a non-OK HARD state
      * Then a CRITICAL notification is sent to its contact
443. **SRV_CRIT_NOTIF_BROKER**: Scenario: in notification_mode=broker, Broker decides the notification and dispatches its execution to the poller
      * Given a service with a contact, in notification_mode=broker (BBDO3)
      * When the service enters a CRITICAL HARD state
      * Then Broker dispatches the notification execution to the supervising poller
      * And the poller runs the contact notification command (no local Engine decision)
444. **SRV_NOTIF_ALLOWED_BY_WLIST**: Scenario: a service notification command allowed by the whitelist is executed
      * Given a service in a non-OK HARD state and a whitelist allowing its notification command
      * When the notification is triggered
      * Then the notification command is executed
445. **SRV_NOTIF_BLOCKED_BY_WLIST**: Scenario: a service notification command blocked by the whitelist is not executed
      * Given a service in a non-OK HARD state and a whitelist not allowing its notification command
      * When the notification is triggered
      * Then the notification command is rejected by the whitelist
446. **SRV_NOTIF_BROKER_RESTART**: Scenario: in notification_mode=broker, the notification chain survives a Broker restart
      * Given a service that sent a CRITICAL notification decided by Broker
      * When Broker is gracefully restarted (persisting and re-injecting its notification state)
      * Then on the service recovery Broker still dispatches a RECOVERY notification to the very contact told about the problem
      * proving the notification number and notified-contact set were persisted across the restart
447. **SRV_NOTIF_DEPENDENCY**: Scenario: a service notification is suppressed by a service dependency
      * Given a service depending on another service that has already been notified
      * When the dependent service enters a CRITICAL state
      * Then it sends no notification because its dependency already notified
448. **SRV_NOTIF_ESCALATIONS**: Scenario: service notifications follow the configured escalations
      * Given services with notification escalations to different contact groups
      * When the services stay CRITICAL across successive notifications
      * Then each escalation level notifies its configured contact group in turn
449. **SRV_NOTIF_ESCALATIONS_BROKER**: Scenario: in notification_mode=broker, Broker resolves the escalations and dispatches the right contact group at each level
      * Given services with notification escalations to different contact groups, in notification_mode=broker (BBDO3)
      * When the services stay CRITICAL across successive notifications
      * Then Broker (not Engine) picks the escalation contact group for each notification number
      * And the poller runs the notification command for the contacts Broker selected
450. **SRV_NOTIF_MULTIPLE_COMMANDS**: Scenario: a contact with several notification commands runs them all
      * Given a contact configured with two service notification commands
      * When a service enters a CRITICAL HARD state
      * Then a notification is sent through each configured command
451. **SRV_NOTIF_ROUTED_TO_CORRECT_CTCT**: Scenario: each service notification is routed to its own contact
      * Given two services each assigned to a different contact
      * When both services enter a CRITICAL HARD state
      * Then each contact receives the notification for its own service
452. **SRV_NOTIF_SUPPR_BY_EMPTY_TP**: Scenario: an empty notification period suppresses notifications
      * Given a CRITICAL service whose notification period is then set to none
      * When the service state changes
      * Then no notification is sent because the notifier is out of its notification period
453. **SRV_NOTIF_SUPPR_DURING_DT**: Scenario: notifications are suppressed while a service is in downtime
      * Given a service with a scheduled downtime
      * When the service enters a CRITICAL state during the downtime
      * Then no notification is sent during the downtime
      * And once the downtime is removed the CRITICAL and then RECOVERY notifications are sent
454. **SRV_NOTIF_SUPPR_DURING_DT_BROKER**: Scenario: in notification_mode=broker, notifications are suppressed while a service is in downtime
      * Given a service with a contact, in notification_mode=broker (BBDO3)
      * And a scheduled downtime set on the service via Broker gRPC
      * When the service enters a CRITICAL HARD state during the downtime
      * Then Broker does not dispatch any notification execution
      * And once the downtime is removed Broker dispatches the CRITICAL then RECOVERY notifications
455. **SRV_NOTIF_SUPPR_DURING_FLAPPING_BROKER**: Scenario: in notification_mode=broker, notifications are suppressed while a service is flapping
      * Given a passive service with a contact, in notification_mode=broker (BBDO3)
      * And flap detection enabled on that service
      * When the service flaps and then enters a CRITICAL HARD state
      * Then Broker suppresses the notification because of the flapping
      * And no execution is dispatched to the poller
      * When flap detection is disabled on the service
      * Then Broker dispatches the CRITICAL notification
456. **SRV_NOTIF_THEN_RM_AND_RELOAD**: Scenario: removing a notified service that is in downtime and reloading does not crash Engine
      * Given a service in a non-OK HARD state that has sent a notification and is under a scheduled downtime
      * When the service is removed from the configuration and Engine and Broker are reloaded
      * Then Engine keeps running without crashing
457. **SRV_REC_NOTIF**: Scenario: a service recovery triggers a recovery notification
      * Given a service that sent a CRITICAL notification
      * When the service returns to an OK HARD state
      * Then a RECOVERY notification is sent
458. **SRV_REC_NOTIF_AFTER_ACK**: Scenario: a recovery notification is sent after an acknowledged problem recovers
      * Given a CRITICAL service that has been acknowledged
      * When the service returns to an OK HARD state
      * Then a RECOVERY notification is sent
459. **SRV_REC_NOTIF_AFTER_ACK_BROKER**: Scenario: in notification_mode=broker, a recovery notification is sent after an acknowledged problem recovers
      * Given a service with a contact, in notification_mode=broker (BBDO3)
      * When the service enters CRITICAL HARD, is acknowledged, then a new CRITICAL check occurs
      * Then Broker suppresses the notification because of the acknowledgement
      * And when the service returns to OK HARD Broker dispatches the RECOVERY notification
460. **SRV_STATE_ALERTS_SOFT_AND_HARD**: Scenario: successive state changes raise SOFT then HARD service alerts
      * Given a service configured with several check attempts
      * When the service stays CRITICAL over successive checks
      * Then SOFT 1, SOFT 2 and HARD 3 service alerts are logged
461. **STORAGE_ON_LUA**: The category 'storage' is applied on the stream connector. Only events of this category should be sent to this stream.
462. **STUPID_FILTER**: Unified SQL is configured with only the bbdo category as filter. Broker should run correctly.
463. **Service_increased_huge_check_interval**: Scenario: New services with huge check interval at creation time.
      * Given Engine and Broker are configured with 1 poller and 10 hosts
      * When Engine is started
      * Then host_1 should be pending
      * When a check result with metrics is processed for service_1
      * Then metrics should be created and sent to rrd broker
      * When service_1 metrics are analyzed
      * Then metrics should have minimal heartbeat of 3000 and pdp_per_row of 300
      * When a new service is created with a check interval of 90
      * And Engine is reloaded
      * Then the new service should be pending
      * When a check result with metrics is processed for the new service
      * Then metrics should be created and sent to rrd Broker
      * When new service metrics are analyzed
      * Then metrics should have minimal heartbeat of 54000 and pdp_per_row of 5400
464. **Services_and_bulks_${id}**: One service is configured with one metric with a name of 150 to 1021 characters.
465. **Start_Stop_Broker_Engine_${id}**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
466. **Start_Stop_Engine_Broker_${id}**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
467. **UNIFIED_SQL_FILTER**: With bbdo version 3.0.1, we watch events written or rejected in unified_sql
468. **VICT_ONE_CHECK_METRIC**: victoria metrics metric output
469. **VICT_ONE_CHECK_METRIC_AFTER_FAILURE**: victoria metrics metric output after victoria shutdown
470. **VICT_ONE_CHECK_STATUS**: 
      * Given a Victoria Metrics setup with Centreon Broker
      * When a service check result is processed
      * Then the service status transitions through OK, WARNING, and CRITICAL states
      * And the corresponding status updates are sent to Victoria Metrics
471. **Whitelist_Directory_NotReadable**: 
      * Given a centengine started by centreon-engine user, whitelist directories are not readable and centengine must log an error
472. **Whitelist_Directory_Rights**: log if /etc/centreon-engine-whitelist has not mandatory rights or owner
473. **Whitelist_Empty_Directory**: log if /etc/centreon-engine-whitelist is empty
474. **Whitelist_Host**: Test on allowed and forbidden commands for hosts
475. **Whitelist_No_Whitelist_Directory**: log if /etc/centreon-engine-whitelist doesn't exist
476. **Whitelist_NotReadable**: 
      * Given a centengine started by centreon-engine user, whitelist files are not readable and centengine must log an error
477. **Whitelist_Perl_Connector**: test allowed and forbidden commands for services
478. **Whitelist_Service**: test allowed and forbidden commands for services
479. **Whitelist_Service_EH**: test allowed and forbidden event handler for services
480. **metric_mapping**: Check if metric name exists using a stream connector
481. **not1_2**: Scenario: Service notification command expands contact macros
      * Given a single service 'service_1' is assigned to contact 'John_Doe' with notifications enabled, options (w,c,r), and period 24x7
      * And 'service_1' has check/retry intervals of 1 and max check attempts of 1
      * And contact 'John_Doe' uses notification command 'command_with_macro' with macros
      * When the broker and engine are started and the service is driven to a HARD CRITICAL state
      * Then the service reaches HARD CRITICAL
      * And a service notification is logged for 'service_1' in CRITICAL state using 'command_with_macro'
      * And the processed notification command in logs is "/bin/echo pager address5 " showing macros are expanded
482. **not21**: No notifications should be sent for UNKNOWN services when host goes DOWN. When a host transitions to DOWN (soft or hard), services become UNKNOWN due to host unreachability. These services should not trigger notifications.
483. **not_remove_host_and_service_in_downtime**: 
      * Given a service in a fixed downtime whose notification options contain 's'
      * When the service and its host are removed from the configuration and engine is reloaded
      * Then the host is removed before the service, so the service downtime is cancelled
      * and a DOWNTIMECANCELLED notification is sent for a service without host
      * And engine does not crash in grab_host_macros_r()

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
1. **BECFGVAL1_${lck_mode}**: Scenario: PHP pushes an invalid poller configuration without asking for a CheckPollerConfig
      * Given a centralized engine configuration where contact U1 has no host_notification_commands
      * And Broker is started in centralized mode
      * When the configuration change is notified to Broker (no CheckPollerConfig call)
      * Then Broker refuses to push the configuration to the poller
      * And it does not store the poller .prot configuration
2. **BECFGVAL2_${lck_mode}**: Scenario: PHP pushes a poller configuration with a contact group referencing an undefined contact
      * Given a centralized engine configuration with a contact group whose member does not exist
      * And Broker is started in centralized mode
      * When the configuration change is notified to Broker (no CheckPollerConfig call)
      * Then Broker refuses to push the configuration to the poller
      * And it does not store the poller .prot configuration
3. **BECFGVAL3_${lck_mode}**: Scenario: PHP pushes a poller configuration with a host dependency referencing an undefined host
      * Given a centralized engine configuration with a host dependency whose dependent host does not exist
      * And Broker is started in centralized mode
      * When the configuration change is notified to Broker (no CheckPollerConfig call)
      * Then Broker refuses to push the configuration to the poller
      * And it does not store the poller .prot configuration
4. **BECFGVAL4_${lck_mode}**: Scenario: PHP pushes a poller configuration with a service dependency referencing an undefined service
      * Given a centralized engine configuration with a service dependency whose dependent service does not exist
      * And Broker is started in centralized mode
      * When the configuration change is notified to Broker (no CheckPollerConfig call)
      * Then Broker refuses to push the configuration to the poller
      * And it does not store the poller .prot configuration
5. **BECFGVAL5_${lck_mode}**: Scenario: PHP pushes a poller configuration with a host escalation referencing an undefined contact group
      * Given a centralized engine configuration with a host escalation whose contact group does not exist
      * And Broker is started in centralized mode
      * When the configuration change is notified to Broker (no CheckPollerConfig call)
      * Then Broker refuses to push the configuration to the poller
      * And it does not store the poller .prot configuration
6. **BECFGVAL6_${lck_mode}**: Scenario: PHP pushes a poller configuration with a service escalation referencing an undefined contact group
      * Given a centralized engine configuration with a service escalation whose contact group does not exist
      * And Broker is started in centralized mode
      * When the configuration change is notified to Broker (no CheckPollerConfig call)
      * Then Broker refuses to push the configuration to the poller
      * And it does not store the poller .prot configuration
7. **BECFGVAL7_${lck_mode}**: Scenario: PHP pushes a poller configuration with a host referencing an undefined notification period
      * Given a centralized engine configuration where a host has a non-existing notification period
      * And Broker is started in centralized mode
      * When the configuration change is notified to Broker (no CheckPollerConfig call)
      * Then Broker refuses to push the configuration to the poller
      * And it does not store the poller .prot configuration
8. **BECFGVAL8_${lck_mode}**: Scenario: PHP pushes a poller configuration with a service referencing an undefined notification period
      * Given a centralized engine configuration where a service has a non-existing notification period
      * And Broker is started in centralized mode
      * When the configuration change is notified to Broker (no CheckPollerConfig call)
      * Then Broker refuses to push the configuration to the poller
      * And it does not store the poller .prot configuration
9. **BECFGVAL9_${lck_mode}**: Scenario: PHP pushes a valid configuration for a poller that is not connected
      * Given a valid centralized engine configuration for poller 1
      * And Broker is started in centralized mode while Engine is left stopped
      * When the configuration change is notified to Broker
      * Then Broker prepares and stores the poller configuration once
      * And it consumes the .lck, new-1.prot being what says the delivery is pending
      * And nothing at all happens about that poller on the following cycles
10. **BECNHG1**: Scenario: Host group synchronization across 3 pollers in centralized configuration
      * Given a centralized engine with 3 pollers
      * And broker is configured with RRD, central module, and SQL debug logging
      * And database connections are set to 5 for both SQL and perfdata outputs
      * When I start the broker and engine with new generation
      * And I add a host group containing 3 hosts (host_1, host_2, host_3)
      * And I notify broker of the engine configuration change
      * Then the logs should confirm membership of all 3 hosts to the host group
      * And each host should be properly associated with host group 1 on instance 1
11. **BECNHG3_${lck_mode}**: Scenario: Host group synchronization across 4 pollers in centralized configuration
      * Given 4 pollers and Broker are started in centralized mode
      * When hostgroup_1 is added with 3 hosts per poller (12 total)
      * Then Broker receives all 12 hosts as hostgroup_1 members
      * When hostgroup configuration files are removed sequentially from each poller
      * Then Broker progressively removes corresponding hosts from database
      * And hostgroup_1 membership decreases from 12 → 9 → 6 → 3 → 0
      * And this holds whichever shape announces the export, pollers.lck or <ID>.lck
12. **BECNHG4**: Scenario: Host group rename synchronization in centralized configuration
      * Given 3 pollers and Broker are started in centralized mode
      * When hostgroup_1 is created on poller 1 with hosts: host_1, host_2, host_3
      * Then Broker receives hostgroup_1 with its 3 members
      * When hostgroup_1 is renamed to hostgroup_test
      * Then Broker updates the hostgroup name to hostgroup_test in database
      * And the same 3 hosts remain as members of hostgroup_test
13. **BECNHG5**: Scenario: Host group removal and recreation with same hosts in centralized configuration
      * Given 3 pollers and Broker are started in centralized mode
      * When hostgroup_1 is created on each poller with different hosts
      * Then Broker receives all hostgroups with their respective members
      * When hostgroup_1 is removed from poller 1 and hostgroup_2 is created with the same hosts
      * Then Broker updates the database to reflect the changes
      * And hostgroup_2 contains the 3 hosts from poller 1
      * And hostgroup_1 still contains the 6 hosts from pollers 2 and 3
14. **BECNSG1**: Scenario: Service group creation and synchronization in centralized configuration
      * Given 3 pollers and Broker are started in centralized mode
      * When a service group is created on poller 1 with 3 services from host_1
      * Then Broker receives the service group configuration
      * And the 3 services are registered as members of the service group in logs
15. **BECNSG2_${lck_mode}**: Feature: Service Groups Management with Unified SQL Database
16. **BECNSG3_${lck_mode}**: Scenario: A servicegroup spread over three pollers is followed by the Lua cache through its whole life
      * Given a central broker with a Lua output dumping the groups held in its cache
      * And three pollers in centralized configuration
      * When a servicegroup gathering services of the three pollers is added and announced
      * Then the database holds its nine members and the Lua cache reports it by name
      * When the servicegroup is renamed on the three pollers and announced again
      * Then the database and the Lua cache both report the new name
      * When the servicegroup is removed from the three pollers and announced again
      * Then the database no longer holds any of its members
      * And the Lua cache no longer reports any service group
      * Note: the centralized configuration currently breaks the broker cache.
17. **BECNSVC1**: 
      * Given a Centreon platform with 3 pollers configured
      * And 50 hosts distributed across pollers (17+17+16)
      * And initially 20 services per host
      * And BBDO3 protocol with unified SQL output enabled
18. **BECNTAG1_${lck_mode}**: Feature: Tag associations in Broker gRPC cache with centralized configuration
19. **BECNTAG2_${lck_mode}**: Feature: Tag rename is reflected in the Broker gRPC cache
20. **BECNTAG3_${lck_mode}**: Feature: GetTags gRPC returns correct content while tags are active
21. **BECNTAG4_${lck_mode}**: Feature: Broker cache is repopulated after broker restart with tags active
22. **BECPN0**: Feature: Parent-Child Host Dependency Management As a monitoring administrator I want child host checks to be queued when parent hosts are down So that unnecessary checks are avoided
23. **BECPN1_${lck_mode}**: Feature: Parent Host Deletion Management As a monitoring administrator I want parent-child relationships to be cleaned up when parent hosts are deleted So that orphaned relationships don't exist in the system
24. **BECPN2_${lck_mode}**: Feature: Child Host Deletion Management As a monitoring administrator I want parent-child relationships to be cleaned up when child hosts are deleted So that orphaned relationships don't exist in the system
25. **BECSS1**: Scenario: Broker sends configuration to engine in new generation
      * Given an engine configuration is provided to the broker
      * And the broker and engine are started in new generation (broker first)
      * And the protocol is bbdo3
      * When the broker detects the configuration for the engine
      * Then the broker sends the configuration to the engine
      * Then both broker and engine are stopped (engine first)
26. **BECSS2**: Scenario: Broker sends configuration to engine in new generation
      * Given an engine configuration is provided to the broker
      * And the broker and engine are started in new generation (broker first)
      * And the protocol is bbdo3
      * When the broker detects the configuration for the engine
      * Then the broker sends the configuration to the engine
      * Then both broker and engine are stopped (engine first)
27. **BECSS3**: Scenario: Broker sends configuration to engine in new generation
      * Given an engine configuration is provided to the broker
      * And the broker and engine are started in new generation (engine first)
      * And the protocol is bbdo3
      * When the broker detects the configuration for the engine
      * Then the broker sends the configuration to the engine
      * Then both broker and engine are stopped (engine first)
28. **BECSS4**: Scenario: Broker sends configuration to engine in new generation
      * Given an engine configuration is provided to the broker
      * And the broker and engine are started in new generation (engine first)
      * And the protocol is bbdo3
      * When the broker detects the configuration for the engine
      * Then the broker sends the configuration to the engine
      * Then both broker and engine are stopped (broker first)
29. **BECSSBQ1**: A very bad queue file is written for broker. Broker and Engine are then started, Broker must read the file raising an error because of that file and then get data sent by Engine.
30. **BECSS_CRYPTED_GRPC1**: Scenario: Repeated start/stop cycles with gRPC and mutual TLS in centralized configuration mode
31. **BECSS_CRYPTED_GRPC2**: Start-Stop grpc version Broker/Engine only server crypted
32. **BECSS_CRYPTED_REVERSED_GRPC1**: Start-Stop grpc version Broker/Engine - well configured
33. **BECSS_CRYPTED_REVERSED_GRPC2**: Start-Stop grpc version Broker/Engine only engine server crypted
34. **BECSS_CRYPTED_REVERSED_GRPC3**: Start-Stop grpc version Broker/Engine only engine crypted
35. **BECSS_ENGINE_DELETE_HOST**: once engine and cbd started, stop and restart cbd, delete an host and reload engine, cbd mustn't core
36. **BECSS_GRPC1**: Scenario: Broker sends configuration to engine in new generation
      * Given an engine configuration is provided to the broker
      * And the broker and engine are started in new generation (broker first)
      * And the protocol is bbdo3
      * When the broker detects the configuration for the engine
      * Then the broker sends the configuration to the engine
      * Then both broker and engine are stopped (engine first)
37. **BECSS_GRPC2**: Scenario: Broker sends configuration to engine in new generation
      * Given an engine configuration is provided to the broker
      * And the broker and engine are started in new generation (broker first)
      * And the protocol is bbdo3
      * When the broker detects the configuration for the engine
      * Then the broker sends the configuration to the engine
      * Then both broker and engine are stopped (engine first)
38. **BECSS_GRPC3**: Scenario: Broker sends configuration to engine in new generation
      * Given an engine configuration is provided to the broker
      * And the broker and engine are started in new generation (engine first)
      * And the protocol is bbdo3
      * When the broker detects the configuration for the engine
      * Then the broker sends the configuration to the engine
      * Then both broker and engine are stopped (engine first)
39. **BECSS_GRPC4**: Scenario: Broker sends configuration to engine in new generation
      * Given an engine configuration is provided to the broker
      * And the broker and engine are started in new generation (engine first)
      * And the protocol is bbdo3
      * When the broker detects the configuration for the engine
      * Then the broker sends the configuration to the engine
      * Then both broker and engine are stopped (broker first)
40. **BECSS_GRPC_COMPRESS1**: Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped last compression activated
41. **BECTAG1**: Feature: Tag Management between Engine and Broker As a Centreon administrator I want to configure tags in Engine So that Broker stores them correctly in centreon_storage.tags table
42. **BECWATCH1**: Scenario: PHP notifies several poller configurations in a burst
      * Given a centralized platform with 3 pollers, all connected
      * When three changed configurations are notified one after another
      * Then Broker handles the whole burst as a single batch
      * And it reads the stored poller configurations only once for it
      * When the same three configurations are notified again, unchanged
      * Then that burst is a single batch too
43. **BECWATCH2_${lck_mode}**: Scenario: the watched cache directory is moved out of the way and back
      * Given a centralized platform with 1 poller and Broker started
      * When the cache directory is renamed, so the inotify watch is lost
      * Then Broker reports the loss and cannot establish the watch again
      * When the directory is put back
      * Then Broker establishes the watch again without waiting for the slow period
      * And a configuration pushed afterwards is detected, whichever shape
      * announces it -- pollers.lck or <ID>.lck
44. **BECWATCH3**: Scenario: PHP announces an export through the poller batch file
      * Given a centralized platform with 3 pollers, all connected
      * When the three configurations are announced by a single pollers.lck
      * And no individual <id>.lck is written
      * Then Broker handles the three of them in one pass
      * And it reads the stored poller configurations only once
      * And it consumes the batch file
      * And this holds whether the file was renamed into place or written directly
45. **BECWATCH4_${lck_mode}**: Scenario: two hosts swap pollers within a single export
      * Given a centralized platform with 2 pollers of 5 hosts each
      * When host_1 moves to poller 2 and host_6 moves to poller 1
      * And both configurations are announced together, by one pollers.lck or
      * by one <ID>.lck each
      * Then the global diff turns each move into a modification, not a removal
      * And both hosts stay enabled, each attached to its new poller
46. **CANO_CFG_SENSITIVITY_SAVED**: 
      * Given an anomaly detection service is configured with a specific sensitivity value in configuration
      * And the threshold file contains prediction data with sensitivity parameters
      * When the engine and broker are started and then stopped
      * Then the configuration-based sensitivity value should be persisted in the retention data
      * because CFG sensitivity parameters are properly saved during retention processing
47. **CANO_DT1**: 
      * Given an anomaly detection service is configured with a dependent service relationship
      * And both services are running normally
      * When a downtime is scheduled on the dependent service
      * Then the dependent service should enter downtime state
      * And the anomaly detection service should automatically inherit the downtime
      * because anomaly detection services inherit downtime from their dependent services
48. **CANO_DT2**: 
      * Given an anomaly detection service is configured with a dependent service relationship
      * And both services are running normally
      * When a downtime is scheduled on the dependent service
      * Then the anomaly detection service should automatically enter downtime
      * When the downtime is deleted from the dependent service
      * Then the anomaly detection service should automatically exit downtime
      * because anomaly detection downtime should follow its dependent service downtime state
49. **CANO_DT3**: 
      * Given an anomaly detection service is configured with a dependent service relationship
      * And both services are running normally
      * When a downtime is scheduled on the dependent service
      * Then the anomaly detection service should automatically enter downtime
      * When the downtime is deleted from the anomaly detection service
      * Then the dependent service should remain in its original downtime state
      * because deleting downtime on anomaly detection should not affect dependent service downtimes
50. **CANO_DT4**: Scenario: Removing downtime from service keeps it on anomaly detection
      * Given an anomaly detection is attached to a service
      * And a downtime is set on both the service and the anomaly detection
      * When the downtime is removed from the service
      * Then the downtime should still be present on the anomaly detection
51. **CANO_EXTCMD_SENSITIVITY_SAVED**: 
      * Given an anomaly detection service is configured with threshold data
      * And the service is running with initial sensitivity parameters
      * When an external command updates the anomaly sensitivity value
      * And the engine and broker are stopped
      * Then the updated sensitivity value should be persisted in the retention data
      * because external command sensitivity changes are properly saved during retention processing
52. **CANO_JSON_SENSITIVITY_NOT_SAVED**: 
      * Given an anomaly detection service is configured with threshold data including sensitivity
      * And the threshold file contains prediction data with a specific sensitivity value
      * When the engine and broker are started and then stopped
      * Then the sensitivity value should not be persisted in the retention data
      * because JSON sensitivity parameters are not saved during retention processing
53. **CANO_NOFILE**: 
      * Given an anomaly detection service is configured for metric monitoring
      * and the threshold configuration file is missing from the system
      * when the service processes a check result with critical state
      * then the anomaly detection service must transition to UNKNOWN state
      * because it cannot determine thresholds without the configuration file
54. **CANO_OUT_LOWER_THAN_LIMIT**: 
      * Given an anomaly detection service is configured with valid threshold data
      * And the threshold file contains lower and upper limits for the metric
      * When a service check provides performance data below the lower threshold limit
      * Then the anomaly detection service must transition to CRITICAL state
      * because the metric value indicates an anomalous condition requiring attention
55. **CANO_OUT_UPPER_THAN_LIMIT**: 
      * Given an anomaly detection service is configured with valid threshold data
      * And the threshold file contains lower and upper limits for the metric
      * When a service check provides performance data above the upper threshold limit
      * Then the anomaly detection service must transition to CRITICAL state
      * because the metric value indicates an anomalous condition requiring attention
56. **CANO_TOO_OLD_FILE**: 
      * Given an anomaly detection service is configured with metric monitoring
      * And a threshold file exists but contains outdated prediction data
      * When the service processes a check result with performance data
      * Then the anomaly detection service must transition to UNKNOWN state
      * because the threshold data is too old to be reliable for current predictions
57. **CAOUTLU1**: 
      * Given an anomaly detection service is configured with valid threshold data using BBDO3 protocol
      * And the threshold file contains lower and upper limits for the metric
      * When a service check provides performance data above the upper threshold limit
      * Then the anomaly detection service must transition to CRITICAL state
      * And the resources table should contain SERVICE, HOST and ANOMALY_DETECTION type entries
58. **CBEUDHOSTS**: 
      * Given a Centreon platform with 3 pollers configured
      * And 50 hosts distributed across pollers (17+17+16)
      * And initially 20 services per host
      * And BBDO3 protocol with unified SQL output enabled
59. **CCCRC1**: 
      * Given a topology Poller1 -> Relay1 -> central cbd
      * When Engine connects to the relay
      * Then the relay sends a ConfigRequest to the central for poller 1
      * And the central logs the receipt of that ConfigRequest.
60. **CCCRC2_${lck_mode}**: Scenario: The central answers a relay asking for a configuration it already holds
      * Given a topology Poller1 -> Relay3 -> central cbd
      * And a poller configuration is pre-created before starting the central broker
      * When the central processes the configuration and the relay sends a ConfigRequest
      * Then the central sends a non-unknown DiffState to the relay.
61. **CCCRC3_${lck_mode}**: Scenario: A relay forwards the configuration to Engine and its acknowledgement back to the central
      * Given a topology Poller1 -> Relay3 -> central cbd
      * And a poller configuration is pre-created before starting the central broker
      * When Engine connects through the relay and the central sends a DiffState
      * Then the relay forwards the DiffState to Engine
      * And the relay forwards the DiffStateAck back to the central.
62. **CCCRC4_${lck_mode}**: Scenario: A configuration pushed after the initial one reaches Engine through the relay
      * Given a topology Poller1 -> Relay3 -> central cbd
      * And a poller configuration is pre-created before starting central
      * When Engine connects and gets the initial config via relay
      * And PHP pushes a new config for poller 1 (5 extra hosts)
      * Then the central sends a new DiffState to the relay
      * And the central receives a new DiffStateAck.
63. **CCCRC5_${lck_mode}**: Scenario: Engine migrating from one relay to another is served through the new one
      * Given Engine initially connected to central via Relay3 (poller_id=4)
      * When Engine migrates to Relay4 (poller_id=5)
      * Then the central sends ConfigRevoke to Relay3
      * And serves the configuration to Engine via Relay4.
64. **CCCRC6_${lck_mode}**: Scenario: A configuration pushed while the central is down is served once it is back
      * Given Engine connected via Relay3 with initial config established
      * When the central is stopped cleanly and a new config is pushed during the outage
      * Then after the central restarts, the relay reconnects and the new DiffState
      * is forwarded to Engine via the relay, and central receives a new DiffStateAck.
65. **CCCRC7_${lck_mode}**: Scenario: GetTopology reports the relay and the poller sitting behind it
      * Given Engine connected via Relay3 with initial config established
      * When GetTopology is called on the central gRPC endpoint
      * Then the response contains Relay3 as a direct broker with poller 1 as its poller.
66. **Centralized_Start_Stop_Broker_Engine_${id}**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
67. **Centralized_Start_Stop_Engine_Broker_${id}**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
68. **RENAME_PARENT_${lck_mode}**: Feature: Parent Host Rename Management As a monitoring administrator I want parent-child relationships to be maintained when parent hosts are renamed So that dependencies remain intact after configuration changes

### Connector perl
1. **CCONPERL**: Scenario: Single host check via Perl Connector in centralized configuration
      * Given a centralized engine and broker configuration with the Perl Connector
      * When a forced host check is scheduled on host_1
      * Then the check execution result should appear in the engine log file.
2. **CCONPERLM**: Scenario: Ten host checks via Perl Connector in centralized configuration
      * Given a centralized engine and broker configuration with the Perl Connector on ten hosts
      * When a forced check is scheduled on each of the ten hosts
      * Then the check execution result for each host should appear in the engine log file.
3. **CONPERL**: The test.pl script is launched using the perl connector. Then we should find its execution in the engine log file.
4. **CONPERLM**: Ten forced checks are scheduled on ten hosts configured with the Perl Connector. The we get the result of each of them.

### Connector ssh
1. **CTest6Hosts**: Scenario: SSH checks succeed on 6 hosts in centralized configuration
      * Given a centralized engine and broker configuration with 6 hosts reachable via SSH
      * When forced checks are scheduled on all 6 hosts
      * Then the expected output for each host address should appear in the log.
2. **CTestBadPwd**: Scenario: SSH check with wrong password fails in centralized configuration
      * Given a centralized engine and broker configuration with a wrong SSH password on host_1
      * When a forced host check is scheduled
      * Then a connection failure message for the bad password should appear in the log.
3. **CTestBadUser**: Scenario: SSH check with unknown user fails in centralized configuration
      * Given a centralized engine and broker configuration with an unknown SSH user on host_1
      * When a forced host check is scheduled
      * Then a connection failure message for the unknown user should appear in the log.
4. **CTestWhiteList**: Scenario: SSH check blocked then allowed by whitelist in centralized configuration
      * Given a centralized engine and broker configuration with a whitelist restricting SSH checks
      * When a forced host check is scheduled and the command is not whitelisted
      * Then a security restriction message should appear in the log.
      * When the whitelist is updated to allow the SSH command
      * Then the check should succeed and the expected output should appear in the log.
5. **Test6Hosts**: as 127.0.0.x point to the localhost address we will simulate check on 6 hosts
6. **TestBadPwd**: test bad password
7. **TestBadUser**: test unknown user
8. **TestWhiteList**: as 127.0.0.x point to the localhost address we will simulate check on 6 hosts

### Engine
1. **CEBSN5**: 
      * Given a centralized Engine configuration where contactgroup_1 is empty and inherits from a full template
      * And the template defines alias, members, and contactgroup_members
      * When Engine and Broker are started
      * Then contactgroup_1 resolves with the template's alias, members, and sub-groups
2. **CEBSN6**: 
      * Given a centralized Engine configuration where contactgroup_1 is full and inherits from a full template
      * And both the group and template define alias, members, and contactgroup_members
      * When Engine and Broker are started
      * Then contactgroup_1's own values take precedence over the template's values
3. **CEBSN7**: 
      * Given a centralized Engine started with contactgroup_1 having only one member
      * And after start, contactgroup_1 is modified to be empty and inherit from a full template
      * When the new configuration is sent to Engine via Broker notification
      * Then contactgroup_1 resolves with the template's alias, members, and sub-groups
4. **CEBSN8**: 
      * Given a centralized Engine started with contactgroup_1 having one member
      * And after start, contactgroup_1 is modified to be full and inherit from a full template
      * When the new configuration is sent to Engine via Broker notification
      * Then contactgroup_1's own values take precedence and template values for overlapping fields are not used
5. **CECI0**: 
      * Given a centralized Engine configuration where John_Doe is empty and inherits from a full contact template
      * And the template defines all notification settings, addresses, and custom variables
      * When Engine and Broker are started
      * Then John_Doe resolves with all values from the template
6. **CECI1**: 
      * Given a centralized Engine configuration where John_Doe is full and inherits from a full contact template
      * And both the contact and template define all notification settings and addresses
      * When Engine and Broker are started
      * Then John_Doe's own values take precedence over the template's values for all fields
7. **CECI2**: 
      * Given a centralized Engine started with a minimal contact configuration
      * And after start, John_Doe is made empty and a full contact template is added
      * When the new configuration is sent to Engine via Broker notification
      * Then John_Doe resolves with all values from the template
8. **CECI3**: 
      * Given a centralized Engine started with a minimal contact configuration
      * And after start, John_Doe is made full with its own values and a full contact template is added
      * When the new configuration is sent to Engine via Broker notification
      * Then John_Doe's own values take precedence over the template's values for all fields
9. **CECMI0**: 
      * Given a centralized Engine configuration with a command template having a command_line
      * And the command inherits from the template with its own command_line deleted
      * When Engine and Broker are started
      * Then the command's resolved command_line matches the template value
10. **CECMI1**: 
      * Given a centralized Engine configuration with a command template having a command_line
      * And the command inherits from the template but keeps its own command_line
      * When Engine and Broker are started
      * Then the command's resolved command_line is the command's own value, not the template's
11. **CECMI2**: 
      * Given a centralized Engine already started with a basic configuration
      * And a command template with a command_line is added with the command inheriting from it and its own command_line deleted
      * When the new configuration is sent via Broker notification
      * Then the command's resolved command_line matches the template value
12. **CECMI3**: 
      * Given a centralized Engine already started with a basic configuration
      * And a command template with a command_line is added with the command inheriting from it while keeping its own command_line
      * When the new configuration is sent via Broker notification
      * Then the command's resolved command_line is the command's own value, not the template's
13. **CECOI0**: 
      * Given a centralized Engine configuration with a connector template having a connector_line
      * And the connector inherits from the template with its own connector_line deleted
      * When Engine and Broker are started
      * Then the connector's resolved connector_line matches the template value
14. **CECOI1**: 
      * Given a centralized Engine configuration with a connector template having a connector_line
      * And the connector inherits from the template but keeps its own connector_line
      * When Engine and Broker are started
      * Then the connector's resolved connector_line is the connector's own value, not the template's
15. **CECOI2**: 
      * Given a centralized Engine already started with a basic configuration
      * And a connector template with a connector_line is added with the connector inheriting from it and its own connector_line deleted
      * When the new configuration is sent via Broker notification
      * Then the connector's resolved connector_line matches the template value
16. **CECOI3**: 
      * Given a centralized Engine already started with a basic configuration
      * And a connector template with a connector_line is added with the connector inheriting from it while keeping its own connector_line
      * When the new configuration is sent via Broker notification
      * Then the connector's resolved connector_line is the connector's own value, not the template's
17. **CEESI0**: 
      * Given Engine is configured with centralized configuration
      * And a service escalation is defined for a service group containing host_1..3/service_1..3
      * And a host group containing host_6 and host_7 is defined
      * When Broker and Engine are started
      * Then each service in the service group gets the escalation applied
      * And services outside the service group have no escalation
18. **CEESI1**: 
      * Given Engine is configured with centralized configuration
      * And a service escalation with no fields inherits from a full service escalation template
      * When Broker and Engine are started
      * Then services in the service group get the escalation settings from the template on engine start
19. **CEESI2**: 
      * Given Engine is configured with centralized configuration
      * And a service escalation with full fields inherits from a full service escalation template
      * When Broker and Engine are started
      * Then the escalation own values take precedence over template values on engine start
      * And services outside the service group have no escalation
20. **CEESI3**: 
      * Given Engine is configured with centralized configuration and started
      * And a service escalation with no fields is changed to inherit from a full service escalation template
      * When Broker notifies Engine of the new configuration
      * Then services in the new template's service group get the escalation settings
      * And services in the old service group have no escalation after the configuration change
21. **CEESI4**: 
      * Given Engine is configured with centralized configuration and started
      * And a service escalation with full fields is changed to also inherit from a full service escalation template
      * When Broker notifies Engine of the new configuration
      * Then the escalation own values take precedence over template values after the configuration change
      * And services outside the escalation's service group have no escalation
22. **CEESI5**: 
      * Given Engine is configured with centralized configuration
      * And a host escalation is defined for a host group containing host_1, host_2, and host_3
      * When Broker and Engine are started
      * Then each host in the host group gets the escalation applied
23. **CEESI6**: 
      * Given Engine is configured with centralized configuration
      * And a host escalation with no fields inherits from a full host escalation template
      * When Broker and Engine are started
      * Then hosts in the host group get all escalation settings from the template on engine start
24. **CEESI7**: 
      * Given Engine is configured with centralized configuration
      * And a host escalation with full fields inherits from a full host escalation template
      * When Broker and Engine are started
      * Then the escalation own values take precedence over template values for hosts in its group
      * And hosts in the template's host group have no escalation on engine start
25. **CEESI8**: 
      * Given Engine is configured with centralized configuration and started
      * And a host escalation with no fields is changed to inherit from a full host escalation template
      * When Broker notifies Engine of the new configuration
      * Then hosts in the new template's host group get the escalation settings
      * And hosts in the old host group have no escalation after the configuration change
26. **CEESI9**: 
      * Given Engine is configured with centralized configuration and started
      * And a host escalation with full fields is changed to also inherit from a full host escalation template
      * When Broker notifies Engine of the new configuration
      * Then the escalation own values take precedence over template values for hosts in its group after the configuration change
      * And hosts in the template's host group have no escalation after the configuration change
27. **CEFHCU1**: 
      * Given Engine is configured with hosts in centralized mode
      * When we force check one host 5 times
      * Then the host transitions through SOFT and HARD DOWN states
      * And the resources table is cleared before starting broker
28. **CEFHCU2**: 
      * Given Engine is configured with hosts in centralized mode
      * When we force check one host 5 times
      * Then the host transitions through SOFT and HARD DOWN states
29. **CEHGI0**: 
      * Given a hostgroup with no fields inheriting from a full template
      * When Engine starts with centralized configuration
      * Then the hostgroup alias, notes, notes_url, action_url and members are inherited from the template
30. **CEHGI1**: 
      * Given a full hostgroup inheriting from a full template
      * When Engine starts with centralized configuration
      * Then the hostgroup's own fields take precedence over the template's fields
31. **CEHGI2**: 
      * Given a hostgroup with no fields inheriting from a full template
      * When Broker notifies Engine of new centralized configuration
      * Then the hostgroup alias, notes, notes_url, action_url and members are inherited from the template
32. **CEHGI3**: 
      * Given a full hostgroup inheriting from a full template
      * When Broker notifies Engine of new centralized configuration
      * Then the hostgroup's own fields take precedence over the template's fields
33. **CEHI0**: 
      * Given a centralized Engine configuration with an empty host inheriting from a full template
      * When Engine and Broker are started in newGeneration mode
      * Then the host should inherit all fields from the template
34. **CEHI1**: 
      * Given a centralized Engine configuration with a full host inheriting from a full template
      * When Engine and Broker are started in newGeneration mode
      * Then the host's own values should take precedence over the template values
35. **CEHI2**: 
      * Given a centralized Engine configuration with an empty host inheriting from a full template
      * When Broker notifies Engine of the new centralized configuration
      * Then the host should inherit all fields from the template
36. **CEHI3**: 
      * Given a centralized Engine configuration with a fully configured host inheriting from a full template
      * When Broker notifies Engine of the new centralized configuration
      * Then the host's own values take precedence over the template values
37. **CEMACROS**: 
      * Given Engine is configured with centralized configuration
      * When a forced service check is scheduled
      * Then the macros ADMINEMAIL and ADMINPAGER are replaced in check outputs
38. **CEMACROS_NOTIF**: 
      * Given Engine is configured with centralized configuration
      * When a service enters a critical state triggering a notification
      * Then the macros ADMINEMAIL and ADMINPAGER are replaced in notification commands
39. **CEMACROS_SEMICOLON**: 
      * Given Engine is configured with centralized configuration
      * When a forced service check is scheduled with a macro containing a semicolon
      * Then the macro value with semicolons is correctly expanded in check outputs
40. **CEMTI0**: 
      * Given a host using a chain of 4 template levels each defining a custom variable
      * When Engine starts with centralized configuration
      * Then all custom variables from every template level are present on the host
41. **CENGINE_MANY_CHECKS**: 
      * Given Engine is configured in centralized mode with many services and a unique check on each service with its own env variables
      * When Broker sends the configuration to Engine and all checks are executed
      * Then the correct check results are found in logs with expected args and service macros
42. **CEPC1**: 
      * Given Engine is configured with a Perl connector
      * When Engine starts
      * Then the Perl connector is launched and data becomes available
43. **CERL**: 
      * Given Engine is started and writing logs to centengine.log
      * When the log file is removed
      * Then Engine continues running but the log file is gone
      * And when Engine is reloaded the centengine.log file is recreated
44. **CERS1**: Scenario: an Engine restarted without retention.dat gets its resources state back from Broker
      * Given Broker and Engine are started in centralized configuration, with passive services
      * And service_1 of host_1 is CRITICAL HARD
      * When Engine is stopped, its retention.dat is deleted and Engine is started again
      * Then Broker, finding the poller up to date, sends it its runtime state alone
      * And Engine restores the state of its hosts and services from that snapshot
      * And an OK result on service_1 is logged as a recovery from CRITICAL, which only a restored state allows
45. **CERS2**: Scenario: a Broker that lost a poller's configuration rebuilds it from Engine and still hands the poller its runtime state
      * Given Broker and Engine are started in centralized configuration, with passive services
      * And service_1 of host_1 is CRITICAL HARD
      * When Broker is stopped, its stored poller configuration (1.prot) is deleted and Broker is started again, Engine still running
      * Then Broker asks Engine for its configuration, stores it, rebuilds the poller's cache entries and restores their runtime from the database
      * And only then sends the poller its runtime state alone, which Engine applies
46. **CESGI0**: 
      * Given a servicegroup with no fields inheriting from a full template
      * When Engine starts with centralized configuration
      * Then the servicegroup alias, notes, notes_url, action_url and members are inherited from the template
47. **CESGI1**: 
      * Given a full servicegroup inheriting from a full template
      * When Engine starts with centralized configuration
      * Then the servicegroup's own fields take precedence over the template's fields
48. **CESGI2**: 
      * Given a servicegroup with no fields inheriting from a full template
      * When Broker notifies Engine of the new centralized configuration
      * Then the servicegroup alias, notes, notes_url, action_url and members are inherited from the template
49. **CESGI3**: 
      * Given a full servicegroup inheriting from a full template
      * When Broker notifies Engine of the new centralized configuration
      * Then the servicegroup's own fields take precedence over the template's fields
50. **CESI0**: 
      * Given Engine is configured with centralized configuration
      * And a service template with full settings is defined
      * And service_1 has no direct settings and inherits from the template
      * When Broker and Engine are started
      * Then service_1 inherits all settings from the service template on engine start
51. **CESI1**: 
      * Given Engine is configured with centralized configuration
      * And both service_1 and a service template have full settings
      * And service_1 inherits from the template
      * When Broker and Engine are started
      * Then service_1 own values take precedence over template values on engine start
52. **CESI2**: 
      * Given Engine is configured with centralized configuration and started
      * And a service template with full settings is defined after start
      * And service_1 has no direct settings and inherits from the template
      * When Broker notifies Engine of new centralized configuration
      * Then service_1 inherits all settings from the service template on engine reload
53. **CESI3**: 
      * Given Engine is configured with centralized configuration and started
      * And both service_1 and a service template have full settings defined after start
      * And service_1 inherits from the template
      * When Broker notifies Engine of new centralized configuration
      * Then service_1 own values take precedence over template values on engine reload
54. **CESS1**: 
      * Given one Engine instance is configured with a module broker
      * When the Engine is started and stopped 5 times with no delay
      * Then no coredump is produced
55. **CESS2**: 
      * Given one Engine instance is configured with a module broker
      * When the Engine is started and stopped 5 times with 300ms delay
      * Then no coredump is produced
56. **CESS3**: 
      * Given three Engine instances are configured with a module broker
      * When the Engine is started and stopped 5 times with no delay
      * Then no coredump is produced
57. **CESS4**: 
      * Given three Engine instances are configured with a module broker
      * When the Engine is started and stopped 5 times with 300ms delay
      * Then no coredump is produced
58. **CESSCTO**: 
      * Given the Engine is configured without the Perl connector
      * When the Engine executes its service commands
      * Then the commands take too long and reach the timeout
      * And the Engine starts and stops as a result
59. **CESSCTOWC**: 
      * Given the Engine is configured with some commands using the Perl connector
      * When the Engine executes its service commands
      * Then the commands take too long and reach the timeout
      * And the Engine starts and stops as a result
60. **CESSOCWNV**: 
      * Given the Engine is configured with a valid old configuration concerning cbmod
      * When the Engine is started
      * Then the Engine starts correctly
      * And the Engine stops correctly
61. **CESS_STATS**: 
      * Given the Engine is started with centralized configuration
      * When we read the Engine's stats file
      * Then the Engine must not crash
62. **CEVOCWNV**: 
      * Given the Engine is configured with a valid old configuration concerning cbmod
      * When the Engine is started to check the configuration
      * Then the Engine reads it as expected
63. **CEXT_CONF1**: 
      * Given Engine is configured with a module broker
      * When Engine starts with an extended JSON configuration overriding log levels
      * Then the log levels from the extended conf are applied at startup
64. **CEXT_CONF2**: 
      * Given Engine is configured with a module broker and an empty extended JSON conf
      * When the extended conf is updated with new log levels and Engine is reloaded
      * Then the new log levels from the updated extended conf are applied after reload
65. **CE_FD_LIMIT**: 
      * Given the Engine is configured with a low file descriptor limit
      * When the Engine is started
      * Then the Engine should not crash
      * And the file descriptor limit should be set correctly
66. **CE_HOST_DOWN_DISABLE_SERVICE_CHECKS**: 
      * Given Engine is configured with centralized configuration and host_down_disable_service_checks enabled
      * When a host goes DOWN
      * Then all its services are switched to UNKNOWN hard state
      * And when the host recovers all services return to OK hard state
67. **CE_HOST_UNREACHABLE_DISABLE_SERVICE_CHECKS**: 
      * Given Engine is configured with centralized configuration and host_down_disable_service_checks enabled
      * When a parent host goes DOWN causing a child host to become UNREACHABLE
      * Then all services on the unreachable host are switched to UNKNOWN hard state
68. **CVERIF**: 
      * Given centengine is configured normally
      * When centengine is started in verification mode
      * Then it does not log in its file
69. **CVERIFY_CONF**: 
      * Given Engine and broker are configured with module
      * And the engine configuration includes deprecated options
      * When Engine starts
      * Then a warning message for 'auto_reschedule_checks' is logged
      * And a warning message for 'auto_rescheduling_interval' is logged
      * And a warning message for 'auto_rescheduling_window' is logged
70. **EBSN5**: Verify contactgroup inheritance : contactgroup(empty) inherit from template (full) , on Start Engine
71. **EBSN6**: Verify contactgroup inheritance : contactgroup(full) inherit from template (full) , on Start Engine
72. **EBSN7**: Verify contactgroup inheritance : contactgroup(empty) inherit from template (full) , on Reload Engine
73. **EBSN8**: Verify contactgroup inheritance : contactgroup(full) inherit from template (full) , on Reload Engine
74. **ECEMPTYNAME**: Scenario: a contact with no name is rejected
      * Given an engine configuration with a contact that has no contact_name
      * When centengine verifies the configuration (-v)
      * Then the configuration is reported invalid (non-zero return code and "Contact has no name")
75. **ECGEMPTYNAME**: Scenario: a contact group with no name is rejected
      * Given an engine configuration with a contact group that has no contactgroup_name
      * When centengine verifies the configuration (-v)
      * Then the configuration is reported invalid (non-zero return code and "Contactgroup has no name")
76. **ECGNCM**: Scenario: a contact group with a non-existing member is rejected
      * Given an engine configuration with a contact group referencing an undefined contact
      * When centengine verifies the configuration (-v)
      * Then the configuration is reported invalid (non-zero return code and the missing contact is named)
77. **ECGRELOAD**: Scenario: centengine refuses a reload adding a contact group with an undefined member
      * Given centengine is started and ready with a valid configuration
      * When a contact group referencing an undefined contact is added and a reload is triggered
      * Then the reload is rejected (it logs the error) and centengine keeps running with the previous configuration
78. **ECGRSTART**: Scenario: centengine refuses to start with a contact group referencing an undefined contact
      * Given an engine configuration with a contact group whose member does not exist
      * When centengine is started on that configuration
      * Then it refuses to start: it exits with a failure code instead of running
79. **ECI0**: Verify contact inheritance : contact(empty) inherit from template (full), on Start Engine
80. **ECI1**: Verify contact inheritance : contact(full) inherit from template (full) , on Start Engine
81. **ECI2**: Verify contact inheritance : contact(empty) inherit from template (full) , on Reload Engine
82. **ECI3**: Verify contact inheritance : contact(full) inherit from template (full) , on Reload Engine
83. **ECI4**: 
      * Given a contact inherits from two templates with overlapping attributes
      * When the engine is started
      * Then the contact's attributes are correctly resolved based on the inheritance rules
84. **ECMI0**: Verify command inheritance : command(empty) inherit from template (full) , on Start Engine
85. **ECMI1**: Verify command inheritance : command(full) inherit from template (full) , on Start Engine
86. **ECMI2**: Verify command inheritance : command(empty) inherit from template (full) , on Reload Engine
87. **ECMI3**: Verify command inheritance : command(full) inherit from template (full) , on reload Engine
88. **ECMI4**: 
      * Given a command inherits from two templates with full configurations,
      * When the engine starts,
      * Then the command should inherit the command line from the first template
      * And the connector from the second template.
89. **ECMI5**: 
      * Given a command inherits from a template with full configurations,
      * When the engine reloads,
      * Then the command should inherit the command line from the first template
      * And the connector from the second template.
90. **ECNHNC**: Scenario: a contact without host notification commands is rejected
      * Given an engine configuration where contact U1 has no host_notification_commands
      * When centengine verifies the configuration (-v)
      * Then the configuration is reported invalid (non-zero return code and at least one error)
91. **ECOI0**: Verify connector inheritance : connector(empty) inherit from template (full) , on Start Engine
92. **ECOI1**: Verify connector inheritance : connector(full) inherit from template (full) , on Start Engine
93. **ECOI2**: Verify connector inheritance : connector(empty) inherit from template (full) , on Reload Engine
94. **ECOI3**: Verify connector inheritance : connector(full) inherit from template (full) , on Reload Engine
95. **ECOI4**: 
      * Given a connector inherits from two templates
      * When the engine starts
      * Then the connector should use the connector_line from the second template
96. **ECRELOAD**: Scenario: centengine refuses a reload of an invalid centengine.cfg and keeps running
      * Given centengine is started and ready with a valid configuration
      * When centengine.cfg is made invalid (contact U1 without host_notification_commands) and a reload is triggered
      * Then the reload is rejected (it logs the error) and centengine keeps running with the previous configuration
97. **ECRSTART**: Scenario: centengine refuses to start with an invalid centengine.cfg
      * Given an engine configuration where contact U1 has no host_notification_commands
      * When centengine is started on that configuration
      * Then it refuses to start: it exits with a failure code instead of running
98. **EESI0**: Verify service escalation : create service escalation for every service in a service group
99. **EESI1**: Verify service escalation  inheritance : escalation(empty) inherit from template (full) , on Start Engine
100. **EESI10**: 
      * Given a service escalation configuration with multiple inheritance templates,
      * When the engine is started,
      * Then the service escalation attributes are correctly inherited and applied to the services in the service group,
      * And services not in the group have no escalation applied.
101. **EESI11**: 
      * Given a host escalation configuration with multiple inheritance templates,
      * When the engine is started,
      * Then the host escalation attributes are correctly inherited and applied to the hosts in the host group,
      * And hosts not in the group have no escalation applied.
102. **EESI12**: Verify host escalation configured with host_name directly (not hostgroup_name) is properly linked to the host and triggers escalation contacts.
103. **EESI13**: Verify service escalation configured with host_name and service_description directly (not servicegroup_name) is properly linked to the service and triggers escalation contacts.
104. **EESI2**: Verify service escalation  inheritance : escalation(full) inherit from template (full) , on Start Engine
105. **EESI3**: Verify service escalation  inheritance : escalation(empty) inherit from template (full) , on Reload Engine
106. **EESI4**: Verify service escalation  inheritance : escalation(full) inherit from template (full) , on Reload Engine
107. **EESI5**: Verfiy host escalation : create host escalation for every host in the hostgroup
108. **EESI6**: Verify host escalation inheritance : escalation(empty) inherit from template (full) , on Start Engine   
109. **EESI7**: Verify host escalation inheritance : escalation(full) inherit from template (full) , on Start Engine    
110. **EESI8**: Verify host escalation inheritance : escalation(empty) inherit from template (full) , on Reload Engine   
111. **EESI9**: Verify host escalation inheritance : escalation(full) inherit from template (full) , on Reload Engine    
112. **EFHC1**: Engine is configured with hosts and we force check one 5 times with bbdo2
113. **EFHC2**: Engine is configured with hosts and we force check on one 5 times on bbdo2
114. **EFHCU1**: Engine is configured with hosts and we force checks on one 5 times on bbdo3. Bbdo3 has no impact on this behavior. resources table is cleared before starting broker.
115. **EFHCU2**: Engine is configured with hosts and we force checks on one 5 times on bbdo3. Bbdo3 has no impact on this behavior.
116. **EHGI0**: Verify hostgroup inheritance : hostgroup(empty) inherit from template (full) , on Start Engine
117. **EHGI1**: Verify hostgroup inheritance : hostgroup(full) inherit from template (full) , on Start Engine
118. **EHGI2**: Verify hostgroup inheritance : hostgroup(empty) inherit from template (full) , on Reload Engine
119. **EHGI3**: Verify hostgroup inheritance : hostgroup(full) inherit from template (full) , on Reload Engine
120. **EHGI4**: 
      * Given a Centreon engine with a predefined configuration of host groups,
      * When the engine processes the host group inheritance rules,
      * Then the resulting configuration should reflect the expected inheritance structure.
121. **EHGNCM**: Scenario: a host group with a non-existing member is rejected
      * Given an engine configuration with a host group referencing an undefined host
      * When centengine verifies the configuration (-v)
      * Then the configuration is reported invalid (non-zero return code and the missing host is named)
122. **EHGRELOAD**: Scenario: centengine refuses a reload adding a host group with an undefined member
      * Given centengine is started and ready with a valid configuration
      * When a host group referencing an undefined host is added and a reload is triggered
      * Then the reload is rejected (it logs the error) and centengine keeps running with the previous configuration
123. **EHGRSTART**: Scenario: centengine refuses to start with a host group referencing an undefined host
      * Given an engine configuration with a host group whose member does not exist
      * When centengine is started on that configuration
      * Then it refuses to start: it exits with a failure code instead of running
124. **EHI0**: Verify inheritance host : host(empty) inherit from template (full) , on Start Engine
125. **EHI1**: Verify inheritance host : host(full) inherit from template (full) , on Start engine
126. **EHI2**: Verify inheritance host : host(empty) inherit from template (full) , on Reload engine
127. **EHI3**: Verify inheritance host : host(full) inherit from template (full) , on engine Reload
128. **EHI4**: 
      * Given a parent host with specific configurations,
      * When a child host is defined to inherit from the parent,
      * Then the child host should correctly apply the inherited configurations.
129. **EMACROS**: macros ADMINEMAIL and ADMINPAGER are replaced in check outputs
130. **EMACROS_NOTIF**: macros ADMINEMAIL and ADMINPAGER are replaced in notification commands
131. **EMACROS_SEMICOLON**: Macros with a semicolon are used even if they contain a semicolon.
132. **EMTI0**: Verify multiple inheritance host
133. **EMTI1**: 
      * Given a host with multiple templates
      * When the templates are configured with custom variables
      * Then the custom variables should be correctly inherited and accessible
134. **ENGINE_MANY_CHECKS**: 
      * Given a engine with many services and a unique check on each service with it's own env variables
      * We expect correct check result in logs and we checks returned args and service macros
135. **ENGINE_MANY_CHECK_OK**: 
      * Given a engine with many services and a command shared between several services
      * We expect correct check result in logs and we checks returned args and service macros
      * All odd indexed services use perl connector to do checks
136. **ENGINE_MANY_LONG_CHECKS**: 
      * Given a engine with many services and a unique check on each service with it's own env variables and a variable check duration
      * We expect no memory leak in centengine despite checks timeouts
137. **EPC1**: Check with perl connector
138. **ERL**: Engine is started and writes logs in centengine.log. Then we remove the log file. The file disappears but Engine is still writing into it. Engine is reloaded and the centengine.log should appear again.
139. **ESGI0**: Verify servicegroup inheritance : servicegroup(empty) inherit from template (full) , on Start Engine
140. **ESGI1**: Verify servicegroup inheritance : servicegroup(empty) inherit from template (full) , on Start Engine
141. **ESGI2**: Verify servicegroup inheritance : servicegroup(empty) inherit from template (full) , on Reload Engine
142. **ESGI3**: Verify servicegroup inheritance : servicegroup(empty) inherit from template (full) , on Reload Engine
143. **ESGI4**: 
      * Given a configured Centreon engine with defined service groups
      * When the inheritance rules are applied
      * Then the service group inheritance should be correctly reflected in the monitoring data
144. **ESGNCM**: Scenario: a service group with a non-existing member is rejected
      * Given an engine configuration with a service group referencing an undefined service
      * When centengine verifies the configuration (-v)
      * Then the configuration is reported invalid (non-zero return code and the missing service is named)
145. **ESGRELOAD**: Scenario: centengine refuses a reload adding a service group with an undefined member
      * Given centengine is started and ready with a valid configuration
      * When a service group referencing an undefined service is added and a reload is triggered
      * Then the reload is rejected (it logs the error) and centengine keeps running with the previous configuration
146. **ESGRSTART**: Scenario: centengine refuses to start with a service group referencing an undefined service
      * Given an engine configuration with a service group whose member does not exist
      * When centengine is started on that configuration
      * Then it refuses to start: it exits with a failure code instead of running
147. **ESI0**: Verify inheritance service : Service(empty) inherit from template (full) , on Start Engine
148. **ESI1**: Verify inheritance service : Service(full) inherit from template (full) , on Start Engine
149. **ESI2**: Verify inheritance service : Service(empty) inherit from template (full) , on Reload Engine
150. **ESI3**: Verify inheritance service : Service(full) inherit from template (full) , on Reload Engine
151. **ESI4**: 
      * Given the Centreon engine is running with a predefined configuration.
      * When a service inherits properties from its parent service.
      * Then the inherited properties should match the expected values.
152. **ESS1**: Start-Stop (0s between start/stop) 5 times one instance of engine and no coredump
153. **ESS2**: Start-Stop (300ms between start/stop) 5 times one instance of engine and no coredump
154. **ESS3**: Start-Stop (0s between start/stop) 5 times three instances of engine and no coredump
155. **ESS4**: Start-Stop (300ms between start/stop) 5 times three instances of engine and no coredump
156. **ESSCTO**: Scenario: Engine services timeout due to missing Perl connector
      * Given the Engine is configured as usual without the Perl connector
      * When the Engine executes its service commands
      * Then the commands take too long and reach the timeout
      * And the Engine starts and stops four times as a result
157. **ESSCTOWC**: Scenario: Engine services timeout due to missing Perl connector
      * Given the Engine is configured as usual with some command using the Perl connector
      * When the Engine executes its service commands
      * Then the commands take too long and reach the timeout
      * And the Engine starts and stops four times as a result
158. **ESSOCWNV**: Scenario: Engine is started with a valid old configuration (concerning cbmod)
      * Given the Engine is configured with a valid old configuration
      * When the Engine is started
      * Then the Engine starts correctly
      * And the Engine stops correctly
159. **ESS_STATS**: Scenario: Reading the stats file after Engine has started
      * Given the Engine is started
      * When we read the Engine's stats file
      * Then Engine must not crash
160. **EVOCWNV**: Scenario: The new Engine checks the old configuration (concerning cbmod)
      * Given the Engine is configured with a valid old configuration
      * When the Engine is started to check the configuration
      * Then the Engine reads it as expected
161. **EXT_CONF1**: Engine configuration is overidden by json conf
162. **EXT_CONF2**: Engine configuration is overidden by json conf after reload
163. **E_FD_LIMIT**: Engine here is started with a low file descriptor limit. The engine should not crash and limit should be set.
164. **E_HOST_DOWN_DISABLE_SERVICE_CHECKS**: host_down_disable_service_checks is set to 1, host down switch all services to UNKNOWN
165. **E_HOST_UNREACHABLE_DISABLE_SERVICE_CHECKS**: host_down_disable_service_checks is set to 1, host unreachable switch all services to UNKNOWN
166. **HOST_DEPENDENCY**: 
      * Given an engine with a host dependency configured, we change it.
      * We expect no error in logs and that configuration is well updated
167. **NOT_MODIFY_HOST_ESCALATION**: 
      * Given an engine with a host escalation configured, we add a contact group to it.
      * We expect no error in logs and that configuration is well updated
168. **NOT_MODIFY_SERVICE_ESCALATION**: 
      * Given an engine with a service escalation configured, we add a contact group to it.
      * We expect no error in logs and that configuration is well updated
169. **NO_BROKER_LOG**: 
      * Given a configuration without broker logs directory, engine must start correctly
170. **NO_HOST_CHECK_COMMAND**: 
      * Given a host without check command, engine should not crash
171. **SERVICE_DEPENDENCY**: 
      * Given an engine with a service dependency configured, we change it.
      * We expect no error in logs and that configuration is well updated
172. **TIMEPERIOD_INHERITANCE**: 
      * Given two timeperiods, one inherit from the second. First onex exclude next hour, checks must not be executed in next 5 minutes
173. **TIMEPERIOD_INHERITANCE_EXCEPTION**: 
      * Given two timeperiods, one inherit from the second. First one allow H24, second one has only one exception, checks must be executed in next minute
174. **TIMEPERIOD_MERGE**: 
      * Given a timeperiod with its own calendar exception (active now) and a template
      * with a different calendar exception (active yesterday), the merge must preserve the child's
      * exception. The service using the child timeperiod as check_period must be checked.
175. **VERIF**: 
      * When centengine is started in verification mode, it does not log in its file.
176. **VERIFY_CONF**: Scenario Verify deprecated engine configuration options are logged as warnings Given the engine and broker are configured with module 1 And the engine configuration is set with deprecated options When the engine is started Then a warning message for 'auto_reschedule_checks' should be logged And a warning message for 'auto_rescheduling_interval' should be logged And a warning message for 'auto_rescheduling_window' should be logged And the engine should be stopped

### Severities
1. **BECSEV1**: Feature: Severity Management between Engine and Broker As a Centreon administrator I want to configure severities in Engine So that Broker stores them correctly in centreon_storage.severities table
2. **BECSEV2**: Scenario: Severity db_ids correctly restored after broker restart
      * Given broker and engine are started with 20 severities configured on poller 1
      * And services 1 to 4 are linked to severity 11
      * And severities are correctly inserted in DB with non-zero db_ids in broker cache
      * When broker is restarted while engine keeps running
      * And the poller reconnects so the config is reprocessed against an already populated DB
      * Then severity db_ids should still be non-zero in broker cache
      * And services should still have correct severity_id in the resources table
3. **BECSEV3**: Scenario: Severity db_ids correctly restored after broker restart with lost prot files
      * Given broker and engine are started with 20 severities configured on poller 1
      * And services 1 to 4 are linked to severity 11
      * And severities are correctly inserted in DB with non-zero db_ids in broker cache
      * When broker is restarted after losing its prot files (simulating a fresh broker with existing DB)
      * And engine sends its full configuration back (DiffState unknown path)
      * Then _add_severities_mariadb is called with all-duplicate rows (ON DUPLICATE KEY UPDATE)
      * And LAST_INSERT_ID() returns 0 for all rows, potentially overwriting db_ids in cache with 0
      * Then severity db_ids should still be non-zero in broker cache
      * And services should still have correct severity_id in the resources table
4. **BECSEV4_${lck_mode}**: Feature: Severity presence in Broker gRPC cache with centralized configuration
5. **BECSEV5_${lck_mode}**: Feature: Severity level change is reflected in the Broker gRPC cache
6. **BECSEV6_${lck_mode}**: Feature: GetSeverities gRPC returns correct content while severities are active
7. **BECSEV7_${lck_mode}**: Feature: Broker cache is repopulated after broker restart with severities active
8. **BESEV1**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Broker is started before.
9. **BESEV2**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Engine is started before.
10. **BETUHSEV1**: Hosts have severities provided by templates.
11. **BETUSEV1**: Services have severities provided by templates.
12. **BETUSEV2**: A service and a host have severities. Then we remove severities from both of them and check they no longer have severities.
13. **BEUHSEV1**: Four hosts have a severity added. Then we remove the severity from host 1. Then we change severity 10 to severity8 for host 3.
14. **BEUHSEV2**: Seven hosts are configured with a severity on two pollers. Then we remove severities from the first and second hosts of the first poller but only the severity from the first host of the second poller.
15. **BEUSEV1**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Broker is started before.
16. **BEUSEV2**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Engine is started before.
17. **BEUSEV3**: Four services have a severity added. Then we remove the severity from service 1. Then we change severity 11 to severity7 for service 3.
18. **BEUSEV4**: Seven services are configured with a severity on two pollers. Then we remove severities from the first and second services of the first poller but only the severity from the first service of the second poller. Then only severities no more used should be removed from the database.
19. **CBESEV1**: Scenario: Severities stored in database when Broker starts first (centralized)
      * Given Engine is configured with centralized setup and 20 severities
      * And Broker components (central, rrd, module) are configured
      * And retention data is cleared
      * When Broker is started before Engine
      * Then severity20 should be of level 5 with icon_id 1
      * And severity1 should be of level 1 with icon_id 5
20. **CBESEV2**: Scenario: Severities stored in database when Engine starts first (centralized)
      * Given Engine is configured with centralized setup and 20 severities
      * And Broker components (central, rrd, module) are configured
      * And retention data is cleared
      * When Engine is started before Broker
      * Then severity20 should be of level 5 with icon_id 1
      * And severity1 should be of level 1 with icon_id 5
21. **CBETUHSEV1**: 
      * Given hosts on two pollers using templates that define severities
      * (template_1: severity 2 on poller 0, severity 6 on poller 1;
      * template_2: severity 4 on poller 0, severity 10 on poller 1),
      * When the engine and broker are started with centralized configuration,
      * Then host 2 and host 4 should have severity_id=2
      * And host 5 should have severity_id=4
      * And host 31 should have severity_id=6
      * And host 33 should have severity_id=10.
22. **CBETUSEV1**: Scenario: Service severities inherited from templates via unified SQL (centralized)
      * Given Engine is configured with centralized setup across 2 pollers and 20 severities each
      * And service templates with severity assignments are configured
      * And Broker is configured with unified SQL output and BBDO3
      * When Engine and Broker are started
      * Then services inheriting template_1 on poller 1 should have severity_id=1
      * And services inheriting template_2 on poller 1 should have severity_id=3
      * And services inheriting template_1 on poller 2 should have severity_id=3
      * And services inheriting template_2 on poller 2 should have severity_id=5
23. **CBEUHSEV1**: 
      * Given four hosts with a severity added,
      * When we remove the severity from host 1
      * And we change severity 10 to severity 8 for host 3,
      * Then host 2 should still have severity_id=10
      * And host 4 should still have severity_id=10
      * And host 3 should have severity_id=8
      * And host 1 should have no severity.
24. **CBEUHSEV2**: Scenario: Host severities removed on one poller and changed on another are applied poller by poller
      * Given seven hosts configured with severities on two pollers,
      * When we remove severities from hosts on the first poller
      * And we change host 28's severity from 16 to 14 on the second poller,
      * Then host 26 should still have severity_id=18
      * And host 27 should still have severity_id=18
      * And host 28 should have severity_id=14
      * And hosts 3, 4 and 5 on the first poller should have no severity.
25. **CBEUSEV1**: Scenario: Severities stored via unified SQL when Broker starts first (centralized)
      * Given Engine is configured with centralized setup and 20 severities
      * And Broker is configured with unified SQL output and BBDO3
      * And retention data is cleared
      * When Broker is started before Engine
      * Then severity20 should be of level 5 with icon_id 1
      * And severity1 should be of level 1 with icon_id 5
26. **CBEUSEV2**: Scenario: Severities stored via unified SQL when Engine starts first (centralized)
      * Given Engine is configured with centralized setup and 20 severities
      * And Broker is configured with unified SQL output and BBDO3
      * And retention data is cleared
      * When Engine is started before Broker
      * Then severity20 should be of level 5 with icon_id 1
      * And severity1 should be of level 1 with icon_id 5
27. **CBEUSEV3**: Scenario: Service severity removal and change via unified SQL (centralized)
      * Given Engine is configured with centralized setup and 20 severities
      * And Broker is configured with unified SQL output and BBDO3
      * And severity 11 is assigned to services 1, 2, 3 and 4
      * When Engine and Broker are started
      * Then service (1, 1) should have severity_id=11
      * When severity is removed from all services and reassigned (11 to services 2,4 and 7 to service 3)
      * And Engine and Broker are reloaded
      * Then service (1, 3) should have severity_id=7
      * And service (1, 1) should have no severity
28. **CBEUSEV4**: Scenario: Severity removal across two pollers via unified SQL (centralized)
      * Given Engine is configured with centralized setup across 2 pollers and 20 severities each
      * And severity 19 is assigned to services 2,4 on poller 1 and services 501,502 on poller 2
      * And severity 17 is assigned to services 3,5 on poller 1 and service 503 on poller 2
      * And Broker is configured with unified SQL output and BBDO3
      * When Engine and Broker are started
      * Then all services should have their expected severity_id values
      * When severities are removed from poller 1 services and severity files reduced to 18
      * And severity 17 is kept on service 503 of poller 2
      * And Engine and Broker are reloaded
      * Then service (26, 503) should still have severity_id=17
      * And services on poller 1 that lost their severity should have severity_id=None

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
12. **CBAEBC**: Scenario: AES256 decryption of non-encrypted content returns an error
      * Given broker is started in centralized mode
      * When AES256 decryption is attempted on content that is not properly encrypted
      * Then broker returns an error indicating the content is not AES256 encrypted
13. **CBAEBS**: Scenario: AES256 encryption with invalid base64 salt returns an error
      * Given broker is started in centralized mode
      * When AES256 encryption is attempted with a non-base64 salt
      * Then broker returns an error about illegal base64 characters
14. **CBAEOK**: Scenario: AES256 encrypt then decrypt returns the original content
      * Given broker is started in centralized mode
      * When AES256 encryption is applied to content
      * Then the decrypted result matches the original content
15. **CBASV**: Scenario: Broker with vault configured but vault server down logs an error
      * Given broker is started in centralized mode
      * And the vault server is not running
      * When broker is configured to retrieve credentials from the vault
      * Then broker logs an error about the inactive http server
16. **CBAV**: Scenario: Broker retrieves database password from a running vault
      * Given broker is started in centralized mode
      * And a vault is running with valid credentials
      * When broker is configured to retrieve the database password from the vault
      * Then broker logs that the database password was retrieved from vault
17. **CBWVC1**: Scenario: Broker with missing vault env file logs an error
      * Given broker configured with a wrong vault configuration and no env file
      * When broker starts
      * Then broker logs an error that the env file could not be opened
18. **CBWVC2**: Scenario: Broker with env file missing APP_SECRET logs an error
      * Given broker configured with a wrong vault configuration
      * And an env file with invalid content (no APP_SECRET)
      * When broker starts
      * Then broker logs an error about missing APP_SECRET
19. **CBWVC3**: Scenario: Broker with wrong vault file path logs a JSON parse error
      * Given broker configured with a strange APP_SECRET and a non-existent vault file
      * When broker starts
      * Then broker logs an error about the wrong vault file
20. **CBWVC4**: Scenario: Broker with malformed vault JSON file logs an error
      * Given broker configured with a strange APP_SECRET and a vault file missing required keys
      * When broker starts
      * Then broker logs an error about the malformed vault file
21. **CBWVC5**: Scenario: Broker with non-string salt in vault file logs a type error
      * Given broker configured with a strange APP_SECRET and a vault file with numeric salt
      * When broker starts
      * Then broker logs an error about the bad encryption type
22. **CBWVC6**: Scenario: Broker with non-base64 salt in vault file logs an encoding error
      * Given broker configured with APP_SECRET and a vault file containing non-base64 salt
      * When broker starts
      * Then broker logs an error about the bad base64 encoding


1006 tests currently implemented.
