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
16. **BA_IMPACT_IMPACT**: A BA of type impact is defined with two BAs of type impact as children. The first child has an impact of 90 and the second one of 10. When they are impacting both, the parent should be critical. When they are not impacting, the parent should be ok.
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
22. **BEBAMIDT1**: A BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. Then we set a downtime on this last one. An inherited downtime is set to the BA. The downtime is removed from the service, the inherited downtime is then deleted.
23. **BEBAMIDT2**: A BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. Then we set a downtime on this last one. An inherited downtime is set to the BA. Engine is restarted. Broker is restarted. The two downtimes are still there with no duplicates. The downtime is removed from the service, the inherited downtime is then deleted.
24. **BEBAMIDTU1**: With bbdo version 3.0.1, a BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. Then we set a downtime on this last one. An inherited downtime is set to the BA. The downtime is removed from the service, the inherited downtime is then deleted.
25. **BEBAMIDTU2**: With bbdo version 3.0.1, a BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. Then we set a downtime on this last one. An inherited downtime is set to the BA. Engine is restarted. Broker is restarted. The two downtimes are still there with no duplicates. The downtime is removed from the service, the inherited downtime is then deleted.
26. **BEBAMIGNDT1**: A BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. A first downtime is cancelled, the BA is still OK, but when the second downtime is cancelled, the BA should be CRITICAL.
27. **BEBAMIGNDT2**: A BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. The first downtime reaches its end, the BA is still OK, but when the second downtime reaches its end, the BA should be CRITICAL.
28. **BEBAMIGNDTU1**: With bbdo version 3.0.1, a BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. A first downtime is cancelled, the BA is still OK, but when the second downtime is cancelled, the BA should be CRITICAL.
29. **BEBAMIGNDTU2**: With bbdo version 3.0.1, a BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. The first downtime reaches its end, the BA is still OK, but when the second downtime reaches its end, the BA should be CRITICAL.
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
5. **ANO_DT4**: set dt on anomaly and on dependent service, delete last one don t delete first one
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
40. **BEDTMASS1**: New services with several pollers are created. Then downtimes are set on all configured hosts. This action results on 1050 downtimes if we also count impacted services. Then all these downtimes are removed. This test is done with BBDO 3.0.0
41. **BEDTMASS2**: New services with several pollers are created. Then downtimes are set on all configured hosts. This action results on 1050 downtimes if we also count impacted services. Then all these downtimes are removed. This test is done with BBDO 2.0
42. **BEDTRRD1**: A service is forced checked then a downtime is set on this service. The service is forced checked again and the downtime is removed. This test is done with BBDO 3.0.0. Then we should not get any error in cbd RRD of kind 'ignored update error in file...'.
43. **BEDTSVCFIXED**: A downtime is set on a service, the total number of downtimes is really 1 then we delete this downtime and the number of downtime is 0.
44. **BEDTSVCREN1**: A downtime is set on a service then the service is renamed. The downtime is still active on the renamed service. The downtime is removed from the renamed service and it is well removed.
45. **BEEXTCMD1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0
46. **BEEXTCMD10**: external command CHANGE_MAX_SVC_CHECK_ATTEMPTS on bbdo2.0
47. **BEEXTCMD11**: external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo3.0
48. **BEEXTCMD12**: external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo2.0
49. **BEEXTCMD13**: external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo3.0
50. **BEEXTCMD14**: external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo2.0
51. **BEEXTCMD15**: external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo3.0
52. **BEEXTCMD16**: external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo2.0
53. **BEEXTCMD17**: external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo3.0
54. **BEEXTCMD18**: external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo2.0
55. **BEEXTCMD19**: external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo3.0
56. **BEEXTCMD2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0
57. **BEEXTCMD20**: external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo2.0
58. **BEEXTCMD21**: external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS and ENABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo3.0
59. **BEEXTCMD22**: external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS and ENABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo2.0
60. **BEEXTCMD23**: external command DISABLE_HOST_CHECK and ENABLE_HOST_CHECK on bbdo3.0
61. **BEEXTCMD24**: external command DISABLE_HOST_CHECK and ENABLE_HOST_CHECK on bbdo2.0
62. **BEEXTCMD25**: external command DISABLE_HOST_EVENT_HANDLER and ENABLE_HOST_EVENT_HANDLER on bbdo3.0
63. **BEEXTCMD26**: external command DISABLE_HOST_EVENT_HANDLER and ENABLE_HOST_EVENT_HANDLER on bbdo2.0
64. **BEEXTCMD27**: external command DISABLE_HOST_FLAP_DETECTION and ENABLE_HOST_FLAP_DETECTION on bbdo3.0
65. **BEEXTCMD28**: external command DISABLE_HOST_FLAP_DETECTION and ENABLE_HOST_FLAP_DETECTION on bbdo2.0
66. **BEEXTCMD29**: external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo3.0
67. **BEEXTCMD3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0
68. **BEEXTCMD30**: external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo2.0
69. **BEEXTCMD31**: external command DISABLE_HOST_SVC_CHECKS and ENABLE_HOST_SVC_CHECKS on bbdo3.0
70. **BEEXTCMD32**: external command DISABLE_HOST_SVC_CHECKS and ENABLE_HOST_SVC_CHECKS on bbdo2.0
71. **BEEXTCMD33**: external command DISABLE_HOST_SVC_NOTIFICATIONS and ENABLE_HOST_SVC_NOTIFICATIONS on bbdo3.0
72. **BEEXTCMD34**: external command DISABLE_HOST_SVC_NOTIFICATIONS and ENABLE_HOST_SVC_NOTIFICATIONS on bbdo2.0
73. **BEEXTCMD35**: external command DISABLE_PASSIVE_HOST_CHECKS and ENABLE_PASSIVE_HOST_CHECKS on bbdo3.0
74. **BEEXTCMD36**: external command DISABLE_PASSIVE_HOST_CHECKS and ENABLE_PASSIVE_HOST_CHECKS on bbdo2.0
75. **BEEXTCMD37**: external command DISABLE_PASSIVE_SVC_CHECKS and ENABLE_PASSIVE_SVC_CHECKS on bbdo3.0
76. **BEEXTCMD38**: external command DISABLE_PASSIVE_SVC_CHECKS and ENABLE_PASSIVE_SVC_CHECKS on bbdo2.0
77. **BEEXTCMD39**: external command START_OBSESSING_OVER_HOST and STOP_OBSESSING_OVER_HOST on bbdo3.0
78. **BEEXTCMD4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0
79. **BEEXTCMD40**: external command START_OBSESSING_OVER_HOST and STOP_OBSESSING_OVER_HOST on bbdo2.0
80. **BEEXTCMD41**: external command START_OBSESSING_OVER_SVC and STOP_OBSESSING_OVER_SVC on bbdo3.0
81. **BEEXTCMD42**: external command START_OBSESSING_OVER_SVC and STOP_OBSESSING_OVER_SVC on bbdo2.0
82. **BEEXTCMD5**: external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo3.0
83. **BEEXTCMD6**: external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo2.0
84. **BEEXTCMD7**: external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo3.0
85. **BEEXTCMD8**: external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo2.0
86. **BEEXTCMD9**: external command CHANGE_MAX_SVC_CHECK_ATTEMPTS on bbdo3.0
87. **BEEXTCMD_COMPRESS_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and compressed grpc
88. **BEEXTCMD_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and grpc
89. **BEEXTCMD_GRPC2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 and grpc
90. **BEEXTCMD_GRPC3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 and grpc
91. **BEEXTCMD_GRPC4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 and grpc
92. **BEEXTCMD_REVERSE_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and reversed gRPC
93. **BEEXTCMD_REVERSE_GRPC2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 and grpc reversed
94. **BEEXTCMD_REVERSE_GRPC3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 and grpc reversed
95. **BEEXTCMD_REVERSE_GRPC4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 and grpc reversed
96. **BEHOSTCHECK**: external command CHECK_HOST_RESULT
97. **BEHS1**: store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
98. **BEINSTANCE**: Instance to bdd
99. **BEINSTANCESTATUS**: Instance status to bdd
100. **BENCH_${nb_checks}STATUS**: external command CHECK_SERVICE_RESULT 1000 times
101. **BENCH_${nb_checks}STATUS_TRACES**: external command CHECK_SERVICE_RESULT ${nb_checks} times
102. **BENCH_${nb_checks}_REVERSE_SERVICE_STATUS_TRACES_WITHOUT_SQL**: Broker is configured without SQL output. The connection between Engine and Broker is reversed. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times. Logs are in trace level.
103. **BENCH_${nb_checks}_REVERSE_SERVICE_STATUS_WITHOUT_SQL**: Broker is configured without SQL output. The connection between Engine and Broker is reversed. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times.
104. **BENCH_${nb_checks}_SERVICE_STATUS_TRACES_WITHOUT_SQL**: Broker is configured without SQL output. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times. Logs are in trace level.
105. **BENCH_${nb_checks}_SERVICE_STATUS_WITHOUT_SQL**: Broker is configured without SQL output. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times.
106. **BENCH_1000STATUS_100${suffixe}**: external command CHECK_SERVICE_RESULT 100 times    with 100 pollers with 20 services
107. **BENCV**: Engine is configured with hosts/services. The first host has no customvariable. Then we add a customvariable to the first host and we reload engine. Then the host should have this new customvariable defined and centengine should not crash.
108. **BEOTEL_CENTREON_AGENT_CEIP**: we connect an agent to engine and we expect a row in agent_information table
109. **BEOTEL_CENTREON_AGENT_CHECK_DIFFERENT_INTERVAL**: 
      * Given and agent who has to execute checks with different intervals, we expect to find these intervals in data_bin
