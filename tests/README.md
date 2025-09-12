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
        GitPython unqlite py-cpuinfo jwt


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
11. **BAWORST_ACK**: **Scenario:** Acknowledging a service acknowledges the BA, and removing it unacknowledges the BA
  
  * **Given** BBDO version is 3.0.1
    * **And** a Business Activity of type "worst" is configured with two services
    * **When** one of the services is acknowledged
    * **Then** the Business Activity is acknowledged
    * **When** the acknowledgement is removed from the service
    * **Then** the Business Activity is no longer acknowledged
12. **BA_BOOL_KPI**: With bbdo version 3.0.1, a BA of type 'worst' with 1 boolean kpi
13. **BA_CHANGED**: A BA of type worst is configured with one service kpi. Then it is modified so that the service kpi is replaced by a boolean rule kpi. When cbd is reloaded, the BA is well updated.
14. **BA_DISABLED**: create a disabled BA with timeperiods and reporting filter don't create error message
15. **BA_IMPACT_2KPI_SERVICES**: With bbdo version 3.0.1, a BA of type 'impact' with 2 serv, ba is critical only if the 2 services are critical
16. **BA_IMPACT_IMPACT**: 
      * Given a Business Activity (BA) of type "impact"
    * **And** it has two child BAs of type "impact"
    * **And** the first child has an impact of 90
    * **And** the second child has an impact of 10
    * **When** both child BAs are impacting
    * **Then** the parent BA should be "critical"
    * **When** both child BAs are not impacting
    * **Then** the parent BA should be "ok"
17. **BA_RATIO_NUMBER_BA_4_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
18. **BA_RATIO_NUMBER_BA_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 2 services and one ba with 1 service
19. **BA_RATIO_PERCENT_BA_4_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
20. **BA_RATIO_PERCENT_BA_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio percent' with 2 serv an 1 ba with one service
21. **BA_SERVICE_PNAME_AFTER_RELOAD**: **Scenario:** Verify that the parent_name of a BA service is not erased after a broker reload
    * **Given** a BA "test" of type "worst" with its service "host_16:service_302"
    * **When** I start broker and engine
    * **Then** the BA service "test" should have a status of 0 within 30 seconds
    * **When** I reload the broker
    * **Then** the database should still contain a BA service with name "test" and parent_name "_Module_BAM_1"
22. **BEBAMIDT1**: 
      * Given a BA of type 'worst' with one service is configured
  
  * **And** The BA is in critical state due to its service
    * **When** a downtime is set on this service
    * **Then** an inherited downtime is set to the BA
    * **When** the downtime is removed from the service
    * **Then** the inherited downtime is deleted from the BA
23. **BEBAMIDT2**: 
      * Given a BA of type 'worst' with one service is configured
    * **And** the BA is in critical state due to its service
    * **And** a downtime is set on this service
    * **Then** an inherited downtime is set to the BA
    * **When** Engine is restarted
    * **And** Broker is restarted
    * **Then** both downtimes are still present with no duplicates
    * **When** the downtime is removed from the service
    * **Then** the inherited downtime is deleted
24. **BEBAMIDTU1**: 
      * Given BBDO version 3.0.1 is running
  
  * **And** a BA of type 'worst' with one service is configured
    * **And** The BA is in critical state due to its service
    * **When** a downtime is set on this service
    * **Then** an inherited downtime is set to the BA
    * **When** the downtime is removed from the service
    * **Then** the inherited downtime is deleted from the BA
25. **BEBAMIDTU2**: 
      * Given BBDO version 3.0.1 is in use
    * **And** a 'worst' type BA with one service is configured
    * **And** The BA is in critical state due to its service
    * **When** a downtime is set on this service
    * **Then** an inherited downtime is set to the BA
    * **When** Engine is restarted
    * **And** Broker is restarted
    * **Then** both downtimes are still present with no duplicates
    * **When** the downtime is removed from the service
    * **Then** the inherited downtime is deleted
26. **BEBAMIGNDT1**: A BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. A first downtime is cancelled, the BA is still OK, but when the second downtime is cancelled, the BA should be CRITICAL.
27. **BEBAMIGNDT2**: A BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. The first downtime reaches its end, the BA is still OK, but when the second downtime reaches its end, the BA should be CRITICAL.
28. **BEBAMIGNDTU1**: With bbdo version 3.0.1, a BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. A first downtime is cancelled, the BA is still OK, but when the second downtime is cancelled, the BA should be CRITICAL.
29. **BEBAMIGNDTU2**: 
      * Given BBDO version 3.0.1 is configured
    * **And** a BA of type "worst" with two services is set up
    * **And** the downtime policy on this BA is "Ignore the indicator in the calculation"
    * **And** the BA is in a critical state due to the second critical service
    * **When** two downtimes are applied to the second critical service
    * **Then** the BA state should be OK due to the policy on indicators
    * **When** the first downtime reaches its end
    * **Then** the BA state should still be OK
    * **When** the second downtime reaches its end
    * **Then** the BA should be in a critical state
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
1. **BC1**: Central and RRD brokers are started. Then we check they are correctly connected. RRD broker is stopped. The connection is lost. Then RRD broker is started again. The connection is re-established. Central broker is stopped. The connection is lost. Then Central broker is started again. The connection is re-established.
2. **BCL1**: Starting broker with option '-s foobar' should return an error
3. **BCL2**: Starting broker with option '-s5' should work
4. **BCL3**: Starting broker with options '-D' should work and activate diagnose mode
5. **BCL4**: Starting broker with options '-s2' and '-D' should work.
6. **BDB1**: Access denied when database name exists but is not the good one for sql output
7. **BDB10**: connection should be established when user password is good for sql/perfdata
8. **BDB2**: Access denied when database name exists but is not the good one for storage output
9. **BDB3**: Access denied when database name does not exist for sql output
10. **BDB4**: Access denied when database name does not exist for storage and sql outputs
11. **BDB5**: cbd does not crash if the storage/sql db_host is wrong
12. **BDB6**: cbd does not crash if the sql db_host is wrong
13. **BDB7**: access denied when database user password is wrong for perfdata/sql
14. **BDB8**: access denied when database user password is wrong for perfdata/sql
15. **BDB9**: access denied when database user password is wrong for sql
16. **BDBM1**: start broker/engine and then start MariaDB => connection is established
17. **BDBMU1**: start broker/engine with unified sql and then start MariaDB => connection is established
18. **BDBU1**: Access denied when database name exists but is not the good one for unified sql output
19. **BDBU10**: Connection should be established when user password is good for unified sql
20. **BDBU3**: Access denied when database name does not exist for unified sql output
21. **BDBU5**: cbd does not crash if the unified sql db_host is wrong
22. **BDBU7**: Access denied when database user password is wrong for unified sql
23. **BEDB2**: start broker/engine and then start MariaDB => connection is established
24. **BEDB3**: start broker/engine, then stop MariaDB and then start it again. The gRPC API should give informations about SQL connections.
25. **BEDB4**: start broker/engine, then stop MariaDB and then start it again. The gRPC API should give informations about SQL connections.
26. **BFC1**: Start broker with invalid filters but one filter ok
27. **BFC2**: Start broker with only invalid filters on an output
28. **BGRPCSS1**: Start-Stop two instances of broker configured with grpc stream and no coredump
29. **BGRPCSS2**: Start/Stop 10 times broker configured with grpc stream with 300ms interval and no coredump
30. **BGRPCSS3**: Start-Stop one instance of broker configured with grpc stream and no coredump
31. **BGRPCSS4**: Start/Stop 10 times broker configured with grpc stream with 1sec interval and no coredump
32. **BGRPCSS5**: Start-Stop with reversed connection on grpc acceptor with only one instance and no deadlock
33. **BGRPCSSU1**: Start-Stop with unified_sql two instances of broker with grpc stream and no coredump
34. **BGRPCSSU2**: Start/Stop with unified_sql 10 times broker configured with grpc stream with 300ms interval and no coredump
35. **BGRPCSSU3**: Start-Stop with unified_sql one instance of broker configured with grpc and no coredump
36. **BGRPCSSU4**: Start/Stop with unified_sql 10 times broker configured with grpc stream with 1sec interval and no coredump
37. **BGRPCSSU5**: Start-Stop with unified_sql with reversed connection on grpc acceptor with only one instance and no deadlock
38. **BLBD**: Start Broker with loggers levels by default
39. **BLDIS1**: Start broker with core logs 'disabled'
40. **BLEC1**: Change live the core level log from trace to debug
41. **BLEC2**: Change live the core level log from trace to foo raises an error
42. **BLEC3**: Change live the foo level log to trace raises an error
43. **BSCSS1**: Start-Stop two instances of broker and no coredump
44. **BSCSS2**: Start/Stop 10 times broker with 300ms interval and no coredump
45. **BSCSS3**: Start-Stop one instance of broker with tcp connection and no coredump
46. **BSCSS4**: Start/Stop 10 times broker with 1sec interval and no coredump
47. **BSCSSC1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is enabled on client side.
48. **BSCSSC2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is disabled on client side.
49. **BSCSSCG1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on client side.
50. **BSCSSCGRR1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on output side. Reversed connection with retention and grpc transport protocol.
51. **BSCSSCGRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on output side. Reversed connection with retention and grpc transport protocol.
52. **BSCSSCRR1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is enabled on client side. Connection reversed with retention.
53. **BSCSSCRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is disabled on client side. Connection reversed with retention.
54. **BSCSSG1**: Start-Stop two instances of broker and no coredump
55. **BSCSSG2**: Start/Stop 10 times broker with 300ms interval and no coredump
56. **BSCSSG3**: Start-Stop one instance of broker with grpc connection and no coredump
57. **BSCSSG4**: Start/Stop 10 times broker with 1sec interval and no coredump
58. **BSCSSGA1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. An authorization token is added on the server. Error messages are raised.
59. **BSCSSGA2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. An authorization token is added on the server and also on the client. All looks ok.
60. **BSCSSGRR1**: Start-Stop two instances of broker and no coredump, reversed and retention, with transport protocol grpc, start-stop 5 times.
61. **BSCSSK1**: Start-Stop two instances of broker, server configured with grpc and client with tcp. No connectrion established and error raised on client side.
62. **BSCSSK2**: Start-Stop two instances of broker, server configured with tcp and client with grpc. No connection established and error raised on client side.
63. **BSCSSP1**: Start-Stop two instances of broker and no coredump. The server contains a listen address
64. **BSCSSPRR1**: Start-Stop two instances of broker and no coredump. The server contains a listen address, reversed and retention. centreon-broker-master-rrd is then a failover.
65. **BSCSSR1**: Start-Stop two instances of broker and no coredump. Connection with bbdo_server/bbdo_client and reversed.
66. **BSCSSRR1**: Start-Stop two instances of broker and no coredump. Connection with bbdo_server/bbdo_client, reversed and retention. centreon-broker-master-rrd is then a failover.
67. **BSCSSRR2**: Start/Stop 10 times broker with 300ms interval and no coredump, reversed and retention. centreon-broker-master-rrd is then a failover.
68. **BSCSST1**: Start-Stop two instances of broker and no coredump. Encryption is enabled on client side.
69. **BSCSST2**: Start-Stop two instances of broker and no coredump. Encryption is enabled on client side.
70. **BSCSSTG1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. This is not sufficient, then an error is raised.
71. **BSCSSTG2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. It works with good certificates and keys.
72. **BSCSSTG3**: Start-Stop two instances of broker. The connection cannot be established if the server private key is missing and an error message explains this issue.
73. **BSCSSTGRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. It works with good certificates and keys. Reversed grpc connection with retention.
74. **BSCSSTRR1**: Start-Stop two instances of broker and no coredump. Encryption is enabled. transport protocol is tcp, reversed and retention.
75. **BSCSSTRR2**: Start-Stop two instances of broker and no coredump. Encryption is enabled.
76. **BSS1**: Start-Stop two instances of broker and no coredump
77. **BSS2**: Start/Stop 10 times broker with 300ms interval and no coredump
78. **BSS3**: Start-Stop one instance of broker 5 times and no coredump
79. **BSS4**: Start/Stop 10 times broker with 1sec interval and no coredump
80. **BSS5**: Start-Stop with reversed connection on TCP acceptor with only one instance and no deadlock
81. **BSSU1**: Start-Stop two instances of broker with BBDO3 and no coredump
82. **BSSU2**: Start/Stop 10 times broker (BBDO3) with 300ms interval and no coredump
83. **BSSU3**: Start-Stop one instance of broker (BBDO3) and no coredump
84. **BSSU4**: Start/Stop 10 times broker with 1sec interval and no coredump
85. **BSSU5**: Start-Stop with reversed connection on TCP acceptor with only one instance and no deadlock
86. **START_STOP_CBD**: restart cbd with unified_sql services state must not be null after restart

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
3. **ANO_DT2**: 
      * Given a service and its AD,
  
  * when we delete downtime on dependent service, AD must not be in downtime anymore
