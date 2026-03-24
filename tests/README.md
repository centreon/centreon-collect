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
        GitPython unqlite py-cpuinfo pyjwt


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
15. **BGRPCSS1**: Start-Stop two instances of broker configured with grpc stream and no coredump
16. **BGRPCSS2**: Start/Stop 10 times broker configured with grpc stream with 300ms interval and no coredump
17. **BGRPCSS3**: Start-Stop one instance of broker configured with grpc stream and no coredump
18. **BGRPCSS4**: Start/Stop 10 times broker configured with grpc stream with 1sec interval and no coredump
19. **BGRPCSS5**: Start-Stop with reversed connection on grpc acceptor with only one instance and no deadlock
20. **BGRPCSSU1**: Start-Stop with unified_sql two instances of broker with grpc stream and no coredump
21. **BGRPCSSU2**: Start/Stop with unified_sql 10 times broker configured with grpc stream with 300ms interval and no coredump
22. **BGRPCSSU3**: Start-Stop with unified_sql one instance of broker configured with grpc and no coredump
23. **BGRPCSSU4**: Start/Stop with unified_sql 10 times broker configured with grpc stream with 1sec interval and no coredump
24. **BGRPCSSU5**: Start-Stop with unified_sql with reversed connection on grpc acceptor with only one instance and no deadlock
25. **BLBD**: Start Broker with loggers levels by default
26. **BLDIS1**: Start broker with core logs 'disabled'
27. **BLEC1**: Change live the core level log from trace to debug
28. **BLEC2**: Change live the core level log from trace to foo raises an error
29. **BLEC3**: Change live the foo level log to trace raises an error
30. **BSCSS1**: Start-Stop two instances of broker and no coredump
31. **BSCSS2**: Start/Stop 10 times broker with 300ms interval and no coredump
32. **BSCSS3**: Start-Stop one instance of broker with tcp connection and no coredump
33. **BSCSS4**: Start/Stop 10 times broker with 1sec interval and no coredump
34. **BSCSSC1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is enabled on client side.
35. **BSCSSC2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is disabled on client side.
36. **BSCSSCG1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on client side.
37. **BSCSSCGRR1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on output side. Reversed connection with retention and grpc transport protocol.
38. **BSCSSCGRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on output side. Reversed connection with retention and grpc transport protocol.
39. **BSCSSCRR1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is enabled on client side. Connection reversed with retention.
40. **BSCSSCRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is disabled on client side. Connection reversed with retention.
41. **BSCSSG1**: Start-Stop two instances of broker and no coredump
42. **BSCSSG2**: Start/Stop 10 times broker with 300ms interval and no coredump
43. **BSCSSG3**: Start-Stop one instance of broker with grpc connection and no coredump
44. **BSCSSG4**: Start/Stop 10 times broker with 1sec interval and no coredump
45. **BSCSSGA1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. An authorization token is added on the server. Error messages are raised.
46. **BSCSSGA2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. An authorization token is added on the server and also on the client. All looks ok.
47. **BSCSSGRR1**: Start-Stop two instances of broker and no coredump, reversed and retention, with transport protocol grpc, start-stop 5 times.
48. **BSCSSK1**: Start-Stop two instances of broker, server configured with grpc and client with tcp. No connectrion established and error raised on client side.
49. **BSCSSK2**: Start-Stop two instances of broker, server configured with tcp and client with grpc. No connection established and error raised on client side.
50. **BSCSSP1**: Start-Stop two instances of broker and no coredump. The server contains a listen address
51. **BSCSSPRR1**: Start-Stop two instances of broker and no coredump. The server contains a listen address, reversed and retention. centreon-broker-master-rrd is then a failover.
52. **BSCSSR1**: Start-Stop two instances of broker and no coredump. Connection with bbdo_server/bbdo_client and reversed.
53. **BSCSSRR1**: Start-Stop two instances of broker and no coredump. Connection with bbdo_server/bbdo_client, reversed and retention. centreon-broker-master-rrd is then a failover.
54. **BSCSSRR2**: Start/Stop 10 times broker with 300ms interval and no coredump, reversed and retention. centreon-broker-master-rrd is then a failover.
55. **BSCSST1**: Start-Stop two instances of broker and no coredump. Encryption is enabled on client side.
56. **BSCSST2**: Start-Stop two instances of broker and no coredump. Encryption is enabled on client side.
57. **BSCSSTG1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. This is not sufficient, then an error is raised.
58. **BSCSSTG2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. It works with good certificates and keys.
59. **BSCSSTG3**: Start-Stop two instances of broker. The connection cannot be established if the server private key is missing and an error message explains this issue.
60. **BSCSSTGRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. It works with good certificates and keys. Reversed grpc connection with retention.
61. **BSCSSTRR1**: Start-Stop two instances of broker and no coredump. Encryption is enabled. transport protocol is tcp, reversed and retention.
62. **BSCSSTRR2**: Start-Stop two instances of broker and no coredump. Encryption is enabled.
63. **BSS1**: Start-Stop two instances of broker and no coredump
64. **BSS2**: Start/Stop 10 times broker with 300ms interval and no coredump
65. **BSS3**: Start-Stop one instance of broker 5 times and no coredump
66. **BSS4**: Start/Stop 10 times broker with 1sec interval and no coredump
67. **BSS5**: Start-Stop with reversed connection on TCP acceptor with only one instance and no deadlock
68. **BSSU1**: Start-Stop two instances of broker with BBDO3 and no coredump
69. **BSSU2**: Start/Stop 10 times broker (BBDO3) with 300ms interval and no coredump
70. **BSSU3**: Start-Stop one instance of broker (BBDO3) and no coredump
71. **BSSU4**: Start/Stop 10 times broker with 1sec interval and no coredump
72. **BSSU5**: Start-Stop with reversed connection on TCP acceptor with only one instance and no deadlock
73. **START_STOP_CBD**: restart cbd with unified_sql services state must not be null after restart

### Broker/database
1. **DEDICATED_DB_CONNECTION_${nb_conn}_${store_in_data_bin}**: count database connection
2. **NetworkDBFail6**: 
     * **GIVEN** a Broker configured with 5 database connections
     * **WHEN** the network connection to the database (port 3306) is disrupted for 60 seconds
     * **THEN** Broker should lose database connectivity during the outage
     * **AND** should resume normal operations after network restoration
3. **NetworkDBFail7**: 
     * **GIVEN** Broker is running with 5 database connections
     * **AND** Engine is connected to Broker
     * **AND** database queries are being executed successfully
     * **WHEN** the network connection on port 3306 is repeatedly disrupted (6 cycles of 10s down / 10s up)
     * **THEN** Broker should handle the intermittent network failures
     * **AND** should acknowledge all events once the network is stable
4. **NetworkDBFail8**: 
     * **GIVEN** Broker with unified_sql and 3 database connections
     * **WHEN** database network is blocked until failure detection
     * **THEN** Broker should log database errors
     * **AND** should recover and execute pending statements after network restoration
5. **NetworkDBFailU6**: 
     * **GIVEN** Broker is running with unified_sql and 5 database connections
     * **AND** Engine is connected to Broker using BBDO3 protocol
     * **AND** database queries are being executed successfully
     * **WHEN** the network connection on port 3306 is blocked for 60 seconds
     * **THEN** database operations should fail during the network outage
     * **AND** Broker should recover and acknowledge events after network restoration
