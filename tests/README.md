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
dnf install "Development Tools" python3-devel -y

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
9. **BAWORST**: With bbdo version 3.0.1, a BA of type 'worst' with two services is configured. We also check stats output
10. **BAWORST2**: a worst ba with a boolean kpi and a ba kpi
11. **BAWORST_ACK**: Scenario: Acknowledging a service acknowledges the BA, and removing it unacknowledges the BA
      * Given BBDO version is 3.0.1
      * And a Business Activity of type "worst" is configured with two services
      * When one of the services is acknowledged
      * Then the Business Activity is acknowledged
      * When the acknowledgement is removed from the service
      * Then the Business Activity is no longer acknowledged
12. **BA_BOOL_KPI**: With bbdo version 3.0.1, a BA of type 'worst' with 1 boolean kpi
13. **BA_CHANGED**: A BA of type worst is configured with one service kpi. Then it is modified so that the service kpi is replaced by a boolean rule kpi. When cbd is reloaded, the BA is well updated.
14. **BA_DISABLED**: create a disabled BA with timeperiods and reporting filter don't create error message
15. **BA_IMPACT_2KPI_SERVICES**: With bbdo version 3.0.1, a BA of type 'impact' with 2 serv, ba is critical only if the 2 services are critical
16. **BA_IMPACT_IMPACT**: 
      * Given a Business Activity (BA) of type "impact"
      * And it has two child BAs of type "impact"
      * And the first child has an impact of 90
      * And the second child has an impact of 10
      * When both child BAs are impacting
      * Then the parent BA should be "critical"
      * When both child BAs are not impacting
      * Then the parent BA should be "ok"
17. **BA_RATIO_NUMBER_BA_4_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
18. **BA_RATIO_NUMBER_BA_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 2 services and one ba with 1 service
19. **BA_RATIO_PERCENT_BA_4_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
20. **BA_RATIO_PERCENT_BA_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio percent' with 2 serv an 1 ba with one service
21. **BA_SERVICE_PNAME_AFTER_RELOAD**: Scenario: Verify that the parent_name of a BA service is not erased after a broker reload
      * Given a BA "test" of type "worst" with its service "host_16:service_302"
      * When I start broker and engine
      * Then the BA service "test" should have a status of 0 within 30 seconds
      * When I reload the broker
      * Then the database should still contain a BA service with name "test" and parent_name "_Module_BAM_1"
22. **BEBAMIDT1**: 
      * Given a BA of type 'worst' with one service is configured
      * And The BA is in critical state due to its service
      * When a downtime is set on this service
      * Then an inherited downtime is set to the BA
      * When the downtime is removed from the service
      * Then the inherited downtime is deleted from the BA
23. **BEBAMIDT2**: 
      * Given a BA of type 'worst' with one service is configured
      * And the BA is in critical state due to its service
      * And a downtime is set on this service
      * Then an inherited downtime is set to the BA
      * When Engine is restarted
      * And Broker is restarted
      * Then both downtimes are still present with no duplicates
      * When the downtime is removed from the service
      * Then the inherited downtime is deleted
24. **BEBAMIDTU1**: 
      * Given BBDO version 3.0.1 is running
      * And a BA of type 'worst' with one service is configured
      * And The BA is in critical state due to its service
      * When a downtime is set on this service
      * Then an inherited downtime is set to the BA
      * When the downtime is removed from the service
      * Then the inherited downtime is deleted from the BA
25. **BEBAMIDTU2**: 
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
26. **BEBAMIGNDT1**: A BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. A first downtime is cancelled, the BA is still OK, but when the second downtime is cancelled, the BA should be CRITICAL.
27. **BEBAMIGNDT2**: A BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. The first downtime reaches its end, the BA is still OK, but when the second downtime reaches its end, the BA should be CRITICAL.
28. **BEBAMIGNDTU1**: With bbdo version 3.0.1, a BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. A first downtime is cancelled, the BA is still OK, but when the second downtime is cancelled, the BA should be CRITICAL.
29. **BEBAMIGNDTU2**: 
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
30. **BEPB_BA_DURATION_EVENT**: use of pb_ba_duration_event message.
31. **BEPB_DIMENSION_BA_BV_RELATION_EVENT**: bbdo_version 3 use pb_dimension_ba_bv_relation_event message.
32. **BEPB_DIMENSION_BA_EVENT**: bbdo_version 3 use pb_dimension_ba_event message.
33. **BEPB_DIMENSION_BA_TIMEPERIOD_RELATION**: use of pb_dimension_ba_timeperiod_relation message.
34. **BEPB_DIMENSION_BV_EVENT**: bbdo_version 3 use pb_dimension_bv_event message.
35. **BEPB_DIMENSION_KPI_EVENT**: bbdo_version 3 use pb_dimension_kpi_event message.
36. **BEPB_DIMENSION_TIMEPERIOD**: use of pb_dimension_timeperiod message.
37. **BEPB_DIMENSION_TRUNCATE_TABLE**: use of pb_dimension_timeperiod message.
38. **BEPB_KPI_STATUS**: bbdo_version 3 use kpi_status message.