4. **ANO_DT3**: delete downtime on anomaly don t delete dependent service one
5. **ANO_DT4**: **Scenario:** Removing downtime from service keeps it on anomaly detection
    * **Given** an anomaly detection is attached to a service
    * **And** a downtime is set on both the service and the anomaly detection
    * **When** the downtime is removed from the service
    * **Then** the downtime should still be present on the anomaly detection
6. **ANO_EXTCMD_SENSITIVITY_SAVED**: extcmd sensitivity saved in retention
7. **ANO_JSON_SENSITIVITY_NOT_SAVED**: json sensitivity not saved in retention
8. **ANO_NOFILE**: an anomaly detection without threshold file must be in unknown state
9. **ANO_NOFILE_VERIF_CONFIG_NO_ERROR**: An anomaly detection without threshold file doesn't display error on config check
10. **ANO_OUT_LOWER_THAN_LIMIT**: an anomaly detection with a perfdata lower than lower limit make a critical state
11. **ANO_OUT_UPPER_THAN_LIMIT**: an anomaly detection with a perfdata upper than upper limit make a critical state
12. **ANO_TOO_OLD_FILE**: An anomaly detection with an oldest threshold file must be in unknown state
13. **AOUTLU1**: an anomaly detection with a perfdata upper than upper limit make a critical state with bbdo 3
14. **BAM_STREAM_FILTER**: With bbdo version 3.0.1, a BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. we watch its events
15. **BEACK1**: **Scenario:** Acknowledging a critical service
  
  * **Given** Engine has a critical service
    * **When** an external command is sent to acknowledge it
    * **Then** the "centreon_storage.acknowledgements" table is updated with this acknowledgement
    * **And** a log in "centreon_storage.logs" concerning this acknowledgement is added.
    * **When** the service is set to OK
    * **Then** the acknowledgement is deleted from the Engine
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
29. **BECNHG1**: **Scenario:** Host group synchronization across 3 pollers in centralized configuration
  
  * **Given** a centralized engine with 3 pollers
    * **And** broker is configured with RRD, central module, and SQL debug logging
    * **And** database connections are set to 5 for both SQL and perfdata outputs
    * **When** I start the broker and engine with new generation
    * **And** I add a host group containing 3 hosts (host_1, host_2, host_3)
    * **And** I notify broker of the engine configuration change
    * **Then** the logs should confirm membership of all 3 hosts to the host group
    * **And** each host should be properly associated with host group 1 on instance 1
30. **BECNHG3**: **Scenario:** Host group synchronization across 4 pollers in centralized configuration
    * **Given** 4 pollers and Broker are started in centralized mode
    * **When** hostgroup_1 is added with 3 hosts per poller (12 total)
    * **Then** Broker receives all 12 hosts as hostgroup_1 members
    * **When** hostgroup configuration files are removed sequentially from each poller
    * **Then** Broker progressively removes corresponding hosts from database
    * **And** hostgroup_1 membership decreases from 12 → 9 → 6 → 3 → 0
31. **BECNHG4**: **Scenario:** Host group rename synchronization in centralized configuration
    * **Given** 3 pollers and Broker are started in centralized mode
    * **When** hostgroup_1 is created on poller 1 with hosts: host_1, host_2, host_3
    * **Then** Broker receives hostgroup_1 with its 3 members
    * **When** hostgroup_1 is renamed to hostgroup_test
    * **Then** Broker updates the hostgroup name to hostgroup_test in database
    * **And** the same 3 hosts remain as members of hostgroup_test
32. **BECNHG5**: **Scenario:** Host group removal and recreation with same hosts in centralized configuration
    * **Given** 3 pollers and Broker are started in centralized mode
    * **When** hostgroup_1 is created on each poller with different hosts
    * **Then** Broker receives all hostgroups with their respective members
    * **When** hostgroup_1 is removed from poller 1 and hostgroup_2 is created with the same hosts
    * **Then** Broker updates the database to reflect the changes
    * **And** hostgroup_2 contains the 3 hosts from poller 1
    * **And** hostgroup_1 still contains the 6 hosts from pollers 2 and 3
33. **BECNSVC1**: 
      * Given a Centreon platform with 3 pollers configured
  
  * **And** 50 hosts distributed across pollers (17+17+16)
    * **And** initially 20 services per host
    * **And** BBDO3 protocol with unified SQL output enabled
    * **When** the number of services per host is progressively increased
    * **And** the configuration is hot-reloaded 3 times (20→24→28 services/host)
    * **Then** each poller should monitor the correct number of resources
    * **And** poller 1 should monitor exactly (17 hosts × services) + 17 hosts
    * **And** poller 2 should monitor exactly (17 hosts × services) + 17 hosts
    * **And** poller 3 should monitor exactly (16 hosts × services) + 16 hosts
    * **And** each verification should complete within 30 seconds
    * **And** the load balancing should remain stable during scaling
34. **BECSS1**: **Scenario:** Broker sends configuration to engine in new generation
  
  * **Given** an engine configuration is provided to the broker
    * **And** the broker and engine are started in new generation (broker first)
    * **And** the protocol is bbdo3
    * **When** the broker detects the configuration for the engine
    * **Then** the broker sends the configuration to the engine
    * **Then** both broker and engine are stopped (engine first)
35. **BECSS2**: **Scenario:** Broker sends configuration to engine in new generation
    * **Given** an engine configuration is provided to the broker
    * **And** the broker and engine are started in new generation (broker first)
    * **And** the protocol is bbdo3
    * **When** the broker detects the configuration for the engine
    * **Then** the broker sends the configuration to the engine
    * **Then** both broker and engine are stopped (engine first)
36. **BECSS3**: **Scenario:** Broker sends configuration to engine in new generation
    * **Given** an engine configuration is provided to the broker
    * **And** the broker and engine are started in new generation (engine first)
    * **And** the protocol is bbdo3
    * **When** the broker detects the configuration for the engine
    * **Then** the broker sends the configuration to the engine
    * **Then** both broker and engine are stopped (engine first)
37. **BECSS4**: **Scenario:** Broker sends configuration to engine in new generation
    * **Given** an engine configuration is provided to the broker
    * **And** the broker and engine are started in new generation (engine first)
    * **And** the protocol is bbdo3
    * **When** the broker detects the configuration for the engine
    * **Then** the broker sends the configuration to the engine
    * **Then** both broker and engine are stopped (broker first)