110. **BEOTEL_CENTREON_AGENT_CHECK_EVENTLOG**: 
      * Given an agent with eventlog check, we expect status, output and metrics
111. **BEOTEL_CENTREON_AGENT_CHECK_HEALTH**: agent check health and we expect to get it in check result
112. **BEOTEL_CENTREON_AGENT_CHECK_HOST**: 
      * Given an agent host checked by centagent, we set a first output to check command, 
      * modify it, reload engine and expect the new output in resource table
113. **BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED**: agent check host with encrypted connection and we expect to get it in check result
114. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_CPU**: agent check service with native check cpu and we expect to get it in check result
115. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_MEMORY**: agent check service with native check memory and we expect to get it in check result
116. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_SERVICE**: agent check service with native check service and we expect to get it in check result
117. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_STORAGE**: agent check service with native check storage and we expect to get it in check result
118. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_UPTIME**: agent check service with native check uptime and we expect to get it in check result
119. **BEOTEL_CENTREON_AGENT_CHECK_PROCESS**: 
      * Given an agent with eventlog check, we expect to get the correct status for thr centagent process running on windows host
120. **BEOTEL_CENTREON_AGENT_CHECK_SERVICE**: agent check service and we expect to get it in check result
121. **BEOTEL_CENTREON_AGENT_LINUX_NO_DEFUNCT_PROCESS**: agent check host and we expect to get it in check result
122. **BEOTEL_CENTREON_AGENT_NO_TRUSTED_TOKEN**: 
      * Given the Centreon Engine is configured with OpenTelemetry server with encryption enabled with no trusted_token
      * When the Centreon Agent attempts to connect with tls
      * Then the connection should be accepted