6. **NetworkDBFailU7**: 
     * **GIVEN** Broker is running with unified_sql and 5 database connections
     * **AND** Engine is connected to Broker using BBDO3 protocol
     * **AND** database queries are being executed successfully
     * **WHEN** the network connection on port 3306 is repeatedly disrupted (6 cycles of 10s down / 10s up)
     * **THEN** Broker should handle the intermittent network failures
     * **AND** should acknowledge all events once the network is stable
7. **NetworkDBFailU8**: 
     * **GIVEN** Broker is running with unified_sql, BBDO3 protocol and 3 database connections
     * **AND** Engine is connected to Broker
     * **AND** database queries are being executed successfully
     * **WHEN** the network connection on port 3306 is blocked indefinitely
     * **THEN** Broker should detect the database failure and log appropriate errors
     * **AND WHEN** the network is restored
     * **THEN** Broker should reconnect and successfully execute pending statements
8. **NetworkDbFail1**: network failure test between broker and database (shutting down connection for 100ms)
9. **NetworkDbFail2**: network failure test between broker and database (shutting down connection for 1s)
10. **NetworkDbFail3**: network failure test between broker and database (shutting down connection for 10s)
11. **NetworkDbFail4**: network failure test between broker and database (shutting down connection for 30s)
12. **NetworkDbFail5**: network failure test between broker and database (shutting down connection for 60s)

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
107. **BENCH_${nb_checks}STATUS**: external command CHECK_SERVICE_RESULT 1000 times
108. **BENCH_${nb_checks}STATUS_TRACES**: external command CHECK_SERVICE_RESULT ${nb_checks} times
109. **BENCH_${nb_checks}_REVERSE_SERVICE_STATUS_TRACES_WITHOUT_SQL**: Broker is configured without SQL output. The connection between Engine and Broker is reversed. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times. Logs are in trace level.
110. **BENCH_${nb_checks}_REVERSE_SERVICE_STATUS_WITHOUT_SQL**: Broker is configured without SQL output. The connection between Engine and Broker is reversed. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times.
111. **BENCH_${nb_checks}_SERVICE_STATUS_TRACES_WITHOUT_SQL**: Broker is configured without SQL output. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times. Logs are in trace level.
112. **BENCH_${nb_checks}_SERVICE_STATUS_WITHOUT_SQL**: Broker is configured without SQL output. External command CHECK_SERVICE_RESULT is sent ${nb_checks} times.
113. **BENCH_1000STATUS_100${suffixe}**: external command CHECK_SERVICE_RESULT 100 times    with 100 pollers with 20 services
114. **BENCV**: Engine is configured with hosts/services. The first host has no customvariable. Then we add a customvariable to the first host and we reload engine. Then the host should have this new customvariable defined and centengine should not crash.
115. **BENHG1**: 
     * **GIVEN** a Centreon platform with 3 Engine instances
     * **AND** Broker is configured with RRD, central and module outputs
     * **AND** the central broker has 5 database connections
     * **WHEN** I create a host group containing 3 hosts
     * **AND** I reload both Broker and Engine configurations
     * **THEN** the membership of all 3 hosts to the host group should be logged
     * **AND** all membership entries should appear within 45 seconds
116. **BENHG4**: 
     * **GIVEN** a platform with 3 Engine instances and unified_sql output with 5 connections
     * **AND** detailed logging is enabled on module0 (neb debug, core and processing error)
     * **WHEN** I create host group 1 with 3 hosts and reload configurations
     * **THEN** at least 2 host memberships should be logged within 45 seconds
     * **WHEN** I rename host group 1 to "hostgroup_test" and reload configurations
     * **THEN** the hostgroup name should be updated in database within 60 seconds
117. **BENHGU1**: 
     * **GIVEN** a Centreon platform with 3 Engine instances
     * **AND** Broker is configured with RRD, central and module outputs
     * **AND** Broker uses unified_sql output for database operations
     * **AND** SQL logging is enabled at info level
     * **AND** the unified_sql output has 5 database connections
     * **WHEN** I create a host group containing 3 hosts
     * **AND** I reload both Broker and Engine configurations
     * **THEN** the membership of all 3 hosts to the host group should be logged
     * **AND** all membership entries should appear within 45 seconds
118. **BENHGU2**: 
     * **GIVEN** a platform with 3 Engine instances and unified_sql output with 5 connections
     * **AND** BBDO3 protocol is enabled
     * **WHEN** I create a host group with 3 hosts and reload configurations
     * **THEN** at least 2 host memberships should be logged within 45 seconds
119. **BENHGU3**: 
     * **GIVEN** a platform with 4 Engine instances and unified_sql output with 5 connections
     * **AND** BBDO3 protocol is enabled with SQL debug logging
     * **WHEN** I create host group 1 across 4 pollers with 3 hosts each and reload
     * **THEN** host group 1 should contain 12 host members within 30 seconds
     * **WHEN** I remove the hostgroups configuration from poller 0 and reload
     * **THEN** host group 1 should contain only 9 host members within 30 seconds
120. **BENHGU4_${test_label}**: 
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
121. **BENSVC1**: New services with several pollers
122. **BEOTEL_CENTREON_AGENT_CEIP**: **SCENARIO:** Agent and "centreon_storage.agent_information" Statistics

     * **GIVEN** Engine connected to Broker
     * **WHEN** an agent connects to Engine
     * **THEN** a message is sent to Broker that results in a new row in the "centreon_storage.agent_information" table.
123. **BEOTEL_CENTREON_AGENT_CHECK_COUNTER**: 
     * **GIVEN** an agent with counter check, we expect to get the correct status for the centagent process running on windows host
124. **BEOTEL_CENTREON_AGENT_CHECK_DIFFERENT_INTERVAL**: 
     * **GIVEN** a Centreon Engine with OpenTelemetry server module configured
     * **AND** an OTEL connector using centreon_agent processor with 5s export period
     * **AND** 3 passive services configured with different check intervals (1, 2, 3 minutes)
     * **AND** interval_length is set to 10 seconds
     * **WHEN** the Engine, Broker and Agent are started
     * **THEN** service_1 should execute checks every 10 seconds (1*10) with 5s tolerance
     * **AND** service_2 should execute checks every 20 seconds (2*10) with 5s tolerance
     * **AND** service_3 should execute checks every 30 seconds (3*10) with 5s tolerance
     * **AND** all check intervals should be verified within 80 seconds
125. **BEOTEL_CENTREON_AGENT_CHECK_EVENTLOG**: 
     * **GIVEN** an agent with eventlog check, we expect status, output and metrics
126. **BEOTEL_CENTREON_AGENT_CHECK_FILES**: 
     * **GIVEN** an agent with file check, we expect to get the correct status for files under monitoring on the Windows host
127. **BEOTEL_CENTREON_AGENT_CHECK_HEALTH**: agent check health and we expect to get it in check result
128. **BEOTEL_CENTREON_AGENT_CHECK_HOST**: 
     * **GIVEN** an agent host checked by centagent, we set a first output to check command, 
     modify it, reload engine and expect the new output in resource table
129. **BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED**: agent check host with encrypted connection and we expect to get it in check result
130. **BEOTEL_CENTREON_AGENT_CHECK_HOST_CRYPTED_ENCRYPTED_CREDENTIALS**: 
     * **GIVEN** an agent host checked by centagent over an encrypted connection,
     Engine use credentials encryption and send encrypted commands
     we set a first output to check command,
     modify it, reload engine and expect the new output in resource table
131. **BEOTEL_CENTREON_AGENT_CHECK_HOST_NO_ENCRYPTED_CREDENTIALS**: 
     * **GIVEN** an agent host checked by centagent over a non encrypted connection,
     Engine use credentials encryption, but send no encrypted commands
     we set a first output to check command,
     modify it, reload engine and expect the new output in resource table
132. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_CPU**: agent check service with native check cpu and we expect to get it in check result
133. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_MEMORY**: agent check service with native check memory and we expect to get it in check result
134. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_SERVICE**: agent check service with native check service and we expect to get it in check result
135. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_STORAGE**: agent check service with native check storage and we expect to get it in check result
136. **BEOTEL_CENTREON_AGENT_CHECK_NATIVE_UPTIME**: agent check service with native check uptime and we expect to get it in check result
137. **BEOTEL_CENTREON_AGENT_CHECK_PROCESS**: 
     * **GIVEN** an agent with eventlog check, we expect to get the correct status for thr centagent process running on windows host
138. **BEOTEL_CENTREON_AGENT_CHECK_SERVICE**: agent check service and we expect to get it in check result
139. **BEOTEL_CENTREON_AGENT_CHECK_TASKSCHEDULER**: 
     * **GIVEN** an agent with task scheduler check, we expect to get the correct status for the centagent process running on windows host
140. **BEOTEL_CENTREON_AGENT_LINUX_NO_DEFUNCT_PROCESS**: agent check host and we expect to get it in check result
141. **BEOTEL_CENTREON_AGENT_NO_TRUSTED_TOKEN**: 
     * **GIVEN** the Centreon Engine is configured with OpenTelemetry server with encryption enabled with no trusted_token
     * **WHEN** the Centreon Agent attempts to connect with tls
     * **THEN** the connection should be accepted
142. **BEOTEL_CENTREON_AGENT_TOKEN**: 
     * **GIVEN** the Centreon Engine is configured with OpenTelemetry server with encryption enabled
     * **WHEN** the Centreon Agent attempts to connect using an valid JWT token
     * **THEN** the connection should be accepted
     * **AND** the log should confirm that the token is valid
143. **BEOTEL_CENTREON_AGENT_TOKEN_AGENT_TELEGRAPH**: 
     * **GIVEN** an OpenTelemetry server is configured with token-based connection
144. **BEOTEL_CENTREON_AGENT_TOKEN_AGENT_TELEGRAPH_2**: 
     * **GIVEN** an OpenTelemetry server is configured with token-based connection
145. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED**: 
     * **GIVEN** the OpenTelemetry server is configured with encryption enabled
146. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED_WHILE_RUNNING**: 
     * **GIVEN** the OpenTelemetry server is configured with encryption enabled
147. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRED_WHILE_RUNNING_REVERSE**: 
     * **GIVEN** the Centreon Engine is configured as client with token and the agent as server with encryption enables
148. **BEOTEL_CENTREON_AGENT_TOKEN_EXPIRE_REVERSE**: 
     * **GIVEN** the Centreon Engine is configured as client with token and the agent as server with encryption enables
     * **WHEN** the Centreon engine attempts to connect using an valid JWT token but expired
     * **THEN** the connection should be refused
     * **AND** the log should confirm that the token is expired
149. **BEOTEL_CENTREON_AGENT_TOKEN_MISSING_HEADER**: 
     * **GIVEN** the Centreon Engine is configured with OpenTelemetry server with encryption enabled
     * **WHEN** the Centreon Agent attempts to connect without a JWT token
     * **THEN** the connection should be refused
     * **AND** the log should contain the message "UNAUTHENTICATED: No authorization header"
150. **BEOTEL_CENTREON_AGENT_TOKEN_REVERSE**: 
     * **GIVEN** the Centreon Engine is configured as client with token and the agent as server with encryption enables
     * **WHEN** the Centreon engine attempts to connect using an valid JWT token
     * **THEN** the connection should be accepted
     * **AND** the log should confirm that the token is valid
151. **BEOTEL_CENTREON_AGENT_TOKEN_UNTRUSTED**: 
     * **GIVEN** the OpenTelemetry server is configured with encryption enabled
152. **BEOTEL_CENTREON_AGENT_TOKEN_UNTRUSTED_REVERSE**: 
     * **GIVEN** the Centreon Engine is configured as client with token and the agent as server with encryption enables
     * **WHEN** the Centreon engine attempts to connect using an invalid JWT token
     * **THEN** the connection should be refused
     * **AND** the log should confirm that the token is not trusted
153. **BEOTEL_CENTREON_AGENT_WHITE_LIST**: **SCENARIO:** Enforcing command whitelist for agent checks

     * **GIVEN** a whitelist file is created with allowed commands for host_1
     * **AND** the engine, broker, and agent are configured and started
     * **WHEN** a check command matching the whitelist is executed for host_1
     * **THEN** the check result is accepted and stored in the resources table
     * **WHEN** a check command not matching the whitelist is configured for host_1 and engine is reloaded
     * **THEN** the command is rejected and a "command not allowed by whitelist" message appears in the log
154. **BEOTEL_INVALID_CHECK_COMMANDS_AND_ARGUMENTS**: 
     * **GIVEN** the agent is configured with native checks for services
     * **AND** the OpenTelemetry server module is added
     * **AND** services are configured with incorrect check commands and arguments
     * **WHEN** the broker, engine, and agent are started
     * **THEN** the resources table should be updated with the correct status
     * **AND** appropriate error messages should be generated for invalid checks
155. **BEOTEL_REVERSE_CENTREON_AGENT_CHECK_HOST**: agent check host with reversed connection and we expect to get it in check result
156. **BEOTEL_REVERSE_CENTREON_AGENT_CHECK_HOST_CRYPTED**: agent check host with encrypted reversed connection and we expect to get it in check result
157. **BEOTEL_REVERSE_CENTREON_AGENT_CHECK_SERVICE**: agent check service with reversed connection and we expect to get it in check result
158. **BEOTEL_SERVE_TELEGRAF_CONFIGURATION_CRYPTED**: we configure engine with a telegraf conf server and we check telegraf conf file
159. **BEOTEL_SERVE_TELEGRAF_CONFIGURATION_NO_CRYPTED**: we configure engine with a telegraf conf server and we check telegraf conf file
160. **BEOTEL_SERVE_TELEGRAF_CONFIGURATION_NO_CRYPTED_1**: **SCENARIO:** Serve telegraf configuration with a complex whitelist

     * **GIVEN** the engine is configured with a telegraf conf server and a complex whitelist
     * **WHEN** I request the telegraf conf file for host_1
     * **THEN** I should receive the expected telegraf configuration for host_1
     * **AND** service_3 should be blacklisted and unavailable for host_1
     * **WHEN** I request the telegraf conf file for host_2
     * **THEN** I should receive the expected telegraf configuration for host_2
     * **AND** service_5 should be blacklisted and unavailable for host_2
161. **BEOTEL_TELEGRAF_CHECK_HOST**: we send nagios telegraf formatted data and we expect to get it in check result
162. **BEOTEL_TELEGRAF_CHECK_SERVICE**: **SCENARIO:** Handling of OK and CRITICAL check results from Telegraf input

     * **GIVEN** the OpenTelemetry server is ready
     * **WHEN** I send a Telegraf-formatted check result with status "OK" to the Engine
     * **THEN** the result should be stored in the Centreon Broker storage database with status "OK"