38. **BECSSBQ1**: A very bad queue file is written for broker. Broker and Engine are then started, Broker must read the file raising an error because of that file and then get data sent by Engine.
39. **BECSS_CRYPTED_GRPC1**: Start-Stop grpc version Broker/Engine - well configured
40. **BECSS_CRYPTED_GRPC2**: Start-Stop grpc version Broker/Engine only server crypted
41. **BECSS_CRYPTED_GRPC3**: Start-Stop grpc version Broker/Engine only engine crypted
42. **BECSS_CRYPTED_REVERSED_GRPC1**: Start-Stop grpc version Broker/Engine - well configured
43. **BECSS_CRYPTED_REVERSED_GRPC2**: Start-Stop grpc version Broker/Engine only engine server crypted
44. **BECSS_CRYPTED_REVERSED_GRPC3**: Start-Stop grpc version Broker/Engine only engine crypted
45. **BECSS_ENGINE_DELETE_HOST**: once engine and cbd started, stop and restart cbd, delete an host and reload engine, cbd mustn't core
46. **BECSS_GRPC1**: **Scenario:** Broker sends configuration to engine in new generation
    * **Given** an engine configuration is provided to the broker
    * **And** the broker and engine are started in new generation (broker first)
    * **And** the protocol is bbdo3
    * **When** the broker detects the configuration for the engine
    * **Then** the broker sends the configuration to the engine
    * **Then** both broker and engine are stopped (engine first)
47. **BECSS_GRPC2**: **Scenario:** Broker sends configuration to engine in new generation
    * **Given** an engine configuration is provided to the broker
    * **And** the broker and engine are started in new generation (broker first)
    * **And** the protocol is bbdo3
    * **When** the broker detects the configuration for the engine
    * **Then** the broker sends the configuration to the engine
    * **Then** both broker and engine are stopped (engine first)
48. **BECSS_GRPC3**: **Scenario:** Broker sends configuration to engine in new generation
    * **Given** an engine configuration is provided to the broker
    * **And** the broker and engine are started in new generation (engine first)
    * **And** the protocol is bbdo3
    * **When** the broker detects the configuration for the engine
    * **Then** the broker sends the configuration to the engine
    * **Then** both broker and engine are stopped (engine first)
49. **BECSS_GRPC4**: **Scenario:** Broker sends configuration to engine in new generation
    * **Given** an engine configuration is provided to the broker
    * **And** the broker and engine are started in new generation (engine first)
    * **And** the protocol is bbdo3
    * **When** the broker detects the configuration for the engine
    * **Then** the broker sends the configuration to the engine
    * **Then** both broker and engine are stopped (broker first)
50. **BECSS_GRPC_COMPRESS1**: Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped last compression activated
51. **BECT1**: Broker/Engine communication with anonymous TLS between central and poller
52. **BECT2**: Broker/Engine communication with TLS between central and poller with key/cert
53. **BECT3**: Broker/Engine communication with anonymous TLS and ca certificate
54. **BECT4**: Broker/Engine communication with TLS between central and poller with key/cert and hostname forced
55. **BECTAG1**: **Feature:** Tag Management between Engine and Broker
  
  * As a Centreon administrator
    * I want to configure tags in Engine
    * So that Broker stores them correctly in centreon_storage.tags table
    * Background:
  
   * **Given** Engine is configured with centralized setup
     * **And** Broker components (central, rrd, module) are configured
     * **And** Database logging is enabled with debug/trace level
     * **And** Retention data is cleared
  
  * **Scenario:** Initial tag configuration
  
   * **Given** Engine is configured with 20 tags
     * **When** Broker and Engine are started
     * **Then** 20 tags should be added/modified in logs
     * **And** INSERT statements should be executed in tags table
     * **And** Configuration file should match database content
     * **And** Tag IDs should be consistent
  
  * **Scenario:** Tag configuration modification
  
   * **Given** Initial configuration with 20 tags is loaded
     * **When** Configuration is modified to 30 tags
     * **And** Engine configuration change is notified
     * **Then** 10 additional tags should be added/modified
     * **And** Configuration file should still match database content
     * **And** Tag IDs should remain consistent
  
  * **Scenario:** Tag configuration reduction
  
   * **Given** Configuration with 30 tags is loaded
     * **When** Configuration is reduced to 11 tags starting at ID 50
     * **And** Engine configuration change is notified
     * **Then** 11 tags should be present in final configuration
     * **And** Unused tags should be implicitly removed
     * **And** Configuration file should match database content
     * **And** Tag IDs should be consistent with new range
56. **BECT_GRPC1**: Broker/Engine communication with GRPC and with anonymous TLS between central and poller
57. **BECT_GRPC2**: Broker/Engine communication with TLS between central and poller with key/cert
58. **BECT_GRPC3**: Broker/Engine communication with anonymous TLS and ca certificate
59. **BECT_GRPC4**: Broker/Engine communication with TLS between central and poller with key/cert and hostname forced
60. **BECUSTOMHOSTVAR**: external command CHANGE_CUSTOM_HOST_VAR on SNMPVERSION
61. **BECUSTOMSVCVAR**: external command CHANGE_CUSTOM_SVC_VAR on CRITICAL
62. **BEDTHOSTFIXED**: A downtime is set on a host, the total number of downtimes is really 21 (1 for the host and 20 for its 20 services) then we delete this downtime and the number is 0.
63. **BEDTHOSTFIXED1**: **Scenario:** Setting and Removing Downtime on a Host and its Services
    * **Given** a downtime is set on a host
    * **Then** the total number of downtimes is 21
    * **And** this includes 1 for the host and 20 for its services
    * **When** the downtime is deleted
    * **Then** the total number of downtimes is 0
64. **BEDTMASS1**: **Scenario:** Setting and Removing Downtimes on Configured Hosts and Services
  
  * **Given** new services with several pollers are created
    * **When** downtimes are set on all configured hosts
    * **Then** the total number of downtimes, including impacted services, is 1050
    * **And** all these downtimes are removed
    * **And** the test is performed with BBDO 3.0.0
65. **BEDTMASS2**: **Scenario:** Setting and Removing Downtimes on Configured Hosts and Services
    * **Given** new services with several pollers are created
    * **When** downtimes are set on all configured hosts
    * **Then** the total number of downtimes, including impacted services, is 1050
    * **And** all these downtimes are removed
    * **And** the test is performed with BBDO 2.0.0
66. **BEDTRRD1**: A service is forced checked then a downtime is set on this service. The service is forced checked again and the downtime is removed. This test is done with BBDO 3.0.0. Then we should not get any error in cbd RRD of kind 'ignored update error in file...'.
67. **BEDTSVCFIXED**: 
      * Given a unique downtime set on a service
    * **When** the downtime is removed
    * **Then** the downtime is well removed
    * **And** the number of downtimes is 0
68. **BEDTSVCFIXED1**: 
      * Given a configuration with BBDO3 and a unique downtime set on a service
    * **When** the downtime is removed
    * **Then** the downtime is well removed
    * **And** the number of downtimes is 0
69. **BEDTSVCREN1**: 
      * Given a downtime set on a service
    * **When** the service is renamed
    * **Then** the downtime is still active on the renamed service
    * **When** the downtime is removed from the renamed service
    * **Then** the downtime is well removed
70. **BEDTSVCREN2**: 
      * Given a configuration with BBDO3 and a downtime set on a service
    * **When** the service is renamed
    * **Then** the downtime is still active on the renamed service
    * **When** the downtime is removed from the renamed service
    * **Then** the downtime is well removed
71. **BEDW**: **Scenario:** Verify Broker configured with cache_config_directory listens to it
    * **Given** the Central Broker is started with cache_config_directory set to a specific Directory
    * **And** the pollers_config_directory is set to its default value: /var/lib/centreon-broker/pollers-configuration.
    * **When** a file of the form <poller_id>.lck is created in the cache_config_directory
    * **Then** Broker logs a message telling the file has been created
    * **When** the corresponding configuration directory doesn't exist
    * **Then** Broker logs a message telling the directory doesn't exist
72. **BEDWEN**: **Scenario:** Verify Broker configured with cache_config_directory listens to it
    * **Given** the Central Broker is started with cache_config_directory set to a specific Directory
    * **And** the pollers_config_directory is set (default value) to /var/lib/centreon-broker/pollers-configuration.
    * **When** a file of the form <poller_id>.lck is created in the cache_config_directory
    * **Then** Broker logs a message telling the file has been created
    * **When** the corresponding configuration directory doesn't exist
    * **Then** Broker logs a message telling the directory doesn't exist
73. **BEDWEND**: **Scenario:** Verify Broker configured with cache_config_directory creates the protobuf serialized configuration
    * **Given** Central Broker is started with cache_config_directory set to a specific Directory
    * **And** the pollers_config_directory is set (default value) to /var/lib/centreon-broker/pollers-configuration.
    * **And** Central Broker has already sent a first configuration to Engine
    * **When** a new configuration is put into the cache_config_directory
    * **Then** Engine should be notified about the new configuration by Broker
    * **And** Engine should update its configuration from a differential configuration
74. **BEDWENF**: **Scenario:** Verify Broker configured with cache_config_directory creates the protobuf serialized configuration
    * **Given** the Central Broker is started with cache_config_directory set to a specific Directory
    * **And** the pollers_config_directory is set (default value) to /var/lib/centreon-broker/pollers-configuration.
    * **When** a file of the form <poller_id>.lck is created after the <poller_id> directory is filled correctly
    * **Then** Broker logs a message telling the file has been created
    * **And** Broker dumps a file <poller_id>.prot in the pollers_conf directory