123. **BEOTEL_CENTREON_AGENT_TOKEN**: 
      * Given the Centreon Engine is configured with OpenTelemetry server with encryption enabled
      * When the Centreon Agent attempts to connect using an valid JWT token
      * Then the connection should be accepted
      * And the log should confirm that the token is valid
124. **BEOTEL_CENTREON_AGENT_TOKEN_AGENT_TELEGRAPH**: 
      * Given an OpenTelemetry server is configured with token-based connection
125. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED**: 
      * Given the OpenTelemetry server is configured with encryption enabled
126. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED_WHILE_RUNNING**: 
      * Given the OpenTelemetry server is configured with encryption enabled
127. **BEOTEL_CENTREON_AGENT_TOKEN_MISSING_HEADER**: 
      * Given the Centreon Engine is configured with OpenTelemetry server with encryption enabled
      * When the Centreon Agent attempts to connect without a JWT token
      * Then the connection should be refused
      * And the log should contain the message "UNAUTHENTICATED: No authorization header"
128. **BEOTEL_CENTREON_AGENT_TOKEN_UNTRUSTED**: 
      * Given the OpenTelemetry server is configured with encryption enabled
129. **BEOTEL_INVALID_CHECK_COMMANDS_AND_ARGUMENTS**: 
      * Given the agent is configured with native checks for services
      * And the OpenTelemetry server module is added
      * And services are configured with incorrect check commands and arguments
      * When the broker, engine, and agent are started
      * Then the resources table should be updated with the correct status
      * And appropriate error messages should be generated for invalid checks
130. **BEOTEL_REVERSE_CENTREON_AGENT_CHECK_HOST**: agent check host with reversed connection and we expect to get it in check result
131. **BEOTEL_REVERSE_CENTREON_AGENT_CHECK_HOST_CRYPTED**: agent check host with encrypted reversed connection and we expect to get it in check result
132. **BEOTEL_REVERSE_CENTREON_AGENT_CHECK_SERVICE**: agent check service with reversed connection and we expect to get it in check result
133. **BEOTEL_SERVE_TELEGRAF_CONFIGURATION_CRYPTED**: we configure engine with a telegraf conf server and we check telegraf conf file
134. **BEOTEL_SERVE_TELEGRAF_CONFIGURATION_NO_CRYPTED**: we configure engine with a telegraf conf server and we check telegraf conf file
135. **BEOTEL_TELEGRAF_CHECK_HOST**: we send nagios telegraf formatted datas and we expect to get it in check result
136. **BEOTEL_TELEGRAF_CHECK_SERVICE**: we send nagios telegraf formatted datas and we expect to get it in check result
137. **BEPBBEE1**: central-module configured with bbdo_version 3.0 but not others. Unable to establish connection.
138. **BEPBBEE2**: bbdo_version 3 not compatible with sql/storage
139. **BEPBBEE3**: bbdo_version 3 generates new bbdo protobuf service status messages.
140. **BEPBBEE4**: bbdo_version 3 generates new bbdo protobuf host status messages.
141. **BEPBBEE5**: bbdo_version 3 generates new bbdo protobuf service messages.
142. **BEPBCVS**: bbdo_version 3 communication of custom variables.
143. **BEPBHostParent**: bbdo_version 3 communication of host parent relations
144. **BEPBINST_CONF**: bbdo_version 3 communication of instance configuration.
145. **BEPBRI1**: bbdo_version 3 use pb_resource new bbdo protobuf ResponsiveInstance message.
146. **BERD1**: Starting/stopping Broker does not create duplicated events.
147. **BERD2**: Starting/stopping Engine does not create duplicated events.
148. **BERDUC1**: Starting/stopping Broker does not create duplicated events in usual cases
149. **BERDUC2**: Starting/stopping Engine does not create duplicated events in usual cases
150. **BERDUC3U1**: Starting/stopping Broker does not create duplicated events in usual cases with unified_sql and BBDO 3.0
151. **BERDUC3U2**: Starting/stopping Engine does not create duplicated events in usual cases with unified_sql and BBDO 3.0
152. **BERDUCA300**: Starting/stopping Engine is stopped ; it should emit a stop event and receive an ack event with events to clean from broker.
153. **BERDUCA301**: Starting/stopping Engine is stopped ; it should emit a stop event and receive an ack event with events to clean from broker with bbdo 3.0.1.
154. **BERDUCU1**: Starting/stopping Broker does not create duplicated events in usual cases with unified_sql
155. **BERDUCU2**: Starting/stopping Engine does not create duplicated events in usual cases with unified_sql
156. **BERES1**: store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
157. **BESERVCHECK**: external command CHECK_SERVICE_RESULT
158. **BESS1**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
159. **BESS2**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
160. **BESS2U**: Start-Stop Broker/Engine - Broker started first - Engine stopped first. Unified_sql is used.
161. **BESS3**: Start-Stop Broker/Engine - Engine started first - Engine stopped first
162. **BESS4**: Start-Stop Broker/Engine - Engine started first - Broker stopped first
163. **BESS5**: Start-Stop Broker/engine - Engine debug level is set to all, it should not hang
164. **BESSBQ1**: A very bad queue file is written for broker. Broker and Engine are then started, Broker must read the file raising an error because of that file and then get data sent by Engine.
165. **BESSCTO**: Scenario: Service commands time out due to missing Perl Connector
      * Given the Engine is configured as usual but without the Perl Connector
      * When the Engine executes its service commands
      * Then the commands take too long and reach the timeout
      * And the Engine starts and stops two times as a result