163. **BEPBBEE1**: central-module configured with bbdo_version 3.0 but not others. Unable to establish connection.
164. **BEPBBEE3**: bbdo_version 3 generates new bbdo protobuf service status messages.
165. **BEPBBEE4**: bbdo_version 3 generates new bbdo protobuf host status messages.
166. **BEPBBEE5**: bbdo_version 3 generates new bbdo protobuf service messages.
167. **BEPBCVS**: bbdo_version 3 communication of custom variables.
168. **BEPBHostParent**: bbdo_version 3 communication of host parent relations
169. **BEPBINST_CONF**: bbdo_version 3 communication of instance configuration.
170. **BEPBRI1**: bbdo_version 3 use pb_resource new bbdo protobuf ResponsiveInstance message.
171. **BERD1**: **SCENARIO:** Starting/stopping Broker does not create duplicated events.
172. **BERD2**: **SCENARIO:** Starting/stopping Engine does not create duplicated events.
173. **BERDUC1**: **SCENARIO:** Starting/stopping Engine does not create duplicated events in usual cases
174. **BERDUC2**: **SCENARIO:** Starting/stopping Engine does not create duplicated events in usual cases
175. **BERDUC3U1**: **SCENARIO:** Starting/stopping Broker does not create duplicated events in usual cases with unified_sql and BBDO 3.0
176. **BERDUC3U2**: **SCENARIO:** Starting/stopping Engine does not create duplicated events in usual cases with unified_sql and BBDO 3.0
177. **BERDUCA300**: **SCENARIO:** Starting/stopping Engine is stopped; it should emit a stop event and receive an ack event with events to clean from broker.
178. **BERDUCA301**: **SCENARIO:** Starting/stopping Engine is stopped; it should emit a stop event and receive an ack event with events to clean from broker with bbdo 3.0.1.
179. **BERDUCU1**: Starting/stopping Broker does not create duplicated events in usual cases with unified_sql7
180. **BERDUCU2**: **SCENARIO:** Starting/stopping Engine does not create duplicated events in usual cases with unified_sql
181. **BERES1**: store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
182. **BESAU2**: New hosts with action_url with more than 2000 characters
183. **BESERVCHECK**: external command CHECK_SERVICE_RESULT
184. **BESN3**: New hosts with notes with more than 500 characters
185. **BESNU1**: New hosts with notes_url with more than 2000 characters
186. **BESS1**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
187. **BESS2**: **SCENARIO:** Start and stop Broker/Engine with Broker started first and Engine stopped first

     * **GIVEN** the Broker is started before the Engine and both use BBDO 3
     * **WHEN** the Engine is started after the Broker
     * **THEN** the connection between Engine and Broker should be established
     * **AND** the poller should be visible in the database
     * **WHEN** the Engine is stopped before the Broker
     * **THEN** the poller should be disabled and not visible in the database
     * **AND** neither Broker nor Engine should crash
188. **BESS3**: Start-Stop Broker/Engine - Engine started first - Engine stopped first
189. **BESS4**: Start-Stop Broker/Engine - Engine started first - Broker stopped first
190. **BESS5**: Start-Stop Broker/engine - Engine debug level is set to all, it should not hang
191. **BESS6_${label}**: **SCENARIO:** Verify Broker and Engine start and establish connections

     * **GIVEN** the Central Broker, RRD Broker, and Central Engine are started
     * **WHEN** we check the connection between them
     * **THEN** the connection should be well established
     * **AND** the central broker should have two peers connected: the central engine and the RRD broker
     * **AND** the RRD broker should correctly recognize its peer as the Central Broker
192. **BESSBQ1**: A very bad queue file is written for broker. Broker and Engine are then started, Broker must read the file raising an error because of that file and then get data sent by Engine.
193. **BESSCTO**: **SCENARIO:** Service commands time out due to missing Perl Connector

     * **GIVEN** the Engine is configured as usual but without the Perl Connector
     * **WHEN** the Engine executes its service commands
     * **THEN** the commands take too long and reach the timeout
     * **AND** the Engine starts and stops two times as a result
194. **BESSCTOWC**: **SCENARIO:** Service commands time out due to missing Perl Connector

     * **GIVEN** the Engine is configured as usual with some commands using the Perl Connector
     * **WHEN** the Engine executes its service commands
     * **THEN** the commands take too long and reach the timeout
     * **AND** the Engine starts and stops two times as a result
195. **BESSG**: **SCENARIO:** Broker handles connection and disconnection with Engine

     * **GIVEN** Broker is configured with only one output that is Graphite
     * **WHEN** the Engine starts and connects to the Broker
     * **THEN** the Broker must be able to handle the connection
     * **WHEN** the Engine stops
     * **THEN** the Broker must be able to handle the disconnection
196. **BESS_CRYPTED_GRPC1**: Start-Stop grpc version Broker/Engine - well configured
197. **BESS_CRYPTED_GRPC2**: Start-Stop grpc version Broker/Engine only server crypted
198. **BESS_CRYPTED_GRPC3**: Start-Stop grpc version Broker/Engine only engine crypted
199. **BESS_CRYPTED_REVERSED_GRPC1**: Start-Stop grpc version Broker/Engine - well configured
200. **BESS_CRYPTED_REVERSED_GRPC2**: Start-Stop grpc version Broker/Engine only engine server crypted
201. **BESS_CRYPTED_REVERSED_GRPC3**: Start-Stop grpc version Broker/Engine only engine crypted
202. **BESS_ENGINE_DELETE_HOST**: once engine and cbd started, stop and restart cbd, delete an host and reload engine, cbd mustn't core
203. **BESS_GRPC1**: Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped first
204. **BESS_GRPC2**: Start-Stop grpc version Broker/Engine - Broker started first - Engine stopped first
205. **BESS_GRPC3**: Start-Stop grpc version Broker/Engine - Engine started first - Engine stopped first
206. **BESS_GRPC4**: Start-Stop grpc version Broker/Engine - Engine started first - Broker stopped first
207. **BESS_GRPC5**: Start-Stop grpc version Broker/engine - Engine debug level is set to all, it should not hang
208. **BESS_GRPC_COMPRESS1**: Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped last compression activated
209. **BETAG1**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Broker is started before.
210. **BETAG2**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
211. **BEUTAG1**: Engine is configured with some tags. When broker receives them through unified_sql stream, it stores them in the centreon_storage.tags table. Broker is started before.
212. **BEUTAG10**: some services are configured with tags on two pollers. Then tags are removed from some of them and in centreon_storage, we can observe resources_tags table updated.
213. **BEUTAG11**: **SCENARIO:** Updating resource tags after changing several tags

     * **GIVEN** some services are configured with tags on two pollers
     * **THEN** the resources_tags table contains them
     * **WHEN** several tags are changed
     * **THEN** the resources_tags table is updated