75. **BEEXTCMD1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0
76. **BEEXTCMD10**: external command CHANGE_MAX_SVC_CHECK_ATTEMPTS on bbdo2.0
77. **BEEXTCMD11**: external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo3.0
78. **BEEXTCMD12**: external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo2.0
79. **BEEXTCMD13**: external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo3.0
80. **BEEXTCMD14**: external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo2.0
81. **BEEXTCMD15**: external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo3.0
82. **BEEXTCMD16**: external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo2.0
83. **BEEXTCMD17**: external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo3.0
84. **BEEXTCMD18**: external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo2.0
85. **BEEXTCMD19**: external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo3.0
86. **BEEXTCMD2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0
87. **BEEXTCMD20**: external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo2.0
88. **BEEXTCMD21**: external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS and ENABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo3.0
89. **BEEXTCMD22**: external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS and ENABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo2.0
90. **BEEXTCMD23**: 
      * Given Engine and broker configured with BBDO3
  
  * **When** the external command DISABLE_HOST_CHECK on host_1 is executed
    * **Then** the host_1 host checks should be disabled
    * **When** the external command ENABLE_HOST_CHECK on host_1 is executed
    * **Then** the host_1 host checks should be enabled
91. **BEEXTCMD24**: external command DISABLE_HOST_CHECK and ENABLE_HOST_CHECK on bbdo2.0
92. **BEEXTCMD25**: external command DISABLE_HOST_EVENT_HANDLER and ENABLE_HOST_EVENT_HANDLER on bbdo3.0
93. **BEEXTCMD26**: external command DISABLE_HOST_EVENT_HANDLER and ENABLE_HOST_EVENT_HANDLER on bbdo2.0
94. **BEEXTCMD27**: external command DISABLE_HOST_FLAP_DETECTION and ENABLE_HOST_FLAP_DETECTION on bbdo3.0
95. **BEEXTCMD28**: external command DISABLE_HOST_FLAP_DETECTION and ENABLE_HOST_FLAP_DETECTION on bbdo2.0
96. **BEEXTCMD29**: external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo3.0
97. **BEEXTCMD3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0
98. **BEEXTCMD30**: external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo2.0
99. **BEEXTCMD31**: external command DISABLE_HOST_SVC_CHECKS and ENABLE_HOST_SVC_CHECKS on bbdo3.0
100. **BEEXTCMD32**: external command DISABLE_HOST_SVC_CHECKS and ENABLE_HOST_SVC_CHECKS on bbdo2.0
101. **BEEXTCMD33**: external command DISABLE_HOST_SVC_NOTIFICATIONS and ENABLE_HOST_SVC_NOTIFICATIONS on bbdo3.0
102. **BEEXTCMD34**: external command DISABLE_HOST_SVC_NOTIFICATIONS and ENABLE_HOST_SVC_NOTIFICATIONS on bbdo2.0
103. **BEEXTCMD35**: external command DISABLE_PASSIVE_HOST_CHECKS and ENABLE_PASSIVE_HOST_CHECKS on bbdo3.0
104. **BEEXTCMD36**: external command DISABLE_PASSIVE_HOST_CHECKS and ENABLE_PASSIVE_HOST_CHECKS on bbdo2.0
105. **BEEXTCMD37**: external command DISABLE_PASSIVE_SVC_CHECKS and ENABLE_PASSIVE_SVC_CHECKS on bbdo3.0
106. **BEEXTCMD38**: external command DISABLE_PASSIVE_SVC_CHECKS and ENABLE_PASSIVE_SVC_CHECKS on bbdo2.0
107. **BEEXTCMD39**: external command START_OBSESSING_OVER_HOST and STOP_OBSESSING_OVER_HOST on bbdo3.0
108. **BEEXTCMD4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0
109. **BEEXTCMD40**: external command START_OBSESSING_OVER_HOST and STOP_OBSESSING_OVER_HOST on bbdo2.0
110. **BEEXTCMD41**: external command START_OBSESSING_OVER_SVC and STOP_OBSESSING_OVER_SVC on bbdo3.0
111. **BEEXTCMD42**: external command START_OBSESSING_OVER_SVC and STOP_OBSESSING_OVER_SVC on bbdo2.0
112. **BEEXTCMD5**: external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo3.0
113. **BEEXTCMD6**: external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo2.0
114. **BEEXTCMD7**: external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo3.0
115. **BEEXTCMD8**: external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo2.0
116. **BEEXTCMD9**: external command CHANGE_MAX_SVC_CHECK_ATTEMPTS with bbdo3.0
117. **BEEXTCMD_COMPRESS_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and compressed grpc
118. **BEEXTCMD_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and grpc
119. **BEEXTCMD_GRPC2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 and grpc
120. **BEEXTCMD_GRPC3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 and grpc
121. **BEEXTCMD_GRPC4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 and grpc
122. **BEEXTCMD_REVERSE_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and reversed gRPC
123. **BEEXTCMD_REVERSE_GRPC2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 and grpc reversed
124. **BEEXTCMD_REVERSE_GRPC3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 and grpc reversed
125. **BEEXTCMD_REVERSE_GRPC4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 and grpc reversed
126. **BEHOSTCHECK**: 
      * Given Engine and Broker configured to work with BBDO 3
  
  * **When** a schedule forced host check command on host host_1 is launched
    * **Then** the result appears in the centreon_storage resources table
127. **BEHS1**: store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
128. **BEINSTANCE**: Instance to bdd
129. **BEINSTANCESTATUS**: Instance status to bdd
130. **BENCH_${nb_checks}STATUS**: external command CHECK_SERVICE_RESULT 1000 times
131. **BENCH_${nb_checks}STATUS_TRACES**: external command CHECK_SERVICE_RESULT ${nb_checks} times
132. **BENCH_${nb_checks}_REVERSE_SERVICE_STATUS_TRACES_WITHOUT_SQL**: Broker is configured without SQL output. The connection between Engine and Broker is reversed. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times. Logs are in trace level.
133. **BENCH_${nb_checks}_REVERSE_SERVICE_STATUS_WITHOUT_SQL**: Broker is configured without SQL output. The connection between Engine and Broker is reversed. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times.
134. **BENCH_${nb_checks}_SERVICE_STATUS_TRACES_WITHOUT_SQL**: Broker is configured without SQL output. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times. Logs are in trace level.
135. **BENCH_${nb_checks}_SERVICE_STATUS_WITHOUT_SQL**: Broker is configured without SQL output. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times.
136. **BENCH_1000STATUS_100${suffixe}**: external command CHECK_SERVICE_RESULT 100 times    with 100 pollers with 20 services
137. **BENCV**: Engine is configured with hosts/services. The first host has no customvariable. Then we add a customvariable to the first host and we reload engine. Then the host should have this new customvariable defined and centengine should not crash.
138. **BENHG1**: New host group with several pollers and connections to DB
139. **BENHG4**: New host group with several pollers and connections to DB with broker and rename this hostgroup
140. **BENHGU1**: New host group with several pollers and connections to DB with broker configured with unified_sql
141. **BENHGU2**: New host group with several pollers and connections to DB with broker configured with unified_sql
142. **BENHGU3**: New host group with several pollers and connections to DB with broker configured with unified_sql
143. **BENHGU4_${test_label}**: New host group with several pollers and connections to DB with broker and rename this hostgroup
144. **BEOTEL_CENTREON_AGENT_CEIP**: **Scenario:** Agent and "centreon_storage.agent_information" Statistics
    * **Given** Engine connected to Broker
    * **When** an agent connects to Engine
  
 * **Then** a message is sent to Broker that results in a new row in the "centreon_storage.agent_information" table.
145. **BEOTEL_CENTREON_AGENT_CHECK_COUNTER**: 
      * Given an agent with counter check, we expect to get the correct status for the centagent process running on windows host
146. **BEOTEL_CENTREON_AGENT_CHECK_DIFFERENT_INTERVAL**: 
      * Given and agent who has to execute checks with different intervals, we expect to find these intervals in data_bin
147. **BEOTEL_CENTREON_AGENT_CHECK_EVENTLOG**: 
      * Given an agent with eventlog check, we expect status, output and metrics
148. **BEOTEL_CENTREON_AGENT_CHECK_FILES**: 
      * Given an agent with file check, we expect to get the correct status for files under monitoring on the Windows host
149. **BEOTEL_CENTREON_AGENT_CHECK_HEALTH**: agent check health and we expect to get it in check result
150. **BEOTEL_CENTREON_AGENT_CHECK_HOST**: 
      * Given an agent host checked by centagent, we set a first output to check command, 
  
  * modify it, reload engine and expect the new output in resource table
151. **BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED**: agent check host with encrypted connection and we expect to get it in check result
152. **BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED_ENCRYPTED_CREDENTIALS**: 
      * Given an agent host checked by centagent over an encrypted connection,
    * Engine use credentials encryption and send encrypted commands
    * we set a first output to check command,
    * modify it, reload engine and expect the new output in resource table
153. **BEOTEL_CENTREON_AGENT_CHECK_HOST_NO_ENCRYPTED_CREDENTIALS**: 
      * Given an agent host checked by centagent over a non encrypted connection,
    * Engine use credentials encryption, but send no encrypted commands
    * we set a first output to check command,
    * modify it, reload engine and expect the new output in resource table
154. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_CPU**: agent check service with native check cpu and we expect to get it in check result
155. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_MEMORY**: agent check service with native check memory and we expect to get it in check result
156. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_SERVICE**: agent check service with native check service and we expect to get it in check result
157. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_STORAGE**: agent check service with native check storage and we expect to get it in check result
158. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_UPTIME**: agent check service with native check uptime and we expect to get it in check result
159. **BEOTEL_CENTREON_AGENT_CHECK_PROCESS**: 
      * Given an agent with eventlog check, we expect to get the correct status for thr centagent process running on windows host
160. **BEOTEL_CENTREON_AGENT_CHECK_SERVICE**: agent check service and we expect to get it in check result
161. **BEOTEL_CENTREON_AGENT_CHECK_TASKSCHEDULER**: 
      * Given an agent with task scheduler check, we expect to get the correct status for the centagent process running on windows host