166. **BESSCTOWC**: Scenario: Service commands time out due to missing Perl Connector
      * Given the Engine is configured as usual with some commands using the Perl Connector
      * When the Engine executes its service commands
      * Then the commands take too long and reach the timeout
      * And the Engine starts and stops two times as a result
167. **BESSG**: Scenario: Broker handles connection and disconnection with Engine
      * Given Broker is configured with only one output that is Graphite
      * When the Engine starts and connects to the Broker
      * Then the Broker must be able to handle the connection
      * When the Engine stops
      * Then the Broker must be able to handle the disconnection
168. **BESS_CRYPTED_GRPC1**: Start-Stop grpc version Broker/Engine - well configured
169. **BESS_CRYPTED_GRPC2**: Start-Stop grpc version Broker/Engine only server crypted
170. **BESS_CRYPTED_GRPC3**: Start-Stop grpc version Broker/Engine only engine crypted
171. **BESS_CRYPTED_REVERSED_GRPC1**: Start-Stop grpc version Broker/Engine - well configured
172. **BESS_CRYPTED_REVERSED_GRPC2**: Start-Stop grpc version Broker/Engine only engine server crypted
173. **BESS_CRYPTED_REVERSED_GRPC3**: Start-Stop grpc version Broker/Engine only engine crypted
174. **BESS_ENGINE_DELETE_HOST**: once engine and cbd started, stop and restart cbd, delete an host and reload engine, cbd mustn't core
175. **BESS_GRPC1**: Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped first
176. **BESS_GRPC2**: Start-Stop grpc version Broker/Engine - Broker started first - Engine stopped first
177. **BESS_GRPC3**: Start-Stop grpc version Broker/Engine - Engine started first - Engine stopped first
178. **BESS_GRPC4**: Start-Stop grpc version Broker/Engine - Engine started first - Broker stopped first
179. **BESS_GRPC5**: Start-Stop grpc version Broker/engine - Engine debug level is set to all, it should not hang
180. **BESS_GRPC_COMPRESS1**: Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped last compression activated
181. **BETAG1**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Broker is started before.
182. **BETAG2**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
183. **BEUTAG1**: Engine is configured with some tags. When broker receives them through unified_sql stream, it stores them in the centreon_storage.tags table. Broker is started before.
184. **BEUTAG10**: some services are configured with tags on two pollers. Then tags are removed from some of them and in centreon_storage, we can observe resources_tags table updated.
185. **BEUTAG11**: some services are configured with tags on two pollers. Then several tags are removed, and we can observe resources_tags table updated.
186. **BEUTAG12**: Engine is configured with some tags. Group tags tag2, tag6 are set to hosts 1 and 2. Category tags tag4 and tag8 are added to hosts 2, 3, 4. The resources and resources_tags tables are well filled. The tag6 and tag8 are removed and resources_tags is also well updated.
187. **BEUTAG2**: Engine is configured with some tags. A new service is added with a tag. Broker should make the relations.
188. **BEUTAG3**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
189. **BEUTAG4**: Engine is configured with some tags. Group tags tag9, tag13 are set to services 1 and 3. Category tags tag3 and tag11 are added to services 1, 3, 5 and 6. The centreon_storage.resources and resources_tags tables are well filled.
190. **BEUTAG5**: Engine is configured with some tags. Group tags tag2, tag6 are set to hosts 1 and 2. Category tags tag4 and tag8 are added to hosts 2, 3, 4. The resources and resources_tags tables are well filled.
191. **BEUTAG6**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.resources_tags table. Engine is started before.
192. **BEUTAG7**: Some services are configured with tags on two pollers. Then tags configuration is modified.
193. **BEUTAG8**: Services have tags provided by templates.
194. **BEUTAG9**: hosts have tags provided by templates.
195. **BEUTAG_REMOVE_HOST_FROM_HOSTGROUP**: remove a host from hostgroup, reload, insert 2 host in the hostgroup must not make sql error
196. **BE_BACKSLASH_CHECK_RESULT**: external command PROCESS_SERVICE_CHECK_RESULT with \:
197. **BE_DEFAULT_NOTIFCATION_INTERVAL_IS_ZERO_SERVICE_RESOURCE**: default notification_interval must be set to NULL in services, hosts and resources tables.
198. **BE_FLAPPING_HOST_RESOURCE**: With BBDO 3, flapping detection must be set in hosts and resources tables.
199. **BE_FLAPPING_SERVICE_RESOURCE**: With BBDO 3, flapping detection must be set in services and resources tables.
200. **BE_NOTIF_OVERFLOW**: bbdo 2.0 notification number =40000. make an overflow => notification_number null in db
201. **BE_TIME_NULL_SERVICE_RESOURCE**: With BBDO 3, notification_interval time must be set to NULL on 0 in services, hosts and resources tables.
202. **BRCS1**: Broker reverse connection stopped
203. **BRCTS1**: Broker reverse connection too slow
204. **BRCTSMN**: Broker connected to map with neb filter
205. **BRCTSMNS**: Broker connected to map with neb and storage filters
206. **BRGC1**: Broker good reverse connection
207. **BRRDCDDID1**: RRD metrics deletion from index ids with rrdcached.
208. **BRRDCDDIDDB1**: RRD metrics deletion from index ids with a query in centreon_storage with rrdcached.
209. **BRRDCDDIDU1**: RRD metrics deletion from index ids with unified sql output with rrdcached.
210. **BRRDCDDM1**: RRD metrics deletion from metric ids with rrdcached.
211. **BRRDCDDMDB1**: RRD metrics deletion from metric ids with a query in centreon_storage and rrdcached.
212. **BRRDCDDMID1**: RRD deletion of non existing metrics and indexes with rrdcached
213. **BRRDCDDMIDU1**: RRD deletion of non existing metrics and indexes with rrdcached
214. **BRRDCDDMU1**: RRD metric deletion on table metric with unified sql output with rrdcached
215. **BRRDCDRB1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with storage/sql sql output and rrdcached.
216. **BRRDCDRBDB1**: RRD metric rebuild with a query in centreon_storage and unified sql with rrdcached
217. **BRRDCDRBU1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with unified_sql output and rrdcached.
218. **BRRDCDRBUDB1**: RRD metric rebuild with a query in centreon_storage and unified sql with rrdcached
219. **BRRDDID1**: RRD metrics deletion from index ids.
220. **BRRDDIDDB1**: RRD metrics deletion from index ids with a query in centreon_storage.
221. **BRRDDIDU1**: RRD metrics deletion from index ids with unified sql output.
222. **BRRDDM1**: RRD metrics deletion from metric ids.
223. **BRRDDMDB1**: RRD metrics deletion from metric ids with a query in centreon_storage.
224. **BRRDDMID1**: RRD deletion of non existing metrics and indexes
225. **BRRDDMIDU1**: RRD deletion of non existing metrics and indexes
226. **BRRDDMU1**: RRD metric deletion on table metric with unified sql output
227. **BRRDRBDB1**: RRD metric rebuild with a query in centreon_storage and unified sql
228. **BRRDRBUDB1**: RRD metric rebuild with a query in centreon_storage and unified sql
229. **BRRDRM1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with storage/sql sql output.
230. **BRRDRMU1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with unified_sql output.
231. **BRRDSTATUS**: We are working with BBDO3. This test checks status are correctly handled independently from their value.
232. **BRRDSTATUSRETENTION**: We are working with BBDO3. This test checks status are not sent twice after Engine reload.
233. **BRRDUPLICATE**: RRD metric rebuild with a query in centreon_storage and unified sql with duplicate rows in database
234. **BRRDWM1**: We are working with BBDO3. This test checks protobuf metrics and status are sent to cbd RRD.
235. **CBD_RELOAD_AND_FILTERS**: We start engine/broker with a classical configuration. All is up and running. Some filters are added to the rrd output and cbd is reloaded. All is still up and running but some events are rejected. Then all is newly set as filter and all events are sent to rrd broker.
236. **CBD_RELOAD_AND_FILTERS_WITH_OPR**: We start engine/broker with an almost classical configuration, just the connection between cbd central and cbd rrd is reversed with one peer retention. All is up and running. Some filters are added to the rrd output and cbd is reloaded. All is still up and running but some events are rejected. Then all is newly set as filter and all events are sent to rrd broker.
237. **DTIM**: New services with several pollers are created. Then downtimes are set on all configured hosts. This action results on 5250 downtimes if we also count impacted services. Then all these downtimes are removed. This test is done with BBDO 3.0.1
238. **EBBM1**: A service status contains metrics that do not fit in a float number.
239. **EBBPS1**: 1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table resources.
240. **EBBPS2**: 1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table services.
241. **EBDP1**: Four new pollers are started and then we remove Poller3.
242. **EBDP2**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
243. **EBDP3**: Three new pollers are started, then they are killed. It is still possible to remove Poller2 if removed from the configuration.
244. **EBDP4**: Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by Broker.
245. **EBDP5**: Four new pollers are started and then we remove Poller3.
246. **EBDP6**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
247. **EBDP7**: Three new pollers are started, then they are killed. It is still possible to remove Poller2 if removed from the configuration.
248. **EBDP8**: Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by broker.
249. **EBDP_GRPC2**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
250. **EBMSSM**: 1000 services are configured with 100 metrics each. The rrd output is removed from the broker configuration. GetSqlManagerStats is called to measure writes into data_bin.
251. **EBMSSMDBD**: 1000 services are configured with 100 metrics each. The rrd output is removed from the broker configuration. While metrics are written in the database, we stop the database and then restart it. Broker must recover its connection to the database and continue to write metrics.
252. **EBMSSMPART**: Scenario: Broker continues writing metrics after partition recreation
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
253. **EBNHG1**: New host group with several pollers and connections to DB
254. **EBNHG4**: New host group with several pollers and connections to DB with broker and rename this hostgroup
255. **EBNHGU1**: New host group with several pollers and connections to DB with broker configured with unified_sql
256. **EBNHGU2**: New host group with several pollers and connections to DB with broker configured with unified_sql
257. **EBNHGU3**: New host group with several pollers and connections to DB with broker configured with unified_sql
258. **EBNHGU4_${test_label}**: New host group with several pollers and connections to DB with broker and rename this hostgroup
259. **EBNSG1**: New service group with several pollers and connections to DB
260. **EBNSGU1**: New service group with several pollers and connections to DB with broker configured with unified_sql
261. **EBNSGU2**: New service group with several pollers and connections to DB with broker configured with unified_sql
262. **EBNSGU3_${test_label}**: New service group with several pollers and connections to DB with broker and rename this servicegroup
263. **EBNSVC1**: New services with several pollers
264. **EBPN0**: Verify if child is in queue when parent is down.
265. **EBPN1**: verify relation parent child when delete parent.
266. **EBPN2**: verify relation parent child when delete child.
267. **EBPS2**: 1000 services are configured with 20 metrics each. The rrd output is removed from the broker configuration to avoid to write too many rrd files. While metrics are written in bulk, the database is stopped. This must not crash broker.
268. **EBSAU2**: New hosts with action_url with more than 2000 characters
269. **EBSIC0**: Verify that the update icon_id for host/service in cfg is well propagated to the database
270. **EBSN3**: New hosts with notes with more than 500 characters
271. **EBSN4**: New hosts with No Alias / Alias and have A Template
272. **EBSNU1**: New hosts with notes_url with more than 2000 characters
273. **ENRSCHE1**: Verify that next check of a rescheduled host is made at last_check + interval_check
274. **FILTER_ON_LUA_EVENT**: stream connector with a bad configured filter generate a log error message
275. **GRPC_CLOUD_FAILURE**: simulate a broker failure in cloud environment, we provide a muted grpc server and there must remain only one grpc connection. Then we start broker and connection must be ok
276. **GRPC_RECONNECT**: We restart broker and engine must reconnect to it and send data
277. **LCDNU**: the lua cache updates correctly service cache.
278. **LCDNUH**: the lua cache updates correctly host cache
279. **LOGV2DB1**: log-v2 disabled old log enabled check broker sink
280. **LOGV2DB2**: log-v2 disabled old log disabled check broker sink
281. **LOGV2DF1**: log-v2 disabled old log enabled check logfile sink
282. **LOGV2DF2**: log-v2 disabled old log disabled check logfile sink
283. **LOGV2EB1**: Checking broker sink when log-v2 is enabled and legacy logs are disabled.
284. **LOGV2EB2**: log-v2 enabled old log enabled check broker sink
285. **LOGV2EBU1**: Checking broker sink when log-v2 is enabled and legacy logs are disabled with bbdo3.
286. **LOGV2EBU2**: Check Broker sink with log-v2 enabled and legacy log enabled with BBDO3.
287. **LOGV2EF1**: log-v2 enabled    old log disabled check logfile sink
288. **LOGV2EF2**: log-v2 enabled old log enabled check logfile sink
289. **LOGV2FE2**: log-v2 enabled old log enabled check logfile sink
290. **LUA_CACHE_SAVE_BBDO3**: 
      * Given a engine broker configured in bbdo2, we check that services and hosts are stored in bbdo3 format in cache
      * To do that we compare host and service event with lua cache
