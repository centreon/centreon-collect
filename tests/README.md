# Centreon Tests

This sub-project contains functional tests for Centreon Broker, Engine and Connectors.
It is based on the [Robot Framework](https://robotframework.org/) with Python functions
we can find in the resources directory. The Python code is formatted using autopep8 and
robot files are formatted using `robottidy --overwrite tests`.

## Getting Started

To get this project, you have to clone centreon-collect.

These tests are executed from the `centreon-tests/robot` folder and uses the [Robot Framework](https://robotframework.org/).

From a Centreon host, you need to install Robot Framework

On CentOS 7, the following commands should work to initialize your robot tests:

```
pip3 install -U robotframework robotframework-databaselibrary robotframework-examples pymysql robotframework-requests

yum install "Development Tools" python3-devel -y

pip3 install grpcio==1.33.2 grpcio_tools==1.33.2

#you need also to provide opentelemetry proto files at the project root with this command
git clone https://github.com/open-telemetry/opentelemetry-proto.git opentelemetry-proto
          
#Then you must have something like that:
#root directory/bbdo
#              /broker
#              /engine
#              /opentelemetry-proto
#              /tests


./init-proto.sh
./init-sql.sh
```

On other rpm based distributions, you can try the following commands to initialize your robot tests:

```
pip3 install -U robotframework robotframework-databaselibrary robotframework-httpctrl robotframework-examples pymysql robotframework-requests

yum install python3-devel -y

pip3 install grpcio grpcio_tools

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
1. [x] **BABEST_SERVICE_CRITICAL**: With bbdo version 3.0.1, a BA of type 'best' with 2 serv, ba is critical only if the 2 services are critical
2. [x] **BABOO**: With bbdo version 3.0.1, a BA of type 'worst' with 2 child services and another BA of type impact with a boolean rule returning if one of its two services are critical are created. These two BA are built from the same services and should have a similar behavior
3. [x] **BABOOAND**: With bbdo version 3.0.1, a BA of type impact with a boolean rule returning if both of its two services are ok is created. When one condition is false, the and operator returns false as a result even if the other child is unknown.
4. [x] **BABOOCOMPL**: With bbdo version 3.0.1, a BA of type impact with a complex boolean rule is configured. We check its correct behaviour following service updates.
5. [x] **BABOOCOMPL_RELOAD**: With bbdo version 3.0.1, a BA of type impact with a complex boolean rule is configured. We check its correct behaviour following service updates.
6. [x] **BABOOCOMPL_RESTART**: With bbdo version 3.0.1, a BA of type impact with a complex boolean rule is configured. We check its correct behaviour following service updates.
7. [x] **BABOOOR**: With bbdo version 3.0.1, a BA of type 'worst' with 2 child services and another BA of type impact with a boolean rule returning if one of its two services are critical are created. These two BA are built from the same services and should have a similar behavior
8. [x] **BABOOORREL**: With bbdo version 3.0.1, a BA of type impact with a boolean rule returning if one of its two services is ok is created. One of the two underlying services must change of state to change the ba state. For this purpose, we change the service state and reload cbd. So the rule is something like "False OR True" which is equal to True. And to pass from True to False, we change the second service.
9. [x] **BAWORST**: With bbdo version 3.0.1, a BA of type 'worst' with two services is configured. We also check stats output
10. [x] **BAWORST2**: a worst ba with a boolean kpi and a ba kpi
11. [x] **BA_BOOL_KPI**: With bbdo version 3.0.1, a BA of type 'worst' with 1 boolean kpi
12. [x] **BA_CHANGED**: A BA of type worst is configured with one service kpi. Then it is modified so that the service kpi is replaced by a boolean rule kpi. When cbd is reloaded, the BA is well updated.
13. [x] **BA_DISABLED**: create a disabled BA with timeperiods and reporting filter don't create error message
14. [x] **BA_IMPACT_2KPI_SERVICES**: With bbdo version 3.0.1, a BA of type 'impact' with 2 serv, ba is critical only if the 2 services are critical
15. [x] **BA_IMPACT_IMPACT**: A BA of type impact is defined with two BAs of type impact as children. The first child has an impact of 90 and the second one of 10. When they are impacting both, the parent should be critical. When they are not impacting, the parent should be ok.
16. [x] **BA_RATIO_NUMBER_BA_4_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
17. [x] **BA_RATIO_NUMBER_BA_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 2 services and one ba with 1 service
18. [x] **BA_RATIO_PERCENT_BA_4_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
19. [x] **BA_RATIO_PERCENT_BA_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio percent' with 2 serv an 1 ba with one service
20. [x] **BEBAMIDT1**: A BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. Then we set a downtime on this last one. An inherited downtime is set to the BA. The downtime is removed from the service, the inherited downtime is then deleted.
21. [x] **BEBAMIDT2**: A BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. Then we set a downtime on this last one. An inherited downtime is set to the BA. Engine is restarted. Broker is restarted. The two downtimes are still there with no duplicates. The downtime is removed from the service, the inherited downtime is then deleted.
22. [x] **BEBAMIDTU1**: With bbdo version 3.0.1, a BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. Then we set a downtime on this last one. An inherited downtime is set to the BA. The downtime is removed from the service, the inherited downtime is then deleted.
23. [x] **BEBAMIDTU2**: With bbdo version 3.0.1, a BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. Then we set a downtime on this last one. An inherited downtime is set to the BA. Engine is restarted. Broker is restarted. The two downtimes are still there with no duplicates. The downtime is removed from the service, the inherited downtime is then deleted.
24. [x] **BEBAMIGNDT1**: A BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. A first downtime is cancelled, the BA is still OK, but when the second downtime is cancelled, the BA should be CRITICAL.
25. [x] **BEBAMIGNDT2**: A BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. The first downtime reaches its end, the BA is still OK, but when the second downtime reaches its end, the BA should be CRITICAL.
26. [x] **BEBAMIGNDTU1**: With bbdo version 3.0.1, a BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. A first downtime is cancelled, the BA is still OK, but when the second downtime is cancelled, the BA should be CRITICAL.
27. [x] **BEBAMIGNDTU2**: With bbdo version 3.0.1, a BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. The first downtime reaches its end, the BA is still OK, but when the second downtime reaches its end, the BA should be CRITICAL.
28. [x] **BEPB_BA_DURATION_EVENT**: use of pb_ba_duration_event message.
29. [x] **BEPB_DIMENSION_BA_BV_RELATION_EVENT**: bbdo_version 3 use pb_dimension_ba_bv_relation_event message.
30. [x] **BEPB_DIMENSION_BA_EVENT**: bbdo_version 3 use pb_dimension_ba_event message.
31. [x] **BEPB_DIMENSION_BA_TIMEPERIOD_RELATION**: use of pb_dimension_ba_timeperiod_relation message.
32. [x] **BEPB_DIMENSION_BV_EVENT**: bbdo_version 3 use pb_dimension_bv_event message.
33. [x] **BEPB_DIMENSION_KPI_EVENT**: bbdo_version 3 use pb_dimension_kpi_event message.
34. [x] **BEPB_DIMENSION_TIMEPERIOD**: use of pb_dimension_timeperiod message.
35. [x] **BEPB_DIMENSION_TRUNCATE_TABLE**: use of pb_dimension_timeperiod message.
36. [x] **BEPB_KPI_STATUS**: bbdo_version 3 use kpi_status message.

### Broker
1. [x] **BCL1**: Starting broker with option '-s foobar' should return an error
2. [x] **BCL2**: Starting broker with option '-s5' should work
3. [x] **BCL3**: Starting broker with options '-D' should work and activate diagnose mode
4. [x] **BCL4**: Starting broker with options '-s2' and '-D' should work.
5. [x] **BDB1**: Access denied when database name exists but is not the good one for sql output
6. [x] **BDB10**: connection should be established when user password is good for sql/perfdata
7. [x] **BDB2**: Access denied when database name exists but is not the good one for storage output
8. [x] **BDB3**: Access denied when database name does not exist for sql output
9. [x] **BDB4**: Access denied when database name does not exist for storage and sql outputs
10. [x] **BDB5**: cbd does not crash if the storage/sql db_host is wrong
11. [x] **BDB6**: cbd does not crash if the sql db_host is wrong
12. [x] **BDB7**: access denied when database user password is wrong for perfdata/sql
13. [x] **BDB8**: access denied when database user password is wrong for perfdata/sql
14. [x] **BDB9**: access denied when database user password is wrong for sql
15. [x] **BDBM1**: start broker/engine and then start MariaDB => connection is established
16. [x] **BDBMU1**: start broker/engine with unified sql and then start MariaDB => connection is established
17. [x] **BDBU1**: Access denied when database name exists but is not the good one for unified sql output
18. [x] **BDBU10**: Connection should be established when user password is good for unified sql
19. [x] **BDBU3**: Access denied when database name does not exist for unified sql output
20. [x] **BDBU5**: cbd does not crash if the unified sql db_host is wrong
21. [x] **BDBU7**: Access denied when database user password is wrong for unified sql
22. [x] **BEDB2**: start broker/engine and then start MariaDB => connection is established
23. [x] **BEDB3**: start broker/engine, then stop MariaDB and then start it again. The gRPC API should give informations about SQL connections.
24. [x] **BEDB4**: start broker/engine, then stop MariaDB and then start it again. The gRPC API should give informations about SQL connections.
25. [x] **BFC1**: Start broker with invalid filters but one filter ok
26. [x] **BFC2**: Start broker with only invalid filters on an output
27. [x] **BGRPCSS1**: Start-Stop two instances of broker configured with grpc stream and no coredump
28. [x] **BGRPCSS2**: Start/Stop 10 times broker configured with grpc stream with 300ms interval and no coredump
29. [x] **BGRPCSS3**: Start-Stop one instance of broker configured with grpc stream and no coredump
30. [x] **BGRPCSS4**: Start/Stop 10 times broker configured with grpc stream with 1sec interval and no coredump
31. [x] **BGRPCSS5**: Start-Stop with reversed connection on grpc acceptor with only one instance and no deadlock
32. [x] **BGRPCSSU1**: Start-Stop with unified_sql two instances of broker with grpc stream and no coredump
33. [x] **BGRPCSSU2**: Start/Stop with unified_sql 10 times broker configured with grpc stream with 300ms interval and no coredump
34. [x] **BGRPCSSU3**: Start-Stop with unified_sql one instance of broker configured with grpc and no coredump
35. [x] **BGRPCSSU4**: Start/Stop with unified_sql 10 times broker configured with grpc stream with 1sec interval and no coredump
36. [x] **BGRPCSSU5**: Start-Stop with unified_sql with reversed connection on grpc acceptor with only one instance and no deadlock
37. [x] **BLDIS1**: Start broker with core logs 'disabled'
38. [x] **BLEC1**: Change live the core level log from trace to debug
39. [x] **BLEC2**: Change live the core level log from trace to foo raises an error
40. [x] **BLEC3**: Change live the foo level log to trace raises an error
41. [x] **BSCSS1**: Start-Stop two instances of broker and no coredump
42. [x] **BSCSS2**: Start/Stop 10 times broker with 300ms interval and no coredump
43. [x] **BSCSS3**: Start-Stop one instance of broker and no coredump
44. [x] **BSCSS4**: Start/Stop 10 times broker with 1sec interval and no coredump
45. [x] **BSCSSC1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is enabled on client side.
46. [x] **BSCSSC2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is disabled on client side.
47. [x] **BSCSSCG1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on client side.
48. [x] **BSCSSCGRR1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on output side. Reversed connection with retention and grpc transport protocol.
49. [x] **BSCSSCGRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on output side. Reversed connection with retention and grpc transport protocol.
50. [x] **BSCSSCRR1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is enabled on client side. Connection reversed with retention.
51. [x] **BSCSSCRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is disabled on client side. Connection reversed with retention.
52. [x] **BSCSSG1**: Start-Stop two instances of broker and no coredump
53. [x] **BSCSSG2**: Start/Stop 10 times broker with 300ms interval and no coredump
54. [x] **BSCSSG3**: Start-Stop one instance of broker and no coredump
55. [x] **BSCSSG4**: Start/Stop 10 times broker with 1sec interval and no coredump
56. [x] **BSCSSGA1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. An authorization token is added on the server. Error messages are raised.
57. [x] **BSCSSGA2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. An authorization token is added on the server and also on the client. All looks ok.
58. [x] **BSCSSGRR1**: Start-Stop two instances of broker and no coredump, reversed and retention, with transport protocol grpc, start-stop 5 times.
59. [x] **BSCSSK1**: Start-Stop two instances of broker, server configured with grpc and client with tcp. No connectrion established and error raised on client side.
60. [x] **BSCSSK2**: Start-Stop two instances of broker, server configured with tcp and client with grpc. No connection established and error raised on client side.
61. [x] **BSCSSP1**: Start-Stop two instances of broker and no coredump. The server contains a listen address
62. [x] **BSCSSPRR1**: Start-Stop two instances of broker and no coredump. The server contains a listen address, reversed and retention. centreon-broker-master-rrd is then a failover.
63. [x] **BSCSSR1**: Start-Stop two instances of broker and no coredump. Connection with bbdo_server/bbdo_client and reversed.
64. [x] **BSCSSRR1**: Start-Stop two instances of broker and no coredump. Connection with bbdo_server/bbdo_client, reversed and retention. centreon-broker-master-rrd is then a failover.
65. [x] **BSCSSRR2**: Start/Stop 10 times broker with 300ms interval and no coredump, reversed and retention. centreon-broker-master-rrd is then a failover.
66. [x] **BSCSST1**: Start-Stop two instances of broker and no coredump. Encryption is enabled on client side.
67. [x] **BSCSST2**: Start-Stop two instances of broker and no coredump. Encryption is enabled on client side.
68. [x] **BSCSSTG1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. This is not sufficient, then an error is raised.
69. [x] **BSCSSTG2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. It works with good certificates and keys.
70. [x] **BSCSSTG3**: Start-Stop two instances of broker. The connection cannot be established if the server private key is missing and an error message explains this issue.
71. [x] **BSCSSTGRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. It works with good certificates and keys. Reversed grpc connection with retention.
72. [x] **BSCSSTRR1**: Start-Stop two instances of broker and no coredump. Encryption is enabled. transport protocol is tcp, reversed and retention.
73. [x] **BSCSSTRR2**: Start-Stop two instances of broker and no coredump. Encryption is enabled.
74. [x] **BSS1**: Start-Stop two instances of broker and no coredump
75. [x] **BSS2**: Start/Stop 10 times broker with 300ms interval and no coredump
76. [x] **BSS3**: Start-Stop one instance of broker and no coredump
77. [x] **BSS4**: Start/Stop 10 times broker with 1sec interval and no coredump
78. [x] **BSS5**: Start-Stop with reversed connection on TCP acceptor with only one instance and no deadlock
79. [x] **BSSU1**: Start-Stop with unified_sql two instances of broker and no coredump
80. [x] **BSSU2**: Start/Stop with unified_sql 10 times broker with 300ms interval and no coredump
81. [x] **BSSU3**: Start-Stop with unified_sql one instance of broker and no coredump
82. [x] **BSSU4**: Start/Stop with unified_sql 10 times broker with 1sec interval and no coredump
83. [x] **BSSU5**: Start-Stop with unified_sql with reversed connection on TCP acceptor with only one instance and no deadlock
84. [x] **START_STOP_CBD**: restart cbd with unified_sql services state must not be null after restart

### Broker/database
1. [x] **DEDICATED_DB_CONNECTION_${nb_conn}_${store_in_data_bin}**: count database connection
2. [x] **NetworkDBFail6**: network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s
3. [x] **NetworkDBFail7**: network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s
4. [x] **NetworkDBFailU6**: network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s (with unified_sql)
5. [x] **NetworkDBFailU7**: network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s (with unified_sql)
6. [x] **NetworkDBFailU8**: network failure test between broker and database: we wait for the connection to be established and then we shutdown the connection until _check_queues failure
7. [x] **NetworkDbFail1**: network failure test between broker and database (shutting down connection for 100ms)
8. [x] **NetworkDbFail2**: network failure test between broker and database (shutting down connection for 1s)
9. [x] **NetworkDbFail3**: network failure test between broker and database (shutting down connection for 10s)
10. [x] **NetworkDbFail4**: network failure test between broker and database (shutting down connection for 30s)
11. [x] **NetworkDbFail5**: network failure test between broker and database (shutting down connection for 60s)

### Broker/engine
1. [x] **ANO_CFG_SENSITIVITY_SAVED**: cfg sensitivity saved in retention
2. [x] **ANO_DT1**: downtime on dependent service is inherited by ano
3. [x] **ANO_DT2**: delete downtime on dependent service delete one on ano serv
4. [x] **ANO_DT3**: delete downtime on anomaly don t delete dependent service one
5. [x] **ANO_DT4**: set dt on anomaly and on dependent service, delete last one don t delete first one
6. [x] **ANO_EXTCMD_SENSITIVITY_SAVED**: extcmd sensitivity saved in retention
7. [x] **ANO_JSON_SENSITIVITY_NOT_SAVED**: json sensitivity not saved in retention
8. [x] **ANO_NOFILE**: an anomaly detection without threshold file must be in unknown state
9. [x] **ANO_NOFILE_VERIF_CONFIG_NO_ERROR**: an anomaly detection without threshold file doesn't display error on config check
10. [x] **ANO_OUT_LOWER_THAN_LIMIT**: an anomaly detection with a perfdata lower than lower limit make a critical state
11. [x] **ANO_OUT_UPPER_THAN_LIMIT**: an anomaly detection with a perfdata upper than upper limit make a critical state
12. [x] **ANO_TOO_OLD_FILE**: An anomaly detection with an oldest threshold file must be in unknown state
13. [x] **AOUTLU1**: an anomaly detection with a perfdata upper than upper limit make a critical state with bbdo 3
14. [x] **BAM_STREAM_FILTER**: With bbdo version 3.0.1, a BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. we watch its events
15. [x] **BEACK1**: Engine has a critical service. An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to OK. And the acknowledgement in database is deleted from engine but still open on the database.
16. [x] **BEACK2**: Configuration is made with BBDO3. Engine has a critical service. An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to OK. And the acknowledgement in database is deleted.
17. [x] **BEACK3**: Engine has a critical service. An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The acknowledgement is removed and the comment in the comments table has its deletion_time column updated.
18. [x] **BEACK4**: Configuration is made with BBDO3. Engine has a critical service. An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The acknowledgement is removed and the comment in the comments table has its deletion_time column updated.
19. [x] **BEACK5**: Engine has a critical service. An external command is sent to acknowledge it ; the acknowledgement is sticky. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to WARNING. And the acknowledgement in database is still there.
20. [x] **BEACK6**: Configuration is made with BBDO3. Engine has a critical service. An external command is sent to acknowledge it ; the acknowledgement is sticky. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to WARNING. And the acknowledgement in database is still there.
21. [x] **BEACK8**: Engine has a critical service. It is configured with BBDO 3. An external command is sent to acknowledge it ; the acknowledgement is normal. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to WARNING. And the acknowledgement in database is removed (not sticky).
22. [x] **BEATOI11**: external command SEND_CUSTOM_HOST_NOTIFICATION with option_number=1 should work
23. [x] **BEATOI12**: external command SEND_CUSTOM_HOST_NOTIFICATION with option_number>7 should fail
24. [x] **BEATOI13**: external command Schedule Service Downtime with duration<0 should fail
25. [x] **BEATOI21**: external command ADD_HOST_COMMENT and DEL_HOST_COMMENT should work
26. [x] **BEATOI22**: external command DEL_HOST_COMMENT with comment_id<0 should fail
27. [x] **BEATOI23**: external command ADD_SVC_COMMENT with persistent=0 should work
28. [x] **BECC1**: Broker/Engine communication with compression between central and poller
29. [x] **BECT1**: Broker/Engine communication with anonymous TLS between central and poller
30. [x] **BECT2**: Broker/Engine communication with TLS between central and poller with key/cert
31. [x] **BECT3**: Broker/Engine communication with anonymous TLS and ca certificate
32. [x] **BECT4**: Broker/Engine communication with TLS between central and poller with key/cert and hostname forced
33. [x] **BECT_GRPC1**: Broker/Engine communication with GRPC and with anonymous TLS between central and poller
34. [x] **BECT_GRPC2**: Broker/Engine communication with TLS between central and poller with key/cert
35. [x] **BECT_GRPC3**: Broker/Engine communication with anonymous TLS and ca certificate
36. [x] **BECT_GRPC4**: Broker/Engine communication with TLS between central and poller with key/cert and hostname forced
37. [x] **BECUSTOMHOSTVAR**: external command CHANGE_CUSTOM_HOST_VAR on SNMPVERSION
38. [x] **BECUSTOMSVCVAR**: external command CHANGE_CUSTOM_SVC_VAR on CRITICAL
39. [x] **BEDTHOSTFIXED**: A downtime is set on a host, the total number of downtimes is really 21 (1 for the host and 20 for its 20 services) then we delete this downtime and the number is 0.
40. [x] **BEDTMASS1**: New services with several pollers are created. Then downtimes are set on all configured hosts. This action results on 1050 downtimes if we also count impacted services. Then all these downtimes are removed. This test is done with BBDO 3.0.0
41. [x] **BEDTMASS2**: New services with several pollers are created. Then downtimes are set on all configured hosts. This action results on 1050 downtimes if we also count impacted services. Then all these downtimes are removed. This test is done with BBDO 2.0
42. [x] **BEDTSVCFIXED**: A downtime is set on a service, the total number of downtimes is really 1 then we delete this downtime and the number of downtime is 0.
43. [x] **BEDTSVCREN1**: A downtime is set on a service then the service is renamed. The downtime is still active on the renamed service. The downtime is removed from the renamed service and it is well removed.
44. [x] **BEEXTCMD1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0
45. [x] **BEEXTCMD10**: external command CHANGE_MAX_SVC_CHECK_ATTEMPTS on bbdo2.0
46. [x] **BEEXTCMD11**: external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo3.0
47. [x] **BEEXTCMD12**: external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo2.0
48. [x] **BEEXTCMD13**: external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo3.0
49. [x] **BEEXTCMD14**: external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo2.0
50. [x] **BEEXTCMD15**: external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo3.0
51. [x] **BEEXTCMD16**: external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo2.0
52. [x] **BEEXTCMD17**: external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo3.0
53. [x] **BEEXTCMD18**: external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo2.0
54. [x] **BEEXTCMD19**: external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo3.0
55. [x] **BEEXTCMD2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0
56. [x] **BEEXTCMD20**: external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo2.0
57. [x] **BEEXTCMD21**: external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS and ENABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo3.0
58. [x] **BEEXTCMD22**: external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS and ENABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo2.0
59. [x] **BEEXTCMD23**: external command DISABLE_HOST_CHECK and ENABLE_HOST_CHECK on bbdo3.0
60. [x] **BEEXTCMD24**: external command DISABLE_HOST_CHECK and ENABLE_HOST_CHECK on bbdo2.0
61. [x] **BEEXTCMD25**: external command DISABLE_HOST_EVENT_HANDLER and ENABLE_HOST_EVENT_HANDLER on bbdo3.0
62. [x] **BEEXTCMD26**: external command DISABLE_HOST_EVENT_HANDLER and ENABLE_HOST_EVENT_HANDLER on bbdo2.0
63. [x] **BEEXTCMD27**: external command DISABLE_HOST_FLAP_DETECTION and ENABLE_HOST_FLAP_DETECTION on bbdo3.0
64. [x] **BEEXTCMD28**: external command DISABLE_HOST_FLAP_DETECTION and ENABLE_HOST_FLAP_DETECTION on bbdo2.0
65. [x] **BEEXTCMD29**: external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo3.0
66. [x] **BEEXTCMD3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0
67. [x] **BEEXTCMD30**: external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo2.0
68. [x] **BEEXTCMD31**: external command DISABLE_HOST_SVC_CHECKS and ENABLE_HOST_SVC_CHECKS on bbdo3.0
69. [x] **BEEXTCMD32**: external command DISABLE_HOST_SVC_CHECKS and ENABLE_HOST_SVC_CHECKS on bbdo2.0
70. [x] **BEEXTCMD33**: external command DISABLE_HOST_SVC_NOTIFICATIONS and ENABLE_HOST_SVC_NOTIFICATIONS on bbdo3.0
71. [x] **BEEXTCMD34**: external command DISABLE_HOST_SVC_NOTIFICATIONS and ENABLE_HOST_SVC_NOTIFICATIONS on bbdo2.0
72. [x] **BEEXTCMD35**: external command DISABLE_PASSIVE_HOST_CHECKS and ENABLE_PASSIVE_HOST_CHECKS on bbdo3.0
73. [x] **BEEXTCMD36**: external command DISABLE_PASSIVE_HOST_CHECKS and ENABLE_PASSIVE_HOST_CHECKS on bbdo2.0
74. [x] **BEEXTCMD37**: external command DISABLE_PASSIVE_SVC_CHECKS and ENABLE_PASSIVE_SVC_CHECKS on bbdo3.0
75. [x] **BEEXTCMD38**: external command DISABLE_PASSIVE_SVC_CHECKS and ENABLE_PASSIVE_SVC_CHECKS on bbdo2.0
76. [x] **BEEXTCMD39**: external command START_OBSESSING_OVER_HOST and STOP_OBSESSING_OVER_HOST on bbdo3.0
77. [x] **BEEXTCMD4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0
78. [x] **BEEXTCMD40**: external command START_OBSESSING_OVER_HOST and STOP_OBSESSING_OVER_HOST on bbdo2.0
79. [x] **BEEXTCMD41**: external command START_OBSESSING_OVER_SVC and STOP_OBSESSING_OVER_SVC on bbdo3.0
80. [x] **BEEXTCMD42**: external command START_OBSESSING_OVER_SVC and STOP_OBSESSING_OVER_SVC on bbdo2.0
81. [x] **BEEXTCMD5**: external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo3.0
82. [x] **BEEXTCMD6**: external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo2.0
83. [x] **BEEXTCMD7**: external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo3.0
84. [x] **BEEXTCMD8**: external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo2.0
85. [x] **BEEXTCMD9**: external command CHANGE_MAX_SVC_CHECK_ATTEMPTS on bbdo3.0
86. [x] **BEEXTCMD_COMPRESS_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and compressed grpc
87. [x] **BEEXTCMD_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and grpc
88. [x] **BEEXTCMD_GRPC2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 and grpc
89. [x] **BEEXTCMD_GRPC3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 and grpc
90. [x] **BEEXTCMD_GRPC4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 and grpc
91. [x] **BEEXTCMD_REVERSE_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and reversed gRPC
92. [x] **BEEXTCMD_REVERSE_GRPC2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 and grpc reversed
93. [x] **BEEXTCMD_REVERSE_GRPC3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 and grpc reversed
94. [x] **BEEXTCMD_REVERSE_GRPC4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 and grpc reversed
95. [x] **BEHOSTCHECK**: external command CHECK_HOST_RESULT
96. [x] **BEHS1**: store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
97. [x] **BEINSTANCE**: Instance to bdd
98. [x] **BEINSTANCESTATUS**: Instance status to bdd
99. [x] **BENCH_${nb_checks}STATUS**: external command CHECK_SERVICE_RESULT 1000 times
100. [x] **BENCH_${nb_checks}STATUS_TRACES**: external command CHECK_SERVICE_RESULT ${nb_checks} times
101. [x] **BENCH_${nb_checks}_REVERSE_SERVICE_STATUS_TRACES_WITHOUT_SQL**: Broker is configured without SQL output. The connection between Engine and Broker is reversed. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times. Logs are in trace level.
102. [x] **BENCH_${nb_checks}_REVERSE_SERVICE_STATUS_WITHOUT_SQL**: Broker is configured without SQL output. The connection between Engine and Broker is reversed. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times.
103. [x] **BENCH_${nb_checks}_SERVICE_STATUS_TRACES_WITHOUT_SQL**: Broker is configured without SQL output. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times. Logs are in trace level.
104. [x] **BENCH_${nb_checks}_SERVICE_STATUS_WITHOUT_SQL**: Broker is configured without SQL output. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times.
105. [x] **BENCH_1000STATUS_100${suffixe}**: external command CHECK_SERVICE_RESULT 100 times    with 100 pollers with 20 services
106. [x] **BEPBBEE1**: central-module configured with bbdo_version 3.0 but not others. Unable to establish connection.
107. [x] **BEPBBEE2**: bbdo_version 3 not compatible with sql/storage
108. [x] **BEPBBEE3**: bbdo_version 3 generates new bbdo protobuf service status messages.
109. [x] **BEPBBEE4**: bbdo_version 3 generates new bbdo protobuf host status messages.
110. [x] **BEPBBEE5**: bbdo_version 3 generates new bbdo protobuf service messages.
111. [x] **BEPBCVS**: bbdo_version 3 communication of custom variables.
112. [x] **BEPBHostParent**: bbdo_version 3 communication of host parent relations
113. [x] **BEPBINST_CONF**: bbdo_version 3 communication of instance configuration.
114. [x] **BEPBRI1**: bbdo_version 3 use pb_resource new bbdo protobuf ResponsiveInstance message.
115. [x] **BEPB_HOST_DEPENDENCY**: BBDO 3 communication of host dependencies.
116. [x] **BEPB_SERVICE_DEPENDENCY**: bbdo_version 3 communication of host dependencies.
117. [x] **BERD1**: Starting/stopping Broker does not create duplicated events.
118. [x] **BERD2**: Starting/stopping Engine does not create duplicated events.
119. [x] **BERDUC1**: Starting/stopping Broker does not create duplicated events in usual cases
120. [x] **BERDUC2**: Starting/stopping Engine does not create duplicated events in usual cases
121. [x] **BERDUC3U1**: Starting/stopping Broker does not create duplicated events in usual cases with unified_sql and BBDO 3.0
122. [x] **BERDUC3U2**: Starting/stopping Engine does not create duplicated events in usual cases with unified_sql and BBDO 3.0
123. [x] **BERDUCA300**: Starting/stopping Engine is stopped ; it should emit a stop event and receive an ack event with events to clean from broker.
124. [x] **BERDUCA301**: Starting/stopping Engine is stopped ; it should emit a stop event and receive an ack event with events to clean from broker with bbdo 3.0.1.
125. [x] **BERDUCU1**: Starting/stopping Broker does not create duplicated events in usual cases with unified_sql
126. [x] **BERDUCU2**: Starting/stopping Engine does not create duplicated events in usual cases with unified_sql
127. [x] **BERES1**: store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
128. [x] **BESERVCHECK**: external command CHECK_SERVICE_RESULT
129. [x] **BESS1**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
130. [x] **BESS2**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
131. [x] **BESS2U**: Start-Stop Broker/Engine - Broker started first - Engine stopped first. Unified_sql is used.
132. [x] **BESS3**: Start-Stop Broker/Engine - Engine started first - Engine stopped first
133. [x] **BESS4**: Start-Stop Broker/Engine - Engine started first - Broker stopped first
134. [x] **BESS5**: Start-Stop Broker/engine - Engine debug level is set to all, it should not hang
135. [x] **BESSBQ1**: A very bad queue file is written for broker. Broker and Engine are then started, Broker must read the file raising an error because of that file and then get data sent by Engine.
136. [x] **BESS_CRYPTED_GRPC1**: Start-Stop grpc version Broker/Engine - well configured
137. [x] **BESS_CRYPTED_GRPC2**: Start-Stop grpc version Broker/Engine only server crypted
138. [x] **BESS_CRYPTED_GRPC3**: Start-Stop grpc version Broker/Engine only engine crypted
139. [x] **BESS_CRYPTED_REVERSED_GRPC1**: Start-Stop grpc version Broker/Engine - well configured
140. [x] **BESS_CRYPTED_REVERSED_GRPC2**: Start-Stop grpc version Broker/Engine only engine server crypted
141. [x] **BESS_CRYPTED_REVERSED_GRPC3**: Start-Stop grpc version Broker/Engine only engine crypted
142. [x] **BESS_ENGINE_DELETE_HOST**: once engine and cbd started, stop and restart cbd, delete an host and reload engine, cbd mustn't core
143. [x] **BESS_GRPC1**: Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped first
144. [x] **BESS_GRPC2**: Start-Stop grpc version Broker/Engine - Broker started first - Engine stopped first
145. [x] **BESS_GRPC3**: Start-Stop grpc version Broker/Engine - Engine started first - Engine stopped first
146. [x] **BESS_GRPC4**: Start-Stop grpc version Broker/Engine - Engine started first - Broker stopped first
147. [x] **BESS_GRPC5**: Start-Stop grpc version Broker/engine - Engine debug level is set to all, it should not hang
148. [x] **BESS_GRPC_COMPRESS1**: Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped last compression activated
149. [x] **BETAG1**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Broker is started before.
150. [x] **BETAG2**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
151. [x] **BEUTAG1**: Engine is configured with some tags. When broker receives them through unified_sql stream, it stores them in the centreon_storage.tags table. Broker is started before.
152. [x] **BEUTAG10**: some services are configured with tags on two pollers. Then tags are removed from some of them and in centreon_storage, we can observe resources_tags table updated.
153. [x] **BEUTAG11**: some services are configured with tags on two pollers. Then several tags are removed, and we can observe resources_tags table updated.
154. [x] **BEUTAG12**: Engine is configured with some tags. Group tags tag2, tag6 are set to hosts 1 and 2. Category tags tag4 and tag8 are added to hosts 2, 3, 4. The resources and resources_tags tables are well filled. The tag6 and tag8 are removed and resources_tags is also well updated.
155. [x] **BEUTAG2**: Engine is configured with some tags. A new service is added with a tag. Broker should make the relations.
156. [x] **BEUTAG3**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
157. [x] **BEUTAG4**: Engine is configured with some tags. Group tags tag9, tag13 are set to services 1 and 3. Category tags tag3 and tag11 are added to services 1, 3, 5 and 6. The centreon_storage.resources and resources_tags tables are well filled.
158. [x] **BEUTAG5**: Engine is configured with some tags. Group tags tag2, tag6 are set to hosts 1 and 2. Category tags tag4 and tag8 are added to hosts 2, 3, 4. The resources and resources_tags tables are well filled.
159. [x] **BEUTAG6**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.resources_tags table. Engine is started before.
160. [x] **BEUTAG7**: Some services are configured with tags on two pollers. Then tags configuration is modified.
161. [x] **BEUTAG8**: Services have tags provided by templates.
162. [x] **BEUTAG9**: hosts have tags provided by templates.
163. [x] **BEUTAG_REMOVE_HOST_FROM_HOSTGROUP**: remove a host from hostgroup, reload, insert 2 host in the hostgroup must not make sql error
164. [x] **BE_DEFAULT_NOTIFCATION_INTERVAL_IS_ZERO_SERVICE_RESOURCE**: default notification_interval must be set to NULL in services, hosts and resources tables.
165. [x] **BE_NOTIF_OVERFLOW**: bbdo 2.0 notification number =40000. make an overflow => notification_number null in db
166. [x] **BE_TIME_NULL_SERVICE_RESOURCE**: With BBDO 3, notification_interval time must be set to NULL on 0 in services, hosts and resources tables.
167. [x] **BRCS1**: Broker reverse connection stopped
168. [x] **BRCTS1**: Broker reverse connection too slow
169. [x] **BRCTSMN**: Broker connected to map with neb filter
170. [x] **BRCTSMNS**: Broker connected to map with neb and storage filters
171. [x] **BRGC1**: Broker good reverse connection
172. [x] **BRRDCDDID1**: RRD metrics deletion from index ids with rrdcached.
173. [x] **BRRDCDDIDDB1**: RRD metrics deletion from index ids with a query in centreon_storage with rrdcached.
174. [x] **BRRDCDDIDU1**: RRD metrics deletion from index ids with unified sql output with rrdcached.
175. [x] **BRRDCDDM1**: RRD metrics deletion from metric ids with rrdcached.
176. [x] **BRRDCDDMDB1**: RRD metrics deletion from metric ids with a query in centreon_storage and rrdcached.
177. [x] **BRRDCDDMID1**: RRD deletion of non existing metrics and indexes with rrdcached
178. [x] **BRRDCDDMIDU1**: RRD deletion of non existing metrics and indexes with rrdcached
179. [x] **BRRDCDDMU1**: RRD metric deletion on table metric with unified sql output with rrdcached
180. [x] **BRRDCDRB1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with storage/sql sql output and rrdcached.
181. [x] **BRRDCDRBDB1**: RRD metric rebuild with a query in centreon_storage and unified sql with rrdcached
182. [x] **BRRDCDRBU1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with unified_sql output and rrdcached.
183. [x] **BRRDCDRBUDB1**: RRD metric rebuild with a query in centreon_storage and unified sql with rrdcached
184. [x] **BRRDDID1**: RRD metrics deletion from index ids.
185. [x] **BRRDDIDDB1**: RRD metrics deletion from index ids with a query in centreon_storage.
186. [x] **BRRDDIDU1**: RRD metrics deletion from index ids with unified sql output.
187. [x] **BRRDDM1**: RRD metrics deletion from metric ids.
188. [x] **BRRDDMDB1**: RRD metrics deletion from metric ids with a query in centreon_storage.
189. [x] **BRRDDMID1**: RRD deletion of non existing metrics and indexes
190. [x] **BRRDDMIDU1**: RRD deletion of non existing metrics and indexes
191. [x] **BRRDDMU1**: RRD metric deletion on table metric with unified sql output
192. [x] **BRRDRBDB1**: RRD metric rebuild with a query in centreon_storage and unified sql
193. [x] **BRRDRBUDB1**: RRD metric rebuild with a query in centreon_storage and unified sql
194. [x] **BRRDRM1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with storage/sql sql output.
195. [x] **BRRDRMU1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with unified_sql output.
196. [x] **BRRDUPLICATE**: RRD metric rebuild with a query in centreon_storage and unified sql with duplicate rows in database
197. [x] **BRRDWM1**: We are working with BBDO3. This test checks protobuf metrics and status are sent to cbd RRD.
198. [x] **CBD_RELOAD_AND_FILTERS**: We start engine/broker with a classical configuration. All is up and running. Some filters are added to the rrd output and cbd is reloaded. All is still up and running but some events are rejected. Then all is newly set as filter and all events are sent to rrd broker.
199. [x] **CBD_RELOAD_AND_FILTERS_WITH_OPR**: We start engine/broker with an almost classical configuration, just the connection between cbd central and cbd rrd is reversed with one peer retention. All is up and running. Some filters are added to the rrd output and cbd is reloaded. All is still up and running but some events are rejected. Then all is newly set as filter and all events are sent to rrd broker.
200. [x] **DTIM**: New services with several pollers are created. Then downtimes are set on all configured hosts. This action results on 5250 downtimes if we also count impacted services. Then all these downtimes are removed. This test is done with BBDO 3.0.1
201. [x] **EBBPS1**: 1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table resources.
202. [x] **EBBPS2**: 1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table services.
203. [x] **EBDP1**: Four new pollers are started and then we remove Poller3.
204. [x] **EBDP2**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
205. [x] **EBDP3**: Three new pollers are started, then they are killed. It is still possible to remove Poller2 if removed from the configuration.
206. [x] **EBDP4**: Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by Broker.
207. [x] **EBDP5**: Four new pollers are started and then we remove Poller3.
208. [x] **EBDP6**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
209. [x] **EBDP7**: Three new pollers are started, then they are killed. It is still possible to remove Poller2 if removed from the configuration.
210. [x] **EBDP8**: Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by broker.
211. [x] **EBDP_GRPC2**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
212. [x] **EBMSSM**: 1000 services are configured with 100 metrics each. The rrd output is removed from the broker configuration. GetSqlManagerStats is called to measure writes into data_bin.
213. [x] **EBNHG1**: New host group with several pollers and connections to DB
214. [x] **EBNHG4**: New host group with several pollers and connections to DB with broker and rename this hostgroup
215. [x] **EBNHGU1**: New host group with several pollers and connections to DB with broker configured with unified_sql
216. [x] **EBNHGU2**: New host group with several pollers and connections to DB with broker configured with unified_sql
217. [x] **EBNHGU3**: New host group with several pollers and connections to DB with broker configured with unified_sql
218. [x] **EBNHGU4_${test_label}**: New host group with several pollers and connections to DB with broker and rename this hostgroup
219. [x] **EBNSG1**: New service group with several pollers and connections to DB
220. [x] **EBNSGU1**: New service group with several pollers and connections to DB with broker configured with unified_sql
221. [x] **EBNSGU2**: New service group with several pollers and connections to DB with broker configured with unified_sql
222. [x] **EBNSGU3_${test_label}**: New service group with several pollers and connections to DB with broker and rename this servicegroup
223. [x] **EBNSVC1**: New services with several pollers
224. [x] **EBPS2**: 1000 services are configured with 20 metrics each. The rrd output is removed from the broker configuration to avoid to write too many rrd files. While metrics are written in bulk, the database is stopped. This must not crash broker.
225. [x] **EBSAU2**: New services with action_url with more than 2000 characters
226. [x] **EBSN3**: New services with notes with more than 500 characters
227. [x] **EBSNU1**: New services with notes_url with more than 2000 characters
228. [x] **ENRSCHE1**: Verify that next check of a rescheduled host is made at last_check + interval_check
229. [x] **FILTER_ON_LUA_EVENT**: stream connector with a bad configured filter generate a log error message
230. [x] **GRPC_CLOUD_FAILURE**: simulate a broker failure in cloud environment, we provide a muted grpc server and there must remain only one grpc connection. Then we start broker and connection must be ok
231. [x] **GRPC_RECONNECT**: We restart broker and engine must reconnect to it and send data
232. [x] **LCDNU**: the lua cache updates correctly service cache.
233. [x] **LCDNUH**: the lua cache updates correctly host cache
234. [x] **LOGV2DB1**: log-v2 disabled old log enabled check broker sink
235. [x] **LOGV2DB2**: log-v2 disabled old log disabled check broker sink
236. [x] **LOGV2DF1**: log-v2 disabled old log enabled check logfile sink
237. [x] **LOGV2DF2**: log-v2 disabled old log disabled check logfile sink
238. [x] **LOGV2EB1**: Checking broker sink when log-v2 is enabled and legacy logs are disabled.
239. [x] **LOGV2EB2**: log-v2 enabled old log enabled check broker sink
240. [x] **LOGV2EBU1**: Checking broker sink when log-v2 is enabled and legacy logs are disabled with bbdo3.
241. [x] **LOGV2EBU2**: Check Broker sink with log-v2 enabled and legacy log enabled with BBDO3.
242. [x] **LOGV2EF1**: log-v2 enabled    old log disabled check logfile sink
243. [x] **LOGV2EF2**: log-v2 enabled old log enabled check logfile sink
244. [x] **LOGV2FE2**: log-v2 enabled old log enabled check logfile sink
245. [x] **RLCode**: Test if reloading LUA code in a stream connector applies the changes
246. [x] **RRD1**: RRD metric rebuild asked with gRPC API. Three non existing indexes IDs are selected then an error message is sent. This is done with unified_sql output.
247. [x] **SDER**: The check attempts and the max check attempts of (host_1,service_1) are changed to 280 thanks to the retention.dat file. Then Engine and Broker are started and Broker should write these values in the services and resources tables. We only test the services table because we need a resources table that allows bigger numbers for these two attributes. But we see that Broker doesn't crash anymore.
248. [x] **SEVERAL_FILTERS_ON_LUA_EVENT**: Two stream connectors with different filters are configured.
249. [x] **STORAGE_ON_LUA**: The category 'storage' is applied on the stream connector. Only events of this category should be sent to this stream.
250. [x] **STUPID_FILTER**: Unified SQL is configured with only the bbdo category as filter. An error is raised by broker and broker should run correctly.
251. [x] **Service_increased_huge_check_interval**: New services with high check interval at creation time.
252. [x] **Services_and_bulks_${id}**: One service is configured with one metric with a name of 150 to 1021 characters.
253. [x] **Start_Stop_Broker_Engine_${id}**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
254. [x] **Start_Stop_Engine_Broker_${id}**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
255. [x] **UNIFIED_SQL_FILTER**: With bbdo version 3.0.1, we watch events written or rejected in unified_sql
256. [x] **VICT_ONE_CHECK_METRIC**: victoria metrics metric output
257. [x] **VICT_ONE_CHECK_METRIC_AFTER_FAILURE**: victoria metrics metric output after victoria shutdown
258. [x] **VICT_ONE_CHECK_STATUS**: victoria metrics status output
259. [x] **Whitelist_Directory_Rights**: log if /etc/centreon-engine-whitelist has not mandatory rights or owner
260. [x] **Whitelist_Empty_Directory**: log if /etc/centreon-engine-whitelist is empty
261. [x] **Whitelist_Host**: test allowed and forbidden commands for hosts
262. [x] **Whitelist_No_Whitelist_Directory**: log if /etc/centreon-engine-whitelist doesn't exist
263. [x] **Whitelist_Perl_Connector**: test allowed and forbidden commands for services
264. [x] **Whitelist_Service**: test allowed and forbidden commands for services
265. [x] **Whitelist_Service_EH**: test allowed and forbidden event handler for services
266. [x] **metric_mapping**: Check if metric name exists using a stream connector
267. [x] **not1**: This test case configures a single service and verifies that a notification is sent when the service is in a non-OK HARD state.
268. [x] **not10**: This test case involves scheduling downtime on a down host that already had a critical notification. When The Host return to UP state we should receive a recovery notification.
269. [x] **not11**: This test case involves configuring one service and checking that three alerts are sent for it.
270. [x] **not12**: Escalations
271. [x] **not13**: notification for a dependensies host
272. [x] **not14**: notification for a Service dependency
273. [x] **not15**: several notification commands for the same user.
274. [x] **not16**: notification for a dependensies services group
275. [x] **not17**: notification for a dependensies host group
276. [x] **not18**: notification delay where first notification delay equal retry check
277. [x] **not19**: notification delay where first notification delay greater than retry check 
278. [x] **not1_WL_KO**: This test case configures a single service. When it is in non-OK HARD state a notification should be sent but it is not allowed by the whitelist
279. [x] **not1_WL_OK**: This test case configures a single service. When it is in non-OK HARD state a notification is sent because it is allowed by the whitelist
280. [x] **not2**: This test case configures a single service and verifies that a recovery notification is sent
281. [x] **not20**: notification delay where first notification delay samller than retry check
282. [x] **not3**: This test case configures a single service and verifies the notification system's behavior during and after downtime
283. [x] **not4**: This test case configures a single service and verifies the notification system's behavior during and after acknowledgement
284. [x] **not5**: This test case configures two services with two different users being notified when the services transition to a critical state.
285. [x] **not6**: This test case validate the behavior when the notification time period is set to null.
286. [x] **not7**: This test case simulates a host alert scenario.
287. [x] **not8**: This test validates the critical host notification.
288. [x] **not9**: This test case configures a single host and verifies that a recovery notification is sent after the host recovers from a non-OK state.
289. [x] **not_in_timeperiod_with_send_recovery_notifications_anyways**: This test case configures a single service and verifies that a notification is sent when the service is in a non-OK state and OK is sent outside timeperiod when _send_recovery_notifications_anyways is set
290. [x] **not_in_timeperiod_without_send_recovery_notifications_anyways**: This test case configures a single service and verifies that a notification is sent when the service is in a non-OK state and OK is not sent outside timeperiod when _send_recovery_notifications_anyways is not set

### Ccc
1. [x] **BECCC1**: ccc without port fails with an error message
2. [x] **BECCC2**: ccc with -p 51001 connects to central cbd gRPC server.
3. [x] **BECCC3**: ccc with -p 50001 connects to centengine gRPC server.
4. [x] **BECCC4**: ccc with -p 51001 -l returns the available functions from Broker gRPC server
5. [x] **BECCC5**: ccc with -p 51001 -l GetVersion returns an error because we can't execute a command with -l.
6. [x] **BECCC6**: ccc with -p 51001 GetVersion{} calls the GetVersion command
7. [x] **BECCC7**: ccc with -p 51001 GetVersion{"idx":1} returns an error because the input message is wrong.
8. [x] **BECCC8**: ccc with -p 50001 EnableServiceNotifications{"names":{"host_name": "host_1", "service_name": "service_1"}} works and returns an empty message.

### Connector perl
1. [x] **test use connector perl exist script**: test exist script
2. [x] **test use connector perl multiple script**: test script multiple
3. [x] **test use connector perl unknown script**: test unknown script

### Connector ssh
1. [x] **Test6Hosts**: as 127.0.0.x point to the localhost address we will simulate check on 6 hosts
2. [x] **TestBadPwd**: test bad password
3. [x] **TestBadUser**: test unknown user
4. [x] **TestWhiteList**: as 127.0.0.x point to the localhost address we will simulate check on 6 hosts

### Engine
1. [x] **EFHC1**: Engine is configured with hosts and we force checks on one 5 times on bbdo2
2. [x] **EFHC2**: Engine is configured with hosts and we force check on one 5 times on bbdo2
3. [x] **EFHCU1**: Engine is configured with hosts and we force checks on one 5 times on bbdo3. Bbdo3 has no impact on this behavior. resources table is cleared before starting broker.
4. [x] **EFHCU2**: Engine is configured with hosts and we force checks on one 5 times on bbdo3. Bbdo3 has no impact on this behavior.
5. [x] **EMACROS**: macros ADMINEMAIL and ADMINPAGER are replaced in check outputs
6. [x] **EMACROS_NOTIF**: macros ADMINEMAIL and ADMINPAGER are replaced in notification commands
7. [x] **EMACROS_SEMICOLON**: Macros with a semicolon are used even if they contain a semicolon.
8. [x] **EPC1**: Check with perl connector
9. [x] **ESS1**: Start-Stop (0s between start/stop) 5 times one instance of engine and no coredump
10. [x] **ESS2**: Start-Stop (300ms between start/stop) 5 times one instance of engine and no coredump
11. [x] **ESS3**: Start-Stop (0s between start/stop) 5 times three instances of engine and no coredump
12. [x] **ESS4**: Start-Stop (300ms between start/stop) 5 times three instances of engine and no coredump
13. [x] **EXT_CONF1**: Engine configuration is overided by json conf
14. [x] **EXT_CONF2**: Engine configuration is overided by json conf after reload

### Migration
1. [x] **MIGRATION**: Migration bbdo2 with sql/storage to bbdo2 with unified_sql and then to bbdo3 with unified_sql and then to bbdo2 with unified_sql and then to bbdo2 with sql/storage

### Severities
1. [x] **BESEV1**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Broker is started before.
2. [x] **BESEV2**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Engine is started before.
3. [x] **BETUHSEV1**: Hosts have severities provided by templates.
4. [x] **BETUSEV1**: Services have severities provided by templates.
5. [x] **BEUHSEV1**: Four hosts have a severity added. Then we remove the severity from host 1. Then we change severity 10 to severity8 for host 3.
6. [x] **BEUHSEV2**: Seven hosts are configured with a severity on two pollers. Then we remove severities from the first and second hosts of the first poller but only the severity from the first host of the second poller.
7. [x] **BEUSEV1**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Broker is started before.
8. [x] **BEUSEV2**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Engine is started before.
9. [x] **BEUSEV3**: Four services have a severity added. Then we remove the severity from service 1. Then we change severity 11 to severity7 for service 3.
10. [x] **BEUSEV4**: Seven services are configured with a severity on two pollers. Then we remove severities from the first and second services of the first poller but only the severity from the first service of the second poller. Then only severities no more used should be removed from the database.