162. **BEOTEL_CENTREON_AGENT_LINUX_NO_DEFUNCT_PROCESS**: agent check host and we expect to get it in check result
163. **BEOTEL_CENTREON_AGENT_NO_TRUSTED_TOKEN**: 
      * Given the Centreon Engine is configured with OpenTelemetry server with encryption enabled with no trusted_token
    * **When** the Centreon Agent attempts to connect with tls
    * **Then** the connection should be accepted
164. **BEOTEL_CENTREON_AGENT_TOKEN**: 
      * Given the Centreon Engine is configured with OpenTelemetry server with encryption enabled
    * **When** the Centreon Agent attempts to connect using an valid JWT token
    * **Then** the connection should be accepted
    * **And** the log should confirm that the token is valid
165. **BEOTEL_CENTREON_AGENT_TOKEN_AGENT_TELEGRAPH**: 
      * Given an OpenTelemetry server is configured with token-based connection
166. **BEOTEL_CENTREON_AGENT_TOKEN_AGENT_TELEGRAPH_2**: 
      * Given an OpenTelemetry server is configured with token-based connection
167. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED**: 
      * Given the OpenTelemetry server is configured with encryption enabled
168. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED_WHILE_RUNNING**: 
      * Given the OpenTelemetry server is configured with encryption enabled
169. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED_WHILE_RUNNING_REVERSE**: 
      * Given the Centreon Engine is configured as client with token and the agent as server with encryption enables
170. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRE_REVERSE**: 
      * Given the Centreon Engine is configured as client with token and the agent as server with encryption enables
    * **When** the Centreon engine attempts to connect using an valid JWT token but expired
    * **Then** the connection should be refused
    * **And** the log should confirm that the token is expired
171. **BEOTEL_CENTREON_AGENT_TOKEN_MISSING_HEADER**: 
      * Given the Centreon Engine is configured with OpenTelemetry server with encryption enabled
    * **When** the Centreon Agent attempts to connect without a JWT token
    * **Then** the connection should be refused
    * **And** the log should contain the message "UNAUTHENTICATED: No authorization header"
172. **BEOTEL_CENTREON_AGENT_TOKEN_REVERSE**: 
      * Given the Centreon Engine is configured as client with token and the agent as server with encryption enables
    * **When** the Centreon engine attempts to connect using an valid JWT token
    * **Then** the connection should be accepted
    * **And** the log should confirm that the token is valid
173. **BEOTEL_CENTREON_AGENT_TOKEN_UNTRUSTED**: 
      * Given the OpenTelemetry server is configured with encryption enabled
174. **BEOTEL_CENTREON_AGENT_TOKEN_UNTRUSTED_REVERSE**: 
      * Given the Centreon Engine is configured as client with token and the agent as server with encryption enables
    * **When** the Centreon engine attempts to connect using an invalid JWT token
    * **Then** the connection should be refused
    * **And** the log should confirm that the token is not trusted
175. **BEOTEL_CENTREON_AGENT_WHITE_LIST**: **Scenario:** Enforcing command whitelist for agent checks
    * **Given** a whitelist file is created with allowed commands for host_1
    * **And** the engine, broker, and agent are configured and started
    * **When** a check command matching the whitelist is executed for host_1
    * **Then** the check result is accepted and stored in the resources table
    * **When** a check command not matching the whitelist is configured for host_1 and engine is reloaded
    * **Then** the command is rejected and a "command not allowed by whitelist" message appears in the log
176. **BEOTEL_INVALID_CHECK_COMMANDS_AND_ARGUMENTS**: 
      * Given the agent is configured with native checks for services
    * **And** the OpenTelemetry server module is added
    * **And** services are configured with incorrect check commands and arguments
    * **When** the broker, engine, and agent are started
    * **Then** the resources table should be updated with the correct status
    * **And** appropriate error messages should be generated for invalid checks
177. **BEOTEL_REVERSE_CENTREON_AGENT_CHECK_HOST**: agent check host with reversed connection and we expect to get it in check result
178. **BEOTEL_REVERSE_CENTREON_AGENT_CHECK_HOST_CRYPTED**: agent check host with encrypted reversed connection and we expect to get it in check result
179. **BEOTEL_REVERSE_CENTREON_AGENT_CHECK_SERVICE**: agent check service with reversed connection and we expect to get it in check result
180. **BEOTEL_SERVE_TELEGRAF_CONFIGURATION_CRYPTED**: we configure engine with a telegraf conf server and we check telegraf conf file
181. **BEOTEL_SERVE_TELEGRAF_CONFIGURATION_NO_CRYPTED**: we configure engine with a telegraf conf server and we check telegraf conf file
182. **BEOTEL_SERVE_TELEGRAF_CONFIGURATION_NO_CRYPTED_1**: **Scenario:** Serve telegraf configuration with a complex whitelist
    * **Given** the engine is configured with a telegraf conf server and a complex whitelist
    * **When** I request the telegraf conf file for host_1
    * **Then** I should receive the expected telegraf configuration for host_1
    * **And** service_3 should be blacklisted and unavailable for host_1
    * **When** I request the telegraf conf file for host_2
    * **Then** I should receive the expected telegraf configuration for host_2
    * **And** service_5 should be blacklisted and unavailable for host_2
183. **BEOTEL_TELEGRAF_CHECK_HOST**: we send nagios telegraf formatted data and we expect to get it in check result
184. **BEOTEL_TELEGRAF_CHECK_SERVICE**: **Scenario:** Handling of OK and CRITICAL check results from Telegraf input
  
  * **Given** the OpenTelemetry server is ready
    * **When** I send a Telegraf-formatted check result with status "OK" to the Engine
    * **Then** the result should be stored in the Centreon Broker storage database with status "OK"
185. **BEPBBEE1**: central-module configured with bbdo_version 3.0 but not others. Unable to establish connection.
186. **BEPBBEE2**: bbdo_version 3 not compatible with sql/storage
187. **BEPBBEE3**: bbdo_version 3 generates new bbdo protobuf service status messages.
188. **BEPBBEE4**: bbdo_version 3 generates new bbdo protobuf host status messages.
189. **BEPBBEE5**: bbdo_version 3 generates new bbdo protobuf service messages.
190. **BEPBCVS**: bbdo_version 3 communication of custom variables.
191. **BEPBHostParent**: bbdo_version 3 communication of host parent relations
192. **BEPBINST_CONF**: bbdo_version 3 communication of instance configuration.
193. **BEPBRI1**: bbdo_version 3 use pb_resource new bbdo protobuf ResponsiveInstance message.
194. **BERD1**: **Scenario:** Starting/stopping Broker does not create duplicated events.
195. **BERD2**: **Scenario:** Starting/stopping Engine does not create duplicated events.
196. **BERDUC1**: **Scenario:** Starting/stopping Engine does not create duplicated events in usual cases
197. **BERDUC2**: **Scenario:** Starting/stopping Engine does not create duplicated events in usual cases
198. **BERDUC3U1**: **Scenario:** Starting/stopping Broker does not create duplicated events in usual cases with unified_sql and BBDO 3.0
199. **BERDUC3U2**: **Scenario:** Starting/stopping Engine does not create duplicated events in usual cases with unified_sql and BBDO 3.0
200. **BERDUCA300**: **Scenario:** Starting/stopping Engine is stopped; it should emit a stop event and receive an ack event with events to clean from broker.
201. **BERDUCA301**: **Scenario:** Starting/stopping Engine is stopped; it should emit a stop event and receive an ack event with events to clean from broker with bbdo 3.0.1.
202. **BERDUCU1**: Starting/stopping Broker does not create duplicated events in usual cases with unified_sql7
203. **BERDUCU2**: **Scenario:** Starting/stopping Engine does not create duplicated events in usual cases with unified_sql
204. **BERES1**: store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
205. **BESERVCHECK**: external command CHECK_SERVICE_RESULT
206. **BESS1**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
207. **BESS2**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
208. **BESS2U**: Start-Stop Broker/Engine - Broker started first - Engine stopped first. Unified_sql is used.
209. **BESS3**: Start-Stop Broker/Engine - Engine started first - Engine stopped first
210. **BESS4**: Start-Stop Broker/Engine - Engine started first - Broker stopped first
211. **BESS5**: Start-Stop Broker/engine - Engine debug level is set to all, it should not hang
212. **BESS6_${label}**: **Scenario:** Verify Broker and Engine start and establish connections
  
  * **Given** the Central Broker, RRD Broker, and Central Engine are started
    * **When** we check the connection between them
    * **Then** the connection should be well established
    * **And** the central broker should have two peers connected: the central engine and the RRD broker
    * **And** the RRD broker should correctly recognize its peer as the Central Broker
213. **BESSBQ1**: A very bad queue file is written for broker. Broker and Engine are then started, Broker must read the file raising an error because of that file and then get data sent by Engine.
214. **BESSCTO**: **Scenario:** Service commands time out due to missing Perl Connector
    * **Given** the Engine is configured as usual but without the Perl Connector
    * **When** the Engine executes its service commands
    * **Then** the commands take too long and reach the timeout
    * **And** the Engine starts and stops two times as a result
215. **BESSCTOWC**: **Scenario:** Service commands time out due to missing Perl Connector
    * **Given** the Engine is configured as usual with some commands using the Perl Connector
    * **When** the Engine executes its service commands
    * **Then** the commands take too long and reach the timeout
    * **And** the Engine starts and stops two times as a result
216. **BESSG**: **Scenario:** Broker handles connection and disconnection with Engine
  
  * **Given** Broker is configured with only one output that is Graphite
    * **When** the Engine starts and connects to the Broker
    * **Then** the Broker must be able to handle the connection
    * **When** the Engine stops
    * **Then** the Broker must be able to handle the disconnection