214. **BEUTAG12**: Engine is configured with some tags. Group tags tag2, tag6 are set to hosts 1 and 2. Category tags tag4 and tag8 are added to hosts 2, 3, 4. The resources and resources_tags tables are well filled. The tag6 and tag8 are removed and resources_tags is also well updated.
215. **BEUTAG2**: Engine is configured with some tags. A new service is added with a tag. Broker should make the relations.
216. **BEUTAG3**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
217. **BEUTAG4**: Engine is configured with some tags. Group tags tag9, tag13 are set to services 1 and 3. Category tags tag3 and tag11 are added to services 1, 3, 5 and 6. The centreon_storage.resources and resources_tags tables are well filled.
218. **BEUTAG5**: Engine is configured with some tags. Group tags tag2, tag6 are set to hosts 1 and 2. Category tags tag4 and tag8 are added to hosts 2, 3, 4. The resources and resources_tags tables are well filled.
219. **BEUTAG6**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.resources_tags table. Engine is started before.
220. **BEUTAG7**: Some services are configured with tags on two pollers. Then tags configuration is modified.
221. **BEUTAG8**: Services have tags provided by templates.
222. **BEUTAG9**: hosts have tags provided by templates.
223. **BEUTAG_REMOVE_HOST_FROM_HOSTGROUP**: remove a host from hostgroup, reload, insert 2 host in the hostgroup must not make sql error
224. **BE_BACKSLASH_CHECK_RESULT**: external command PROCESS_SERVICE_CHECK_RESULT with \:
225. **BE_DEFAULT_NOTIFICATION_INTERVAL_IS_ZERO_SERVICE_RESOURCE**: default notification_interval must be set to NULL in services, hosts and resources tables.
226. **BE_FLAPPING_HOST_RESOURCE**: With BBDO 3, flapping detection must be set in hosts and resources tables.
227. **BE_FLAPPING_SERVICE_RESOURCE**: With BBDO 3, flapping detection must be set in services and resources tables.
228. **BE_NOTIF_OVERFLOW**: bbdo 2.0 notification number =40000. make an overflow => notification_number null in db
229. **BE_TIME_NULL_SERVICE_RESOURCE**: With BBDO 3, notification_interval time must be set to NULL on 0 in services, hosts and resources tables.
230. **BRCS1**: Broker reverse connection stopped
231. **BRCTS1**: Broker reverse connection too slow
232. **BRCTSMN**: 
     * **GIVEN** Broker, Engine configured as usual
     * **AND** map also connected to Broker with a filter allowing only 'neb' category
     * **WHEN** Engine sends pb_service, pb_host, pb_service_status and pb_host_status
     * **THEN** map receives correctly them.
233. **BRCTSMNS**: 
     * **GIVEN** Broker, Engine configured as usual
     * **AND** map also connected to Broker with a filter allowing 'neb' and 'storage' categories
     * **WHEN** Engine sends pb_service, pb_host, pb_service_status, pb_host_status and metrics
     * **THEN** Map receives correctly them.
234. **BRGC1**: Broker good reverse connection
235. **BRRDCDDID1**: RRD metrics deletion from index ids with rrdcached.
236. **BRRDCDDIDDB1**: RRD metrics deletion from index ids with a query in centreon_storage with rrdcached.
237. **BRRDCDDIDU1**: RRD metrics deletion from index ids with unified sql output with rrdcached.
238. **BRRDCDDM1**: RRD metrics deletion from metric ids with rrdcached.
239. **BRRDCDDMDB1**: RRD metrics deletion from metric ids with a query in centreon_storage and rrdcached.
240. **BRRDCDDMID1**: RRD deletion of non existing metrics and indexes with rrdcached
241. **BRRDCDDMIDU1**: RRD deletion of non existing metrics and indexes with rrdcached
242. **BRRDCDDMU1**: RRD metric deletion on table metric with unified sql output with rrdcached
243. **BRRDCDRB1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with storage/sql sql output and rrdcached.
244. **BRRDCDRBDB1**: RRD metric rebuild with a query in centreon_storage and unified sql with rrdcached
245. **BRRDCDRBU1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with unified_sql output and rrdcached.
246. **BRRDCDRBUDB1**: RRD metric rebuild with a query in centreon_storage and unified sql with rrdcached
247. **BRRDDID1**: RRD metrics deletion from index ids.
248. **BRRDDIDDB1**: RRD metrics deletion from index ids with a query in centreon_storage.
249. **BRRDDIDU1**: RRD metrics deletion from index ids with unified sql output.
250. **BRRDDM1**: RRD metrics deletion from metric ids.
251. **BRRDDMDB1**: RRD metrics deletion from metric ids with a query in centreon_storage.
252. **BRRDDMID1**: RRD deletion of non existing metrics and indexes
253. **BRRDDMIDU1**: RRD deletion of non existing metrics and indexes
254. **BRRDDMU1**: RRD metric deletion on table metric with unified sql output
255. **BRRDRBDB1**: RRD metric rebuild with a query in centreon_storage and unified sql
256. **BRRDRBUDB1**: RRD metric rebuild with a query in centreon_storage and unified sql
257. **BRRDRM1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with storage/sql sql output.
258. **BRRDRMU1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with unified_sql output.
259. **BRRDSTATUS**: We are working with BBDO3. This test checks status are correctly handled independently from their value.
260. **BRRDSTATUSRETENTION**: We are working with BBDO3. This test checks status are not sent twice after Engine reload.
261. **BRRDUPLICATE**: RRD metric rebuild with a query in centreon_storage and unified sql with duplicate rows in database
262. **BRRDWM1**: We are working with BBDO3. This test checks protobuf metrics and status are sent to cbd RRD.
263. **CBD_RELOAD_AND_FILTERS**: We start engine/broker with a classical configuration. All is up and running. Some filters are added to the rrd output and cbd is reloaded. All is still up and running but some events are rejected. Then all is newly set as filter and all events are sent to rrd broker.
264. **CBD_RELOAD_AND_FILTERS_WITH_OPR**: We start engine/broker with an almost classical configuration, just the connection between cbd central and cbd rrd is reversed with one peer retention. All is up and running. Some filters are added to the rrd output and cbd is reloaded. All is still up and running but some events are rejected. Then all is newly set as filter and all events are sent to rrd broker.
265. **DTIM**: New services with several pollers are created. Then downtimes are set on all configured hosts. This action results on 5250 downtimes if we also count impacted services. Then all these downtimes are removed. This test is done with BBDO 3.0.1
266. **EBBM1**: A service status contains metrics that do not fit in a float number.
267. **EBBPS1**: 1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table resources.
268. **EBBPS2**: 1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table services.
269. **EBDP1**: Four new pollers are started and then we remove Poller3.
270. **EBDP2**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
271. **EBDP3**: Three new pollers are started, then they are killed. It is still possible to remove Poller2 if removed from the configuration.
272. **EBDP4**: Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by Broker.
273. **EBDP5**: Four new pollers are started and then we remove Poller3.
274. **EBDP6**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
275. **EBDP7**: Three new pollers are started, then they are killed. It is still possible to remove Poller2 if removed from the configuration.
276. **EBDP8**: Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by broker.
277. **EBDP_GRPC2**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
278. **EBMSSM**: 1000 services are configured with 100 metrics each. The rrd output is removed from the broker configuration. GetSqlManagerStats is called to measure writes into data_bin.
279. **EBMSSMDBD**: 1000 services are configured with 100 metrics each. The rrd output is removed from the broker configuration. While metrics are written in the database, we stop the database and then restart it. Broker must recover its connection to the database and continue to write metrics.
280. **EBMSSMPART**: **SCENARIO:** Broker continues writing metrics after partition recreation

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
281. **EBNSG1**: New service group with several pollers and connections to DB
282. **EBNSGU1**: New service group with several pollers and connections to DB with broker configured with unified_sql
283. **EBNSGU2**: New service group with several pollers and connections to DB with broker configured with unified_sql
284. **EBNSGU3_${test_label}**: New service group with several pollers and connections to DB with broker and rename this servicegroup
285. **EBPN0**: Verify if child is in queue when parent is down.
286. **EBPN1**: verify relation parent child when delete parent.
287. **EBPN2**: verify relation parent child when delete child.
288. **EBPS2**: 1000 services are configured with 20 metrics each. The rrd output is removed from the broker configuration to avoid to write too many rrd files. While metrics are written in bulk, the database is stopped. This must not crash broker.
289. **EBSAU2**: New services with action_url with more than 2000 characters
290. **EBSN3**: New services with notes with more than 500 characters
291. **EBSN4**: New hosts with No Alias / Alias and have A Template
292. **EBSNU1**: New services with notes_url with more than 2000 characters
293. **ENRSCHE1**: Verify that next check of a rescheduled host is made at last_check + interval_check
294. **FILTER_ON_LUA_EVENT**: stream connector with a bad configured filter generate a log error message
295. **GRPC_CLOUD_FAILURE**: simulate a broker failure in cloud environment, we provide a muted grpc server and there must remain only one grpc connection. Then we start broker and connection must be ok
296. **GRPC_RECONNECT**: We restart broker and engine must reconnect to it and send data
297. **LCDNU**: the lua cache updates correctly service cache.
298. **LCDNUH**: the lua cache updates correctly host cache
299. **LOGV2DB1**: log-v2 disabled old log enabled check broker sink
300. **LOGV2DB2**: log-v2 disabled old log disabled check broker sink
301. **LOGV2DF1**: log-v2 disabled old log enabled check logfile sink
302. **LOGV2DF2**: log-v2 disabled old log disabled check logfile sink
303. **LOGV2EB1**: Checking broker sink when log-v2 is enabled and legacy logs are disabled.
304. **LOGV2EB2**: log-v2 enabled old log enabled check broker sink
305. **LOGV2EBU1**: Checking broker sink when log-v2 is enabled and legacy logs are disabled with bbdo3.
306. **LOGV2EBU2**: Check Broker sink with log-v2 enabled and legacy log enabled with BBDO3.
307. **LOGV2EF1**: log-v2 enabled    old log disabled check logfile sink
308. **LOGV2EF2**: log-v2 enabled old log enabled check logfile sink
309. **LOGV2FE2**: log-v2 enabled old log enabled check logfile sink
310. **LUA_CACHE_SAVE_BBDO3**: 
     * **GIVEN** a engine broker configured in bbdo2, we check that services and hosts are stored in bbdo3 format in cache
     To do that we compare host and service event with lua cache
