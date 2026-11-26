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
10. **BAM_CIRCULAR**: 
      * Given a bad bam configuration with circular ba dependency.
      * Circular Ba comment must be updated with an error message
11. **BAM_CORRUPTED_REPORTING_BA_EVENTS**: 
      * Given broker with bam configured
      * we inject a bad mod_bam_reporting_ba_events record.
      * A ba status changed should set end_time of bad record
12. **BAM_RELOAD_ON_CBD_RELOAD**: 
      * Given broker with bam configured
      * we should find bam restart after broker reload
13. **BAWORST**: With bbdo version 3.0.1, a BA of type 'worst' with two services is configured. We also check stats output
14. **BAWORST2**: a worst ba with a boolean kpi and a ba kpi
15. **BAWORST_ACK**: Scenario: Acknowledging a service acknowledges the BA, and removing it unacknowledges the BA
      * Given BBDO version is 3.0.1
      * And a Business Activity of type "worst" is configured with two services
      * When one of the services is acknowledged
      * Then the Business Activity is acknowledged
      * When the acknowledgement is removed from the service
      * Then the Business Activity is no longer acknowledged
      * We also check that we have no bam filter error
16. **BA_BOOL_KPI**: With bbdo version 3.0.1, a BA of type 'worst' with 1 boolean kpi
17. **BA_CHANGED**: A BA of type worst is configured with one service kpi. Then it is modified so that the service kpi is replaced by a boolean rule kpi. When cbd is reloaded, the BA is well updated.
18. **BA_DISABLED**: create a disabled BA with timeperiods and reporting filter don't create error message
19. **BA_IMPACT_2KPI_SERVICES**: With bbdo version 3.0.1, a BA of type 'impact' with 2 serv, ba is critical only if the 2 services are critical
20. **BA_IMPACT_IMPACT**: 
      * Given a Business Activity (BA) of type "impact"
      * And it has two child BAs of type "impact"
      * And the first child has an impact of 90
      * And the second child has an impact of 10
      * When both child BAs are impacting
      * Then the parent BA should be "critical"
      * When both child BAs are not impacting
      * Then the parent BA should be "ok"
21. **BA_RATIO_NUMBER_BA_4_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
22. **BA_RATIO_NUMBER_BA_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 2 services and one ba with 1 service
23. **BA_RATIO_PERCENT_BA_4_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
24. **BA_RATIO_PERCENT_BA_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio percent' with 2 serv an 1 ba with one service
25. **BA_SERVICE_PNAME_AFTER_RELOAD**: Scenario: Verify that the parent_name of a BA service is not erased after a broker reload
      * Given a BA "test" of type "worst" with its service "host_16:service_302"
      * When I start broker and engine
      * Then the BA service "test" should have a status of 0 within 30 seconds
      * When I reload the broker
      * Then the database should still contain a BA service with name "test" and parent_name "_Module_BAM_1"
26. **BEBAMIDT1**: 
      * Given a BA of type 'worst' with one service is configured
      * And The BA is in critical state due to its service
      * When a downtime is set on this service
      * Then an inherited downtime is set to the BA
      * When the downtime is removed from the service
      * Then the inherited downtime is deleted from the BA
27. **BEBAMIDT2**: 
      * Given a BA of type 'worst' with one service is configured
      * And the BA is in critical state due to its service
      * And a downtime is set on this service
      * Then an inherited downtime is set to the BA
      * When Engine is restarted
      * And Broker is restarted
      * Then both downtimes are still present with no duplicates
      * When the downtime is removed from the service
      * Then the inherited downtime is deleted
28. **BEBAMIDTU1**: 
      * Given BBDO version 3.0.1 is running
      * And a BA of type 'worst' with one service is configured
      * And The BA is in critical state due to its service
      * When a downtime is set on this service
      * Then an inherited downtime is set to the BA
      * When the downtime is removed from the service
      * Then the inherited downtime is deleted from the BA
29. **BEBAMIDTU2**: 
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
30. **BEBAMIDTU3**: 
      * Given BBDO version 3.0.1 is running
      * And the central Broker and the Engine Broker module have the poller_id 1 and the uid 1001
      * And the BAM virtual host of the Engine configuration is named '_Module_BAM_1'
      * And a BA of type 'worst' with one service is in critical state due to its service
      * When a downtime is set on this service
      * Then BAM sends the inherited downtime to '_Module_BAM_1001' and Engine sets it on the BA
      * When the downtime is removed from the service
      * Then BAM deletes the inherited downtime on '_Module_BAM_1001' and Engine removes it from the BA
31. **BEBAMIGNDT1**: A BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. A first downtime is cancelled, the BA is still OK, but when the second downtime is cancelled, the BA should be CRITICAL.
32. **BEBAMIGNDT2**: A BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. The first downtime reaches its end, the BA is still OK, but when the second downtime reaches its end, the BA should be CRITICAL.
33. **BEBAMIGNDTU1**: With bbdo version 3.0.1, a BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. A first downtime is cancelled, the BA is still OK, but when the second downtime is cancelled, the BA should be CRITICAL.
34. **BEBAMIGNDTU2**: 
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
35. **BEPB_BA_DURATION_EVENT**: use of pb_ba_duration_event message.
36. **BEPB_DIMENSION_BA_BV_RELATION_EVENT**: bbdo_version 3 use pb_dimension_ba_bv_relation_event message.
37. **BEPB_DIMENSION_BA_EVENT**: bbdo_version 3 use pb_dimension_ba_event message.
38. **BEPB_DIMENSION_BA_TIMEPERIOD_RELATION**: use of pb_dimension_ba_timeperiod_relation message.
39. **BEPB_DIMENSION_BV_EVENT**: bbdo_version 3 use pb_dimension_bv_event message.
40. **BEPB_DIMENSION_KPI_EVENT**: bbdo_version 3 use pb_dimension_kpi_event message.
41. **BEPB_DIMENSION_TIMEPERIOD**: use of pb_dimension_timeperiod message.
42. **BEPB_DIMENSION_TRUNCATE_TABLE**: use of pb_dimension_timeperiod message.
43. **BEPB_KPI_STATUS**: bbdo_version 3 use kpi_status message.

### Broker
1. **BBDO_300_3100**: 
      * Given a central configured in bbdo 3.0.0 and a rrd cbd configured in 3.1.0, rrd cbd should operate in bbdo 3.0.0
      * Then we wait event of type 131074 (bbdo2 ack).
2. **BC1**: Central and RRD brokers are started. Then we check they are correctly connected. RRD broker is stopped. The connection is lost. Then RRD broker is started again. The connection is re-established. Central broker is stopped. The connection is lost. Then Central broker is started again. The connection is re-established.
3. **BCL1**: Starting broker with option '-s foobar' should return an error
4. **BCL2**: Starting broker with option '-s5' should work
5. **BCL3**: Starting broker with options '-D' should work and activate diagnose mode
6. **BCL4**: Starting broker with options '-s2' and '-D' should work.
7. **BDB1**: Access denied when database name exists but is not the good one for sql output
8. **BDB10**: connection should be established when user password is good for sql/perfdata
9. **BDB2**: Access denied when database name exists but is not the good one for storage output
10. **BDB3**: Access denied when database name does not exist for sql output
11. **BDB4**: Access denied when database name does not exist for storage and sql outputs
12. **BDB5**: cbd does not crash if the storage/sql db_host is wrong
13. **BDB6**: cbd does not crash if the sql db_host is wrong
14. **BDB7**: access denied when database user password is wrong for perfdata/sql
15. **BDB8**: access denied when database user password is wrong for perfdata/sql
16. **BDB9**: access denied when database user password is wrong for sql
17. **BDBM1**: 
      * Given the broker and engine are configured and started
      * When MariaDB is restarted
      * Then the connection between the broker/engine and MariaDB should be successfully established and 
      * number of connections in the stats file should be equal to the configured connections_count
18. **BDBMU1**: start broker/engine with unified sql and then start MariaDB => connection is established
19. **BDBU1**: Access denied when database name exists but is not the good one for unified sql output
20. **BDBU10**: Connection should be established when user password is good for unified sql
21. **BDBU3**: Access denied when database name does not exist for unified sql output
22. **BDBU5**: cbd does not crash if the unified sql db_host is wrong
23. **BDBU7**: Access denied when database user password is wrong for unified sql
24. **BEDB2**: start broker/engine and then start MariaDB => connection is established
25. **BEDB3**: start broker/engine, then stop MariaDB and then start it again. The gRPC API should give informations about SQL connections.
26. **BEDB4**: start broker/engine, then stop MariaDB and then start it again. The gRPC API should give informations about SQL connections.
27. **BFC1**: Start broker with invalid filters but one filter ok
28. **BFC2**: Start broker with only invalid filters on an output
29. **BGRPCSS1**: Start-Stop two instances of broker configured with grpc stream and no coredump
30. **BGRPCSS2**: Start/Stop 10 times broker configured with grpc stream with 300ms interval and no coredump
31. **BGRPCSS3**: Start-Stop one instance of broker configured with grpc stream and no coredump
32. **BGRPCSS4**: Start/Stop 10 times broker configured with grpc stream with 1sec interval and no coredump
33. **BGRPCSS5**: Start-Stop with reversed connection on grpc acceptor with only one instance and no deadlock
34. **BGRPCSSU1**: Start-Stop with unified_sql two instances of broker with grpc stream and no coredump
35. **BGRPCSSU2**: Start/Stop with unified_sql 10 times broker configured with grpc stream with 300ms interval and no coredump
36. **BGRPCSSU3**: Start-Stop with unified_sql one instance of broker configured with grpc and no coredump
37. **BGRPCSSU4**: Start/Stop with unified_sql 10 times broker configured with grpc stream with 1sec interval and no coredump
38. **BGRPCSSU5**: Start-Stop with unified_sql with reversed connection on grpc acceptor with only one instance and no deadlock
39. **BLBD**: Start Broker with loggers levels by default
40. **BLDIS1**: Start broker with core logs 'disabled'
41. **BLEC1**: Change live the core level log from trace to debug
42. **BLEC2**: Change live the core level log from trace to foo raises an error
43. **BLEC3**: Change live the foo level log to trace raises an error
44. **BSCSS1**: Start-Stop two instances of broker and no coredump
45. **BSCSS2**: Start/Stop 10 times broker with 300ms interval and no coredump
46. **BSCSS3**: Start-Stop one instance of broker with tcp connection and no coredump
47. **BSCSS4**: Start/Stop 10 times broker with 1sec interval and no coredump
48. **BSCSSC1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is enabled on client side.
49. **BSCSSC2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is disabled on client side.
50. **BSCSSCG1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on client side.
51. **BSCSSCGRR1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on output side. Reversed connection with retention and grpc transport protocol.
52. **BSCSSCGRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on output side. Reversed connection with retention and grpc transport protocol.
53. **BSCSSCRR1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is enabled on client side. Connection reversed with retention.
54. **BSCSSCRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is disabled on client side. Connection reversed with retention.
55. **BSCSSG1**: Start-Stop two instances of broker and no coredump
56. **BSCSSG2**: Start/Stop 10 times broker with 300ms interval and no coredump
57. **BSCSSG3**: Start-Stop one instance of broker with grpc connection and no coredump
58. **BSCSSG4**: Start/Stop 10 times broker with 1sec interval and no coredump
59. **BSCSSGA1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. An authorization token is added on the server. Error messages are raised.
60. **BSCSSGA2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. An authorization token is added on the server and also on the client. All looks ok.
61. **BSCSSGA3**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. The authorization token is retrieved from the vault on the server side.
62. **BSCSSGRR1**: Start-Stop two instances of broker and no coredump, reversed and retention, with transport protocol grpc, start-stop 5 times.
63. **BSCSSK1**: Start-Stop two instances of broker, server configured with grpc and client with tcp. No connectrion established and error raised on client side.
64. **BSCSSK2**: Start-Stop two instances of broker, server configured with tcp and client with grpc. No connection established and error raised on client side.
65. **BSCSSP1**: Start-Stop two instances of broker and no coredump. The server contains a listen address
66. **BSCSSPRR1**: Start-Stop two instances of broker and no coredump. The server contains a listen address, reversed and retention. centreon-broker-master-rrd is then a failover.
67. **BSCSSR1**: Start-Stop two instances of broker and no coredump. Connection with bbdo_server/bbdo_client and reversed.
68. **BSCSSRR1**: Start-Stop two instances of broker and no coredump. Connection with bbdo_server/bbdo_client, reversed and retention. centreon-broker-master-rrd is then a failover.
69. **BSCSSRR2**: Start/Stop 10 times broker with 300ms interval and no coredump, reversed and retention. centreon-broker-master-rrd is then a failover.
70. **BSCSST1**: Start-Stop two instances of broker and no coredump. Encryption is enabled on client side.
71. **BSCSST2**: Start-Stop two instances of broker and no coredump. Encryption is enabled on client side.
72. **BSCSSTG1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. This is not sufficient, then an error is raised.
73. **BSCSSTG2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. It works with good certificates and keys.
74. **BSCSSTG3**: Start-Stop two instances of broker. The connection cannot be established if the server private key is missing and an error message explains this issue.
75. **BSCSSTGRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. It works with good certificates and keys. Reversed grpc connection with retention.
76. **BSCSSTRR1**: Start-Stop two instances of broker and no coredump. Encryption is enabled. transport protocol is tcp, reversed and retention.
77. **BSCSSTRR2**: Start-Stop two instances of broker and no coredump. Encryption is enabled.
78. **BSS1**: Start-Stop two instances of broker and no coredump
79. **BSS2**: Start/Stop 10 times broker with 300ms interval and no coredump
80. **BSS3**: Start-Stop one instance of broker 5 times and no coredump
81. **BSS4**: Start/Stop 10 times broker with 1sec interval and no coredump
82. **BSS5**: Start-Stop with reversed connection on TCP acceptor with only one instance and no deadlock
83. **BSSU1**: Start-Stop two instances of broker with BBDO3 and no coredump
84. **BSSU2**: Start/Stop 10 times broker (BBDO3) with 300ms interval and no coredump
85. **BSSU3**: Start-Stop one instance of broker (BBDO3) and no coredump
86. **BSSU4**: Start/Stop 10 times broker with 1sec interval and no coredump
87. **BSSU5**: Start-Stop with reversed connection on TCP acceptor with only one instance and no deadlock
88. **START_STOP_CBD**: restart cbd with unified_sql services state must not be null after restart

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
9. **NetworkDBFail6**: network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s
10. **NetworkDBFail7**: network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s
11. **NetworkDBFailU6**: network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s (with unified_sql)
12. **NetworkDBFailU7**: network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s (with unified_sql)
13. **NetworkDBFailU8**: network failure test between broker and database: we wait for the connection to be established and then we shutdown the connection until _check_queues failure
14. **NetworkDbFail1**: network failure test between broker and database (shutting down connection for 100ms)
15. **NetworkDbFail2**: network failure test between broker and database (shutting down connection for 1s)
16. **NetworkDbFail3**: network failure test between broker and database (shutting down connection for 10s)
17. **NetworkDbFail4**: network failure test between broker and database (shutting down connection for 30s)
18. **NetworkDbFail5**: network failure test between broker and database (shutting down connection for 60s)

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
22. **BEACK_DT**: Scenario: a downtime must not remove an acknowledgement
      * Given a configuration with BBDO3 and a critical service that is acknowledged
      * When a downtime is scheduled on this service and starts
      * Then the acknowledgement must not be deleted
      * And the acknowledgements, services and resources tables must stay consistent.