217. **BESS_CRYPTED_GRPC1**: Start-Stop grpc version Broker/Engine - well configured
218. **BESS_CRYPTED_GRPC2**: Start-Stop grpc version Broker/Engine only server crypted
219. **BESS_CRYPTED_GRPC3**: Start-Stop grpc version Broker/Engine only engine crypted
220. **BESS_CRYPTED_REVERSED_GRPC1**: Start-Stop grpc version Broker/Engine - well configured
221. **BESS_CRYPTED_REVERSED_GRPC2**: Start-Stop grpc version Broker/Engine only engine server crypted
222. **BESS_CRYPTED_REVERSED_GRPC3**: Start-Stop grpc version Broker/Engine only engine crypted
223. **BESS_ENGINE_DELETE_HOST**: once engine and cbd started, stop and restart cbd, delete an host and reload engine, cbd mustn't core
224. **BESS_GRPC1**: Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped first
225. **BESS_GRPC2**: Start-Stop grpc version Broker/Engine - Broker started first - Engine stopped first
226. **BESS_GRPC3**: Start-Stop grpc version Broker/Engine - Engine started first - Engine stopped first
227. **BESS_GRPC4**: Start-Stop grpc version Broker/Engine - Engine started first - Broker stopped first
228. **BESS_GRPC5**: Start-Stop grpc version Broker/engine - Engine debug level is set to all, it should not hang
229. **BESS_GRPC_COMPRESS1**: Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped last compression activated
230. **BETAG1**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Broker is started before.
231. **BETAG2**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
232. **BEUS**: **Scenario:** Verify resources table after initialization
  
  * **Given** the database is cleaned
    * **And** the Engine is started with a default configuration
    * **And** the Broker is started with a default configuration
    * **Then** the "resources" table contains exactly 1050 rows
    * **And** the "resources" table contains the good hosts and services
    * **And** the "hosts" table contains the good hosts
    * **And** the "services" table contains the good services
    * **And** the "customvariables" table contains the good customvariables
    * **Then** Engine and Broker are stopped
    * **And** we check again these contents
233. **BEUTAG1**: Engine is configured with some tags. When broker receives them through unified_sql stream, it stores them in the centreon_storage.tags table. Broker is started before.
234. **BEUTAG10**: some services are configured with tags on two pollers. Then tags are removed from some of them and in centreon_storage, we can observe resources_tags table updated.
235. **BEUTAG11**: **Scenario:** Updating resource tags after changing several tags
  
  * **Given** some services are configured with tags on two pollers
    * **Then** the resources_tags table contains them
    * **When** several tags are changed
    * **Then** the resources_tags table is updated
236. **BEUTAG12**: Engine is configured with some tags. Group tags tag2, tag6 are set to hosts 1 and 2. Category tags tag4 and tag8 are added to hosts 2, 3, 4. The resources and resources_tags tables are well filled. The tag6 and tag8 are removed and resources_tags is also well updated.
237. **BEUTAG2**: Engine is configured with some tags. A new service is added with a tag. Broker should make the relations.
238. **BEUTAG3**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
239. **BEUTAG4**: Engine is configured with some tags. Group tags tag9, tag13 are set to services 1 and 3. Category tags tag3 and tag11 are added to services 1, 3, 5 and 6. The centreon_storage.resources and resources_tags tables are well filled.
240. **BEUTAG5**: Engine is configured with some tags. Group tags tag2, tag6 are set to hosts 1 and 2. Category tags tag4 and tag8 are added to hosts 2, 3, 4. The resources and resources_tags tables are well filled.
241. **BEUTAG6**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.resources_tags table. Engine is started before.
242. **BEUTAG7**: Some services are configured with tags on two pollers. Then tags configuration is modified.
243. **BEUTAG8**: Services have tags provided by templates.
244. **BEUTAG9**: hosts have tags provided by templates.
245. **BEUTAG_REMOVE_HOST_FROM_HOSTGROUP**: remove a host from hostgroup, reload, insert 2 host in the hostgroup must not make sql error
246. **BE_BACKSLASH_CHECK_RESULT**: external command PROCESS_SERVICE_CHECK_RESULT with \:
247. **BE_DEFAULT_NOTIFICATION_INTERVAL_IS_ZERO_SERVICE_RESOURCE**: default notification_interval must be set to NULL in services, hosts and resources tables.
248. **BE_FLAPPING_HOST_RESOURCE**: With BBDO 3, flapping detection must be set in hosts and resources tables.
249. **BE_FLAPPING_SERVICE_RESOURCE**: With BBDO 3, flapping detection must be set in services and resources tables.
250. **BE_NOTIF_OVERFLOW**: bbdo 2.0 notification number =40000. make an overflow => notification_number null in db
251. **BE_TIME_NULL_SERVICE_RESOURCE**: With BBDO 3, notification_interval time must be set to NULL on 0 in services, hosts and resources tables.
252. **BRCS1**: Broker reverse connection stopped
253. **BRCTS1**: Broker reverse connection too slow
254. **BRCTSMN**: 
      * Given Broker, Engine configured as usual
  
  * **And** map also connected to Broker with a filter allowing only 'neb' category
    * **When** Engine sends pb_service, pb_host, pb_service_status and pb_host_status
    * **Then** map receives correctly them.
255. **BRCTSMNS**: 
      * Given Broker, Engine configured as usual
    * **And** map also connected to Broker with a filter allowing 'neb' and 'storage' categories
    * **When** Engine sends pb_service, pb_host, pb_service_status, pb_host_status and metrics
    * **Then** Map receives correctly them.
256. **BRGC1**: Broker good reverse connection
257. **BRRDCDDID1**: RRD metrics deletion from index ids with rrdcached.
258. **BRRDCDDIDDB1**: RRD metrics deletion from index ids with a query in centreon_storage with rrdcached.
259. **BRRDCDDIDU1**: RRD metrics deletion from index ids with unified sql output with rrdcached.
260. **BRRDCDDM1**: RRD metrics deletion from metric ids with rrdcached.
261. **BRRDCDDMDB1**: RRD metrics deletion from metric ids with a query in centreon_storage and rrdcached.
262. **BRRDCDDMID1**: RRD deletion of non existing metrics and indexes with rrdcached
263. **BRRDCDDMIDU1**: RRD deletion of non existing metrics and indexes with rrdcached
264. **BRRDCDDMU1**: RRD metric deletion on table metric with unified sql output with rrdcached
265. **BRRDCDRB1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with storage/sql sql output and rrdcached.
266. **BRRDCDRBDB1**: RRD metric rebuild with a query in centreon_storage and unified sql with rrdcached
267. **BRRDCDRBU1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with unified_sql output and rrdcached.
268. **BRRDCDRBUDB1**: RRD metric rebuild with a query in centreon_storage and unified sql with rrdcached
269. **BRRDDID1**: RRD metrics deletion from index ids.
270. **BRRDDIDDB1**: RRD metrics deletion from index ids with a query in centreon_storage.
271. **BRRDDIDU1**: RRD metrics deletion from index ids with unified sql output.
272. **BRRDDM1**: RRD metrics deletion from metric ids.
273. **BRRDDMDB1**: RRD metrics deletion from metric ids with a query in centreon_storage.
274. **BRRDDMID1**: RRD deletion of non existing metrics and indexes
275. **BRRDDMIDU1**: RRD deletion of non existing metrics and indexes
276. **BRRDDMU1**: RRD metric deletion on table metric with unified sql output
277. **BRRDRBDB1**: RRD metric rebuild with a query in centreon_storage and unified sql
278. **BRRDRBUDB1**: RRD metric rebuild with a query in centreon_storage and unified sql
279. **BRRDRM1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with storage/sql sql output.
280. **BRRDRMU1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with unified_sql output.
281. **BRRDSTATUS**: We are working with BBDO3. This test checks status are correctly handled independently from their value.
282. **BRRDSTATUSRETENTION**: We are working with BBDO3. This test checks status are not sent twice after Engine reload.
283. **BRRDUPLICATE**: RRD metric rebuild with a query in centreon_storage and unified sql with duplicate rows in database
284. **BRRDWM1**: We are working with BBDO3. This test checks protobuf metrics and status are sent to cbd RRD.
285. **CBD_RELOAD_AND_FILTERS**: We start engine/broker with a classical configuration. All is up and running. Some filters are added to the rrd output and cbd is reloaded. All is still up and running but some events are rejected. Then all is newly set as filter and all events are sent to rrd broker.
286. **CBD_RELOAD_AND_FILTERS_WITH_OPR**: We start engine/broker with an almost classical configuration, just the connection between cbd central and cbd rrd is reversed with one peer retention. All is up and running. Some filters are added to the rrd output and cbd is reloaded. All is still up and running but some events are rejected. Then all is newly set as filter and all events are sent to rrd broker.
287. **Centralized_Start_Stop_Broker_Engine_${id}**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
288. **Centralized_Start_Stop_Engine_Broker_${id}**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
289. **DTIM**: New services with several pollers are created. Then downtimes are set on all configured hosts. This action results on 5250 downtimes if we also count impacted services. Then all these downtimes are removed. This test is done with BBDO 3.0.1
290. **EBBM1**: A service status contains metrics that do not fit in a float number.
291. **EBBPS1**: 1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table resources.
292. **EBBPS2**: 1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table services.
293. **EBDP1**: Four new pollers are started and then we remove Poller3.
294. **EBDP2**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
295. **EBDP3**: Three new pollers are started, then they are killed. It is still possible to remove Poller2 if removed from the configuration.
296. **EBDP4**: Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by Broker.
297. **EBDP5**: Four new pollers are started and then we remove Poller3.
298. **EBDP6**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
299. **EBDP7**: Three new pollers are started, then they are killed. It is still possible to remove Poller2 if removed from the configuration.
300. **EBDP8**: Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by broker.
301. **EBDP_GRPC2**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
302. **EBMSSM**: 1000 services are configured with 100 metrics each. The rrd output is removed from the broker configuration. GetSqlManagerStats is called to measure writes into data_bin.
303. **EBMSSMDBD**: 1000 services are configured with 100 metrics each. The rrd output is removed from the broker configuration. While metrics are written in the database, we stop the database and then restart it. Broker must recover its connection to the database and continue to write metrics.
304. **EBMSSMPART**: **Scenario:** Broker continues writing metrics after partition recreation
  
  * **Given** 1000 services are configured with 100 metrics each
    * **And** the rrd output is removed from the broker configuration
    * **And** the data_bin table is configured with two partitions "p1" and "p2"
    * **And** "p1" contains old data
    * **And** "p2" contains current data
    * **When** metrics are being written in the database
    * **And** the "p2" partition is removed
    * **And** the "p2" partition is recreated
    * **Then** the broker must recover its connection to the database
    * **And** it must continue writing metrics
    * **When** a last service check is forced
    * **Then** its metrics must be written in the database