### Broker
1. **BCL1**: Starting broker with option '-s foobar' should return an error
2. **BCL2**: Starting broker with option '-s5' should work
3. **BCL3**: Starting broker with options '-D' should work and activate diagnose mode
4. **BCL4**: Starting broker with options '-s2' and '-D' should work.
5. **BDB1**: Access denied when database name exists but is not the good one for sql output
6. **BDB10**: connection should be established when user password is good for sql/perfdata
7. **BDB2**: Access denied when database name exists but is not the good one for storage output
8. **BDB3**: Access denied when database name does not exist for sql output
9. **BDB4**: Access denied when database name does not exist for storage and sql outputs
10. **BDB5**: cbd does not crash if the storage/sql db_host is wrong
11. **BDB6**: cbd does not crash if the sql db_host is wrong
12. **BDB7**: access denied when database user password is wrong for perfdata/sql
13. **BDB8**: access denied when database user password is wrong for perfdata/sql
14. **BDB9**: access denied when database user password is wrong for sql
15. **BDBM1**: start broker/engine and then start MariaDB => connection is established
16. **BDBMU1**: start broker/engine with unified sql and then start MariaDB => connection is established
17. **BDBU1**: Access denied when database name exists but is not the good one for unified sql output
18. **BDBU10**: Connection should be established when user password is good for unified sql
19. **BDBU3**: Access denied when database name does not exist for unified sql output
20. **BDBU5**: cbd does not crash if the unified sql db_host is wrong
21. **BDBU7**: Access denied when database user password is wrong for unified sql
22. **BEDB2**: start broker/engine and then start MariaDB => connection is established
23. **BEDB3**: start broker/engine, then stop MariaDB and then start it again. The gRPC API should give informations about SQL connections.
24. **BEDB4**: start broker/engine, then stop MariaDB and then start it again. The gRPC API should give informations about SQL connections.
25. **BFC1**: Start broker with invalid filters but one filter ok
26. **BFC2**: Start broker with only invalid filters on an output
27. **BGRPCSS1**: Start-Stop two instances of broker configured with grpc stream and no coredump
28. **BGRPCSS2**: Start/Stop 10 times broker configured with grpc stream with 300ms interval and no coredump
29. **BGRPCSS3**: Start-Stop one instance of broker configured with grpc stream and no coredump
30. **BGRPCSS4**: Start/Stop 10 times broker configured with grpc stream with 1sec interval and no coredump
31. **BGRPCSS5**: Start-Stop with reversed connection on grpc acceptor with only one instance and no deadlock
32. **BGRPCSSU1**: Start-Stop with unified_sql two instances of broker with grpc stream and no coredump
33. **BGRPCSSU2**: Start/Stop with unified_sql 10 times broker configured with grpc stream with 300ms interval and no coredump
34. **BGRPCSSU3**: Start-Stop with unified_sql one instance of broker configured with grpc and no coredump
35. **BGRPCSSU4**: Start/Stop with unified_sql 10 times broker configured with grpc stream with 1sec interval and no coredump
36. **BGRPCSSU5**: Start-Stop with unified_sql with reversed connection on grpc acceptor with only one instance and no deadlock
37. **BLBD**: Start Broker with loggers levels by default
38. **BLDIS1**: Start broker with core logs 'disabled'
39. **BLEC1**: Change live the core level log from trace to debug
40. **BLEC2**: Change live the core level log from trace to foo raises an error
41. **BLEC3**: Change live the foo level log to trace raises an error
42. **BSCSS1**: Start-Stop two instances of broker and no coredump
43. **BSCSS2**: Start/Stop 10 times broker with 300ms interval and no coredump
44. **BSCSS3**: Start-Stop one instance of broker and no coredump
45. **BSCSS4**: Start/Stop 10 times broker with 1sec interval and no coredump
46. **BSCSSC1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is enabled on client side.
47. **BSCSSC2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is disabled on client side.
48. **BSCSSCG1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on client side.
49. **BSCSSCGRR1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on output side. Reversed connection with retention and grpc transport protocol.
50. **BSCSSCGRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on output side. Reversed connection with retention and grpc transport protocol.
51. **BSCSSCRR1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is enabled on client side. Connection reversed with retention.
52. **BSCSSCRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is disabled on client side. Connection reversed with retention.
53. **BSCSSG1**: Start-Stop two instances of broker and no coredump
54. **BSCSSG2**: Start/Stop 10 times broker with 300ms interval and no coredump
55. **BSCSSG3**: Start-Stop one instance of broker and no coredump
56. **BSCSSG4**: Start/Stop 10 times broker with 1sec interval and no coredump
57. **BSCSSGA1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. An authorization token is added on the server. Error messages are raised.
58. **BSCSSGA2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. An authorization token is added on the server and also on the client. All looks ok.
59. **BSCSSGRR1**: Start-Stop two instances of broker and no coredump, reversed and retention, with transport protocol grpc, start-stop 5 times.
60. **BSCSSK1**: Start-Stop two instances of broker, server configured with grpc and client with tcp. No connectrion established and error raised on client side.
61. **BSCSSK2**: Start-Stop two instances of broker, server configured with tcp and client with grpc. No connection established and error raised on client side.
62. **BSCSSP1**: Start-Stop two instances of broker and no coredump. The server contains a listen address
63. **BSCSSPRR1**: Start-Stop two instances of broker and no coredump. The server contains a listen address, reversed and retention. centreon-broker-master-rrd is then a failover.
64. **BSCSSR1**: Start-Stop two instances of broker and no coredump. Connection with bbdo_server/bbdo_client and reversed.
65. **BSCSSRR1**: Start-Stop two instances of broker and no coredump. Connection with bbdo_server/bbdo_client, reversed and retention. centreon-broker-master-rrd is then a failover.
66. **BSCSSRR2**: Start/Stop 10 times broker with 300ms interval and no coredump, reversed and retention. centreon-broker-master-rrd is then a failover.
67. **BSCSST1**: Start-Stop two instances of broker and no coredump. Encryption is enabled on client side.
68. **BSCSST2**: Start-Stop two instances of broker and no coredump. Encryption is enabled on client side.
69. **BSCSSTG1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. This is not sufficient, then an error is raised.
70. **BSCSSTG2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. It works with good certificates and keys.
71. **BSCSSTG3**: Start-Stop two instances of broker. The connection cannot be established if the server private key is missing and an error message explains this issue.
72. **BSCSSTGRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. It works with good certificates and keys. Reversed grpc connection with retention.
73. **BSCSSTRR1**: Start-Stop two instances of broker and no coredump. Encryption is enabled. transport protocol is tcp, reversed and retention.
74. **BSCSSTRR2**: Start-Stop two instances of broker and no coredump. Encryption is enabled.
75. **BSS1**: Start-Stop two instances of broker and no coredump
76. **BSS2**: Start/Stop 10 times broker with 300ms interval and no coredump
77. **BSS3**: Start-Stop one instance of broker and no coredump
78. **BSS4**: Start/Stop 10 times broker with 1sec interval and no coredump
79. **BSS5**: Start-Stop with reversed connection on TCP acceptor with only one instance and no deadlock
80. **BSSU1**: Start-Stop with unified_sql two instances of broker and no coredump
81. **BSSU2**: Start/Stop with unified_sql 10 times broker with 300ms interval and no coredump
82. **BSSU3**: Start-Stop with unified_sql one instance of broker and no coredump
83. **BSSU4**: Start/Stop with unified_sql 10 times broker with 1sec interval and no coredump
84. **BSSU5**: Start-Stop with unified_sql with reversed connection on TCP acceptor with only one instance and no deadlock
85. **START_STOP_CBD**: restart cbd with unified_sql services state must not be null after restart

### Broker/database
1. **DEDICATED_DB_CONNECTION_${nb_conn}_${store_in_data_bin}**: count database connection
2. **NetworkDBFail6**: network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s
3. **NetworkDBFail7**: network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s
4. **NetworkDBFailU6**: network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s (with unified_sql)
5. **NetworkDBFailU7**: network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s (with unified_sql)
6. **NetworkDBFailU8**: network failure test between broker and database: we wait for the connection to be established and then we shutdown the connection until _check_queues failure
7. **NetworkDbFail1**: network failure test between broker and database (shutting down connection for 100ms)
8. **NetworkDbFail2**: network failure test between broker and database (shutting down connection for 1s)
9. **NetworkDbFail3**: network failure test between broker and database (shutting down connection for 10s)
10. **NetworkDbFail4**: network failure test between broker and database (shutting down connection for 30s)
11. **NetworkDbFail5**: network failure test between broker and database (shutting down connection for 60s)

### Broker/engine
1. **ANO_CFG_SENSITIVITY_SAVED**: cfg sensitivity saved in retention
2. **ANO_DT1**: downtime on dependent service is inherited by ano
3. **ANO_DT2**: delete downtime on dependent service delete one on ano serv
4. **ANO_DT3**: delete downtime on anomaly don t delete dependent service one
5. **ANO_DT4**: Scenario: Removing downtime from service keeps it on anomaly detection
      * Given an anomaly detection is attached to a service
      * And a downtime is set on both the service and the anomaly detection
      * When the downtime is removed from the service
      * Then the downtime should still be present on the anomaly detection
6. **ANO_EXTCMD_SENSITIVITY_SAVED**: extcmd sensitivity saved in retention
7. **ANO_JSON_SENSITIVITY_NOT_SAVED**: json sensitivity not saved in retention
8. **ANO_NOFILE**: an anomaly detection without threshold file must be in unknown state
9. **ANO_NOFILE_VERIF_CONFIG_NO_ERROR**: an anomaly detection without threshold file doesn't display error on config check
10. **ANO_OUT_LOWER_THAN_LIMIT**: an anomaly detection with a perfdata lower than lower limit make a critical state
11. **ANO_OUT_UPPER_THAN_LIMIT**: an anomaly detection with a perfdata upper than upper limit make a critical state
12. **ANO_TOO_OLD_FILE**: An anomaly detection with an oldest threshold file must be in unknown state
13. **AOUTLU1**: an anomaly detection with a perfdata upper than upper limit make a critical state with bbdo 3
14. **BAM_STREAM_FILTER**: With bbdo version 3.0.1, a BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. we watch its events
15. **BEACK1**: Scenario: Acknowledging a critical service
      * Given Engine has a critical service
      * When an external command is sent to acknowledge it
      * Then the "centreon_storage.acknowledgements" table is updated with this acknowledgement
      * And a log in "centreon_storage.logs" concerning this acknowledgement is added.
      * When the service is set to OK
      * Then the acknowledgement is deleted from the Engine
      * But it remains open in the database
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
40. **BEDTHOSTFIXED1**: Scenario: Setting and Removing Downtime on a Host and its Services
      * Given a downtime is set on a host
      * Then the total number of downtimes is 21
      * And this includes 1 for the host and 20 for its services
      * When the downtime is deleted
      * Then the total number of downtimes is 0