291. **MOVE_HOST_OF_HOSTGROUP_TO_ANOTHER_POLLER**: Scenario: Moving hosts between pollers without losing hostgroup tag
      * Given two pollers each with two hosts
      * And all hosts belong to the same hostgroup
      * When I move two hosts from one poller to the other
      * Then the hostgroup tag of the moved hosts is not erased
292. **NON_TLS_CONNECTION_WARNING**: 
      * Given an agent starts a non-TLS connection,
      * we expect to get a warning message.
293. **NON_TLS_CONNECTION_WARNING_ENCRYPTED**: 
      * Given agent with encrypted connection, we expect no warning message.
294. **NON_TLS_CONNECTION_WARNING_FULL**: 
      * Given an agent starts a non-TLS connection,
      * we expect to get a warning message.
      * After 1 hour, we expect to get a warning message about the connection time expired
      * and the connection killed.
295. **NON_TLS_CONNECTION_WARNING_FULL_REVERSED**: 
      * Given an agent starts a non-TLS connection reverse,
      * we expect to get a warning message.
      * After 1 hour, we expect to get a warning message about the connection time expired
      * and the connection killed.
296. **NON_TLS_CONNECTION_WARNING_REVERSED**: 
      * Given an agent starts a non-TLS connection reversed,
      * we expect to get a warning message.