305. **EBNSG1**: New service group with several pollers and connections to DB
306. **EBNSGU1**: New service group with several pollers and connections to DB with broker configured with unified_sql
307. **EBNSGU2**: New service group with several pollers and connections to DB with broker configured with unified_sql
308. **EBNSGU3_${test_label}**: New service group with several pollers and connections to DB with broker and rename this servicegroup
309. **EBNSVC1**: New services with several pollers
310. **EBPN0**: Verify if child is in queue when parent is down.
311. **EBPN1**: verify relation parent child when delete parent.
312. **EBPN2**: verify relation parent child when delete child.
313. **EBPS2**: 1000 services are configured with 20 metrics each. The rrd output is removed from the broker configuration to avoid to write too many rrd files. While metrics are written in bulk, the database is stopped. This must not crash broker.
314. **EBSAU2**: New hosts with action_url with more than 2000 characters
315. **EBSN3**: New hosts with notes with more than 500 characters
316. **EBSN4**: New hosts with No Alias / Alias and have A Template
317. **EBSNU1**: New hosts with notes_url with more than 2000 characters
318. **ENRSCHE1**: Verify that next check of a rescheduled host is made at last_check + interval_check
319. **FILTER_ON_LUA_EVENT**: stream connector with a bad configured filter generate a log error message
320. **GRPC_CLOUD_FAILURE**: simulate a broker failure in cloud environment, we provide a muted grpc server and there must remain only one grpc connection. Then we start broker and connection must be ok
321. **GRPC_RECONNECT**: We restart broker and engine must reconnect to it and send data
322. **LCDNU**: the lua cache updates correctly service cache.
323. **LCDNUH**: the lua cache updates correctly host cache
324. **LOGV2DB1**: log-v2 disabled old log enabled check broker sink
325. **LOGV2DB2**: log-v2 disabled old log disabled check broker sink
326. **LOGV2DF1**: log-v2 disabled old log enabled check logfile sink
327. **LOGV2DF2**: log-v2 disabled old log disabled check logfile sink
328. **LOGV2EB1**: Checking broker sink when log-v2 is enabled and legacy logs are disabled.
329. **LOGV2EB2**: log-v2 enabled old log enabled check broker sink
330. **LOGV2EBU1**: Checking broker sink when log-v2 is enabled and legacy logs are disabled with bbdo3.
331. **LOGV2EBU2**: Check Broker sink with log-v2 enabled and legacy log enabled with BBDO3.
332. **LOGV2EF1**: log-v2 enabled    old log disabled check logfile sink
333. **LOGV2EF2**: log-v2 enabled old log enabled check logfile sink
334. **LOGV2FE2**: log-v2 enabled old log enabled check logfile sink
335. **LUA_CACHE_SAVE_BBDO3**: 
      * Given a engine broker configured in bbdo2, we check that services and hosts are stored in bbdo3 format in cache
  
  * To do that we compare host and service event with lua cache
336. **MOVE_HOST_OF_HOSTGROUP_TO_ANOTHER_POLLER**: **Scenario:** Moving hosts between pollers without losing hostgroup tag
    * **Given** two pollers each with two hosts
    * **And** all hosts belong to the same hostgroup
    * **When** I move two hosts from one poller to the other
    * **Then** the hostgroup tag of the moved hosts is not erased
337. **NON_TLS_CONNECTION_WARNING**: 
      * Given an agent starts a non-TLS connection,
  
  * we expect to get a warning message.
338. **NON_TLS_CONNECTION_WARNING_ENCRYPTED**: 
      * Given agent with encrypted connection, we expect no warning message.
339. **NON_TLS_CONNECTION_WARNING_FULL**: 
      * Given an agent starts a non-TLS connection,
    * we expect to get a warning message.
    * After 1 hour, we expect to get a warning message about the connection time expired
    * and the connection killed.
340. **NON_TLS_CONNECTION_WARNING_FULL_REVERSED**: 
      * Given an agent starts a non-TLS connection reverse,
    * we expect to get a warning message.
    * After 1 hour, we expect to get a warning message about the connection time expired
    * and the connection killed.
341. **NON_TLS_CONNECTION_WARNING_REVERSED**: 
      * Given an agent starts a non-TLS connection reversed,
    * we expect to get a warning message.
342. **NON_TLS_CONNECTION_WARNING_REVERSED_ENCRYPTED**: 
      * Given agent with encrypted reversed connection, we expect no warning message.
343. **NO_FILTER_NO_ERROR**: no filter configured => no filter error.
344. **RENAME_PARENT**: 
      * Given an host with a parent host. We rename the parent host and check if the child host is still linked to the parent.
  
  * Engine mustn't crash and log an error on reload.
345. **RLCode**: Test if reloading LUA code in a stream connector applies the changes
346. **RRD1**: RRD metric rebuild asked with gRPC API. Three non existing indexes IDs are selected then an error message is sent. This is done with unified_sql output.
347. **SDER**: The check attempts and the max check attempts of (host_1,service_1) are changed to 280 thanks to the retention.dat file. Then Engine and Broker are started and Broker should write these values in the services and resources tables. We only test the services table because we need a resources table that allows bigger numbers for these two attributes. But we see that Broker doesn't crash anymore.
348. **SEVERAL_FILTERS_ON_LUA_EVENT**: Two stream connectors with different filters are configured.
349. **STORAGE_ON_LUA**: The category 'storage' is applied on the stream connector. Only events of this category should be sent to this stream.
350. **STUPID_FILTER**: Unified SQL is configured with only the bbdo category as filter. An error is raised by broker and broker should run correctly.
351. **Service_increased_huge_check_interval**: New services with huge check interval at creation time. Given Engine and Broker are configured with 1 poller and 10 hosts When Engine is started Then host_1 should be pending When a check result with metrics is processed for service_1 Then metrics should be created and sent to rrd broker When service_1 metrics are analyzed Then metrics should have minimal heartbeat of 3000 and pdp_per_row of 300 When a new service is created with a check interval of 90 And Engine is reloaded Then the new service should be pending When a check result with metrics is processed for the new service Then metrics should be created and sent to rrd Broker When new service metrics are analyzed Then metrics should have minimal heartbeat of 54000 and pdp_per_row of 5400
352. **Services_and_bulks_${id}**: One service is configured with one metric with a name of 150 to 1021 characters.
353. **Start_Stop_Broker_Engine_${id}**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
354. **Start_Stop_Engine_Broker_${id}**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
355. **UNIFIED_SQL_FILTER**: With bbdo version 3.0.1, we watch events written or rejected in unified_sql
356. **VICT_ONE_CHECK_METRIC**: victoria metrics metric output
357. **VICT_ONE_CHECK_METRIC_AFTER_FAILURE**: victoria metrics metric output after victoria shutdown
358. **VICT_ONE_CHECK_STATUS**: victoria metrics status output
359. **Whitelist_Directory_NotReadable**: 
      * Given a centengine started by centreon-engine user, whitelist directories are not readable and centengine must log an error
360. **Whitelist_Directory_Rights**: log if /etc/centreon-engine-whitelist has not mandatory rights or owner
361. **Whitelist_Empty_Directory**: log if /etc/centreon-engine-whitelist is empty
362. **Whitelist_Host**: Test on allowed and forbidden commands for hosts
363. **Whitelist_No_Whitelist_Directory**: log if /etc/centreon-engine-whitelist doesn't exist
364. **Whitelist_NotReadable**: 
      * Given a centengine started by centreon-engine user, whitelist files are not readable and centengine must log an error