41. **BEDTMASS1**: Scenario: Setting and Removing Downtimes on Configured Hosts and Services
      * Given new services with several pollers are created
      * When downtimes are set on all configured hosts
      * Then the total number of downtimes, including impacted services, is 1050
      * And all these downtimes are removed
      * And the test is performed with BBDO 3.0.0
42. **BEDTMASS2**: Scenario: Setting and Removing Downtimes on Configured Hosts and Services
      * Given new services with several pollers are created
      * When downtimes are set on all configured hosts
      * Then the total number of downtimes, including impacted services, is 1050
      * And all these downtimes are removed
      * And the test is performed with BBDO 2.0.0
43. **BEDTRRD1**: A service is forced checked then a downtime is set on this service. The service is forced checked again and the downtime is removed. This test is done with BBDO 3.0.0. Then we should not get any error in cbd RRD of kind 'ignored update error in file...'.
44. **BEDTSVCFIXED**: 
      * Given a unique downtime set on a service
      * When the downtime is removed
      * Then the downtime is well removed
      * And the number of downtimes is 0
45. **BEDTSVCFIXED1**: 
      * Given a configuration with BBDO3 and a unique downtime set on a service
      * When the downtime is removed
      * Then the downtime is well removed
      * And the number of downtimes is 0
46. **BEDTSVCREN1**: 
      * Given a downtime set on a service
      * When the service is renamed
      * Then the downtime is still active on the renamed service
      * When the downtime is removed from the renamed service
      * Then the downtime is well removed
47. **BEDTSVCREN2**: 
      * Given a configuration with BBDO3 and a downtime set on a service
      * When the service is renamed
      * Then the downtime is still active on the renamed service
      * When the downtime is removed from the renamed service
      * Then the downtime is well removed
48. **BEEXTCMD1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0
49. **BEEXTCMD10**: external command CHANGE_MAX_SVC_CHECK_ATTEMPTS on bbdo2.0
50. **BEEXTCMD11**: external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo3.0
51. **BEEXTCMD12**: external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo2.0
52. **BEEXTCMD13**: external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo3.0
53. **BEEXTCMD14**: external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo2.0
54. **BEEXTCMD15**: external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo3.0
55. **BEEXTCMD16**: external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo2.0
56. **BEEXTCMD17**: external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo3.0
57. **BEEXTCMD18**: external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo2.0
58. **BEEXTCMD19**: external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo3.0
59. **BEEXTCMD2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0
60. **BEEXTCMD20**: external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo2.0
61. **BEEXTCMD21**: external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS and ENABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo3.0
62. **BEEXTCMD22**: external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS and ENABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo2.0
63. **BEEXTCMD23**: 
      * Given Engine and broker configured with BBDO3
      * When the external command DISABLE_HOST_CHECK on host_1 is executed
      * Then the host_1 host checks should be disabled
      * When the external command ENABLE_HOST_CHECK on host_1 is executed
      * Then the host_1 host checks should be enabled
64. **BEEXTCMD24**: external command DISABLE_HOST_CHECK and ENABLE_HOST_CHECK on bbdo2.0
65. **BEEXTCMD25**: external command DISABLE_HOST_EVENT_HANDLER and ENABLE_HOST_EVENT_HANDLER on bbdo3.0
66. **BEEXTCMD26**: external command DISABLE_HOST_EVENT_HANDLER and ENABLE_HOST_EVENT_HANDLER on bbdo2.0
67. **BEEXTCMD27**: external command DISABLE_HOST_FLAP_DETECTION and ENABLE_HOST_FLAP_DETECTION on bbdo3.0
68. **BEEXTCMD28**: external command DISABLE_HOST_FLAP_DETECTION and ENABLE_HOST_FLAP_DETECTION on bbdo2.0
69. **BEEXTCMD29**: external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo3.0
70. **BEEXTCMD3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0
71. **BEEXTCMD30**: external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo2.0
72. **BEEXTCMD31**: external command DISABLE_HOST_SVC_CHECKS and ENABLE_HOST_SVC_CHECKS on bbdo3.0
73. **BEEXTCMD32**: external command DISABLE_HOST_SVC_CHECKS and ENABLE_HOST_SVC_CHECKS on bbdo2.0
74. **BEEXTCMD33**: external command DISABLE_HOST_SVC_NOTIFICATIONS and ENABLE_HOST_SVC_NOTIFICATIONS on bbdo3.0
75. **BEEXTCMD34**: external command DISABLE_HOST_SVC_NOTIFICATIONS and ENABLE_HOST_SVC_NOTIFICATIONS on bbdo2.0
76. **BEEXTCMD35**: external command DISABLE_PASSIVE_HOST_CHECKS and ENABLE_PASSIVE_HOST_CHECKS on bbdo3.0
77. **BEEXTCMD36**: external command DISABLE_PASSIVE_HOST_CHECKS and ENABLE_PASSIVE_HOST_CHECKS on bbdo2.0
78. **BEEXTCMD37**: external command DISABLE_PASSIVE_SVC_CHECKS and ENABLE_PASSIVE_SVC_CHECKS on bbdo3.0
79. **BEEXTCMD38**: external command DISABLE_PASSIVE_SVC_CHECKS and ENABLE_PASSIVE_SVC_CHECKS on bbdo2.0
80. **BEEXTCMD39**: external command START_OBSESSING_OVER_HOST and STOP_OBSESSING_OVER_HOST on bbdo3.0
81. **BEEXTCMD4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0
82. **BEEXTCMD40**: external command START_OBSESSING_OVER_HOST and STOP_OBSESSING_OVER_HOST on bbdo2.0
83. **BEEXTCMD41**: external command START_OBSESSING_OVER_SVC and STOP_OBSESSING_OVER_SVC on bbdo3.0
84. **BEEXTCMD42**: external command START_OBSESSING_OVER_SVC and STOP_OBSESSING_OVER_SVC on bbdo2.0
85. **BEEXTCMD5**: external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo3.0
86. **BEEXTCMD6**: external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo2.0
87. **BEEXTCMD7**: external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo3.0
88. **BEEXTCMD8**: external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo2.0
89. **BEEXTCMD9**: external command CHANGE_MAX_SVC_CHECK_ATTEMPTS with bbdo3.0
90. **BEEXTCMD_COMPRESS_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and compressed grpc
91. **BEEXTCMD_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and grpc
92. **BEEXTCMD_GRPC2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 and grpc
93. **BEEXTCMD_GRPC3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 and grpc
94. **BEEXTCMD_GRPC4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 and grpc
95. **BEEXTCMD_REVERSE_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and reversed gRPC
96. **BEEXTCMD_REVERSE_GRPC2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 and grpc reversed
97. **BEEXTCMD_REVERSE_GRPC3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 and grpc reversed
98. **BEEXTCMD_REVERSE_GRPC4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 and grpc reversed
99. **BEHOSTCHECK**: 
      * Given Engine and Broker configured to work with BBDO 3
      * When a schedule forced host check command on host host_1 is launched
      * Then the result appears in the centreon_storage resources table