311. **MOVE_HOST_OF_HOSTGROUP_TO_ANOTHER_POLLER**: **SCENARIO:** Moving hosts between pollers without losing hostgroup tag

     * **GIVEN** two pollers each with two hosts
     * **AND** all hosts belong to the same hostgroup
     * **WHEN** I move two hosts from one poller to the other
     * **THEN** the hostgroup tag of the moved hosts is not erased
312. **NON_TLS_CONNECTION_WARNING**: 
     * **GIVEN** an agent starts a non-TLS connection,
     we expect to get a warning message.
313. **NON_TLS_CONNECTION_WARNING_ENCRYPTED**: 
     * **GIVEN** agent with encrypted connection, we expect no warning message.
314. **NON_TLS_CONNECTION_WARNING_FULL**: 
     * **GIVEN** an agent starts a non-TLS connection,
     we expect to get a warning message.
     After 1 hour, we expect to get a warning message about the connection time expired
     * **AND** the connection killed.
315. **NON_TLS_CONNECTION_WARNING_FULL_REVERSED**: 
     * **GIVEN** an agent starts a non-TLS connection reverse,
     we expect to get a warning message.
     After 1 hour, we expect to get a warning message about the connection time expired
     * **AND** the connection killed.
316. **NON_TLS_CONNECTION_WARNING_REVERSED**: 
     * **GIVEN** an agent starts a non-TLS connection reversed,
     we expect to get a warning message.
317. **NON_TLS_CONNECTION_WARNING_REVERSED_ENCRYPTED**: 
     * **GIVEN** agent with encrypted reversed connection, we expect no warning message.
318. **NO_FILTER_NO_ERROR**: no filter configured => no filter error.
319. **RENAME_PARENT**: 
     * **GIVEN** an host with a parent host. We rename the parent host and check if the child host is still linked to the parent.
     Engine mustn't crash and log an error on reload.
320. **RLCode**: Test if reloading LUA code in a stream connector applies the changes
321. **RRD1**: RRD metric rebuild asked with gRPC API. Three non existing indexes IDs are selected then an error message is sent. This is done with unified_sql output.
322. **SDER**: The check attempts and the max check attempts of (host_1,service_1) are changed to 280 thanks to the retention.dat file. Then Engine and Broker are started and Broker should write these values in the services and resources tables. We only test the services table because we need a resources table that allows bigger numbers for these two attributes. But we see that Broker doesn't crash anymore.
323. **SEVERAL_FILTERS_ON_LUA_EVENT**: Two stream connectors with different filters are configured.
324. **STORAGE_ON_LUA**: The category 'storage' is applied on the stream connector. Only events of this category should be sent to this stream.
325. **STUPID_FILTER**: Unified SQL is configured with only the bbdo category as filter. An error is raised by broker and broker should run correctly.
326. **Service_increased_huge_check_interval**: **SCENARIO:** New services with huge check interval at creation time.

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
327. **Services_and_bulks_${id}**: One service is configured with one metric with a name of 150 to 1021 characters.
328. **Start_Stop_Broker_Engine_${id}**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
329. **Start_Stop_Engine_Broker_${id}**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
330. **UNIFIED_SQL_FILTER**: With bbdo version 3.0.1, we watch events written or rejected in unified_sql
331. **VICT_ONE_CHECK_METRIC**: victoria metrics metric output
332. **VICT_ONE_CHECK_METRIC_AFTER_FAILURE**: victoria metrics metric output after victoria shutdown
333. **VICT_ONE_CHECK_STATUS**: victoria metrics status output
334. **Whitelist_Directory_NotReadable**: 
     * **GIVEN** a centengine started by centreon-engine user, whitelist directories are not readable and centengine must log an error
335. **Whitelist_Directory_Rights**: log if /etc/centreon-engine-whitelist has not mandatory rights or owner
336. **Whitelist_Empty_Directory**: log if /etc/centreon-engine-whitelist is empty
337. **Whitelist_Host**: Test on allowed and forbidden commands for hosts
338. **Whitelist_No_Whitelist_Directory**: log if /etc/centreon-engine-whitelist doesn't exist
339. **Whitelist_NotReadable**: 
     * **GIVEN** a centengine started by centreon-engine user, whitelist files are not readable and centengine must log an error
340. **Whitelist_Perl_Connector**: test allowed and forbidden commands for services
341. **Whitelist_Service**: test allowed and forbidden commands for services
342. **Whitelist_Service_EH**: test allowed and forbidden event handler for services
343. **metric_mapping**: Check if metric name exists using a stream connector
344. **not1**: This test case configures a single service and verifies that a notification is sent when the service is in a non-OK HARD state.
345. **not10**: This test case involves scheduling downtime on a down host that already had a critical notification. When The Host return to UP state we should receive a recovery notification.
346. **not11**: This test case involves configuring one service and checking that three alerts are sent for it.
347. **not12**: Escalations
348. **not13**: notification for a dependencies host
349. **not14**: notification for a Service dependency
350. **not15**: several notification commands for the same user.
351. **not16**: notification for dependencies services group
352. **not17**: notification for a dependensies host group
353. **not18**: notification delay where first notification delay equal retry check
354. **not19**: notification delay where first notification delay greater than retry check 
355. **not1_WL_KO**: This test case configures a single service. When it is in non-OK HARD state a notification should be sent but it is not allowed by the whitelist
356. **not1_WL_OK**: This test case configures a single service. When it is in non-OK HARD state a notification is sent because it is allowed by the whitelist
357. **not1_reload**: This test case configures a single service and set the service in a non-OK HARD state so engine sends a notification. Then the service is removed from the configuration and Engine is reloaded. And Engine doesn't crash.
358. **not2**: This test case configures a single service and verifies that a recovery notification is sent
359. **not20**: notification delay where first notification delay samller than retry check
360. **not3**: This test case configures a single service and verifies the notification system's behavior during and after downtime
361. **not4**: This test case configures a single service and verifies the notification system's behavior during and after acknowledgement
362. **not5**: This test case configures two services with two different users being notified when the services transition to a critical state.
363. **not6**: This test case validate the behavior when the notification time period is set to null.
364. **not7**: This test case simulates a host alert scenario.
365. **not8**: This test validates the critical host notification.
366. **not9**: This test case configures a single host and verifies that a recovery notification is sent after the host recovers from a non-OK state.
367. **not_in_timeperiod_with_send_recovery_notifications_anyways**: **SCENARIO:** Verify notification is sent when service is in non-OK state and recovery is sent outside timeperiod if setting is enabled

     * **GIVEN** a configured single service
     * **AND** the service enters a non-OK state
     * **WHEN** the service remains in a non-OK state
     * **THEN** a notification should be sent
     * **AND** an OK notification should be sent outside the time period
     * **WHEN** the setting "_send_recovery_notifications_anyways" is set