23. **BEATOI11**: external command SEND_CUSTOM_HOST_NOTIFICATION with option_number=1 should work
24. **BEATOI12**: external command SEND_CUSTOM_HOST_NOTIFICATION with option_number>7 should fail
25. **BEATOI13**: external command Schedule Service Downtime with duration<0 should fail
26. **BEATOI21**: external command ADD_HOST_COMMENT and DEL_HOST_COMMENT should work
27. **BEATOI22**: external command DEL_HOST_COMMENT with comment_id<0 should fail
28. **BEATOI23**: external command ADD_SVC_COMMENT with persistent=0 should work
29. **BECC1**: Broker/Engine communication with compression between central and poller
30. **BECT1**: Broker/Engine communication with anonymous TLS between central and poller
31. **BECT2**: Broker/Engine communication with TLS between central and poller with key/cert
32. **BECT3**: Broker/Engine communication with anonymous TLS and ca certificate
33. **BECT4**: Broker/Engine communication with TLS between central and poller with key/cert and hostname forced
34. **BECT_GRPC1**: Broker/Engine communication with GRPC and with anonymous TLS between central and poller
35. **BECT_GRPC2**: Broker/Engine communication with TLS between central and poller with key/cert
36. **BECT_GRPC3**: Broker/Engine communication with anonymous TLS and ca certificate
37. **BECT_GRPC4**: Broker/Engine communication with TLS between central and poller with key/cert and hostname forced
38. **BECUSTOMHOSTVAR**: external command CHANGE_CUSTOM_HOST_VAR on SNMPVERSION
39. **BECUSTOMSVCVAR**: external command CHANGE_CUSTOM_SVC_VAR on CRITICAL
40. **BEDSVCDTDEL**: 
      * Given a configuration with BBDO3 and a unique downtime set on a service
      * When the downtime is finished
      * Then the downtime is well removed
      * And the number of downtimes is 0
41. **BEDTCANCELCLEAN**: 
      * Given a downtime running on a poller that is lost brutally
      * When the poller comes back without its retention
      * Then unified_sql cancels the orphaned downtime AND sets its actual_end_time
      * So that availability reporting does not consider it running forever.
42. **BEDTHOSTFIXED**: A downtime is set on a host, the total number of downtimes is really 21 (1 for the host and 20 for its 20 services) then we delete this downtime and the number is 0.
43. **BEDTHOSTFIXED1**: Scenario: Setting and Removing Downtime on a Host and its Services
      * Given a downtime is set on a host
      * Then the total number of downtimes is 21
      * And this includes 1 for the host and 20 for its services
      * When the downtime is deleted
      * Then the total number of downtimes is 0
44. **BEDTMASS1**: Scenario: Setting and Removing Downtimes on Configured Hosts and Services
      * Given new services with several pollers are created
      * When downtimes are set on all configured hosts
      * Then the total number of downtimes, including impacted services, is 1050
      * And all these downtimes are removed
      * And the test is performed with BBDO 3.0.0
45. **BEDTMASS2**: Scenario: Setting and Removing Downtimes on Configured Hosts and Services
      * Given new services with several pollers are created
      * When downtimes are set on all configured hosts
      * Then the total number of downtimes, including impacted services, is 1050
      * And all these downtimes are removed
      * And the test is performed with BBDO 2.0.0
46. **BEDTRRD1**: A service is forced checked then a downtime is set on this service. The service is forced checked again and the downtime is removed. This test is done with BBDO 3.0.0. Then we should not get any error in cbd RRD of kind 'ignored update error in file...'.
47. **BEDTSVCFIXED**: 
      * Given a unique downtime set on a service
      * When the downtime is removed
      * Then the downtime is well removed
      * And the number of downtimes is 0
48. **BEDTSVCFIXED1**: 
      * Given a configuration with BBDO3 and a unique downtime set on a service
      * When the downtime is removed
      * Then the downtime is well removed
      * And the number of downtimes is 0
49. **BEDTSVCREN1**: 
      * Given a downtime set on a service
      * When the service is renamed
      * Then the downtime is still active on the renamed service
      * When the downtime is removed from the renamed service
      * Then the downtime is well removed
50. **BEDTSVCREN2**: 
      * Given a configuration with BBDO3 and a downtime set on a service
      * When the service is renamed
      * Then the downtime is still active on the renamed service
      * When the downtime is removed from the renamed service
      * Then the downtime is well removed
51. **BEEXTCMD1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0
52. **BEEXTCMD10**: external command CHANGE_MAX_SVC_CHECK_ATTEMPTS on bbdo2.0
53. **BEEXTCMD11**: external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo3.0
54. **BEEXTCMD12**: external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo2.0
55. **BEEXTCMD13**: external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo3.0
56. **BEEXTCMD14**: external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo2.0
57. **BEEXTCMD15**: external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo3.0
58. **BEEXTCMD16**: external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo2.0
59. **BEEXTCMD17**: external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo3.0
60. **BEEXTCMD18**: external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo2.0
61. **BEEXTCMD19**: external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo3.0
62. **BEEXTCMD2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0
63. **BEEXTCMD20**: external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo2.0
64. **BEEXTCMD21**: external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS and ENABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo3.0
65. **BEEXTCMD22**: external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS and ENABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo2.0
66. **BEEXTCMD23**: 
      * Given Engine and broker configured with BBDO3
      * When the external command DISABLE_HOST_CHECK on host_1 is executed
      * Then the host_1 host checks should be disabled
      * When the external command ENABLE_HOST_CHECK on host_1 is executed
      * Then the host_1 host checks should be enabled
67. **BEEXTCMD24**: external command DISABLE_HOST_CHECK and ENABLE_HOST_CHECK on bbdo2.0
68. **BEEXTCMD25**: external command DISABLE_HOST_EVENT_HANDLER and ENABLE_HOST_EVENT_HANDLER on bbdo3.0
69. **BEEXTCMD26**: external command DISABLE_HOST_EVENT_HANDLER and ENABLE_HOST_EVENT_HANDLER on bbdo2.0
70. **BEEXTCMD27**: external command DISABLE_HOST_FLAP_DETECTION and ENABLE_HOST_FLAP_DETECTION on bbdo3.0
71. **BEEXTCMD28**: external command DISABLE_HOST_FLAP_DETECTION and ENABLE_HOST_FLAP_DETECTION on bbdo2.0
72. **BEEXTCMD29**: external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo3.0
73. **BEEXTCMD3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0
74. **BEEXTCMD30**: external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo2.0
75. **BEEXTCMD31**: external command DISABLE_HOST_SVC_CHECKS and ENABLE_HOST_SVC_CHECKS on bbdo3.0
76. **BEEXTCMD32**: external command DISABLE_HOST_SVC_CHECKS and ENABLE_HOST_SVC_CHECKS on bbdo2.0
77. **BEEXTCMD33**: external command DISABLE_HOST_SVC_NOTIFICATIONS and ENABLE_HOST_SVC_NOTIFICATIONS on bbdo3.0
78. **BEEXTCMD34**: external command DISABLE_HOST_SVC_NOTIFICATIONS and ENABLE_HOST_SVC_NOTIFICATIONS on bbdo2.0
79. **BEEXTCMD35**: external command DISABLE_PASSIVE_HOST_CHECKS and ENABLE_PASSIVE_HOST_CHECKS on bbdo3.0
80. **BEEXTCMD36**: external command DISABLE_PASSIVE_HOST_CHECKS and ENABLE_PASSIVE_HOST_CHECKS on bbdo2.0
81. **BEEXTCMD37**: external command DISABLE_PASSIVE_SVC_CHECKS and ENABLE_PASSIVE_SVC_CHECKS on bbdo3.0
82. **BEEXTCMD38**: external command DISABLE_PASSIVE_SVC_CHECKS and ENABLE_PASSIVE_SVC_CHECKS on bbdo2.0
83. **BEEXTCMD39**: external command START_OBSESSING_OVER_HOST and STOP_OBSESSING_OVER_HOST on bbdo3.0
84. **BEEXTCMD4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0
85. **BEEXTCMD40**: external command START_OBSESSING_OVER_HOST and STOP_OBSESSING_OVER_HOST on bbdo2.0
86. **BEEXTCMD41**: external command START_OBSESSING_OVER_SVC and STOP_OBSESSING_OVER_SVC on bbdo3.0
87. **BEEXTCMD42**: external command START_OBSESSING_OVER_SVC and STOP_OBSESSING_OVER_SVC on bbdo2.0
88. **BEEXTCMD5**: external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo3.0
89. **BEEXTCMD6**: external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo2.0
90. **BEEXTCMD7**: external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo3.0
91. **BEEXTCMD8**: external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo2.0
92. **BEEXTCMD9**: external command CHANGE_MAX_SVC_CHECK_ATTEMPTS with bbdo3.0
93. **BEEXTCMD_COMPRESS_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and compressed grpc
94. **BEEXTCMD_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and grpc
95. **BEEXTCMD_GRPC2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 and grpc
96. **BEEXTCMD_GRPC3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 and grpc
97. **BEEXTCMD_GRPC4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 and grpc
98. **BEEXTCMD_REVERSE_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and reversed gRPC
99. **BEEXTCMD_REVERSE_GRPC2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 and grpc reversed
100. **BEEXTCMD_REVERSE_GRPC3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 and grpc reversed
101. **BEEXTCMD_REVERSE_GRPC4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 and grpc reversed
102. **BEHOSTCHECK**: 
      * Given Engine and Broker configured to work with BBDO 3
      * When a schedule forced host check command on host host_1 is launched
      * Then the result appears in the centreon_storage resources table