100. **BEHS1**: store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
101. **BEINSTANCE**: Instance to bdd
102. **BEINSTANCESTATUS**: Instance status to bdd
103. **BENCH_${nb_checks}STATUS**: external command CHECK_SERVICE_RESULT 1000 times
104. **BENCH_${nb_checks}STATUS_TRACES**: external command CHECK_SERVICE_RESULT ${nb_checks} times
105. **BENCH_${nb_checks}_REVERSE_SERVICE_STATUS_TRACES_WITHOUT_SQL**: Broker is configured without SQL output. The connection between Engine and Broker is reversed. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times. Logs are in trace level.
106. **BENCH_${nb_checks}_REVERSE_SERVICE_STATUS_WITHOUT_SQL**: Broker is configured without SQL output. The connection between Engine and Broker is reversed. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times.
107. **BENCH_${nb_checks}_SERVICE_STATUS_TRACES_WITHOUT_SQL**: Broker is configured without SQL output. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times. Logs are in trace level.
108. **BENCH_${nb_checks}_SERVICE_STATUS_WITHOUT_SQL**: Broker is configured without SQL output. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times.
109. **BENCH_1000STATUS_100${suffixe}**: external command CHECK_SERVICE_RESULT 100 times    with 100 pollers with 20 services
110. **BENCV**: Engine is configured with hosts/services. The first host has no customvariable. Then we add a customvariable to the first host and we reload engine. Then the host should have this new customvariable defined and centengine should not crash.
111. **BEOTEL_CENTREON_AGENT_CEIP**: Scenario: Agent and "centreon_storage.agent_information" Statistics
      * Given Engine connected to Broker
      * When an agent connects to Engine
112. **BEOTEL_CENTREON_AGENT_CHECK_COUNTER**: 
      * Given an agent with counter check, we expect to get the correct status for the centagent process running on windows host
113. **BEOTEL_CENTREON_AGENT_CHECK_DIFFERENT_INTERVAL**: 
      * Given and agent who has to execute checks with different intervals, we expect to find these intervals in data_bin
114. **BEOTEL_CENTREON_AGENT_CHECK_EVENTLOG**: 
      * Given an agent with eventlog check, we expect status, output and metrics
115. **BEOTEL_CENTREON_AGENT_CHECK_HEALTH**: agent check health and we expect to get it in check result
116. **BEOTEL_CENTREON_AGENT_CHECK_HOST**: 
      * Given an agent host checked by centagent, we set a first output to check command, 
      * modify it, reload engine and expect the new output in resource table
117. **BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED**: agent check host with encrypted connection and we expect to get it in check result
118. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_CPU**: agent check service with native check cpu and we expect to get it in check result
119. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_MEMORY**: agent check service with native check memory and we expect to get it in check result
120. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_SERVICE**: agent check service with native check service and we expect to get it in check result
121. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_STORAGE**: agent check service with native check storage and we expect to get it in check result
122. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_UPTIME**: agent check service with native check uptime and we expect to get it in check result
123. **BEOTEL_CENTREON_AGENT_CHECK_PROCESS**: 
      * Given an agent with eventlog check, we expect to get the correct status for thr centagent process running on windows host
124. **BEOTEL_CENTREON_AGENT_CHECK_SERVICE**: agent check service and we expect to get it in check result
125. **BEOTEL_CENTREON_AGENT_LINUX_NO_DEFUNCT_PROCESS**: agent check host and we expect to get it in check result
126. **BEOTEL_CENTREON_AGENT_TOKEN**: 
      * Given the Centreon Engine is configured with OpenTelemetry server with encryption enabled
      * When the Centreon Agent attempts to connect using an valid JWT token
      * Then the connection should be accepted
      * And the log should confirm that the token is valid
127. **BEOTEL_CENTREON_AGENT_TOKEN_AGENT_TELEGRAPH**: 
      * Given an OpenTelemetry server is configured with token-based connection
128. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED**: 
      * Given the OpenTelemetry server is configured with encryption enabled
129. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED_WHILE_RUNNING**: 
      * Given the OpenTelemetry server is configured with encryption enabled
130. **BEOTEL_CENTREON_AGENT_TOKEN_UNTRUSTED**: 
      * Given the OpenTelemetry server is configured with encryption enabled
131. **BEOTEL_INVALID_CHECK_COMMANDS_AND_ARGUMENTS**: 
      * Given the agent is configured with native checks for services
      * And the OpenTelemetry server module is added
      * And services are configured with incorrect check commands and arguments
      * When the broker, engine, and agent are started
      * Then the resources table should be updated with the correct status
      * And appropriate error messages should be generated for invalid checks
132. **BEOTEL_REVERSE_CENTREON_AGENT_CHECK_HOST**: agent check host with reversed connection and we expect to get it in check result
133. **BEOTEL_REVERSE_CENTREON_AGENT_CHECK_HOST_CRYPTED**: agent check host with encrypted reversed connection and we expect to get it in check result
134. **BEOTEL_REVERSE_CENTREON_AGENT_CHECK_SERVICE**: agent check service with reversed connection and we expect to get it in check result
135. **BEOTEL_SERVE_TELEGRAF_CONFIGURATION_CRYPTED**: we configure engine with a telegraf conf server and we check telegraf conf file
136. **BEOTEL_SERVE_TELEGRAF_CONFIGURATION_NO_CRYPTED**: we configure engine with a telegraf conf server and we check telegraf conf file
137. **BEOTEL_TELEGRAF_CHECK_HOST**: we send nagios telegraf formatted datas and we expect to get it in check result
138. **BEOTEL_TELEGRAF_CHECK_SERVICE**: we send nagios telegraf formatted datas and we expect to get it in check result
139. **BEPBBEE1**: central-module configured with bbdo_version 3.0 but not others. Unable to establish connection.
140. **BEPBBEE2**: bbdo_version 3 not compatible with sql/storage
141. **BEPBBEE3**: bbdo_version 3 generates new bbdo protobuf service status messages.
142. **BEPBBEE4**: bbdo_version 3 generates new bbdo protobuf host status messages.
143. **BEPBBEE5**: bbdo_version 3 generates new bbdo protobuf service messages.
144. **BEPBCVS**: bbdo_version 3 communication of custom variables.
145. **BEPBHostParent**: bbdo_version 3 communication of host parent relations
146. **BEPBINST_CONF**: bbdo_version 3 communication of instance configuration.
147. **BEPBRI1**: bbdo_version 3 use pb_resource new bbdo protobuf ResponsiveInstance message.
148. **BERD1**: Scenario: Starting/stopping Broker does not create duplicated events.
149. **BERD2**: Scenario: Starting/stopping Engine does not create duplicated events.
150. **BERDUC1**: Scenario: Starting/stopping Engine does not create duplicated events in usual cases
151. **BERDUC2**: Scenario: Starting/stopping Engine does not create duplicated events in usual cases
152. **BERDUC3U1**: Scenario: Starting/stopping Broker does not create duplicated events in usual cases with unified_sql and BBDO 3.0
153. **BERDUC3U2**: Scenario: Starting/stopping Engine does not create duplicated events in usual cases with unified_sql and BBDO 3.0
154. **BERDUCA300**: Scenario: Starting/stopping Engine is stopped; it should emit a stop event and receive an ack event with events to clean from broker.
155. **BERDUCA301**: Scenario: Starting/stopping Engine is stopped; it should emit a stop event and receive an ack event with events to clean from broker with bbdo 3.0.1.
156. **BERDUCU1**: Starting/stopping Broker does not create duplicated events in usual cases with unified_sql7
157. **BERDUCU2**: Scenario: Starting/stopping Engine does not create duplicated events in usual cases with unified_sql
158. **BERES1**: store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
159. **BESERVCHECK**: external command CHECK_SERVICE_RESULT
160. **BESS1**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
161. **BESS2**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
162. **BESS2U**: Start-Stop Broker/Engine - Broker started first - Engine stopped first. Unified_sql is used.
163. **BESS3**: Start-Stop Broker/Engine - Engine started first - Engine stopped first
164. **BESS4**: Start-Stop Broker/Engine - Engine started first - Broker stopped first
165. **BESS5**: Start-Stop Broker/engine - Engine debug level is set to all, it should not hang
166. **BESSBQ1**: A very bad queue file is written for broker. Broker and Engine are then started, Broker must read the file raising an error because of that file and then get data sent by Engine.
167. **BESSCTO**: Scenario: Service commands time out due to missing Perl Connector
      * Given the Engine is configured as usual but without the Perl Connector
      * When the Engine executes its service commands
      * Then the commands take too long and reach the timeout
      * And the Engine starts and stops two times as a result