297. **NON_TLS_CONNECTION_WARNING_REVERSED_ENCRYPTED**: 
      * Given agent with encrypted reversed connection, we expect no warning message.
298. **NO_FILTER_NO_ERROR**: no filter configured => no filter error.
299. **RENAME_PARENT**: 
      * Given an host with a parent host. We rename the parent host and check if the child host is still linked to the parent.
      * Engine mustn't crash and log an error on reload.
300. **RLCode**: Test if reloading LUA code in a stream connector applies the changes
301. **RRD1**: RRD metric rebuild asked with gRPC API. Three non existing indexes IDs are selected then an error message is sent. This is done with unified_sql output.
302. **SDER**: The check attempts and the max check attempts of (host_1,service_1) are changed to 280 thanks to the retention.dat file. Then Engine and Broker are started and Broker should write these values in the services and resources tables. We only test the services table because we need a resources table that allows bigger numbers for these two attributes. But we see that Broker doesn't crash anymore.
303. **SEVERAL_FILTERS_ON_LUA_EVENT**: Two stream connectors with different filters are configured.
304. **STORAGE_ON_LUA**: The category 'storage' is applied on the stream connector. Only events of this category should be sent to this stream.
305. **STUPID_FILTER**: Unified SQL is configured with only the bbdo category as filter. An error is raised by broker and broker should run correctly.
306. **Service_increased_huge_check_interval**: New services with high check interval at creation time.
307. **Services_and_bulks_${id}**: One service is configured with one metric with a name of 150 to 1021 characters.
308. **Start_Stop_Broker_Engine_${id}**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
309. **Start_Stop_Engine_Broker_${id}**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
310. **UNIFIED_SQL_FILTER**: With bbdo version 3.0.1, we watch events written or rejected in unified_sql
311. **VICT_ONE_CHECK_METRIC**: victoria metrics metric output
312. **VICT_ONE_CHECK_METRIC_AFTER_FAILURE**: victoria metrics metric output after victoria shutdown
313. **VICT_ONE_CHECK_STATUS**: victoria metrics status output
314. **Whitelist_Directory_Rights**: log if /etc/centreon-engine-whitelist has not mandatory rights or owner
315. **Whitelist_Empty_Directory**: log if /etc/centreon-engine-whitelist is empty
316. **Whitelist_Host**: test allowed and forbidden commands for hosts
317. **Whitelist_No_Whitelist_Directory**: log if /etc/centreon-engine-whitelist doesn't exist
318. **Whitelist_Perl_Connector**: test allowed and forbidden commands for services
319. **Whitelist_Service**: test allowed and forbidden commands for services
320. **Whitelist_Service_EH**: test allowed and forbidden event handler for services
321. **metric_mapping**: Check if metric name exists using a stream connector
322. **not1**: This test case configures a single service and verifies that a notification is sent when the service is in a non-OK HARD state.
323. **not10**: This test case involves scheduling downtime on a down host that already had a critical notification. When The Host return to UP state we should receive a recovery notification.
324. **not11**: This test case involves configuring one service and checking that three alerts are sent for it.
325. **not12**: 
      * Given the engine is configured with two hosts
      * And each host has one service: (host_1;service_1) and (host_2;service_2)
      * And a service group is configured with both services
      * And three contact groups are configured:
      * | ID | Users       |
      * | 1  | U1          |
      * | 2  | U2, U3      |
      * | 3  | U4          |
      * And an escalation "esc1" is configured to notify contact group 2 on notification number 2 only
      * And an escalation "esc2" is configured to notify contact group 3 from notification number 3 and forever
      * And services are configured to always notify contact group 1
      * Scenario: Escalation notifications for service alerts
      * When services go to state CRITICAL HARD at step 1
      * Then user U1 is notified
      * When services are confirmed in CRITICAL HARD at step 2
      * Then users U2 and U3 are notified as members of contact group 2
      * When services are confirmed in CRITICAL HARD at step 3
      * Then user U4 is notified as member of contact group 3
      * And we wait for 1 minute
      * When services are confirmed in CRITICAL HARD at step 4
      * Then user U4 is notified again as member of contact group 3
      * And we verify that since the first escalation, user U1 is no longer notified