103. **BEHS1**: store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
104. **BEINSTANCE**: Instance to bdd
105. **BEINSTANCESTATUS**: Instance status to bdd
106. **BEMC1**: 
      * When host_1 is moved from engine0 to engine1, reloading engine1 first
      * (so broker cache records instance_id=2 for host_1) and then reloading
      * engine0 (which sends a DELETE with instance_id=1) must not evict host_1
      * from broker cache. The broker log must contain "ignoring stale deletion".
107. **BEMC2**: 
      * When service_1 on host_1 is moved from engine0 to engine1, reloading
      * engine1 first (broker records instance_id=2) and then reloading engine0
      * (DELETE with instance_id=1) must not evict service_1 from broker cache.
108. **BENCH_${nb_checks}STATUS**: external command CHECK_SERVICE_RESULT 1000 times
109. **BENCH_${nb_checks}STATUS_TRACES**: external command CHECK_SERVICE_RESULT ${nb_checks} times
110. **BENCH_${nb_checks}_REVERSE_SERVICE_STATUS_TRACES_WITHOUT_SQL**: Broker is configured without SQL output. The connection between Engine and Broker is reversed. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times. Logs are in trace level.
111. **BENCH_${nb_checks}_REVERSE_SERVICE_STATUS_WITHOUT_SQL**: Broker is configured without SQL output. The connection between Engine and Broker is reversed. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times.
112. **BENCH_${nb_checks}_SERVICE_STATUS_TRACES_WITHOUT_SQL**: Broker is configured without SQL output. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times. Logs are in trace level.
113. **BENCH_${nb_checks}_SERVICE_STATUS_WITHOUT_SQL**: Broker is configured without SQL output. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times.
114. **BENCH_1000STATUS_100${suffixe}**: external command CHECK_SERVICE_RESULT 100 times    with 100 pollers with 20 services
115. **BENCR1**: 
      * Given an engine configured with hosts and services with check commands,
      * when I change the check commands in the configuration and reload the engine,
116. **BENCV**: Engine is configured with hosts/services. The first host has no customvariable. Then we add a customvariable to the first host and we reload engine. Then the host should have this new customvariable defined and centengine should not crash.
117. **BEOTEL_CENTREON_AGENT_ACCENTED_PERFDATA**: Scenario: plugin output and perfdata label with accented characters (SSL certificate subject)
      * Given an agent that executes two plugins printing an SSL certificate check result
      * whose output and perfdata label contain ô, é and à
      * And service_1 plugin prints it in UTF-8
      * And service_2 plugin prints it in ISO-8859-1 (not valid UTF-8)
      * When Broker, Engine and Agent are started
      * Then the OTel messages are serialized without error and both services reach HARD OK
      * And their outputs and metric names are stored in UTF-8 in the database
118. **BEOTEL_CENTREON_AGENT_CEIP**: Scenario: Agent and "centreon_storage.agent_information" Statistics
      * Given Engine connected to Broker
      * When an agent connects to Engine
119. **BEOTEL_CENTREON_AGENT_CHECK_COUNTER**: 
      * Given an agent with counter check, we expect to get the correct status for the centagent process running on windows host
120. **BEOTEL_CENTREON_AGENT_CHECK_DIFFERENT_INTERVAL**: 
      * Given and agent who has to execute checks with different intervals, we expect to find these intervals in data_bin
121. **BEOTEL_CENTREON_AGENT_CHECK_EVENTLOG**: 
      * Given an agent with eventlog check, we expect status, output and metrics
122. **BEOTEL_CENTREON_AGENT_CHECK_FILES**: 
      * Given an agent with file check, we expect to get the correct status for files under monitoring on the Windows host
123. **BEOTEL_CENTREON_AGENT_CHECK_HEALTH**: agent check health and we expect to get it in check result
124. **BEOTEL_CENTREON_AGENT_CHECK_HOST**: 
      * Given an agent host checked by centagent, we set a first output to check command, 
      * modify it, reload engine and expect the new output in resource table
125. **BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED**: agent check host with encrypted connection and we expect to get it in check result
126. **BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED_ENCRYPTED_CREDENTIALS**: 
      * Given an agent host checked by centagent over an encrypted connection,
      * Engine use credentials encryption and send encrypted commands
      * we set a first output to check command, 
      * modify it, reload engine and expect the new output in resource table
127. **BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED_MANY_AGENT**: 
      * Given an engine listening for incomming CMA connections,
      * We connect two agent with the same host_name and engine must not crash
128. **BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED_RENEW_CERT**: 
      * Given an engine with an encrypted otel server with one minute ttl, we expect that otel server
      * will restart after on minute, then we recreate certificates and otel server must restart also
129. **BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED_RENEW_CERT_WITH_DEFAULT_CERT**: 
      * Given an engine with an encrypted otel server with one minute ttl and default engine generated certificates, 
      * we expect that otel server will restart after on minute, then we recreate certificates and otel server must restart also
130. **BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED_SAN_IP**: 
      * Given an engine with an encrypted otel server using a self-signed cert
      * whose CN does not match the hostname but has IP:127.0.0.1 and DNS:<hostname> in SAN,
      * with minute_certificate_ttl triggering cert regeneration,
      * the agent should connect successfully because SANs are copied to the generated cert.
131. **BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED_WITHOUT_CERT**: 
      * Given Engine is configured with a full TLS gRPC server (port 4318) exposing its CA certificate,
      * And the CMA has only a fingerprint (no CA certificate file) and a JWT token,
      * When the CMA starts and attempts a full TLS connection to Engine (which fails — no CA yet),
      * Then the CMA falls back to an insecure TLS connection to retrieve the CA certificate from Engine,
      * And Engine logs a security entry confirming the CA certificate was delivered,
      * And the CMA validates the retrieved CA certificate against the configured fingerprint,
      * And the CMA logs a security entry confirming the CA bootstrap succeeded,
      * And the CMA stores the CA certificate and reconnects using full TLS with the JWT token,
      * Then the host check result is reported successfully in the resources table.
132. **BEOTEL_CENTREON_AGENT_CHECK_HOST_NO_ENCRYPTED_CREDENTIALS**: 
      * Given an agent host checked by centagent over a non encrypted connection,
      * Engine use credentials encryption, but send no encrypted commands
      * we set a first output to check command, 
      * modify it, reload engine and expect the new output in resource table
133. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_CPU**: agent check service with native check cpu and we expect to get it in check result
134. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_CUSTOM**: Scenario: Native custom check echoes provided arguments
      * Given Centreon Engine is configured with an OpenTelemetry server module and OTEL connector
      * And service_1 is set as a passive service with defined check and retry intervals
      * And the check command "otel_check" runs the native custom check "check_echo" with ARG1 "user" and ARG2 "hello"
      * When Broker, Engine and Agent are started and the OTEL server becomes ready
      * Then service_1 must reach HARD OK state within 120 seconds
      * And the service output must be "hello user from custom check"
135. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_MEMORY**: agent check service with native check memory and we expect to get it in check result
136. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_SERVICE**: agent check service with native check service and we expect to get it in check result
137. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_SERVICE_AUTO**: 
      * Given a window agent with native service check, we check cma service with different configurations
138. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_SERVICE_FILTER_STATE**: 
      * Given a windows agent with native service check, 
      * we filter on CentreonMonitoringAgent service and use warning-state and critical-state to trigger status changes
139. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_STORAGE**: agent check service with native check storage and we expect to get it in check result, with the <drive>:#storage.space.free.bytes, usage.bytes and usage.percentage perfdata
140. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_STORAGE_FILTER_FS**: 
      * Given a windows agent with native check storage,
      * when filter-fs or exclude-fs refuses a drive, then this drive is refused before any system call
      * on it (no GetVolumeInformation or GetDiskFreeSpaceEx), so no error is logged even for drives
      * without media. filter-fs and exclude-fs are case insensitive.
      * With filter-fs c:.*, the count perfdata is 1 and C:#storage.space.usage.percentage is sent.
141. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_STORAGE_FILTER_FS_MACRO**: 
      * Given a windows agent with native check storage whose filter-fs and exclude-fs
      * come from FILTERFS and EXCLUDEFS service macros,
      * when backslashes are escaped once or twice (legacy form) in these macros,
      * then both forms give the same result: C: is checked with FILTERFS and refused with EXCLUDEFS.
      * A FILTERFS that matches no drive still refuses every drive.
      * C:#storage.space.usage.percentage is also expected with each FILTERFS form.
142. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_STORAGE_FILTER_STORAGE_TYPE**: 
      * Given a windows agent with native check storage,
      * when filter-storage-type contains a storage type (hrstoragefixeddisk, whatever its case)
      * or a file system format (hrfsntfs, other|ntfs), then C: drive is checked and service is OK.
      * When filter-storage-type doesn't match any drive, then service is CRITICAL with
      * 'No storage found (filters issue)'.
      * Between each filter, a filter that refuses all drives is applied in order to be sure that
      * OK state comes from the new filter.
      * C:#storage.space.usage.percentage is also expected for each filter.
143. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_UPTIME**: agent check service with native check uptime and we expect to get it in check result, with the last reboot time in ISO 8601 UTC in the output
144. **BEOTEL_CENTREON_AGENT_CHECK_PROCESS**: 
      * Given an agent with eventlog check, we expect to get the correct status for thr centagent process running on windows host
145. **BEOTEL_CENTREON_AGENT_CHECK_SERVICE**: 
      * Given an Engine configured with an OpenTelemetry server module 
146. **BEOTEL_CENTREON_AGENT_CHECK_SERVICE_FRESHNESS**: 
      * Given Centreon Engine is configured with an OpenTelemetry server module
147. **BEOTEL_CENTREON_AGENT_CHECK_SERVICE_TIMEPERIODS**: CMA timeperiod gating: services with always-active periods are checked, services outside their period are deferred with the correct next-open offset. service_1 -> 24x7           always active (built-in) service_4 -> opens_in_1h    inactive now, opens in ~1 hour
148. **BEOTEL_CENTREON_AGENT_CHECK_TASKSCHEDULER**: 
      * Given an agent with task scheduler check, we expect to get the correct status for the centagent process running on windows host
149. **BEOTEL_CENTREON_AGENT_CMD_DATABASE**: 
      * Given an Engine configured 
150. **BEOTEL_CENTREON_AGENT_EVENT_HANDLER**: Scenario: the agent runs the event handler of a service on state changes
      * Given an agent that checks service_1 with max_check_attempts 2
      * And service_1 has an event handler writing $SERVICESTATE$ $SERVICESTATETYPE$ $SERVICEATTEMPT$ to a file
      * When service_1 goes CRITICAL until HARD, then OK
      * Then the agent runs the event handler on CRITICAL SOFT 1, CRITICAL HARD 2 and OK HARD 1
      * And the state macros are replaced by the agent with the state of each transition