168. **BESSCTOWC**: Scenario: Service commands time out due to missing Perl Connector
      * Given the Engine is configured as usual with some commands using the Perl Connector
      * When the Engine executes its service commands
      * Then the commands take too long and reach the timeout
      * And the Engine starts and stops two times as a result
169. **BESSG**: Scenario: Broker handles connection and disconnection with Engine
      * Given Broker is configured with only one output that is Graphite
      * When the Engine starts and connects to the Broker
      * Then the Broker must be able to handle the connection
      * When the Engine stops
      * Then the Broker must be able to handle the disconnection
170. **BESS_CRYPTED_GRPC1**: Start-Stop grpc version Broker/Engine - well configured
171. **BESS_CRYPTED_GRPC2**: Start-Stop grpc version Broker/Engine only server crypted
172. **BESS_CRYPTED_GRPC3**: Start-Stop grpc version Broker/Engine only engine crypted
173. **BESS_CRYPTED_REVERSED_GRPC1**: Start-Stop grpc version Broker/Engine - well configured
174. **BESS_CRYPTED_REVERSED_GRPC2**: Start-Stop grpc version Broker/Engine only engine server crypted
175. **BESS_CRYPTED_REVERSED_GRPC3**: Start-Stop grpc version Broker/Engine only engine crypted
176. **BESS_ENGINE_DELETE_HOST**: once engine and cbd started, stop and restart cbd, delete an host and reload engine, cbd mustn't core
177. **BESS_GRPC1**: Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped first
178. **BESS_GRPC2**: Start-Stop grpc version Broker/Engine - Broker started first - Engine stopped first
179. **BESS_GRPC3**: Start-Stop grpc version Broker/Engine - Engine started first - Engine stopped first
180. **BESS_GRPC4**: Start-Stop grpc version Broker/Engine - Engine started first - Broker stopped first
181. **BESS_GRPC5**: Start-Stop grpc version Broker/engine - Engine debug level is set to all, it should not hang
182. **BESS_GRPC_COMPRESS1**: Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped last compression activated
183. **BETAG1**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Broker is started before.
184. **BETAG2**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
185. **BEUTAG1**: Engine is configured with some tags. When broker receives them through unified_sql stream, it stores them in the centreon_storage.tags table. Broker is started before.
186. **BEUTAG10**: some services are configured with tags on two pollers. Then tags are removed from some of them and in centreon_storage, we can observe resources_tags table updated.
187. **BEUTAG11**: Scenario: Updating resource tags after changing several tags
      * Given some services are configured with tags on two pollers
      * Then the resources_tags table contains them
      * When several tags are changed
      * Then the resources_tags table is updated
188. **BEUTAG12**: Engine is configured with some tags. Group tags tag2, tag6 are set to hosts 1 and 2. Category tags tag4 and tag8 are added to hosts 2, 3, 4. The resources and resources_tags tables are well filled. The tag6 and tag8 are removed and resources_tags is also well updated.
189. **BEUTAG2**: Engine is configured with some tags. A new service is added with a tag. Broker should make the relations.
190. **BEUTAG3**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
191. **BEUTAG4**: Engine is configured with some tags. Group tags tag9, tag13 are set to services 1 and 3. Category tags tag3 and tag11 are added to services 1, 3, 5 and 6. The centreon_storage.resources and resources_tags tables are well filled.
192. **BEUTAG5**: Engine is configured with some tags. Group tags tag2, tag6 are set to hosts 1 and 2. Category tags tag4 and tag8 are added to hosts 2, 3, 4. The resources and resources_tags tables are well filled.
193. **BEUTAG6**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.resources_tags table. Engine is started before.
194. **BEUTAG7**: Some services are configured with tags on two pollers. Then tags configuration is modified.
195. **BEUTAG8**: Services have tags provided by templates.
196. **BEUTAG9**: hosts have tags provided by templates.
197. **BEUTAG_REMOVE_HOST_FROM_HOSTGROUP**: remove a host from hostgroup, reload, insert 2 host in the hostgroup must not make sql error
198. **BE_BACKSLASH_CHECK_RESULT**: external command PROCESS_SERVICE_CHECK_RESULT with \:
199. **BE_DEFAULT_NOTIFCATION_INTERVAL_IS_ZERO_SERVICE_RESOURCE**: default notification_interval must be set to NULL in services, hosts and resources tables.
200. **BE_FLAPPING_HOST_RESOURCE**: With BBDO 3, flapping detection must be set in hosts and resources tables.
201. **BE_FLAPPING_SERVICE_RESOURCE**: With BBDO 3, flapping detection must be set in services and resources tables.
202. **BE_NOTIF_OVERFLOW**: bbdo 2.0 notification number =40000. make an overflow => notification_number null in db
203. **BE_TIME_NULL_SERVICE_RESOURCE**: With BBDO 3, notification_interval time must be set to NULL on 0 in services, hosts and resources tables.
204. **BRCS1**: Broker reverse connection stopped
205. **BRCTS1**: Broker reverse connection too slow
206. **BRCTSMN**: 
      * Given Broker, Engine configured as usual
      * And map also connected to Broker with a filter allowing only 'neb' category
      * When Engine sends pb_service, pb_host, pb_service_status and pb_host_status
      * Then map receives correctly them.
207. **BRCTSMNS**: 
      * Given Broker, Engine configured as usual
      * And map also connected to Broker with a filter allowing 'neb' and 'storage' categories
      * When Engine sends pb_service, pb_host, pb_service_status, pb_host_status and metrics
      * Then Map receives correctly them.