368. **not_in_timeperiod_without_send_recovery_notifications_anyways**: **SCENARIO:** Verify notification is sent when service is in non-OK state and recovery is not sent outside timeperiod

     * **GIVEN** a configured single service
     * **AND** the service enters a non-OK state
     * **WHEN** the service remains in a non-OK state
     * **THEN** a notification should be sent
     * **AND** no OK notification should be sent outside the time period
     * **WHEN** the setting "send_recovery_notifications_anyways" is not set

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
7. **BECNSG3**: FIXME DBO: This test is broken because we currently have no cache. Test about lua cache. But the centralized configuration currently breaks the broker cache. FIXME: we have to fix this test when the centralized broker cache will be done.
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
9. **BECPN0**: **FEATURE:** Parent-Child Host Dependency Management
     As a monitoring administrator
     I want child host checks to be queued when parent hosts are down
     So that unnecessary checks are avoided
10. **BECPN1**: **FEATURE:** Parent Host Deletion Management
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
11. **BECPN2**: **FEATURE:** Child Host Deletion Management
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
12. **BECSS1**: **SCENARIO:** Broker sends configuration to engine in new generation

     * **GIVEN** an engine configuration is provided to the broker
     * **AND** the broker and engine are started in new generation (broker first)
     * **AND** the protocol is bbdo3
     * **WHEN** the broker detects the configuration for the engine
     * **THEN** the broker sends the configuration to the engine
     * **THEN** both broker and engine are stopped (engine first)
13. **BECSS2**: **SCENARIO:** Broker sends configuration to engine in new generation

     * **GIVEN** an engine configuration is provided to the broker
     * **AND** the broker and engine are started in new generation (broker first)
     * **AND** the protocol is bbdo3
     * **WHEN** the broker detects the configuration for the engine
     * **THEN** the broker sends the configuration to the engine
     * **THEN** both broker and engine are stopped (engine first)
14. **BECSS3**: **SCENARIO:** Broker sends configuration to engine in new generation

     * **GIVEN** an engine configuration is provided to the broker
     * **AND** the broker and engine are started in new generation (engine first)
     * **AND** the protocol is bbdo3
     * **WHEN** the broker detects the configuration for the engine
     * **THEN** the broker sends the configuration to the engine
     * **THEN** both broker and engine are stopped (engine first)
15. **BECSS4**: **SCENARIO:** Broker sends configuration to engine in new generation

     * **GIVEN** an engine configuration is provided to the broker
     * **AND** the broker and engine are started in new generation (engine first)
     * **AND** the protocol is bbdo3
     * **WHEN** the broker detects the configuration for the engine
     * **THEN** the broker sends the configuration to the engine
     * **THEN** both broker and engine are stopped (broker first)
16. **BECSSBQ1**: A very bad queue file is written for broker. Broker and Engine are then started, Broker must read the file raising an error because of that file and then get data sent by Engine.
17. **BECSS_CRYPTED_GRPC1**: Start-Stop grpc version Broker/Engine - well configured FIXME DBO: We don't have the global cache for now. So this test can't pass. The first step, broker ends the full configuration to engine. At the second step, both restart and know the engine configuration. Broker doesn't send it to engine and engine doesn't ask for it. The global diff is not aware of the engine configuration. Disabled resources are not re-enabled. And we can't just enable resources match this poller id because disabling a resource means two things: 1) This resource is really disabled for now. 2) This resource doesn't exist anymore.  In the first case, the resource must be enabled whereas in the second case, the resource must not be enabled.
18. **BECSS_CRYPTED_GRPC2**: Start-Stop grpc version Broker/Engine only server crypted
19. **BECSS_CRYPTED_REVERSED_GRPC1**: Start-Stop grpc version Broker/Engine - well configured
20. **BECSS_CRYPTED_REVERSED_GRPC2**: Start-Stop grpc version Broker/Engine only engine server crypted
21. **BECSS_CRYPTED_REVERSED_GRPC3**: Start-Stop grpc version Broker/Engine only engine crypted
22. **BECSS_ENGINE_DELETE_HOST**: once engine and cbd started, stop and restart cbd, delete an host and reload engine, cbd mustn't core
23. **BECSS_GRPC1**: **SCENARIO:** Broker sends configuration to engine in new generation

     * **GIVEN** an engine configuration is provided to the broker
     * **AND** the broker and engine are started in new generation (broker first)
     * **AND** the protocol is bbdo3
     * **WHEN** the broker detects the configuration for the engine
     * **THEN** the broker sends the configuration to the engine
     * **THEN** both broker and engine are stopped (engine first)
24. **BECSS_GRPC2**: **SCENARIO:** Broker sends configuration to engine in new generation

     * **GIVEN** an engine configuration is provided to the broker
     * **AND** the broker and engine are started in new generation (broker first)
     * **AND** the protocol is bbdo3
     * **WHEN** the broker detects the configuration for the engine
     * **THEN** the broker sends the configuration to the engine
     * **THEN** both broker and engine are stopped (engine first)
25. **BECSS_GRPC3**: **SCENARIO:** Broker sends configuration to engine in new generation

     * **GIVEN** an engine configuration is provided to the broker
     * **AND** the broker and engine are started in new generation (engine first)
     * **AND** the protocol is bbdo3
     * **WHEN** the broker detects the configuration for the engine
     * **THEN** the broker sends the configuration to the engine
     * **THEN** both broker and engine are stopped (engine first)
26. **BECSS_GRPC4**: **SCENARIO:** Broker sends configuration to engine in new generation

     * **GIVEN** an engine configuration is provided to the broker
     * **AND** the broker and engine are started in new generation (engine first)
     * **AND** the protocol is bbdo3
     * **WHEN** the broker detects the configuration for the engine
     * **THEN** the broker sends the configuration to the engine
     * **THEN** both broker and engine are stopped (broker first)
27. **BECSS_GRPC_COMPRESS1**: Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped last compression activated
28. **BECTAG1**: **FEATURE:** Tag Management between Engine and Broker
     As a Centreon administrator
     I want to configure tags in Engine
     So that Broker stores them correctly in centreon_storage.tags table
     Background:

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
29. **CANO_CFG_SENSITIVITY_SAVED**: 
     * **GIVEN** an anomaly detection service is configured with a specific sensitivity value in configuration
     * **AND** the threshold file contains prediction data with sensitivity parameters
     * **WHEN** the engine and broker are started and then stopped
     * **THEN** the configuration-based sensitivity value should be persisted in the retention data
     because CFG sensitivity parameters are properly saved during retention processing