151. **BEOTEL_CENTREON_AGENT_FORCE_CHECK**: 
      * Given an agent host with 20 services checked by centagent, we force check to 10nth service and then to the host and 
      * we expect correct status in bdd
152. **BEOTEL_CENTREON_AGENT_INSECURE**: 
      * Given the Centreon Engine is configured with OpenTelemetry server with encryption enabled 
      * When the Centreon Agent connects using a certificate with a mismatched hostname and the insecure option enabled
      * Then the connection succeeds
      * And the engine log confirms the token was accepted
153. **BEOTEL_CENTREON_AGENT_LINUX_NO_DEFUNCT_PROCESS**: agent check host and we expect to get it in check result
154. **BEOTEL_CENTREON_AGENT_NO_TRUSTED_TOKEN**: Scenario: Encrypted connection rejected with untrusted token
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
155. **BEOTEL_CENTREON_AGENT_TLS_BAD_CERT**: 
      * Given the Centreon Engine is configured with OpenTelemetry server with encryption enabled 
      * When the Centreon Agent connects using a certificate with a mismatched hostname and the full option enabled
      * Then the connection is refused
      * And the engine log confirms the certificate hostname mismatch
156. **BEOTEL_CENTREON_AGENT_TOKEN**: Scenario: Validate agent connection with valid JWT token
      * Given the OpenTelemetry server is configured with encryption enabled and the server trusts a JWT token
      * And the Centreon Agent is configured to use that valid token and the server certificate
      * When the broker, engine and agent are started
      * Then the engine should expose an encrypted OTEL listener
      * And the engine log should report that the token is valid
      * When the agent is reconfigured to use an unencrypted connection and the engine is reloaded
      * And the agent is restarted
      * Then the engine should expose an unencrypted OTEL listener
      * And the engine log should still report that the token is valid
157. **BEOTEL_CENTREON_AGENT_TOKEN_AGENT_TELEGRAPH**: 
      * Given an OpenTelemetry server is configured with token-based connection
158. **BEOTEL_CENTREON_AGENT_TOKEN_AGENT_TELEGRAPH_2**: 
      * Given an OpenTelemetry server is configured with token-based connection
159. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED**: Scenario: Encrypted connection with expired token
160. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED_WHILE_RUNNING**: Scenario: Token expires during encrypted connection
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
161. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED_WHILE_RUNNING_REVERSE**: Scenario: Token expires during encrypted reverse connection
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
162. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRE_REVERSE**: Scenario: Encrypted reversed connection with expired token
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
163. **BEOTEL_CENTREON_AGENT_TOKEN_MISSING_HEADER**: Scenario: Missing JWT header over encrypted OTEL connection
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
164. **BEOTEL_CENTREON_AGENT_TOKEN_REVERSE**: Scenario: Encrypted reversed connection with valid token
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
165. **BEOTEL_CENTREON_AGENT_TOKEN_UNTRUSTED**: Scenario: Encrypted connection with untrusted token
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
166. **BEOTEL_CENTREON_AGENT_TOKEN_UNTRUSTED_REVERSE**: Scenario: Encrypted reversed connection refused with untrusted token
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
167. **BEOTEL_CENTREON_AGENT_UNKNOWN_HOST**: 
      * Given an engine configured, a broker with event_script output
      * We connect a CMA with an hostname unknown from engine configuration, 
      * we expect that broker will call the script with the new agent infos
      * Then we add unknown host to engine config and we wait for a check result
168. **BEOTEL_CENTREON_AGENT_WHITE_LIST**: Scenario: Enforcing command whitelist for agent checks
      * Given a whitelist file is created with allowed commands for host_1
      * And the engine, broker, and agent are configured and started
      * When a check command matching the whitelist is executed for host_1
      * Then the check result is accepted and stored in the resources table
      * When a check command not matching the whitelist is configured for host_1 and engine is reloaded
      * Then the command is rejected and a "command not allowed by whitelist" message appears in the log
169. **BEOTEL_CENTREON_AGENT_WHITE_LIST_FORCE_CHECK_HOST**: Scenario: forced checks of a CMA host use the CMA whitelist
      * Given a whitelist file where the cma-whitelist accepts the CMA check command of host_1
      * And the engine whitelist does not accept this command
      * And the engine, broker, and agent are configured and started
      * When the agent executes the check of host_1
      * Then the check result is accepted and stored in the resources table
      * When a forced check is scheduled on host_1
      * Then no whitelist error appears in the engine log
      * And the check result is stored in the resources table
170. **BEOTEL_CENTREON_AGENT_WHITE_LIST_FORCE_CHECK_SERVICE**: Scenario: forced checks of a CMA service use the CMA whitelist
      * Given a whitelist file where the cma-whitelist accepts the CMA check command of host_1/service_1
      * And the engine whitelist does not accept this command
      * And the engine, broker, and agent are configured and started
      * When the agent executes the check of service_1
      * Then the check result is accepted and stored in the resources table
      * When a forced check is scheduled on service_1
      * Then no whitelist error appears in the engine log
      * And the check result is stored in the resources table
171. **BEOTEL_INVALID_CHECK_COMMANDS_AND_ARGUMENTS**: 
      * Given the agent is configured with native checks for services
      * And the OpenTelemetry server module is added
      * And services are configured with incorrect check commands and arguments
      * When the broker, engine, and agent are started
      * Then the resources table should be updated with the correct status
      * And appropriate error messages should be generated for invalid checks
172. **BEOTEL_REVERSE_CENTREON_AGENT_CHECK_HOST**: agent check host with reversed connection and we expect to get it in check result
173. **BEOTEL_REVERSE_CENTREON_AGENT_CHECK_HOST_CRYPTED**: agent check host with encrypted reversed connection and we expect to get it in check result
174. **BEOTEL_REVERSE_CENTREON_AGENT_CHECK_SERVICE**: agent check service with reversed connection and we expect to get it in check result
175. **BEOTEL_SERVE_TELEGRAF_CONFIGURATION_CRYPTED**: we configure engine with a telegraf conf server and we check telegraf conf file
176. **BEOTEL_SERVE_TELEGRAF_CONFIGURATION_NO_CRYPTED**: we configure engine with a telegraf conf server and we check telegraf conf file
177. **BEOTEL_SERVE_TELEGRAF_CONFIGURATION_NO_CRYPTED_1**: Scenario: Serve telegraf configuration with a complex whitelist
      * Given the engine is configured with a telegraf conf server and a complex whitelist
      * When I request the telegraf conf file for host_1
      * Then I should receive the expected telegraf configuration for host_1
      * And service_3 should be blacklisted and unavailable for host_1
      * When I request the telegraf conf file for host_2
      * Then I should receive the expected telegraf configuration for host_2
      * And service_5 should be blacklisted and unavailable for host_2
178. **BEOTEL_TELEGRAF_CHECK_HOST**: we send nagios telegraf formatted data and we expect to get it in check result
179. **BEOTEL_TELEGRAF_CHECK_SERVICE**: Scenario: Handling of OK and CRITICAL check results from Telegraf input
      * Given the OpenTelemetry server is ready
      * When I send a Telegraf-formatted check result with status "OK" to the Engine
      * Then the result should be stored in the Centreon Broker storage database with status "OK"
180. **BEPBBEE1**: central-module configured with bbdo_version 3.0 but not others. Unable to establish connection.
181. **BEPBBEE2**: bbdo_version 3 not compatible with sql/storage
182. **BEPBBEE3**: bbdo_version 3 generates new bbdo protobuf service status messages.
183. **BEPBBEE4**: bbdo_version 3 generates new bbdo protobuf host status messages.
184. **BEPBBEE5**: bbdo_version 3 generates new bbdo protobuf service messages.
185. **BEPBCVS**: bbdo_version 3 communication of custom variables.
186. **BEPBHostParent**: bbdo_version 3 communication of host parent relations
187. **BEPBINST_CONF**: bbdo_version 3 communication of instance configuration.
188. **BEPBRI1**: bbdo_version 3 use pb_resource new bbdo protobuf ResponsiveInstance message.
189. **BEPBUID1**: uid > 2^32 in broker module config overrides poller_id. The instances, hosts and resources tables must store this 64-bit value.
190. **BERD1**: Scenario: Starting/stopping Broker does not create duplicated events.
191. **BERD2**: Scenario: Starting/stopping Engine does not create duplicated events.
192. **BERDUC1**: Scenario: Starting/stopping Engine does not create duplicated events in usual cases
193. **BERDUC2**: Scenario: Starting/stopping Engine does not create duplicated events in usual cases
194. **BERDUC3U1**: Scenario: Starting/stopping Broker does not create duplicated events in usual cases with unified_sql and BBDO 3.0
195. **BERDUC3U2**: Scenario: Starting/stopping Engine does not create duplicated events in usual cases with unified_sql and BBDO 3.0
196. **BERDUCA300**: Scenario: Starting/stopping Engine is stopped; it should emit a stop event and receive an ack event with events to clean from broker.
197. **BERDUCA301**: Scenario: Starting/stopping Engine is stopped; it should emit a stop event and receive an ack event with events to clean from broker with bbdo 3.0.1.
198. **BERDUCU1**: Starting/stopping Broker does not create duplicated events in usual cases with unified_sql7
199. **BERDUCU2**: Scenario: Starting/stopping Engine does not create duplicated events in usual cases with unified_sql
200. **BERES1**: store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
201. **BESERVCHECK**: external command CHECK_SERVICE_RESULT
202. **BESS1**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
203. **BESS2**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
204. **BESS2U**: Start-Stop Broker/Engine - Broker started first - Engine stopped first. Unified_sql is used.
205. **BESS3**: Start-Stop Broker/Engine - Engine started first - Engine stopped first
206. **BESS4**: Start-Stop Broker/Engine - Engine started first - Broker stopped first
207. **BESS5**: Start-Stop Broker/engine - Engine debug level is set to all, it should not hang
208. **BESSBQ1**: A very bad queue file is written for broker. Broker and Engine are then started, Broker must read the file raising an error because of that file and then get data sent by Engine.
209. **BESSCTO**: Scenario: Service commands time out due to missing Perl Connector
      * Given the Engine is configured as usual but without the Perl Connector
      * When the Engine executes its service commands
      * Then the commands take too long and reach the timeout
      * And the Engine starts and stops two times as a result
210. **BESSCTOWC**: Scenario: Service commands time out due to missing Perl Connector
      * Given the Engine is configured as usual with some commands using the Perl Connector
      * When the Engine executes its service commands
      * Then the commands take too long and reach the timeout
      * And the Engine starts and stops two times as a result
211. **BESSG**: Scenario: Broker handles connection and disconnection with Engine
      * Given Broker is configured with only one output that is Graphite
      * When the Engine starts and connects to the Broker
      * Then the Broker must be able to handle the connection
      * When the Engine stops
      * Then the Broker must be able to handle the disconnection