208. **BRGC1**: Broker good reverse connection
209. **BRRDCDDID1**: RRD metrics deletion from index ids with rrdcached.
210. **BRRDCDDIDDB1**: RRD metrics deletion from index ids with a query in centreon_storage with rrdcached.
211. **BRRDCDDIDU1**: RRD metrics deletion from index ids with unified sql output with rrdcached.
212. **BRRDCDDM1**: RRD metrics deletion from metric ids with rrdcached.
213. **BRRDCDDMDB1**: RRD metrics deletion from metric ids with a query in centreon_storage and rrdcached.
214. **BRRDCDDMID1**: RRD deletion of non existing metrics and indexes with rrdcached
215. **BRRDCDDMIDU1**: RRD deletion of non existing metrics and indexes with rrdcached
216. **BRRDCDDMU1**: RRD metric deletion on table metric with unified sql output with rrdcached
217. **BRRDCDRB1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with storage/sql sql output and rrdcached.
218. **BRRDCDRBDB1**: RRD metric rebuild with a query in centreon_storage and unified sql with rrdcached
219. **BRRDCDRBU1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with unified_sql output and rrdcached.
220. **BRRDCDRBUDB1**: RRD metric rebuild with a query in centreon_storage and unified sql with rrdcached
221. **BRRDDID1**: RRD metrics deletion from index ids.
222. **BRRDDIDDB1**: RRD metrics deletion from index ids with a query in centreon_storage.
223. **BRRDDIDU1**: RRD metrics deletion from index ids with unified sql output.
224. **BRRDDM1**: RRD metrics deletion from metric ids.
225. **BRRDDMDB1**: RRD metrics deletion from metric ids with a query in centreon_storage.
226. **BRRDDMID1**: RRD deletion of non existing metrics and indexes
227. **BRRDDMIDU1**: RRD deletion of non existing metrics and indexes
228. **BRRDDMU1**: RRD metric deletion on table metric with unified sql output
229. **BRRDRBDB1**: RRD metric rebuild with a query in centreon_storage and unified sql
230. **BRRDRBUDB1**: RRD metric rebuild with a query in centreon_storage and unified sql
231. **BRRDRM1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with storage/sql sql output.
232. **BRRDRMU1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with unified_sql output.
233. **BRRDSTATUS**: We are working with BBDO3. This test checks status are correctly handled independently from their value.
234. **BRRDSTATUSRETENTION**: We are working with BBDO3. This test checks status are not sent twice after Engine reload.
235. **BRRDUPLICATE**: RRD metric rebuild with a query in centreon_storage and unified sql with duplicate rows in database
236. **BRRDWM1**: We are working with BBDO3. This test checks protobuf metrics and status are sent to cbd RRD.
237. **CBD_RELOAD_AND_FILTERS**: We start engine/broker with a classical configuration. All is up and running. Some filters are added to the rrd output and cbd is reloaded. All is still up and running but some events are rejected. Then all is newly set as filter and all events are sent to rrd broker.
238. **CBD_RELOAD_AND_FILTERS_WITH_OPR**: We start engine/broker with an almost classical configuration, just the connection between cbd central and cbd rrd is reversed with one peer retention. All is up and running. Some filters are added to the rrd output and cbd is reloaded. All is still up and running but some events are rejected. Then all is newly set as filter and all events are sent to rrd broker.
239. **DTIM**: New services with several pollers are created. Then downtimes are set on all configured hosts. This action results on 5250 downtimes if we also count impacted services. Then all these downtimes are removed. This test is done with BBDO 3.0.1
240. **EBBM1**: A service status contains metrics that do not fit in a float number.
241. **EBBPS1**: 1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table resources.
242. **EBBPS2**: 1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table services.
243. **EBDP1**: Four new pollers are started and then we remove Poller3.
244. **EBDP2**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
245. **EBDP3**: Three new pollers are started, then they are killed. It is still possible to remove Poller2 if removed from the configuration.
246. **EBDP4**: Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by Broker.
247. **EBDP5**: Four new pollers are started and then we remove Poller3.
248. **EBDP6**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
249. **EBDP7**: Three new pollers are started, then they are killed. It is still possible to remove Poller2 if removed from the configuration.
250. **EBDP8**: Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by broker.
251. **EBDP_GRPC2**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
252. **EBMSSM**: 1000 services are configured with 100 metrics each. The rrd output is removed from the broker configuration. GetSqlManagerStats is called to measure writes into data_bin.
253. **EBMSSMDBD**: 1000 services are configured with 100 metrics each. The rrd output is removed from the broker configuration. While metrics are written in the database, we stop the database and then restart it. Broker must recover its connection to the database and continue to write metrics.
254. **EBMSSMPART**: Scenario: Broker continues writing metrics after partition recreation
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
255. **EBNHG1**: New host group with several pollers and connections to DB
256. **EBNHG4**: New host group with several pollers and connections to DB with broker and rename this hostgroup
257. **EBNHGU1**: New host group with several pollers and connections to DB with broker configured with unified_sql
258. **EBNHGU2**: New host group with several pollers and connections to DB with broker configured with unified_sql
259. **EBNHGU3**: New host group with several pollers and connections to DB with broker configured with unified_sql
260. **EBNHGU4_${test_label}**: New host group with several pollers and connections to DB with broker and rename this hostgroup
261. **EBNSG1**: New service group with several pollers and connections to DB
262. **EBNSGU1**: New service group with several pollers and connections to DB with broker configured with unified_sql
263. **EBNSGU2**: New service group with several pollers and connections to DB with broker configured with unified_sql
264. **EBNSGU3_${test_label}**: New service group with several pollers and connections to DB with broker and rename this servicegroup
265. **EBNSVC1**: New services with several pollers
266. **EBPN0**: Verify if child is in queue when parent is down.
267. **EBPN1**: verify relation parent child when delete parent.
268. **EBPN2**: verify relation parent child when delete child.
269. **EBPS2**: 1000 services are configured with 20 metrics each. The rrd output is removed from the broker configuration to avoid to write too many rrd files. While metrics are written in bulk, the database is stopped. This must not crash broker.
270. **EBSAU2**: New hosts with action_url with more than 2000 characters
271. **EBSN3**: New hosts with notes with more than 500 characters
272. **EBSN4**: New hosts with No Alias / Alias and have A Template
273. **EBSNU1**: New hosts with notes_url with more than 2000 characters
274. **ENRSCHE1**: Verify that next check of a rescheduled host is made at last_check + interval_check
275. **FILTER_ON_LUA_EVENT**: stream connector with a bad configured filter generate a log error message
276. **GRPC_CLOUD_FAILURE**: simulate a broker failure in cloud environment, we provide a muted grpc server and there must remain only one grpc connection. Then we start broker and connection must be ok
277. **GRPC_RECONNECT**: We restart broker and engine must reconnect to it and send data
278. **LCDNU**: the lua cache updates correctly service cache.
279. **LCDNUH**: the lua cache updates correctly host cache
280. **LOGV2DB1**: log-v2 disabled old log enabled check broker sink
281. **LOGV2DB2**: log-v2 disabled old log disabled check broker sink
282. **LOGV2DF1**: log-v2 disabled old log enabled check logfile sink
283. **LOGV2DF2**: log-v2 disabled old log disabled check logfile sink
284. **LOGV2EB1**: Checking broker sink when log-v2 is enabled and legacy logs are disabled.
285. **LOGV2EB2**: log-v2 enabled old log enabled check broker sink
286. **LOGV2EBU1**: Checking broker sink when log-v2 is enabled and legacy logs are disabled with bbdo3.
287. **LOGV2EBU2**: Check Broker sink with log-v2 enabled and legacy log enabled with BBDO3.
288. **LOGV2EF1**: log-v2 enabled    old log disabled check logfile sink
289. **LOGV2EF2**: log-v2 enabled old log enabled check logfile sink
290. **LOGV2FE2**: log-v2 enabled old log enabled check logfile sink
291. **LUA_CACHE_SAVE_BBDO3**: 
      * Given a engine broker configured in bbdo2, we check that services and hosts are stored in bbdo3 format in cache
      * To do that we compare host and service event with lua cache
292. **MOVE_HOST_OF_HOSTGROUP_TO_ANOTHER_POLLER**: Scenario: Moving hosts between pollers without losing hostgroup tag
      * Given two pollers each with two hosts
      * And all hosts belong to the same hostgroup
      * When I move two hosts from one poller to the other
      * Then the hostgroup tag of the moved hosts is not erased
293. **NON_TLS_CONNECTION_WARNING**: 
      * Given an agent starts a non-TLS connection,
      * we expect to get a warning message.
294. **NON_TLS_CONNECTION_WARNING_ENCRYPTED**: 
      * Given agent with encrypted connection, we expect no warning message.
295. **NON_TLS_CONNECTION_WARNING_FULL**: 
      * Given an agent starts a non-TLS connection,
      * we expect to get a warning message.
      * After 1 hour, we expect to get a warning message about the connection time expired
      * and the connection killed.
296. **NON_TLS_CONNECTION_WARNING_FULL_REVERSED**: 
      * Given an agent starts a non-TLS connection reverse,
      * we expect to get a warning message.
      * After 1 hour, we expect to get a warning message about the connection time expired
      * and the connection killed.
297. **NON_TLS_CONNECTION_WARNING_REVERSED**: 
      * Given an agent starts a non-TLS connection reversed,
      * we expect to get a warning message.
298. **NON_TLS_CONNECTION_WARNING_REVERSED_ENCRYPTED**: 
      * Given agent with encrypted reversed connection, we expect no warning message.