326. **not13**: notification for a dependencies host
327. **not14**: notification for a Service dependency
328. **not15**: several notification commands for the same user.
329. **not16**: notification for dependencies services group
330. **not17**: notification for a dependensies host group
331. **not18**: notification delay where first notification delay equal retry check
332. **not19**: notification delay where first notification delay greater than retry check 
333. **not1_WL_KO**: This test case configures a single service. When it is in non-OK HARD state a notification should be sent but it is not allowed by the whitelist
334. **not1_WL_OK**: This test case configures a single service. When it is in non-OK HARD state a notification is sent because it is allowed by the whitelist
335. **not2**: This test case configures a single service and verifies that a recovery notification is sent
336. **not20**: notification delay where first notification delay samller than retry check
337. **not3**: This test case configures a single service and verifies the notification system's behavior during and after downtime
338. **not4**: This test case configures a single service and verifies the notification system's behavior during and after acknowledgement
339. **not5**: This test case configures two services with two different users being notified when the services transition to a critical state.
340. **not6**: This test case validate the behavior when the notification time period is set to null.
341. **not7**: This test case simulates a host alert scenario.
342. **not8**: This test validates the critical host notification.
343. **not9**: This test case configures a single host and verifies that a recovery notification is sent after the host recovers from a non-OK state.
344. **not_in_timeperiod_with_send_recovery_notifications_anyways**: This test case configures a single service and verifies that a notification is sent when the service is in a non-OK state and OK is sent outside timeperiod when _send_recovery_notifications_anyways is set
345. **not_in_timeperiod_without_send_recovery_notifications_anyways**: This test case configures a single service and verifies that a notification is sent when the service is in a non-OK state and OK is not sent outside timeperiod when _send_recovery_notifications_anyways is not set

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
1. **EFHC1**: Engine is configured with hosts and we force checks on one 5 times on bbdo2
2. **EFHC2**: Engine is configured with hosts and we force check on one 5 times on bbdo2
3. **EFHCU1**: Engine is configured with hosts and we force checks on one 5 times on bbdo3. Bbdo3 has no impact on this behavior. resources table is cleared before starting broker.
4. **EFHCU2**: Engine is configured with hosts and we force checks on one 5 times on bbdo3. Bbdo3 has no impact on this behavior.
5. **EMACROS**: macros ADMINEMAIL and ADMINPAGER are replaced in check outputs
6. **EMACROS_NOTIF**: macros ADMINEMAIL and ADMINPAGER are replaced in notification commands
7. **EMACROS_SEMICOLON**: Macros with a semicolon are used even if they contain a semicolon.
8. **EPC1**: Check with perl connector
9. **ERL**: Engine is started and writes logs in centengine.log. Then we remove the log file. The file disappears but Engine is still writing into it. Engine is reloaded and the centengine.log should appear again.
10. **ESS1**: Start-Stop (0s between start/stop) 5 times one instance of engine and no coredump
11. **ESS2**: Start-Stop (300ms between start/stop) 5 times one instance of engine and no coredump
12. **ESS3**: Start-Stop (0s between start/stop) 5 times three instances of engine and no coredump
13. **ESS4**: Start-Stop (300ms between start/stop) 5 times three instances of engine and no coredump
14. **ESSCTO**: Scenario: Engine services timeout due to missing Perl connector
      * Given the Engine is configured as usual without the Perl connector
      * When the Engine executes its service commands
      * Then the commands take too long and reach the timeout
      * And the Engine starts and stops two times as a result