212. **BESS_CRYPTED_GRPC1**: Start-Stop grpc version Broker/Engine - well configured
213. **BESS_CRYPTED_GRPC2**: Start-Stop grpc version Broker/Engine only server crypted
214. **BESS_CRYPTED_GRPC3**: Start-Stop grpc version Broker/Engine only engine crypted
215. **BESS_CRYPTED_REVERSED_GRPC1**: Start-Stop grpc version Broker/Engine - well configured
216. **BESS_CRYPTED_REVERSED_GRPC2**: Start-Stop grpc version Broker/Engine only engine server crypted
217. **BESS_CRYPTED_REVERSED_GRPC3**: Start-Stop grpc version Broker/Engine only engine crypted
218. **BESS_ENGINE_DELETE_HOST**: once engine and cbd started, stop and restart cbd, delete an host and reload engine, cbd mustn't core
219. **BESS_GRPC1**: Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped first
220. **BESS_GRPC2**: Start-Stop grpc version Broker/Engine - Broker started first - Engine stopped first
221. **BESS_GRPC3**: Start-Stop grpc version Broker/Engine - Engine started first - Engine stopped first
222. **BESS_GRPC4**: Start-Stop grpc version Broker/Engine - Engine started first - Broker stopped first
223. **BESS_GRPC5**: Start-Stop grpc version Broker/engine - Engine debug level is set to all, it should not hang
224. **BESS_GRPC_COMPRESS1**: Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped last compression activated
225. **BETAG1**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Broker is started before.
226. **BETAG2**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
227. **BEUTAG1**: Engine is configured with some tags. When broker receives them through unified_sql stream, it stores them in the centreon_storage.tags table. Broker is started before.
228. **BEUTAG10**: some services are configured with tags on two pollers. Then tags are removed from some of them and in centreon_storage, we can observe resources_tags table updated.
229. **BEUTAG11**: Scenario: Updating resource tags after changing several tags
      * Given some services are configured with tags on two pollers
      * Then the resources_tags table contains them
      * When several tags are changed
      * Then the resources_tags table is updated
230. **BEUTAG12**: Engine is configured with some tags. Group tags tag2, tag6 are set to hosts 1 and 2. Category tags tag4 and tag8 are added to hosts 2, 3, 4. The resources and resources_tags tables are well filled. The tag6 and tag8 are removed and resources_tags is also well updated.
231. **BEUTAG2**: Engine is configured with some tags. A new service is added with a tag. Broker should make the relations.
232. **BEUTAG3**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
233. **BEUTAG4**: Engine is configured with some tags. Group tags tag9, tag13 are set to services 1 and 3. Category tags tag3 and tag11 are added to services 1, 3, 5 and 6. The centreon_storage.resources and resources_tags tables are well filled.
234. **BEUTAG5**: Engine is configured with some tags. Group tags tag2, tag6 are set to hosts 1 and 2. Category tags tag4 and tag8 are added to hosts 2, 3, 4. The resources and resources_tags tables are well filled.
235. **BEUTAG6**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.resources_tags table. Engine is started before.
236. **BEUTAG7**: Some services are configured with tags on two pollers. Then tags configuration is modified.
237. **BEUTAG8**: Services have tags provided by templates.
238. **BEUTAG9**: hosts have tags provided by templates.
239. **BEUTAG_REMOVE_HOST_FROM_HOSTGROUP**: remove a host from hostgroup, reload, insert 2 host in the hostgroup must not make sql error
240. **BE_BACKSLASH_CHECK_RESULT**: external command PROCESS_SERVICE_CHECK_RESULT with \:
241. **BE_DEFAULT_NOTIFCATION_INTERVAL_IS_ZERO_SERVICE_RESOURCE**: default notification_interval must be set to NULL in services, hosts and resources tables.
242. **BE_FLAPPING_HOST_RESOURCE**: With BBDO 3, flapping detection must be set in hosts and resources tables.
243. **BE_FLAPPING_SERVICE_RESOURCE**: With BBDO 3, flapping detection must be set in services and resources tables.
244. **BE_NOTIF_OVERFLOW**: bbdo 2.0 notification number =40000. make an overflow => notification_number null in db
245. **BE_TIME_NULL_SERVICE_RESOURCE**: With BBDO 3, notification_interval time must be set to NULL on 0 in services, hosts and resources tables.
246. **BRCS1**: Broker reverse connection stopped
247. **BRCTS1**: Broker reverse connection too slow
248. **BRCTSMN**: 
      * Given Broker, Engine configured as usual
      * And map also connected to Broker with a filter allowing only 'neb' category
      * When Engine sends pb_service, pb_host, pb_service_status and pb_host_status
      * Then map receives correctly them.
249. **BRCTSMNS**: 
      * Given Broker, Engine configured as usual
      * And map also connected to Broker with a filter allowing 'neb' and 'storage' categories
      * When Engine sends pb_service, pb_host, pb_service_status, pb_host_status and metrics
      * Then Map receives correctly them.
250. **BRGC1**: Broker good reverse connection
251. **BROKER_LUA_ENCRYPTION**: 
      * Given an engine with configured encryption, and key and salt, 
      * cbmod and broker use a lua script that use encrypted credentials
      * we give it an encrypted macro that contains path to a lua output files and 
      * we expect that this file will becreated by lua.
252. **BRRDCDDID1**: RRD metrics deletion from index ids with rrdcached.
253. **BRRDCDDIDDB1**: RRD metrics deletion from index ids with a query in centreon_storage with rrdcached.
254. **BRRDCDDIDU1**: RRD metrics deletion from index ids with unified sql output with rrdcached.
255. **BRRDCDDM1**: RRD metrics deletion from metric ids with rrdcached.
256. **BRRDCDDMDB1**: RRD metrics deletion from metric ids with a query in centreon_storage and rrdcached.
257. **BRRDCDDMID1**: RRD deletion of non existing metrics and indexes with rrdcached
258. **BRRDCDDMIDU1**: RRD deletion of non existing metrics and indexes with rrdcached
259. **BRRDCDDMU1**: RRD metric deletion on table metric with unified sql output with rrdcached
260. **BRRDCDRB1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with storage/sql sql output and rrdcached.
261. **BRRDCDRBDB1**: RRD metric rebuild with a query in centreon_storage and unified sql with rrdcached
262. **BRRDCDRBU1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with unified_sql output and rrdcached.
263. **BRRDCDRBUDB1**: RRD metric rebuild with a query in centreon_storage and unified sql with rrdcached
264. **BRRDDID1**: RRD metrics deletion from index ids.
265. **BRRDDIDDB1**: RRD metrics deletion from index ids with a query in centreon_storage.
266. **BRRDDIDU1**: RRD metrics deletion from index ids with unified sql output.
267. **BRRDDM1**: RRD metrics deletion from metric ids.
268. **BRRDDMDB1**: RRD metrics deletion from metric ids with a query in centreon_storage.
269. **BRRDDMID1**: RRD deletion of non existing metrics and indexes
270. **BRRDDMIDU1**: RRD deletion of non existing metrics and indexes
271. **BRRDDMU1**: RRD metric deletion on table metric with unified sql output
272. **BRRDRBDB1**: RRD metric rebuild with a query in centreon_storage and unified sql
273. **BRRDRBUDB1**: RRD metric rebuild with a query in centreon_storage and unified sql
274. **BRRDRM1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with storage/sql sql output.
275. **BRRDRMU1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with unified_sql output.
276. **BRRDSTATUS**: We are working with BBDO3. This test checks status are correctly handled independently from their value.
277. **BRRDSTATUSRETENTION**: We are working with BBDO3. This test checks status are not sent twice after Engine reload.
278. **BRRDUPLICATE**: RRD metric rebuild with a query in centreon_storage and unified sql with duplicate rows in database
279. **BRRDWM1**: We are working with BBDO3. This test checks protobuf metrics and status are sent to cbd RRD.
280. **BRRD_RRDLEN**: 
      * Given an RRD with default length 15552000s, 180 days,
      * We change length to 86400s, 1 day, we rebuild metrics and we expect to find 289 rows in metrics
281. **BRTCPWQF**: 
      * Given Broker configured with a map output and a small event queue
      * When a map client connects with a tiny receive buffer and stops reading
      * Then Broker logs "write queue full => remove oldest event"
282. **CBD_RELOAD_AND_FILTERS**: We start engine/broker with a classical configuration. All is up and running. Some filters are added to the rrd output and cbd is reloaded. All is still up and running but some events are rejected. Then all is newly set as filter and all events are sent to rrd broker.
283. **CBD_RELOAD_AND_FILTERS_WITH_OPR**: We start engine/broker with an almost classical configuration, just the connection between cbd central and cbd rrd is reversed with one peer retention. All is up and running. Some filters are added to the rrd output and cbd is reloaded. All is still up and running but some events are rejected. Then all is newly set as filter and all events are sent to rrd broker.
284. **CUSTOM_MACRO_RECURSIVE_RESOLUTION**: Custom macros can reference other macros and get them resolved: - host custom containing host standard ($HOSTADDRESS$) - host custom containing another host custom ($_HOSTINNER$) - host custom containing resource macro ($USER1$) - service custom containing service standard ($SERVICEDESC$) - service custom containing host standard + host custom + service custom - service custom containing $USER1$ + $HOSTADDRESS$ - recursion stops at depth 2 (3-level chain leaves innermost unresolved)
285. **CUSTOM_MACRO_RECURSIVE_RESOLUTION_WITH_ONE_DOLLAR**: Custom macros can reference other macros and get them resolved: - host custom containing host standard ($HOSTADDRESS$) - host custom containing another host custom ($_HOSTINNER$) - host custom containing resource macro ($USER1$) - service custom containing service standard ($SERVICEDESC$) - service custom containing host standard + host custom + service custom - service custom containing $USER1$ + $HOSTADDRESS$ - recursion stops at depth 2 (3-level chain leaves innermost unresolved)
286. **CUSTOM_MACRO_RESOLUTION_WITH_FALSE_MACRO**: 
      * Given engine is configured with host and service custom macros whose values contain dollar signs
      * When a service check runs using old-style $MACRO$ syntax with a non-existent macro at the end of the command
      * Then all macros are resolved correctly, dollar signs inside values are preserved, and the non-existent macro expands to empty
287. **CUSTOM_MACRO_RESOLUTION_WITH_NEW_STYLE_MACRO**: 
      * Given engine is configured with host and service custom macros, one value embedding a new-style non-existent macro {{$TITI$}}
      * When a service check runs using new-style {{$MACRO$}} syntax with a non-existent macro at the end of the command
      * Then all macros are resolved correctly, dollar signs inside values are preserved, and non-existent macros expand to empty
288. **CUSTOM_MACRO_RESOLUTION_WITH_ONE_DOLLAR**: Custom macros with one dollar can be resolved an dollar is not removed
289. **DTIM**: New services with several pollers are created. Then downtimes are set on all configured hosts. This action results on 5250 downtimes if we also count impacted services. Then all these downtimes are removed. This test is done with BBDO 3.0.1
290. **DT_WITH_QUOTES**: 
      * Given a centreon running, we set a downtime with quotes
      * Then we expect to get these quotes in bdd
291. **EBBM1**: A service status contains metrics that do not fit in a float number.
292. **EBBPS1**: 1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table resources.
293. **EBBPS2**: 1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table services.
294. **EBDP1**: Four new pollers are started and then we remove Poller3.
295. **EBDP2**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
296. **EBDP3**: Three new pollers are started, then they are killed. It is still possible to remove Poller2 if removed from the configuration.
297. **EBDP4**: Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by Broker.
298. **EBDP5**: Four new pollers are started and then we remove Poller3.
299. **EBDP6**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
300. **EBDP7**: Three new pollers are started, then they are killed. It is still possible to remove Poller2 if removed from the configuration.
301. **EBDP8**: Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by broker.
302. **EBDP_GRPC2**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
303. **EBMSSM**: 1000 services are configured with 100 metrics each. The rrd output is removed from the broker configuration. GetSqlManagerStats is called to measure writes into data_bin.
304. **EBMSSMDBD**: 1000 services are configured with 100 metrics each. The rrd output is removed from the broker configuration. While metrics are written in the database, we stop the database and then restart it. Broker must recover its connection to the database and continue to write metrics.
305. **EBMSSMPART**: Scenario: Broker continues writing metrics after partition recreation
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
306. **EBNHG1**: New host group with several pollers and connections to DB
307. **EBNHG4**: New host group with several pollers and connections to DB with broker and rename this hostgroup
308. **EBNHG5**: Scenario: Host group creation and membership updates with unified SQL and BBDO3
      * And a host group 1 is defined with 7 members across instances (hosts host_1 to host_7)
      * When the broker and engine start and the engine becomes ready
      * And the engine configuration file hostgroups.cfg is added and the engine is reloaded
      * Then the system reports 7 relations between hostgroup 1 and its hosts
      * When host host_1 is removed and hostgroup_1 members are updated to exclude host_1
      * And the engine is reloaded
      * Then the system reports 6 relations between hostgroup 1 and its hosts