299. **NO_FILTER_NO_ERROR**: no filter configured => no filter error.
300. **RENAME_PARENT**: 
      * Given an host with a parent host. We rename the parent host and check if the child host is still linked to the parent.
      * Engine mustn't crash and log an error on reload.
301. **RLCode**: Test if reloading LUA code in a stream connector applies the changes
302. **RRD1**: RRD metric rebuild asked with gRPC API. Three non existing indexes IDs are selected then an error message is sent. This is done with unified_sql output.
303. **SDER**: The check attempts and the max check attempts of (host_1,service_1) are changed to 280 thanks to the retention.dat file. Then Engine and Broker are started and Broker should write these values in the services and resources tables. We only test the services table because we need a resources table that allows bigger numbers for these two attributes. But we see that Broker doesn't crash anymore.
304. **SEVERAL_FILTERS_ON_LUA_EVENT**: Two stream connectors with different filters are configured.
305. **STORAGE_ON_LUA**: The category 'storage' is applied on the stream connector. Only events of this category should be sent to this stream.
306. **STUPID_FILTER**: Unified SQL is configured with only the bbdo category as filter. An error is raised by broker and broker should run correctly.
307. **Service_increased_huge_check_interval**: New services with huge check interval at creation time. Given Engine and Broker are configured with 1 poller and 10 hosts When Engine is started Then host_1 should be pending When a check result with metrics is processed for service_1 Then metrics should be created and sent to rrd broker When service_1 metrics are analyzed Then metrics should have minimal heartbeat of 3000 and pdp_per_row of 300 When a new service is created with a check interval of 90 And Engine is reloaded Then the new service should be pending When a check result with metrics is processed for the new service Then metrics should be created and sent to rrd Broker When new service metrics are analyzed Then metrics should have minimal heartbeat of 54000 and pdp_per_row of 5400
308. **Services_and_bulks_${id}**: One service is configured with one metric with a name of 150 to 1021 characters.
309. **Start_Stop_Broker_Engine_${id}**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
310. **Start_Stop_Engine_Broker_${id}**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
311. **UNIFIED_SQL_FILTER**: With bbdo version 3.0.1, we watch events written or rejected in unified_sql
312. **VICT_ONE_CHECK_METRIC**: victoria metrics metric output
313. **VICT_ONE_CHECK_METRIC_AFTER_FAILURE**: victoria metrics metric output after victoria shutdown
314. **VICT_ONE_CHECK_STATUS**: victoria metrics status output
315. **Whitelist_Directory_Rights**: log if /etc/centreon-engine-whitelist has not mandatory rights or owner
316. **Whitelist_Empty_Directory**: log if /etc/centreon-engine-whitelist is empty
317. **Whitelist_Host**: Test on allowed and forbidden commands for hosts
318. **Whitelist_No_Whitelist_Directory**: log if /etc/centreon-engine-whitelist doesn't exist
319. **Whitelist_Perl_Connector**: test allowed and forbidden commands for services
320. **Whitelist_Service**: test allowed and forbidden commands for services
321. **Whitelist_Service_EH**: test allowed and forbidden event handler for services
322. **metric_mapping**: Check if metric name exists using a stream connector
323. **not1**: This test case configures a single service and verifies that a notification is sent when the service is in a non-OK HARD state.
324. **not10**: This test case involves scheduling downtime on a down host that already had a critical notification. When The Host return to UP state we should receive a recovery notification.
325. **not11**: This test case involves configuring one service and checking that three alerts are sent for it.
326. **not12**: Escalations
327. **not13**: notification for a dependencies host
328. **not14**: notification for a Service dependency
329. **not15**: several notification commands for the same user.
330. **not16**: notification for dependencies services group
331. **not17**: notification for a dependensies host group
332. **not18**: notification delay where first notification delay equal retry check
333. **not19**: notification delay where first notification delay greater than retry check 
334. **not1_WL_KO**: This test case configures a single service. When it is in non-OK HARD state a notification should be sent but it is not allowed by the whitelist
335. **not1_WL_OK**: This test case configures a single service. When it is in non-OK HARD state a notification is sent because it is allowed by the whitelist
336. **not2**: This test case configures a single service and verifies that a recovery notification is sent
337. **not20**: notification delay where first notification delay samller than retry check
338. **not3**: This test case configures a single service and verifies the notification system's behavior during and after downtime
339. **not4**: This test case configures a single service and verifies the notification system's behavior during and after acknowledgement
340. **not5**: This test case configures two services with two different users being notified when the services transition to a critical state.
341. **not6**: This test case validate the behavior when the notification time period is set to null.
342. **not7**: This test case simulates a host alert scenario.
343. **not8**: This test validates the critical host notification.
344. **not9**: This test case configures a single host and verifies that a recovery notification is sent after the host recovers from a non-OK state.
345. **not_in_timeperiod_with_send_recovery_notifications_anyways**: This test case configures a single service and verifies that a notification is sent when the service is in a non-OK state and OK is sent outside timeperiod when _send_recovery_notifications_anyways is set
346. **not_in_timeperiod_without_send_recovery_notifications_anyways**: This test case configures a single service and verifies that a notification is sent when the service is in a non-OK state and OK is not sent outside timeperiod when _send_recovery_notifications_anyways is not set

### Ccc
1. **BECCC1**: ccc without port fails with an error message
2. **BECCC2**: ccc with -p 51001 connects to central cbd gRPC server.
3. **BECCC3**: ccc with -p 50001 connects to centengine gRPC server.
4. **BECCC4**: ccc with -p 51001 -l returns the available functions from Broker gRPC server
5. **BECCC5**: ccc with -p 51001 -l GetVersion returns an error because we can't execute a command with -l.
6. **BECCC6**: ccc with -p 51001 GetVersion{} calls the GetVersion command
7. **BECCC7**: ccc with -p 51001 GetVersion{"idx":1} returns an error because the input message is wrong.
8. **BECCC8**: ccc with -p 50001 EnableServiceNotifications{"names":{"host_name": "host_1", "service_name": "service_1"}} works and returns an empty message.

### Connector perl
1. **test use connector perl exist script**: test exist script
2. **test use connector perl multiple script**: test script multiple
3. **test use connector perl unknown script**: test unknown script

### Connector ssh
1. **Test6Hosts**: as 127.0.0.x point to the localhost address we will simulate check on 6 hosts
2. **TestBadPwd**: test bad password
3. **TestBadUser**: test unknown user
4. **TestWhiteList**: as 127.0.0.x point to the localhost address we will simulate check on 6 hosts