15. **ESSCTOWC**: Scenario: Engine services timeout due to missing Perl connector
      * Given the Engine is configured as usual with some command using the Perl connector
      * When the Engine executes its service commands
      * Then the commands take too long and reach the timeout
      * And the Engine starts and stops two times as a result
16. **EXT_CONF1**: Engine configuration is overided by json conf
17. **EXT_CONF2**: Engine configuration is overided by json conf after reload
18. **E_FD_LIMIT**: Engine here is started with a low file descriptor limit. The engine should not crash and limit should be set.
19. **E_HOST_DOWN_DISABLE_SERVICE_CHECKS**: host_down_disable_service_checks is set to 1, host down switch all services to UNKNOWN
20. **E_HOST_UNREACHABLE_DISABLE_SERVICE_CHECKS**: host_down_disable_service_checks is set to 1, host unreachable switch all services to UNKNOWN
21. **VERIFIY_CONF**: Verify deprecated engine configuration options are logged as warnings Given the engine and broker are configured with module 1 And the engine configuration is set with deprecated options When the engine is started Then a warning message for 'auto_reschedule_checks' should be logged And a warning message for 'auto_rescheduling_interval' should be logged And a warning message for 'auto_rescheduling_window' should be logged And the engine should be stopped

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


526 tests currently implemented.