309. **EBNHG6**: Scenario: Two pollers are connected, hostgroup 1 is defined on poller 0 with host_1 and
      * host_2 as members. Both hosts are then moved from poller 0's configuration to poller 1's
      * configuration, poller 1 is reloaded first, then poller 0. The hostgroup and its host
      * memberships must not disappear from the database.
310. **EBNHGU1**: New host group with several pollers and connections to DB with broker configured with unified_sql
311. **EBNHGU2**: New host group with several pollers and connections to DB with broker configured with unified_sql
312. **EBNHGU3**: New host group with several pollers and connections to DB with broker configured with unified_sql
313. **EBNHGU4_${test_label}**: New host group with several pollers and connections to DB with broker and rename this hostgroup
314. **EBNSG1**: New service group with several pollers and connections to DB
315. **EBNSG2**: Scenario: Two pollers are connected, servicegroup 1 is defined on poller 0 with
      * host_1/service_1 and host_2/service_2 as members. Both hosts and their services are then
      * moved from poller 0's configuration to poller 1's configuration, poller 1 is reloaded first,
      * then poller 0. The servicegroup and its service memberships must not disappear from the
      * database.
316. **EBNSGU1**: New service group with several pollers and connections to DB with broker configured with unified_sql
317. **EBNSGU2**: New service group with several pollers and connections to DB with broker configured with unified_sql
318. **EBNSGU3_${test_label}**: New service group with several pollers and connections to DB with broker and rename this servicegroup
319. **EBNSVC1**: New services with several pollers
320. **EBPN0**: Verify if child is in queue when parent is down.
321. **EBPN1**: verify relation parent child when delete parent.
322. **EBPN2**: verify relation parent child when delete child.
323. **EBPS2**: 1000 services are configured with 20 metrics each. The rrd output is removed from the broker configuration to avoid to write too many rrd files. While metrics are written in bulk, the database is stopped. This must not crash broker.
324. **EBSAU2**: New hosts with action_url with more than 2000 characters
325. **EBSG_1**: Scenario: Service group creation and membership updates with unified SQL and BBDO3
      * And a service group 1 is defined with 7 members across hosts (including host_1 services 1-5 and host_2 services 6-7)
      * When the broker and engine start and the engine becomes ready
      * And the engine configuration file servicegroups.cfg is added and the engine is reloaded
      * Then the system reports 7 relations between servicegroup 1 and its services
      * When service host_1/service_1 is removed and servicegroup_1 members are updated to exclude service_1
      * And the engine is reloaded
      * Then the system reports 6 relations between servicegroup 1 and its services
326. **EBSN3**: New hosts with notes with more than 500 characters
327. **EBSN4**: New hosts with No Alias / Alias and have A Template
328. **EBSNU1**: New hosts with notes_url with more than 2000 characters
329. **ENGINE_ENCRYPTION_BAD_CONF**: 
      * Given an engine with configured encryption, but without key and salt, 
      * we give him several macros and we expect to retrieve them in logs without decrypt.
      * As engine is not ready to receive encrypted macros, is_encryption_ready must be equal to 0 in db
330. **ENGINE_ENCRYPTION_GOOD_CONF**: 
      * Given an engine with configured encryption, and key and salt, 
      * we give him several macros and we expect to retrieve them in logs without decrypt.
      * As engine is ready to receive encrypted macros, is_encryption_ready must be equal to 1 in db
331. **ENRSCHE1**: Verify that next check of a rescheduled host is made at last_check + interval_check
332. **EVENT_SCRIPT_BASIC_SERVICE_CHECK**: 
      * Given an event_script output configured in broker
      * When a service check result is processed
      * Then the event is passed to the script as JSON
333. **FILTER_ON_LUA_EVENT**: stream connector with a bad configured filter generate a log error message
334. **GRAPHITE_BROKER_RESTART_TEST**: 
      * Given a central broker configured with a Graphite output (same config as GRAPHITE_FORMAT_TEST)
      * And engine already running
      * When broker is started, a service check result is processed for host_16 / service_314,
      * the TCP server receives a graphite frame, then broker is killed with SIGKILL (repeated 10 times)
      * Then each cycle produces a valid graphite metric or status frame on the TCP server
      * The goal is to test cache resilience to broker crash
335. **GRAPHITE_FORMAT_TEST**: 
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
336. **GRPC_CLOUD_FAILURE**: simulate a broker failure in cloud environment, we provide a muted grpc server and there must remain only one grpc connection. Then we start broker and connection must be ok
337. **GRPC_RECONNECT**: We restart broker and engine must reconnect to it and send data
338. **HUGE_CONF**: 
      * Given a broker with 3 pollers, we wait that all engine are ready,
      * Then once all data are saved in db, we stop broker, we start broker and we test cache content with a lua script. 
      * We also check that we don't have cache error in broker logs
339. **INFLUXDB_FORMAT_TEST**: 
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
340. **LCDNU**: the lua cache updates correctly service cache.
341. **LCDNUH**: the lua cache updates correctly host cache
342. **LOGV2DB1**: log-v2 disabled old log enabled check broker sink
343. **LOGV2DB2**: log-v2 disabled old log disabled check broker sink
344. **LOGV2DF1**: log-v2 disabled old log enabled check logfile sink
345. **LOGV2DF2**: log-v2 disabled old log disabled check logfile sink
346. **LOGV2EB1**: Checking broker sink when log-v2 is enabled and legacy logs are disabled.
347. **LOGV2EB2**: log-v2 enabled old log enabled check broker sink
348. **LOGV2EBU1**: Checking broker sink when log-v2 is enabled and legacy logs are disabled with bbdo3.
349. **LOGV2EBU2**: Check Broker sink with log-v2 enabled and legacy log enabled with BBDO3.
350. **LOGV2EF1**: log-v2 enabled    old log disabled check logfile sink
351. **LOGV2EF2**: log-v2 enabled old log enabled check logfile sink
352. **LOGV2FE2**: log-v2 enabled old log enabled check logfile sink
353. **LUA_CACHE_SAVE_BBDO3**: 
      * Given a engine broker configured in bbdo2, we check that services and hosts are stored in bbdo3 format in cache
      * To do that we compare host and service event with lua cache
354. **LUA_CACHE_SAVE_HOST_GROUP**: 
      * Given two engines configured, 
      * we create a first host group of two hosts of poller1, then we create a second host group that only contains one host of poller1
      * then we delete all hosts from second host group, we delete a host from first host
      * we create a group that contains one host of each poller, we remove first host group, we remove one host from the last created group
      * we expect that lua cache will be synchronized with all that modifications
355. **LUA_CACHE_SAVE_SERVICE_GROUP**: 
      * Given two engines configured, 
      * we create a first service group of two services of poller1, then we create a second service group that only contains one service of poller1
      * then we delete all services from second service group, we delete a service from first service
      * we create a group that contains one service of each poller, we remove first service group, we remove one service from the last created group
      * we expect that lua cache will be synchronized with all that modifications
356. **LUA_FAILURE**: 
      * Given a engine broker configured with a SC, we check write intervals in case of lua write returns nothing
357. **MOVE_HOST_OF_HOSTGROUP_TO_ANOTHER_POLLER**: Scenario: Moving hosts between pollers without losing hostgroup tag
      * Given two pollers each with two hosts
      * And all hosts belong to the same hostgroup
      * When I move two hosts from one poller to the other
      * Then the hostgroup tag of the moved hosts is not erased
358. **NON_TLS_CONNECTION_WARNING**: 
      * Given an agent starts a non-TLS connection,
      * we expect to get a warning message.
359. **NON_TLS_CONNECTION_WARNING_ENCRYPTED**: 
      * Given agent with encrypted connection, we expect no warning message.
360. **NON_TLS_CONNECTION_WARNING_FULL**: 
      * Given an agent starts a non-TLS connection,
      * we expect to get a warning message.
      * After 1 hour, we expect to get a warning message about the connection time expired
      * and the connection killed.
361. **NON_TLS_CONNECTION_WARNING_FULL_REVERSED**: 
      * Given an agent starts a non-TLS connection reverse,
      * we expect to get a warning message.
      * After 1 hour, we expect to get a warning message about the connection time expired
      * and the connection killed.
362. **NON_TLS_CONNECTION_WARNING_REVERSED**: 
      * Given an agent starts a non-TLS connection reversed,
      * we expect to get a warning message.
363. **NON_TLS_CONNECTION_WARNING_REVERSED_ENCRYPTED**: 
      * Given agent with encrypted reversed connection, we expect no warning message.
364. **NO_ENGINE_ENCRYPTION**: 
      * Given an engine without configured encryption, we give him several macros and we expect to retrieve them in logs.
      * As engine is not ready to receive encrypted macros, is_encryption_ready must be equal to 0 in db
365. **NO_FILTER_NO_ERROR**: no filter configured => no filter error.
366. **OTLP_AGENT_HOST_METADATA**: Scenario: CMA host information becomes host and os resource attributes
      * Given poller 10 owns the host and link-local filtering is left disabled
      * And exports carry no host.id
      * When an AgentHostInfo event is received for the host
      * Then exports carry typed host.* and os.* resource attributes
      * And host.ip keeps the IPv4 and IPv6 link-local addresses
      * When a later AgentHostInfo event is empty
      * Then all these attributes are removed
367. **OTLP_AGENT_HOST_METADATA_PERSISTENCE**: Scenario: Broker restores CMA host information from the OTLP output cache after a crash
      * Given poller 10 owns the host
      * And an AgentHostInfo event gave it host.id robot-machine
      * And Broker saved it in the OTLP output cache
      * When Broker is killed and restarted, and the test peer sends the host again but no AgentHostInfo
      * Then exports still carry host.id robot-machine
368. **OTLP_BBDO2_MACRO_CONVERSION**: Scenario: Broker converts BBDO2 custom variable payloads
      * Given poller 10 owns the host
      * When BBDO2 configuration events set name "legacy" and namespace "initial"
      * Then exports carry legacy/initial
      * When a BBDO2 status event changes the namespace to "runtime"
      * Then exports carry legacy/runtime
      * When a disabled BBDO2 configuration event removes the name
      * Then exports carry centreon-broker/runtime
369. **OTLP_BROKER_RESTART**: Scenario: A runtime identity survives a Broker restart
      * Given host_1 is configured with name "configured"
      * When the name is changed to "runtime" at runtime
      * Then exports carry runtime/centreon
      * When Broker is restarted while Engine keeps running
      * Then exports still carry runtime/centreon
370. **OTLP_CACHE_PERSISTENCE**: Scenario: Broker restores both macros from its persisted cache
      * Given poller 10 owns the host
      * And it sends name "persisted" as configuration and namespace "runtime" as status
      * And exports carry persisted/runtime
      * When Broker is restarted and the test peer reconnects without replaying any event
      * Then exports still carry persisted/runtime
371. **OTLP_DEFAULT_IDENTITY**: Scenario: Without OTel macros, the service identity names Broker
      * Given host_1 defines neither _OTEL_SERVICE_NAME nor _OTEL_SERVICE_NAMESPACE
      * When a passive check result of service_1 is exported
      * Then its resource carries service.name centreon-broker and service.namespace centreon
      * And its resource carries a non-empty service.version
      * And centreon.service.description stays on the datapoint, not on the resource
372. **OTLP_ENGINE_RESTART_REMOVAL**: Scenario: Macros removed while Engine is stopped do not survive its restart
      * Given host_1 is configured with name and namespace "removed"
      * And exports carry removed/removed
      * When Engine is stopped, both macros are removed from its configuration and Engine is started
      * Then its startup dump restores centreon-broker/centreon