### Engine
1. **EBSN5**: Verify contactgroup inheritance : contactgroup(empty) inherit from template (full) , on Start Engine
2. **EBSN6**: Verify contactgroup inheritance : contactgroup(full) inherit from template (full) , on Start Engine
3. **EBSN7**: Verify contactgroup inheritance : contactgroup(empty) inherit from template (full) , on Reload Engine
4. **EBSN8**: Verify contactgroup inheritance : contactgroup(full) inherit from template (full) , on Reload Engine
5. **ECI0**: Verify contact inheritance : contact(empty) inherit from template (full) , on Start Engine
6. **ECI1**: Verify contact inheritance : contact(full) inherit from template (full) , on Start Engine
7. **ECI2**: Verify contact inheritance : contact(empty) inherit from template (full) , on Reload Engine
8. **ECI3**: Verify contact inheritance : contact(full) inherit from template (full) , on Reload Engine
9. **ECMI0**: Verify command inheritance : command(empty) inherit from template (full) , on Start Engine
10. **ECMI1**: Verify command inheritance : command(full) inherit from template (full) , on Start Engine
11. **ECMI2**: Verify command inheritance : command(empty) inherit from template (full) , on Reload Engine
12. **ECMI3**: Verify command inheritance : command(full) inherit from template (full) , on reload Engine
13. **ECOI0**: Verify connector inheritance : connector(empty) inherit from template (full) , on Start Engine
14. **ECOI1**: Verify connector inheritance : connector(full) inherit from template (full) , on Start Engine
15. **ECOI2**: Verify connector inheritance : connector(empty) inherit from template (full) , on Reload Engine
16. **ECOI3**: Verify connector inheritance : connector(full) inherit from template (full) , on Reload Engine
17. **EESI0**: Verify service escalation : create service escalation for every service in a service group
18. **EESI1**: Verify service escalation  inheritance : escalation(empty) inherit from template (full) , on Start Engine
19. **EESI2**: Verify service escalation  inheritance : escalation(full) inherit from template (full) , on Start Engine
20. **EESI3**: Verify service escalation  inheritance : escalation(empty) inherit from template (full) , on Reload Engine
21. **EESI4**: Verify service escalation  inheritance : escalation(full) inherit from template (full) , on Reload Engine
22. **EESI5**: Verfiy host escalation : create host escalation for every host in the hostgroup
23. **EESI6**: Verify host escalation inheritance : escalation(empty) inherit from template (full) , on Start Engine   
24. **EESI7**: Verify host escalation inheritance : escalation(full) inherit from template (full) , on Start Engine    
25. **EESI8**: Verify host escalation inheritance : escalation(empty) inherit from template (full) , on Reload Engine   
26. **EESI9**: Verify host escalation inheritance : escalation(full) inherit from template (full) , on Reload Engine    
27. **EFHC1**: Engine is configured with hosts and we force check one 5 times with bbdo2
28. **EFHC2**: Engine is configured with hosts and we force check on one 5 times on bbdo2
29. **EFHCU1**: Engine is configured with hosts and we force checks on one 5 times on bbdo3. Bbdo3 has no impact on this behavior. resources table is cleared before starting broker.
30. **EFHCU2**: Engine is configured with hosts and we force checks on one 5 times on bbdo3. Bbdo3 has no impact on this behavior.
31. **EHGI0**: Verify hostgroup inheritance : hostgroup(empty) inherit from template (full) , on Start Engine
32. **EHGI1**: Verify hostgroup inheritance : hostgroup(full) inherit from template (full) , on Start Engine
33. **EHGI2**: Verify hostgroup inheritance : hostgroup(empty) inherit from template (full) , on Reload Engine
34. **EHGI3**: Verify hostgroup inheritance : hostgroup(full) inherit from template (full) , on Reload Engine
35. **EHI0**: Verify inheritance host : host(empty) inherit from template (full) , on Start Engine
36. **EHI1**: Verify inheritance host : host(full) inherit from template (full) , on Start engine
37. **EHI2**: Verify inheritance host : host(empty) inherit from template (full) , on Reload engine
38. **EHI3**: Verify inheritance host : host(full) inherit from template (full) , on engine Reload
39. **EMACROS**: macros ADMINEMAIL and ADMINPAGER are replaced in check outputs
40. **EMACROS_NOTIF**: macros ADMINEMAIL and ADMINPAGER are replaced in notification commands
41. **EMACROS_SEMICOLON**: Macros with a semicolon are used even if they contain a semicolon.
42. **EMTI0**: Verify multiple inheritance host
43. **ENGINE_MANY_CHECKS**: 
      * Given a engine with many services and a unique check on each service with it's own env variables
      * We expect correct check result in logs and we checks returned args and service macros
44. **EPC1**: Check with perl connector
45. **ERL**: Engine is started and writes logs in centengine.log. Then we remove the log file. The file disappears but Engine is still writing into it. Engine is reloaded and the centengine.log should appear again.
46. **ESGI0**: Verify servicegroup inheritance : servicegroup(empty) inherit from template (full) , on Start Engine
47. **ESGI1**: Verify servicegroup inheritance : servicegroup(empty) inherit from template (full) , on Start Engine
48. **ESGI2**: Verify servicegroup inheritance : servicegroup(empty) inherit from template (full) , on Reload Engine
49. **ESGI3**: Verify servicegroup inheritance : servicegroup(empty) inherit from template (full) , on Reload Engine
50. **ESI0**: Verify inheritance service : Service(empty) inherit from template (full) , on Start Engine
51. **ESI1**: Verify inheritance service : Service(full) inherit from template (full) , on Start Engine
52. **ESI2**: Verify inheritance service : Service(empty) inherit from template (full) , on Reload Engine
53. **ESI3**: Verify inheritance service : Service(full) inherit from template (full) , on Reload Engine
54. **ESS1**: Start-Stop (0s between start/stop) 5 times one instance of engine and no coredump
55. **ESS2**: Start-Stop (300ms between start/stop) 5 times one instance of engine and no coredump
56. **ESS3**: Start-Stop (0s between start/stop) 5 times three instances of engine and no coredump
57. **ESS4**: Start-Stop (300ms between start/stop) 5 times three instances of engine and no coredump
58. **ESSCTO**: Scenario: Engine services timeout due to missing Perl connector
      * Given the Engine is configured as usual without the Perl connector
      * When the Engine executes its service commands
      * Then the commands take too long and reach the timeout
      * And the Engine starts and stops two times as a result
59. **ESSCTOWC**: Scenario: Engine services timeout due to missing Perl connector
      * Given the Engine is configured as usual with some command using the Perl connector
      * When the Engine executes its service commands
      * Then the commands take too long and reach the timeout
      * And the Engine starts and stops two times as a result
60. **EXT_CONF1**: Engine configuration is overided by json conf
61. **EXT_CONF2**: Engine configuration is overided by json conf after reload
62. **E_FD_LIMIT**: Engine here is started with a low file descriptor limit. The engine should not crash and limit should be set.
63. **E_HOST_DOWN_DISABLE_SERVICE_CHECKS**: host_down_disable_service_checks is set to 1, host down switch all services to UNKNOWN
64. **E_HOST_UNREACHABLE_DISABLE_SERVICE_CHECKS**: host_down_disable_service_checks is set to 1, host unreachable switch all services to UNKNOWN
65. **VERIF**: 
      * When centengine is started in verification mode, it does not log in its file.
66. **VERIFIY_CONF**: Verify deprecated engine configuration options are logged as warnings Given the engine and broker are configured with module 1 And the engine configuration is set with deprecated options When the engine is started Then a warning message for 'auto_reschedule_checks' should be logged And a warning message for 'auto_rescheduling_interval' should be logged And a warning message for 'auto_rescheduling_window' should be logged And the engine should be stopped

### Migration
1. **MIGRATION**: Migration bbdo2 with sql/storage to bbdo2 with unified_sql and then to bbdo3 with unified_sql and then to bbdo2 with unified_sql and then to bbdo2 with sql/storage

### Severities
1. **BESEV1**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Broker is started before.
2. **BESEV2**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Engine is started before.
3. **BETUHSEV1**: Hosts have severities provided by templates.
4. **BETUSEV1**: Services have severities provided by templates.
5. **BEUHSEV1**: Four hosts have a severity added. Then we remove the severity from host 1. Then we change severity 10 to severity8 for host 3.
6. **BEUHSEV2**: Seven hosts are configured with a severity on two pollers. Then we remove severities from the first and second hosts of the first poller but only the severity from the first host of the second poller.
7. **BEUSEV1**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Broker is started before.
8. **BEUSEV2**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Engine is started before.
9. **BEUSEV3**: Four services have a severity added. Then we remove the severity from service 1. Then we change severity 11 to severity7 for service 3.
10. **BEUSEV4**: Seven services are configured with a severity on two pollers. Then we remove severities from the first and second services of the first poller but only the severity from the first service of the second poller. Then only severities no more used should be removed from the database.



583 tests currently implemented.