30. **CANO_DT1**: 
     * **GIVEN** an anomaly detection service is configured with a dependent service relationship
     * **AND** both services are running normally
     * **WHEN** a downtime is scheduled on the dependent service
     * **THEN** the dependent service should enter downtime state
     * **AND** the anomaly detection service should automatically inherit the downtime
     because anomaly detection services inherit downtime from their dependent services
31. **CANO_DT2**: 
     * **GIVEN** an anomaly detection service is configured with a dependent service relationship
     * **AND** both services are running normally
     * **WHEN** a downtime is scheduled on the dependent service
     * **THEN** the anomaly detection service should automatically enter downtime
     * **WHEN** the downtime is deleted from the dependent service
     * **THEN** the anomaly detection service should automatically exit downtime
     because anomaly detection downtime should follow its dependent service downtime state
32. **CANO_DT3**: 
     * **GIVEN** an anomaly detection service is configured with a dependent service relationship
     * **AND** both services are running normally
     * **WHEN** a downtime is scheduled on the dependent service
     * **THEN** the anomaly detection service should automatically enter downtime
     * **WHEN** the downtime is deleted from the anomaly detection service
     * **THEN** the dependent service should remain in its original downtime state
     because deleting downtime on anomaly detection should not affect dependent service downtimes
33. **CANO_DT4**: **SCENARIO:** Removing downtime from service keeps it on anomaly detection

     * **GIVEN** an anomaly detection is attached to a service
     * **AND** a downtime is set on both the service and the anomaly detection
     * **WHEN** the downtime is removed from the service
     * **THEN** the downtime should still be present on the anomaly detection
34. **CANO_EXTCMD_SENSITIVITY_SAVED**: 
     * **GIVEN** an anomaly detection service is configured with threshold data
     * **AND** the service is running with initial sensitivity parameters
     * **WHEN** an external command updates the anomaly sensitivity value
     * **AND** the engine and broker are stopped
     * **THEN** the updated sensitivity value should be persisted in the retention data
     because external command sensitivity changes are properly saved during retention processing
35. **CANO_JSON_SENSITIVITY_NOT_SAVED**: 
     * **GIVEN** an anomaly detection service is configured with threshold data including sensitivity
     * **AND** the threshold file contains prediction data with a specific sensitivity value
     * **WHEN** the engine and broker are started and then stopped
     * **THEN** the sensitivity value should not be persisted in the retention data
     because JSON sensitivity parameters are not saved during retention processing
36. **CANO_NOFILE**: 
     * **GIVEN** an anomaly detection service is configured for metric monitoring
     * **AND** the threshold configuration file is missing from the system
     * **WHEN** the service processes a check result with critical state
     * **THEN** the anomaly detection service must transition to UNKNOWN state
     because it cannot determine thresholds without the configuration file
37. **CANO_OUT_LOWER_THAN_LIMIT**: 
     * **GIVEN** an anomaly detection service is configured with valid threshold data
     * **AND** the threshold file contains lower and upper limits for the metric
     * **WHEN** a service check provides performance data below the lower threshold limit
     * **THEN** the anomaly detection service must transition to CRITICAL state
     because the metric value indicates an anomalous condition requiring attention
38. **CANO_OUT_UPPER_THAN_LIMIT**: 
     * **GIVEN** an anomaly detection service is configured with valid threshold data
     * **AND** the threshold file contains lower and upper limits for the metric
     * **WHEN** a service check provides performance data above the upper threshold limit
     * **THEN** the anomaly detection service must transition to CRITICAL state
     because the metric value indicates an anomalous condition requiring attention
39. **CANO_TOO_OLD_FILE**: 
     * **GIVEN** an anomaly detection service is configured with metric monitoring
     * **AND** a threshold file exists but contains outdated prediction data
     * **WHEN** the service processes a check result with performance data
     * **THEN** the anomaly detection service must transition to UNKNOWN state
     because the threshold data is too old to be reliable for current predictions
40. **CAOUTLU1**: 
     * **GIVEN** an anomaly detection service is configured with valid threshold data using BBDO3 protocol
     * **AND** the threshold file contains lower and upper limits for the metric
     * **WHEN** a service check provides performance data above the upper threshold limit
     * **THEN** the anomaly detection service must transition to CRITICAL state
     * **AND** the resources table should contain SERVICE, HOST and ANOMALY_DETECTION type entries
41. **CBEUDHOSTS**: 
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
42. **Centralized_Start_Stop_Broker_Engine_${id}**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
43. **Centralized_Start_Stop_Engine_Broker_${id}**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
44. **RENAME_PARENT**: **FEATURE:** Parent Host Rename Management
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
43. **ENGINE_MANY_CHECKS**: 
     * **GIVEN** a engine with many services and a unique check on each service with it's own env variables
     We expect correct check result in logs and we checks returned args and service macros
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
58. **ESSCTO**: **SCENARIO:** Engine services timeout due to missing Perl connector

     * **GIVEN** the Engine is configured as usual without the Perl connector
     * **WHEN** the Engine executes its service commands
     * **THEN** the commands take too long and reach the timeout
     * **AND** the Engine starts and stops two times as a result
59. **ESSCTOWC**: **SCENARIO:** Engine services timeout due to missing Perl connector

     * **GIVEN** the Engine is configured as usual with some command using the Perl connector
     * **WHEN** the Engine executes its service commands
     * **THEN** the commands take too long and reach the timeout
     * **AND** the Engine starts and stops two times as a result
60. **ESSOCWNV**: **SCENARIO:** Engine is started with a valid old configuration (concerning cbmod)

     * **GIVEN** the Engine is configured with a valid old configuration
     * **WHEN** the Engine is started
     * **THEN** the Engine starts correctly
     * **AND** the Engine stops correctly
61. **ESS_STATS**: **SCENARIO:** Reading the stats file after Engine has started

     * **GIVEN** the Engine is started
     * **WHEN** we read the Engine's stats file
     * **THEN** Engine must not crash
62. **EVOCWNV**: **SCENARIO:** The new Engine checks the old configuration (concerning cbmod)

     * **GIVEN** the Engine is configured with a valid old configuration
     * **WHEN** the Engine is started to check the configuration
     * **THEN** the Engine reads it as expected
63. **EXT_CONF1**: Engine configuration is overidden by json conf
64. **EXT_CONF2**: Engine configuration is overidden by json conf after reload
65. **E_FD_LIMIT**: Engine here is started with a low file descriptor limit. The engine should not crash and limit should be set.
66. **E_HOST_DOWN_DISABLE_SERVICE_CHECKS**: host_down_disable_service_checks is set to 1, host down switch all services to UNKNOWN
67. **E_HOST_UNREACHABLE_DISABLE_SERVICE_CHECKS**: host_down_disable_service_checks is set to 1, host unreachable switch all services to UNKNOWN
68. **VERIF**: 
     * **WHEN** centengine is started in verification mode, it does not log in its file.
69. **VERIFY_CONF**: Scenario Verify deprecated engine configuration options are logged as warnings Given the engine and broker are configured with module 1 And the engine configuration is set with deprecated options When the engine is started Then a warning message for 'auto_reschedule_checks' should be logged And a warning message for 'auto_rescheduling_interval' should be logged And a warning message for 'auto_rescheduling_window' should be logged And the engine should be stopped

### Migration
1. **MIGRATION**: Migration bbdo2 with sql/storage to bbdo2 with unified_sql and then to bbdo3 with unified_sql and then to bbdo2 with unified_sql and then to bbdo2 with sql/storage

### Severities
1. **BECSEV1**: **FEATURE:** Severity Management between Engine and Broker
     As a Centreon administrator
     I want to configure severities in Engine
     So that Broker stores them correctly in centreon_storage.severities table
     Background:

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


641 tests currently implemented.