373. **OTLP_EXCLUDE_LINK_LOCAL**: Scenario: The opt-in filter removes link-local addresses from host.ip
      * Given host_ip_exclude_link_local is enabled on the OTLP output
      * When an AgentHostInfo event lists link-local and other IPv4 and IPv6 addresses
      * Then host.ip only keeps 192.0.2.10 and 2001:db8::1
374. **OTLP_HOST_AND_INSTANCE_CLEANUP**: Scenario: Owner poller startup and host deletion clear the host identity
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
375. **OTLP_HOST_MACROS**: Scenario: Host macros override the service identity of each host independently
      * Given host_1 defines _OTEL_SERVICE_NAME, _OTEL_SERVICE_NAMESPACE and a _SECRET macro
      * And host_2 only defines _OTEL_SERVICE_NAMESPACE
      * And service_1 defines its own _OTEL_SERVICE_NAME
      * When passive check results of both hosts are exported
      * Then host_1 carries payments/shop without service.version
      * And host_2 carries centreon-broker/production
      * And no resource carries the secret macro
      * When the service_1 macro is changed at runtime
      * Then host_1 still carries payments/shop
376. **OTLP_MAPPING_RELOAD**: Scenario: The JSON metric mapping is applied and reloaded live
      * Given a mapping file renames robot_cpu to system.cpu.utilization with a 0.01 scale
      * When a check result with robot_cpu=25% is received
      * Then the exported point is system.cpu.utilization=0.25 with unit 1 and cpu.mode user
      * When the mapping file is rewritten to rename robot_cpu to robot.cpu.percent
      * Then Broker reloads it and the next point is robot.cpu.percent=26 with unit %
      * When the mapping file is replaced by invalid JSON
      * Then Broker keeps the previous mapping and the next point is robot.cpu.percent=27
377. **OTLP_MIGRATION_WITHOUT_MACROS**: Scenario: Moving a host to a poller without macros clears its old identity
      * Given host_1 is monitored by poller 0 with name and namespace "old"
      * When host_1 moves to poller 1 without OTel macros and poller 1 is reloaded
      * Then exports through poller 1 carry centreon-broker/centreon
      * When poller 0, not reloaded yet, sends a "stale" runtime name for host_1
      * Then exports through poller 0 still carry centreon-broker/centreon
      * When poller 0 is reloaded
      * Then exports through poller 1 still carry centreon-broker/centreon
378. **OTLP_OLDER_PROTOBUF_SENDERS**: Scenario: Macro events without instance_id from older senders are still accepted
      * Given a BBDO test peer whose transport source_id is not a poller ID
      * And the host moved from poller 10 to poller 20
      * When lowercase macro events without instance_id are received
      * Then exports carry legacy/compatibility
379. **OTLP_POLLER_MIGRATION**: Scenario: Delayed updates from the old poller cannot overwrite a moved host identity
      * Given host_1 is monitored by poller 0 with name and namespace "old"
      * When host_1 moves to poller 1 with name and namespace "new" and poller 1 is reloaded
      * Then exports through poller 1 carry new/new
      * When poller 0, not reloaded yet, sends "stale" runtime updates for host_1
      * Then exports through poller 0 still carry new/new
      * When poller 0 is reloaded
      * Then exports through poller 1 still carry new/new
380. **OTLP_RELOAD_AND_REMOVAL**: Scenario: Configuration reloads update and then remove host macros
      * Given host_1 is configured with name "initial" and namespace "shop"
      * When the name is changed in the configuration and Engine is reloaded
      * Then the next export carries changed/shop
      * When the updated name is removed from the configuration and Engine is reloaded
      * Then the removal reaches Broker, since the update kept is_sent
      * And service.name falls back to centreon-broker
      * When the namespace is removed too and Engine is reloaded
      * Then the identity falls back to centreon-broker/centreon
381. **OTLP_RUNTIME_MACROS**: Scenario: Runtime host macro updates apply to the next exports
      * Given host_1 is configured with name and namespace "initial"
      * When both macros are changed at runtime with surrounding spaces
      * Then the next export carries the trimmed values payments/shop
      * When the name is changed to spaces only
      * Then service.name falls back to centreon-broker and the namespace is kept
      * When the namespace is changed to an empty value
      * Then the identity falls back to centreon-broker/centreon
382. **OTLP_STALE_CONFIGURATION_EVENTS**: Scenario: Macro events are only accepted from the poller owning the host
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
383. **OTLP_TLS_EXPORT**: Scenario: The OTLP output reads its CA certificate from the path in its configuration
      * Given the collector only accepts TLS, with a self-signed certificate for 127.0.0.1
      * And the OTLP output has encryption on and ca_certificate set to the path of that certificate
      * When a check result is received
      * Then it is exported to the collector over TLS
384. **OTLP_TLS_MISSING_FILE**: Scenario: A TLS file that cannot be read is reported with the parameter that names it
      * Given the OTLP output has encryption on and ca_certificate set to a path that does not exist
      * When Broker starts
      * Then Broker stops with an error
      * And it logs that it cannot read ca_certificate for the robot-otlp endpoint
385. **RENAME_PARENT**: 
      * Given an host with a parent host. We rename the parent host and check if the child host is still linked to the parent.
      * Engine mustn't crash and log an error on reload.
386. **RLCode**: Test if reloading LUA code in a stream connector applies the changes
387. **RRD1**: RRD metric rebuild asked with gRPC API. Three non existing indexes IDs are selected then an error message is sent. This is done with unified_sql output.
388. **SDER**: The check attempts and the max check attempts of (host_1,service_1) are changed to 280 thanks to the retention.dat file. Then Engine and Broker are started and Broker should write these values in the services and resources tables. We only test the services table because we need a resources table that allows bigger numbers for these two attributes. But we see that Broker doesn't crash anymore.
389. **SEVERAL_FILTERS_ON_LUA_EVENT**: Two stream connectors with different filters are configured.
390. **STORAGE_ON_LUA**: The category 'storage' is applied on the stream connector. Only events of this category should be sent to this stream.
391. **STUPID_FILTER**: Unified SQL is configured with only the bbdo category as filter. Broker should run correctly.
392. **Service_increased_huge_check_interval**: New services with huge check interval at creation time. Given Engine and Broker are configured with 1 poller and 10 hosts When Engine is started Then host_1 should be pending When a check result with metrics is processed for service_1 Then metrics should be created and sent to rrd broker When service_1 metrics are analyzed Then metrics should have minimal heartbeat of 3000 and pdp_per_row of 300 When a new service is created with a check interval of 90 And Engine is reloaded Then the new service should be pending When a check result with metrics is processed for the new service Then metrics should be created and sent to rrd Broker When new service metrics are analyzed Then metrics should have minimal heartbeat of 54000 and pdp_per_row of 5400
393. **Services_and_bulks_${id}**: One service is configured with one metric with a name of 150 to 1021 characters.
394. **Start_Stop_Broker_Engine_${id}**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
395. **Start_Stop_Engine_Broker_${id}**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
396. **UNIFIED_SQL_FILTER**: With bbdo version 3.0.1, we watch events written or rejected in unified_sql
397. **VICT_ONE_CHECK_METRIC**: victoria metrics metric output
398. **VICT_ONE_CHECK_METRIC_AFTER_FAILURE**: victoria metrics metric output after victoria shutdown
399. **VICT_ONE_CHECK_STATUS**: 
      * Given a Victoria Metrics setup with Centreon Broker
      * When a service check result is processed
      * Then the service status transitions through OK, WARNING, and CRITICAL states
      * And the corresponding status updates are sent to Victoria Metrics
400. **Whitelist_Directory_NotReadable**: 
      * Given a centengine started by centreon-engine user, whitelist directories are not readable and centengine must log an error
401. **Whitelist_Directory_Rights**: log if /etc/centreon-engine-whitelist has not mandatory rights or owner
402. **Whitelist_Empty_Directory**: log if /etc/centreon-engine-whitelist is empty
403. **Whitelist_Host**: Test on allowed and forbidden commands for hosts
404. **Whitelist_No_Whitelist_Directory**: log if /etc/centreon-engine-whitelist doesn't exist
405. **Whitelist_NotReadable**: 
      * Given a centengine started by centreon-engine user, whitelist files are not readable and centengine must log an error
406. **Whitelist_Perl_Connector**: test allowed and forbidden commands for services
407. **Whitelist_Service**: test allowed and forbidden commands for services
408. **Whitelist_Service_EH**: test allowed and forbidden event handler for services
409. **metric_mapping**: Check if metric name exists using a stream connector
410. **not1**: This test case configures a single service and verifies that a notification is sent when the service is in a non-OK HARD state.
411. **not10**: This test case involves scheduling downtime on a down host that already had a critical notification. When The Host return to UP state we should receive a recovery notification.
412. **not11**: This test case involves configuring one service and checking that three alerts are sent for it.
413. **not12**: Escalations
414. **not13**: notification for a dependencies host
415. **not14**: notification for a Service dependency
416. **not15**: several notification commands for the same user.
417. **not16**: notification for dependencies services group
418. **not17**: notification for a dependensies host group
419. **not18**: notification delay where first notification delay equal retry check
420. **not19**: notification delay where first notification delay greater than retry check 
421. **not1_2**: Scenario: Service notification command expands contact macros
      * Given a single service 'service_1' is assigned to contact 'John_Doe' with notifications enabled, options (w,c,r), and period 24x7
      * And 'service_1' has check/retry intervals of 1 and max check attempts of 1
      * And contact 'John_Doe' uses notification command 'command_with_macro' with macros
      * When the broker and engine are started and the service is driven to a HARD CRITICAL state
      * Then the service reaches HARD CRITICAL
      * And a service notification is logged for 'service_1' in CRITICAL state using 'command_with_macro'
      * And the processed notification command in logs is "/bin/echo pager address5 " showing macros are expanded
422. **not1_WL_KO**: This test case configures a single service. When it is in non-OK HARD state a notification should be sent but it is not allowed by the whitelist
423. **not1_WL_OK**: This test case configures a single service. When it is in non-OK HARD state a notification is sent because it is allowed by the whitelist
424. **not1_reload**: This test case configures a single service and set the service in a non-OK HARD state so engine sends a notification. Then the service is removed from the configuration and Engine is reloaded. And Engine doesn't crash.
425. **not2**: This test case configures a single service and verifies that a recovery notification is sent
426. **not20**: notification delay where first notification delay smaller than retry check
427. **not21**: No notifications should be sent for UNKNOWN services when host goes DOWN. When a host transitions to DOWN (soft or hard), services become UNKNOWN due to host unreachability. These services should not trigger notifications.
428. **not3**: This test case configures a single service and verifies the notification system's behavior during and after downtime
429. **not4**: This test case configures a single service and verifies the notification system's behavior during and after acknowledgement
430. **not5**: This test case configures two services with two different users being notified when the services transition to a critical state.
431. **not6**: This test case validate the behavior when the notification time period is set to null.
432. **not7**: This test case simulates a host alert scenario.
433. **not8**: This test validates the critical host notification.
434. **not9**: This test case configures a single host and verifies that a recovery notification is sent after the host recovers from a non-OK state.
435. **not_in_timeperiod_with_send_recovery_notifications_anyways**: This test case configures a single service and verifies that a notification is sent when the service is in a non-OK state and OK is sent outside timeperiod when _send_recovery_notifications_anyways is set
436. **not_in_timeperiod_without_send_recovery_notifications_anyways**: This test case configures a single service and verifies that a notification is sent when the service is in a non-OK state and OK is not sent outside timeperiod when _send_recovery_notifications_anyways is not set
437. **not_remove_host_and_service_in_downtime**: 
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
9. **ECI4**: 
      * Given a contact inherits from two templates with overlapping attributes
      * When the engine is started
      * Then the contact's attributes are correctly resolved based on the inheritance rules