365. **Whitelist_Perl_Connector**: test allowed and forbidden commands for services
366. **Whitelist_Service**: test allowed and forbidden commands for services
367. **Whitelist_Service_EH**: test allowed and forbidden event handler for services
368. **metric_mapping**: Check if metric name exists using a stream connector
369. **not1**: This test case configures a single service and verifies that a notification is sent when the service is in a non-OK HARD state.
370. **not10**: This test case involves scheduling downtime on a down host that already had a critical notification. When The Host return to UP state we should receive a recovery notification.
371. **not11**: This test case involves configuring one service and checking that three alerts are sent for it.
372. **not12**: Escalations
373. **not13**: notification for a dependencies host
374. **not14**: notification for a Service dependency
375. **not15**: several notification commands for the same user.
376. **not16**: notification for dependencies services group
377. **not17**: notification for a dependensies host group
378. **not18**: notification delay where first notification delay equal retry check
379. **not19**: notification delay where first notification delay greater than retry check 
380. **not1_WL_KO**: This test case configures a single service. When it is in non-OK HARD state a notification should be sent but it is not allowed by the whitelist
381. **not1_WL_OK**: This test case configures a single service. When it is in non-OK HARD state a notification is sent because it is allowed by the whitelist
382. **not1_reload**: This test case configures a single service and set the service in a non-OK HARD state so engine sends a notification. Then the service is removed from the configuration and Engine is reloaded. And Engine doesn't crash.
383. **not2**: This test case configures a single service and verifies that a recovery notification is sent
384. **not20**: notification delay where first notification delay samller than retry check
385. **not3**: This test case configures a single service and verifies the notification system's behavior during and after downtime
386. **not4**: This test case configures a single service and verifies the notification system's behavior during and after acknowledgement
387. **not5**: This test case configures two services with two different users being notified when the services transition to a critical state.
388. **not6**: This test case validate the behavior when the notification time period is set to null.
389. **not7**: This test case simulates a host alert scenario.
390. **not8**: This test validates the critical host notification.
391. **not9**: This test case configures a single host and verifies that a recovery notification is sent after the host recovers from a non-OK state.
392. **not_in_timeperiod_with_send_recovery_notifications_anyways**: **Scenario:** Verify notification is sent when service is in non-OK state and recovery is sent outside timeperiod if setting is enabled
    * **Given** a configured single service
    * **And** the service enters a non-OK state
    * **When** the service remains in a non-OK state
    * **Then** a notification should be sent
    * **And** an OK notification should be sent outside the time period
    * **When** the setting "_send_recovery_notifications_anyways" is set
393. **not_in_timeperiod_without_send_recovery_notifications_anyways**: **Scenario:** Verify notification is sent when service is in non-OK state and recovery is not sent outside timeperiod
  
  * **Given** a configured single service
    * **And** the service enters a non-OK state
    * **When** the service remains in a non-OK state
    * **Then** a notification should be sent
    * **And** no OK notification should be sent outside the time period
    * **When** the setting "send_recovery_notifications_anyways" is not set

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
1. **CONPERL**: The test.pl script is launched using the perl connector. Then we should find its execution in the engine log file.
2. **CONPERLM**: Ten forced checks are scheduled on ten hosts configured with the Perl Connector. The we get the result of each of them.

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
5. **ECI0**: Verify contact inheritance : contact(empty) inherit from template (full), on Start Engine
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
43. **ENGINE_ENCRYPTION_BAD_CONF**: 
      * Given an engine with configured encryption, but without key and salt,
    * **When** we provide the service macros CLEAR_MAC, RAW_MAC and ENCRYPT_MAC
    * **Then** we should see these macros in the logs without decryption.
44. **ENGINE_ENCRYPTION_GOOD_CONF**: 
      * Given an engine with configured encryption, with also key and salt,
    * **When** we provide the service macros CLEAR_MAC, RAW_MAC and ENCRYPT_MAC
    * **Then** we should see these macros in logs but with encrypted values hidden.
45. **ENGINE_MANY_CHECKS**: 
      * Given a engine with many services and a unique check on each service with it's own env variables
  
     * We expect correct check result in logs and we checks returned args and service macros
46. **EPC1**: Check with perl connector
47. **ERL**: Engine is started and writes logs in centengine.log. Then we remove the log file. The file disappears but Engine is still writing into it. Engine is reloaded and the centengine.log should appear again.
48. **ESGI0**: Verify servicegroup inheritance : servicegroup(empty) inherit from template (full) , on Start Engine
49. **ESGI1**: Verify servicegroup inheritance : servicegroup(empty) inherit from template (full) , on Start Engine
50. **ESGI2**: Verify servicegroup inheritance : servicegroup(empty) inherit from template (full) , on Reload Engine
51. **ESGI3**: Verify servicegroup inheritance : servicegroup(empty) inherit from template (full) , on Reload Engine
52. **ESI0**: Verify inheritance service : Service(empty) inherit from template (full) , on Start Engine
53. **ESI1**: Verify inheritance service : Service(full) inherit from template (full) , on Start Engine
54. **ESI2**: Verify inheritance service : Service(empty) inherit from template (full) , on Reload Engine
55. **ESI3**: Verify inheritance service : Service(full) inherit from template (full) , on Reload Engine
56. **ESS1**: Start-Stop (0s between start/stop) 5 times one instance of engine and no coredump
57. **ESS2**: Start-Stop (300ms between start/stop) 5 times one instance of engine and no coredump
58. **ESS3**: Start-Stop (0s between start/stop) 5 times three instances of engine and no coredump
59. **ESS4**: Start-Stop (300ms between start/stop) 5 times three instances of engine and no coredump
60. **ESSCTO**: **Scenario:** Engine services timeout due to missing Perl connector
  
  * **Given** the Engine is configured as usual without the Perl connector
    * **When** the Engine executes its service commands
    * **Then** the commands take too long and reach the timeout
    * **And** the Engine starts and stops two times as a result
61. **ESSCTOWC**: **Scenario:** Engine services timeout due to missing Perl connector
    * **Given** the Engine is configured as usual with some command using the Perl connector
    * **When** the Engine executes its service commands
    * **Then** the commands take too long and reach the timeout
    * **And** the Engine starts and stops two times as a result
62. **ESSOCWNV**: **Scenario:** Engine is started with a valid old configuration (concerning cbmod)
    * **Given** the Engine is configured with a valid old configuration
    * **When** the Engine is started
    * **Then** the Engine starts correctly
    * **And** the Engine stops correctly
63. **ESS_STATS**: **Scenario:** Reading the stats file after Engine has started
    * **Given** the Engine is started
    * **When** we read the Engine's stats file
    * **Then** Engine must not crash
64. **EVOCWNV**: **Scenario:** The new Engine checks the old configuration (concerning cbmod)
  
  * **Given** the Engine is configured with a valid old configuration
    * **When** the Engine is started to check the configuration
    * **Then** the Engine reads it as expected
65. **EXT_CONF1**: Engine configuration is overidden by json conf
66. **EXT_CONF2**: Engine configuration is overidden by json conf after reload
67. **E_FD_LIMIT**: Engine here is started with a low file descriptor limit. The engine should not crash and limit should be set.
68. **E_HOST_DOWN_DISABLE_SERVICE_CHECKS**: host_down_disable_service_checks is set to 1, host down switch all services to UNKNOWN
69. **E_HOST_UNREACHABLE_DISABLE_SERVICE_CHECKS**: host_down_disable_service_checks is set to 1, host unreachable switch all services to UNKNOWN
70. **NO_ENGINE_ENCRYPTION**: 
      * Given an engine without configured encryption
  
  * **When** we provide the service macros CLEAR_MAC, RAW_MAC and ENCRYPT_MAC
    * **Then** we should see these macros in the logs
71. **VERIF**: 
      * When centengine is started in verification mode, it does not log in its file.
72. **VERIFY_CONF**: Scenario Verify deprecated engine configuration options are logged as warnings Given the engine and broker are configured with module 1 And the engine configuration is set with deprecated options When the engine is started Then a warning message for 'auto_reschedule_checks' should be logged And a warning message for 'auto_rescheduling_interval' should be logged And a warning message for 'auto_rescheduling_window' should be logged And the engine should be stopped

### Migration
1. **MIGRATION**: Migration bbdo2 with sql/storage to bbdo2 with unified_sql and then to bbdo3 with unified_sql and then to bbdo2 with unified_sql and then to bbdo2 with sql/storage

### Severities
1. **BECSEV1**: **Feature:** Severity Management between Engine and Broker
  
  * As a Centreon administrator
    * I want to configure severities in Engine
    * So that Broker stores them correctly in centreon_storage.severities table
    * Background:
  
   * **Given** Engine is configured with centralized setup
     * **And** Broker components (central, rrd, module) are configured
     * **And** Database logging is enabled with debug/trace level
     * **And** Retention data is cleared
  
  * **Scenario:** Initial severity configuration
  
   * **Given** Engine is configured with 20 severities
     * **When** Broker and Engine are started
     * **Then** 20 severities should be added/modified in logs
     * **And** INSERT statements should be executed in severities table
     * **And** Configuration file should match database content
     * **And** Severity IDs should be consistent
  
  * **Scenario:** Severity configuration modification
  
   * **Given** Initial configuration with 20 severities is loaded
     * **When** Configuration is modified to 30 severities
     * **And** Engine configuration change is notified
     * **Then** 10 additional severities should be added/modified
     * **And** Configuration file should still match database content
     * **And** Severity IDs should remain consistent
  
  * **Scenario:** Severity configuration reduction
  
   * **Given** Configuration with 30 severities is loaded
     * **When** Configuration is reduced to 11 severities starting at ID 50
     * **And** Engine configuration change is notified
     * **Then** 11 severities should be present in final configuration
     * **And** Unused severities should be implicitly removed
     * **And** Configuration file should match database content
     * **And** Severity IDs should be consistent with new range
2. **BESEV1**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Broker is started before.
3. **BESEV2**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Engine is started before.
4. **BETUHSEV1**: Hosts have severities provided by templates.
5. **BETUSEV1**: Services have severities provided by templates.
6. **BEUHSEV1**: Four hosts have a severity added. Then we remove the severity from host 1. Then we change severity 10 to severity8 for host 3.
7. **BEUHSEV2**: Seven hosts are configured with a severity on two pollers. Then we remove severities from the first and second hosts of the first poller but only the severity from the first host of the second poller.
8. **BEUSEV1**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Broker is started before.
9. **BEUSEV2**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Engine is started before.
10. **BEUSEV3**: Four services have a severity added. Then we remove the severity from service 1. Then we change severity 11 to severity7 for service 3.
11. **BEUSEV4**: Seven services are configured with a severity on two pollers. Then we remove severities from the first and second services of the first poller but only the severity from the first service of the second poller. Then only severities no more used should be removed from the database.

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


637 tests currently implemented.