10. **ECMI0**: Verify command inheritance : command(empty) inherit from template (full) , on Start Engine
11. **ECMI1**: Verify command inheritance : command(full) inherit from template (full) , on Start Engine
12. **ECMI2**: Verify command inheritance : command(empty) inherit from template (full) , on Reload Engine
13. **ECMI3**: Verify command inheritance : command(full) inherit from template (full) , on reload Engine
14. **ECMI4**: 
      * Given a command inherits from two templates with full configurations,
      * When the engine starts,
      * Then the command should inherit the command line from the first template
      * And the connector from the second template.
15. **ECMI5**: 
      * Given a command inherits from a template with full configurations,
      * When the engine reloads,
      * Then the command should inherit the command line from the first template
      * And the connector from the second template.
16. **ECOI0**: Verify connector inheritance : connector(empty) inherit from template (full) , on Start Engine
17. **ECOI1**: Verify connector inheritance : connector(full) inherit from template (full) , on Start Engine
18. **ECOI2**: Verify connector inheritance : connector(empty) inherit from template (full) , on Reload Engine
19. **ECOI3**: Verify connector inheritance : connector(full) inherit from template (full) , on Reload Engine
20. **ECOI4**: 
      * Given a connector inherits from two templates
      * When the engine starts
      * Then the connector should use the connector_line from the second template
21. **EESI0**: Verify service escalation : create service escalation for every service in a service group
22. **EESI1**: Verify service escalation  inheritance : escalation(empty) inherit from template (full) , on Start Engine
23. **EESI10**: 
      * Given a service escalation configuration with multiple inheritance templates,
      * When the engine is started,
      * Then the service escalation attributes are correctly inherited and applied to the services in the service group,
      * And services not in the group have no escalation applied.
24. **EESI11**: 
      * Given a host escalation configuration with multiple inheritance templates,
      * When the engine is started,
      * Then the host escalation attributes are correctly inherited and applied to the hosts in the host group,
      * And hosts not in the group have no escalation applied.
25. **EESI12**: Verify host escalation configured with host_name directly (not hostgroup_name) is properly linked to the host and triggers escalation contacts.
26. **EESI13**: Verify service escalation configured with host_name and service_description directly (not servicegroup_name) is properly linked to the service and triggers escalation contacts.
27. **EESI2**: Verify service escalation  inheritance : escalation(full) inherit from template (full) , on Start Engine
28. **EESI3**: Verify service escalation  inheritance : escalation(empty) inherit from template (full) , on Reload Engine
29. **EESI4**: Verify service escalation  inheritance : escalation(full) inherit from template (full) , on Reload Engine
30. **EESI5**: Verfiy host escalation : create host escalation for every host in the hostgroup
31. **EESI6**: Verify host escalation inheritance : escalation(empty) inherit from template (full) , on Start Engine   
32. **EESI7**: Verify host escalation inheritance : escalation(full) inherit from template (full) , on Start Engine    
33. **EESI8**: Verify host escalation inheritance : escalation(empty) inherit from template (full) , on Reload Engine   
34. **EESI9**: Verify host escalation inheritance : escalation(full) inherit from template (full) , on Reload Engine    
35. **EFHC1**: Engine is configured with hosts and we force check one 5 times with bbdo2
36. **EFHC2**: Engine is configured with hosts and we force check on one 5 times on bbdo2
37. **EFHCU1**: Engine is configured with hosts and we force checks on one 5 times on bbdo3. Bbdo3 has no impact on this behavior. resources table is cleared before starting broker.
38. **EFHCU2**: Engine is configured with hosts and we force checks on one 5 times on bbdo3. Bbdo3 has no impact on this behavior.
39. **EHGI0**: Verify hostgroup inheritance : hostgroup(empty) inherit from template (full) , on Start Engine
40. **EHGI1**: Verify hostgroup inheritance : hostgroup(full) inherit from template (full) , on Start Engine
41. **EHGI2**: Verify hostgroup inheritance : hostgroup(empty) inherit from template (full) , on Reload Engine
42. **EHGI3**: Verify hostgroup inheritance : hostgroup(full) inherit from template (full) , on Reload Engine
43. **EHGI4**: 
      * Given a Centreon engine with a predefined configuration of host groups,
      * When the engine processes the host group inheritance rules,
      * Then the resulting configuration should reflect the expected inheritance structure.
44. **EHI0**: Verify inheritance host : host(empty) inherit from template (full) , on Start Engine
45. **EHI1**: Verify inheritance host : host(full) inherit from template (full) , on Start engine
46. **EHI2**: Verify inheritance host : host(empty) inherit from template (full) , on Reload engine
47. **EHI3**: Verify inheritance host : host(full) inherit from template (full) , on engine Reload
48. **EHI4**: 
      * Given a parent host with specific configurations,
      * When a child host is defined to inherit from the parent,
      * Then the child host should correctly apply the inherited configurations.
49. **EMACROS**: macros ADMINEMAIL and ADMINPAGER are replaced in check outputs
50. **EMACROS_NOTIF**: macros ADMINEMAIL and ADMINPAGER are replaced in notification commands
51. **EMACROS_SEMICOLON**: Macros with a semicolon are used even if they contain a semicolon.
52. **EMTI0**: Verify multiple inheritance host
53. **EMTI1**: 
      * Given a host with multiple templates
      * When the templates are configured with custom variables
      * Then the custom variables should be correctly inherited and accessible
54. **ENGINE_MANY_CHECKS**: 
      * Given a engine with many services and a unique check on each service with it's own env variables
      * We expect correct check result in logs and we checks returned args and service macros
55. **ENGINE_MANY_CHECK_OK**: 
      * Given a engine with many services and a command shared between several services
      * We expect correct check result in logs and we checks returned args and service macros
      * All odd indexed services use perl connector to do checks
56. **ENGINE_MANY_LONG_CHECKS**: 
      * Given a engine with many services and a unique check on each service with it's own env variables and a variable check duration
      * We expect no memory leak in centengine despite checks timeouts
57. **EPC1**: Check with perl connector
58. **ERL**: Engine is started and writes logs in centengine.log. Then we remove the log file. The file disappears but Engine is still writing into it. Engine is reloaded and the centengine.log should appear again.
59. **ESGI0**: Verify servicegroup inheritance : servicegroup(empty) inherit from template (full) , on Start Engine
60. **ESGI1**: Verify servicegroup inheritance : servicegroup(empty) inherit from template (full) , on Start Engine
61. **ESGI2**: Verify servicegroup inheritance : servicegroup(empty) inherit from template (full) , on Reload Engine
62. **ESGI3**: Verify servicegroup inheritance : servicegroup(empty) inherit from template (full) , on Reload Engine
63. **ESGI4**: 
      * Given a configured Centreon engine with defined service groups
      * When the inheritance rules are applied
      * Then the service group inheritance should be correctly reflected in the monitoring data
64. **ESI0**: Verify inheritance service : Service(empty) inherit from template (full) , on Start Engine
65. **ESI1**: Verify inheritance service : Service(full) inherit from template (full) , on Start Engine
66. **ESI2**: Verify inheritance service : Service(empty) inherit from template (full) , on Reload Engine
67. **ESI3**: Verify inheritance service : Service(full) inherit from template (full) , on Reload Engine
68. **ESI4**: 
      * Given the Centreon engine is running with a predefined configuration.
      * When a service inherits properties from its parent service.
      * Then the inherited properties should match the expected values.
69. **ESS1**: Start-Stop (0s between start/stop) 5 times one instance of engine and no coredump
70. **ESS2**: Start-Stop (300ms between start/stop) 5 times one instance of engine and no coredump
71. **ESS3**: Start-Stop (0s between start/stop) 5 times three instances of engine and no coredump
72. **ESS4**: Start-Stop (300ms between start/stop) 5 times three instances of engine and no coredump
73. **ESSCTO**: Scenario: Engine services timeout due to missing Perl connector
      * Given the Engine is configured as usual without the Perl connector
      * When the Engine executes its service commands
      * Then the commands take too long and reach the timeout
      * And the Engine starts and stops two times as a result
74. **ESSCTOWC**: Scenario: Engine services timeout due to missing Perl connector
      * Given the Engine is configured as usual with some command using the Perl connector
      * When the Engine executes its service commands
      * Then the commands take too long and reach the timeout
      * And the Engine starts and stops two times as a result
75. **ESSOCWNV**: Scenario: Engine is started with a valid old configuration (concerning cbmod)
      * Given the Engine is configured with a valid old configuration
      * When the Engine is started
      * Then the Engine starts correctly
      * And the Engine stops correctly
76. **ESS_STATS**: Scenario: Reading the stats file after Engine has started
      * Given the Engine is started
      * When we read the Engine's stats file
      * Then Engine must not crash
77. **EVOCWNV**: Scenario: The new Engine checks the old configuration (concerning cbmod)
      * Given the Engine is configured with a valid old configuration
      * When the Engine is started to check the configuration
      * Then the Engine reads it as expected
78. **EXT_CONF1**: Engine configuration is overided by json conf
79. **EXT_CONF2**: Engine configuration is overided by json conf after reload
80. **E_FD_LIMIT**: Engine here is started with a low file descriptor limit. The engine should not crash and limit should be set.
81. **E_HOST_DOWN_DISABLE_SERVICE_CHECKS**: host_down_disable_service_checks is set to 1, host down switch all services to UNKNOWN
82. **E_HOST_UNREACHABLE_DISABLE_SERVICE_CHECKS**: host_down_disable_service_checks is set to 1, host unreachable switch all services to UNKNOWN
83. **HOST_DEPENDENCY**: 
      * Given an engine with a host dependency configured, we change it.
      * We expect no error in logs and that configuration is well updated
84. **NOT_MODIFY_HOST_ESCALATION**: 
      * Given an engine with a host escalation configured, we add a contact group to it.
      * We expect no error in logs and that configuration is well updated
85. **NOT_MODIFY_SERVICE_ESCALATION**: 
      * Given an engine with a service escalation configured, we add a contact group to it.
      * We expect no error in logs and that configuration is well updated
86. **NO_BROKER_LOG**: 
      * Given a configuration without broker logs directory, engine must start correctly
87. **NO_HOST_CHECK_COMMAND**: 
      * Given a host without check command, engine should not crash
88. **SERVICE_DEPENDENCY**: 
      * Given an engine with a service dependency configured, we change it.
      * We expect no error in logs and that configuration is well updated
89. **TIMEPERIOD_INHERITANCE**: 
      * Given two timeperiods, one inherit from the second. First onex exclude next hour, checks must not be executed in next 5 minutes
90. **TIMEPERIOD_INHERITANCE_EXCEPTION**: 
      * Given two timeperiods, one inherit from the second. First one allow H24, second one has only one exception, checks must be executed in next minute
91. **TIMEPERIOD_MERGE**: 
      * Given a timeperiod with its own calendar exception (active now) and a template
      * with a different calendar exception (active yesterday), the merge must preserve the child's
      * exception. The service using the child timeperiod as check_period must be checked.
92. **VERIF**: 
      * When centengine is started in verification mode, it does not log in its file.
93. **VERIFIY_CONF**: Verify deprecated engine configuration options are logged as warnings Given the engine and broker are configured with module 1 And the engine configuration is set with deprecated options When the engine is started Then a warning message for 'auto_reschedule_checks' should be logged And a warning message for 'auto_rescheduling_interval' should be logged And a warning message for 'auto_rescheduling_window' should be logged And the engine should be stopped

### Migration
1. **MIGRATION**: Migration bbdo2 with sql/storage to bbdo2 with unified_sql and then to bbdo3 with unified_sql and then to bbdo2 with unified_sql and then to bbdo2 with sql/storage

### Severities
1. **BESEV1**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Broker is started before.
2. **BESEV2**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Engine is started before.
3. **BETUHSEV1**: Hosts have severities provided by templates.
4. **BETUSEV1**: Services have severities provided by templates.
5. **BETUSEV2**: A service and a host have severities. Then we remove severities from both of them and check they no longer have severities.
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


716 tests currently implemented.
