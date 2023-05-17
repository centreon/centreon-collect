# Centreon Tests

This sub-project contains functional tests for Centreon Broker, Engine and Connectors.
It is based on the [Robot Framework](https://robotframework.org/) with Python functions
we can find in the resources directory. The Python code is formatted using autopep8.

## Getting Started

To get this project, you have to clone centreon-collect.

These tests are executed from the `centreon-tests/robot` folder and uses the [Robot Framework](https://robotframework.org/).

From a Centreon host, you need to install Robot Framework

On CentOS 7, the following commands should work to initialize your robot tests:

```
pip3 install -U robotframework robotframework-databaselibrary pymysql

yum install "Development Tools" python3-devel -y

pip3 install grpcio==1.33.2 grpcio_tools==1.33.2

./init-proto.sh
./init-sql.sh
```

On other rpm based distributions, you can try the following commands to initialize your robot tests:

```
pip3 install -U robotframework robotframework-databaselibrary pymysql

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

## Implemented tests

Here is the list of the currently implemented tests:

### Bam
- [x] **BABEST_SERVICE_CRITICAL**: With bbdo version 3.0.1, a BA of type 'best' with 2 serv, ba is critical only if the 2 services are critical
- [x] **BAPBSTATUS**: With bbdo version 3.0.1, a BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. 
- [x] **BA_BOOL_KPI**: With bbdo version 3.0.1, a BA of type 'worst' with 1 boolean kpi
- [x] **BA_IMPACT_2KPI_SERVICES**: With bbdo version 3.0.1, a BA of type 'impact' with 2 serv, ba is critical only if the 2 services are critical
- [x] **BA_RATIO_NUMBER_BA_4_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
- [x] **BA_RATIO_NUMBER_BA_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio number' with 2 services and one ba with 1 service
- [x] **BA_RATIO_PERCENT_BA_4_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio percent' with 4 serv
- [x] **BA_RATIO_PERCENT_BA_SERVICE**: With bbdo version 3.0.1, a BA of type 'ratio percent' with 2 serv an 1 ba with one service
- [x] **BEBAMIDT1**: A BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. Then we set a downtime on this last one. An inherited downtime is set to the BA. The downtime is removed from the service, the inherited downtime is then deleted.
- [x] **BEBAMIDT2**: A BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. Then we set a downtime on this last one. An inherited downtime is set to the BA. Engine is restarted. Broker is restarted. The two downtimes are still there with no duplicates. The downtime is removed from the service, the inherited downtime is then deleted.
- [x] **BEBAMIDTU1**: With bbdo version 3.0.1, a BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. Then we set a downtime on this last one. An inherited downtime is set to the BA. The downtime is removed from the service, the inherited downtime is then deleted.
- [x] **BEBAMIDTU2**: With bbdo version 3.0.1, a BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. Then we set a downtime on this last one. An inherited downtime is set to the BA. Engine is restarted. Broker is restarted. The two downtimes are still there with no duplicates. The downtime is removed from the service, the inherited downtime is then deleted.
- [x] **BEBAMIGNDT1**: A BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. A first downtime is cancelled, the BA is still OK, but when the second downtime is cancelled, the BA should be CRITICAL.
- [x] **BEBAMIGNDT2**: A BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. The first downtime reaches its end, the BA is still OK, but when the second downtime reaches its end, the BA should be CRITICAL.
- [x] **BEBAMIGNDTU1**: With bbdo version 3.0.1, a BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. A first downtime is cancelled, the BA is still OK, but when the second downtime is cancelled, the BA should be CRITICAL.
- [x] **BEBAMIGNDTU2**: With bbdo version 3.0.1, a BA of type 'worst' with two services is configured. The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in critical state, because of the second critical service. Then we apply two downtimes on this last one. The BA state is ok because of the policy on indicators. The first downtime reaches its end, the BA is still OK, but when the second downtime reaches its end, the BA should be CRITICAL.
- [x] **BEPB_BA_DURATION_EVENT**: creating a BA with services, connecting to database and insert a record into timeperiod table and deleting records from mod_bam_relations_ba_timeperiods and mod_bam_reporting_ba_events_durations tables in the database, seting a services to critical status and then to OK status and calculing that the duration of the event is equal to the difference between the end time and start time of the event.
- [x] **BEPB_DIMENSION_BA_BV_RELATION_EVENT**: bbdo_version 3 Creating a BA  with services and adding it to the broker, adding a Lua output configuration to the broker and starting the broker and engine, waiting for a lua file to be created, and searching the file for a specific event.
- [x] **BEPB_DIMENSION_BA_EVENT**: bbdo_version 3 creating a BA with its service and For up to 10 iterations, search the log file for an event with specific attributes related to the BA created earlier.
- [x] **BEPB_DIMENSION_BA_TIMEPERIOD_RELATION**: creating a Ba with worst status, inserting inputs  into timeperiod table deleting mod_bam_relations_ba_timeperiods table and insertin inputs in mod_bam_relations_ba_timeperiods anftre connecting to database the table mod_bam_reporting_relations_ba_timeperiods must be updated.
- [x] **BEPB_DIMENSION_BV_EVENT**: bbdo_version 3 creation of a BA group and checking for a log message that includes the attributes below to initiate the  seccussful creation of the BA group.
- [x] **BEPB_DIMENSION_KPI_EVENT**: bbdo_version 3 creating a BA with a service and adds a boolean KPI with a value of 100 and checking if the output created in the databese is equal to the expected output.
- [x] **BEPB_DIMENSION_TIMEPERIOD**: Adding BAM and Lua configurations to the broker,connects to  database and executes an SQL insert statement, checking whether the event message is correctly logged by the system.
- [x] **BEPB_DIMENSION_TRUNCATE_TABLE**: Removing lua file before starting, creating a BA and adding lua output to broker waiting for the lua file to be created again and searching for a specific event in the log file using the Grep File keyword, the event should have a certain type, category, element, and update_started status.
- [x] **BEPB_KPI_STATUS**: bbdo_version 3 Create a new BA for a specific service, start the broker and engine,check the status of a service three times until hard and set it to critical,verifing that the status of the service has been updated in the database and the timestamp of the last status change is correct.

### Broker
- [x] **BCL1**: Starting broker with option '-s foobar' should return an error
- [x] **BCL2**: Starting broker with option '-s5' should work
- [x] **BCL3**: Starting broker with options '-D' should work and activate diagnose mode
- [x] **BCL4**: Starting broker with options '-s2' and '-D' should work.
- [x] **BDB1**: Access denied when database name exists but is not the good one for sql output
- [x] **BDB10**: connection should be established when user password is good for sql/perfdata
- [x] **BDB2**: Access denied when database name exists but is not the good one for storage output
- [x] **BDB3**: Access denied when database name does not exist for sql output
- [x] **BDB4**: Access denied when database name does not exist for storage and sql outputs
- [x] **BDB5**: cbd does not crash if the storage/sql db_host is wrong
- [x] **BDB6**: cbd does not crash if the sql db_host is wrong
- [x] **BDB7**: access denied when database user password is wrong for perfdata/sql
- [x] **BDB8**: access denied when database user password is wrong for perfdata/sql
- [x] **BDB9**: access denied when database user password is wrong for sql
- [x] **BDBM1**: start broker/engine and then start MariaDB => connection is established
- [x] **BDBMU1**: start broker/engine with unified sql and then start MariaDB => connection is established
- [x] **BDBU1**: Access denied when database name exists but is not the good one for unified sql output
- [x] **BDBU10**: Connection should be established when user password is good for unified sql
- [x] **BDBU3**: Access denied when database name does not exist for unified sql output
- [x] **BDBU5**: cbd does not crash if the unified sql db_host is wrong
- [x] **BDBU7**: Access denied when database user password is wrong for unified sql
- [x] **BEDB2**: start broker/engine and then start MariaDB => connection is established
- [x] **BEDB3**: start broker/engine, then stop MariaDB and then start it again. The gRPC API should give informations about SQL connections.
- [x] **BEDB4**: start broker/engine, then stop MariaDB and then start it again. The gRPC API should give informations about SQL connections.
- [x] **BFC1**: Start broker with invalid filters but one filter ok
- [x] **BFC2**: Start broker with only invalid filters on an output
- [x] **BGRPCSS1**: Start-Stop two instances of broker configured with grpc stream and no coredump
- [x] **BGRPCSS2**: Start/Stop 10 times broker configured with grpc stream with 300ms interval and no coredump
- [x] **BGRPCSS3**: Start-Stop one instance of broker configured with grpc stream and no coredump
- [x] **BGRPCSS4**: Start/Stop 10 times broker configured with grpc stream with 1sec interval and no coredump
- [x] **BGRPCSS5**: Start-Stop with reversed connection on grpc acceptor with only one instance and no deadlock
- [x] **BGRPCSSU1**: Start-Stop with unified_sql two instances of broker with grpc stream and no coredump
- [x] **BGRPCSSU2**: Start/Stop with unified_sql 10 times broker configured with grpc stream with 300ms interval and no coredump
- [x] **BGRPCSSU3**: Start-Stop with unified_sql one instance of broker configured with grpc and no coredump
- [x] **BGRPCSSU4**: Start/Stop with unified_sql 10 times broker configured with grpc stream with 1sec interval and no coredump
- [x] **BGRPCSSU5**: Start-Stop with unified_sql with reversed connection on grpc acceptor with only one instance and no deadlock
- [x] **BLDIS1**: Start broker with core logs 'disabled'
- [x] **BLEC1**: Change live the core level log from trace to debug
- [x] **BLEC2**: Change live the core level log from trace to foo raises an error
- [x] **BLEC3**: Change live the foo level log to trace raises an error
- [x] **BSCSS1**: Start-Stop two instances of broker and no coredump
- [x] **BSCSS2**: Start/Stop 10 times broker with 300ms interval and no coredump
- [x] **BSCSS3**: Start-Stop one instance of broker and no coredump
- [x] **BSCSS4**: Start/Stop 10 times broker with 1sec interval and no coredump
- [x] **BSCSSC1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is enabled on client side.
- [x] **BSCSSC2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is disabled on client side.
- [x] **BSCSSCG1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on client side.
- [x] **BSCSSCGRR1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on output side. Reversed connection with retention and grpc transport protocol.
- [x] **BSCSSCGRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on output side. Reversed connection with retention and grpc transport protocol.
- [x] **BSCSSCRR1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is enabled on client side. Connection reversed with retention.
- [x] **BSCSSCRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is disabled on client side. Connection reversed with retention.
- [x] **BSCSSG1**: Start-Stop two instances of broker and no coredump
- [x] **BSCSSG2**: Start/Stop 10 times broker with 300ms interval and no coredump
- [x] **BSCSSG3**: Start-Stop one instance of broker and no coredump
- [x] **BSCSSG4**: Start/Stop 10 times broker with 1sec interval and no coredump
- [x] **BSCSSGA1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. An authorization token is added on the server. Error messages are raised.
- [x] **BSCSSGA2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. An authorization token is added on the server and also on the client. All looks ok.
- [x] **BSCSSGRR1**: Start-Stop two instances of broker and no coredump, reversed and retention, with transport protocol grpc, start-stop 5 times.
- [x] **BSCSSK1**: Start-Stop two instances of broker, server configured with grpc and client with tcp. No connectrion established and error raised on client side.
- [x] **BSCSSK2**: Start-Stop two instances of broker, server configured with tcp and client with grpc. No connection established and error raised on client side.
- [x] **BSCSSP1**: Start-Stop two instances of broker and no coredump. The server contains a listen address
- [x] **BSCSSPRR1**: Start-Stop two instances of broker and no coredump. The server contains a listen address, reversed and retention. central-broker-master-output is then a failover.
- [x] **BSCSSR1**: Start-Stop two instances of broker and no coredump. Connection with bbdo_server/bbdo_client and reversed.
- [x] **BSCSSRR1**: Start-Stop two instances of broker and no coredump. Connection with bbdo_server/bbdo_client, reversed and retention. central-broker-master-output is then a failover.
- [x] **BSCSSRR2**: Start/Stop 10 times broker with 300ms interval and no coredump, reversed and retention. central-broker-master-output is then a failover.
- [x] **BSCSST1**: Start-Stop two instances of broker and no coredump. Encryption is enabled on client side.
- [x] **BSCSST2**: Start-Stop two instances of broker and no coredump. Encryption is enabled on client side.
- [x] **BSCSSTG1**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. This is not sufficient, then an error is raised.
- [x] **BSCSSTG2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. It works with good certificates and keys.
- [x] **BSCSSTG3**: Start-Stop two instances of broker. The connection cannot be established if the server private key is missing and an error message explains this issue.
- [x] **BSCSSTGRR2**: Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. It works with good certificates and keys. Reversed grpc connection with retention.
- [x] **BSCSSTRR1**: Start-Stop two instances of broker and no coredump. Encryption is enabled. transport protocol is tcp, reversed and retention.
- [x] **BSCSSTRR2**: Start-Stop two instances of broker and no coredump. Encryption is enabled.
- [x] **BSS1**: Start-Stop two instances of broker and no coredump
- [x] **BSS2**: Start/Stop 10 times broker with 300ms interval and no coredump
- [x] **BSS3**: Start-Stop one instance of broker and no coredump
- [x] **BSS4**: Start/Stop 10 times broker with 1sec interval and no coredump
- [x] **BSS5**: Start-Stop with reversed connection on TCP acceptor with only one instance and no deadlock
- [x] **BSSU1**: Start-Stop with unified_sql two instances of broker and no coredump
- [x] **BSSU2**: Start/Stop with unified_sql 10 times broker with 300ms interval and no coredump
- [x] **BSSU3**: Start-Stop with unified_sql one instance of broker and no coredump
- [x] **BSSU4**: Start/Stop with unified_sql 10 times broker with 1sec interval and no coredump
- [x] **BSSU5**: Start-Stop with unified_sql with reversed connection on TCP acceptor with only one instance and no deadlock

### Broker/database
- [x] **NetworkDBFail6**: network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s
- [x] **NetworkDBFail7**: network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s
- [x] **NetworkDBFailU6**: network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s (with unified_sql)
- [x] **NetworkDBFailU7**: network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s (with unified_sql)
- [x] **NetworkDbFail1**: network failure test between broker and database (shutting down connection for 100ms)
- [x] **NetworkDbFail2**: network failure test between broker and database (shutting down connection for 1s)
- [x] **NetworkDbFail3**: network failure test between broker and database (shutting down connection for 10s)
- [x] **NetworkDbFail4**: network failure test between broker and database (shutting down connection for 30s)
- [x] **NetworkDbFail5**: network failure test between broker and database (shutting down connection for 60s)

### Broker/engine
- [x] **ANO_CFG_SENSITIVITY_SAVED**: cfg sensitivity saved in retention
- [x] **ANO_DT1**: downtime on dependent service is inherited by the anomaly service state.
- [x] **ANO_DT2**: deleting downtime on dependent service must delete the downtime on anomaly detection service.
- [x] **ANO_DT3**: deletting a downtime on anomaly detection service don't delete the downtime on the dependent service.
- [x] **ANO_DT4**: setting downtime on anomaly and on dependent service, delete last the downtime of the dependent service not the one linked to anomaly detection service.
- [x] **ANO_EXTCMD_SENSITIVITY_SAVED**: extcmd sensitivity saved in retention
- [x] **ANO_JSON_SENSITIVITY_NOT_SAVED**: json sensitivity not saved in retention
- [x] **ANO_NOFILE**: an anomaly detection without threshold file must be in unknown state
- [x] **ANO_OUT_LOWER_THAN_LIMIT**: an anomaly detection with a perfdata lower than lower limit make a critical state
- [x] **ANO_OUT_UPPER_THAN_LIMIT**: an anomaly detection with a perfdata upper than upper limit make a critical state
- [x] **ANO_TOO_OLD_FILE**: an anomaly detection with an oldest threshold file must be in unknown state
- [x] **AOUTLU1**: an anomaly detection with a perfdata upper than upper limit make a critical state with bbdo 3
- [x] **BEACK1**: Engine has a critical service. An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to OK. And the acknowledgement in database is deleted from engine but still open on the database.
- [x] **BEACK2**: Configuration is made with BBDO3. Engine has a critical service. An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to OK. And the acknowledgement in database is deleted.
- [x] **BEACK3**: Engine has a critical service. An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The acknowledgement is removed and the comment in the comments table has its deletion_time column updated.
- [x] **BEACK4**: Configuration is made with BBDO3. Engine has a critical service. An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The acknowledgement is removed and the comment in the comments table has its deletion_time column updated.
- [x] **BEACK5**: Engine has a critical service. An external command is sent to acknowledge it ; the acknowledgement is sticky. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to WARNING. And the acknowledgement in database is still there.
- [x] **BEACK6**: Configuration is made with BBDO3. Engine has a critical service. An external command is sent to acknowledge it ; the acknowledgement is sticky. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to WARNING. And the acknowledgement in database is still there.
- [x] **BEATOI11**: external command SEND_CUSTOM_HOST_NOTIFICATION with option_number=1 should work
- [x] **BEATOI12**: external command SEND_CUSTOM_HOST_NOTIFICATION with option_number>7 should fail
- [x] **BEATOI13**: external command SCHEDULE SERVICE DOWNTIME with duration<0 should fail
- [x] **BEATOI21**: external command ADD_HOST_COMMENT and DEL_HOST_COMMENT should work
- [x] **BEATOI22**: external command DEL_HOST_COMMENT with comment_id<0 should fail
- [x] **BEATOI23**: external command ADD_SVC_COMMENT with persistent=0 should work
- [x] **BECC1**: Broker/Engine communication with compression between central and poller
- [x] **BECT1**: Broker/Engine communication with anonymous TLS between central and poller
- [x] **BECT2**: Broker/Engine communication with TLS between central and poller with key/cert
- [x] **BECT3**: Broker/Engine communication with anonymous TLS and ca certificate
- [x] **BECT4**: Broker/Engine communication with TLS between central and poller with key/cert and hostname forced
- [x] **BECT_GRPC1**: Broker/Engine communication with GRPC and with anonymous TLS between central and poller
- [x] **BECT_GRPC2**: Broker/Engine communication with TLS between central and poller with key/cert
- [x] **BECT_GRPC3**: Broker/Engine communication with anonymous TLS and ca certificate
- [x] **BECT_GRPC4**: Broker/Engine communication with TLS between central and poller with key/cert and hostname forced
- [x] **BECUSTOMHOSTVAR**: external command CHANGE_CUSTOM_HOST_VAR on SNMPVERSION
- [x] **BECUSTOMSVCVAR**: external command CHANGE_CUSTOM_SVC_VAR on CRITICAL
- [x] **BEDTHOSTFIXED**: A downtime is set on a host, the total number of downtimes is really 21 (1 for the host and 20 for its 20 services) then we delete this downtime and the number is 0.
- [x] **BEDTMASS1**: New services with several pollers are created. Then downtimes are set on all configured hosts. This action results on 1050 downtimes if we also count impacted services. Then all these downtimes are removed. This test is done with BBDO 3.0.0
- [x] **BEDTMASS2**: New services with several pollers are created. Then downtimes are set on all configured hosts. This action results on 1050 downtimes if we also count impacted services. Then all these downtimes are removed. This test is done with BBDO 2.0
- [x] **BEDTSVCFIXED**: A downtime is set on a service, the total number of downtimes is really 1 then we delete this downtime and the number of downtime is 0.
- [x] **BEDTSVCREN1**: A downtime is set on a service then the service is renamed. The downtime is still active on the renamed service. The downtime is removed from the renamed service and it is well removed.
- [x] **BEEXTCMD1**: using bbdo3.0 the script configures the engine and broker, starts them, executes external commands to change the normal service check interval, verifies the interval values in the database, and stops the engine and broker. The process is repeated for different use_grpc values.
- [x] **BEEXTCMD10**: external command CHANGE_MAX_SVC_CHECK_ATTEMPTS on bbdo2.0 executes external commands to change the maximum service check attempts, verifies the interval value in the database for both services and resources, and stops the engine and broker. The process is repeated for different use_grpc values.
- [x] **BEEXTCMD11**: external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo3.0 executing external commands to change the maximum host check attempts, verifies the maximum attempts value in the database for both hosts and resources, and stops the engine and broker. The process is repeated for different use_grpc values.
- [x] **BEEXTCMD12**: external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo2.0 executing external commands to change the maximum host check attempts, verifies the maximum attempts value in the database for both hosts and resources, and stops the engine and broker. The process is repeated for different use_grpc values.
- [x] **BEEXTCMD13**: external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo3.0 executing an external command to change the host check time period, verifies the time period value in the database for the host, and stops the engine and broker. The process is repeated for different use_grpc values. 
- [x] **BEEXTCMD14**: external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo2.0 executing an external command to change the host check time period, verifies the time period value in the database for the host, and stops the engine and broker. The process is repeated for different use_grpc values.
- [x] **BEEXTCMD15**: external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo3.0 configures the engine and broker, starts them, executes an external command to change the host notification time period, verifies the time period value in the database for the host, and stops the engine and broker. The process is repeated for different use_grpc values.
- [x] **BEEXTCMD16**: external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo2.0 configures the engine and broker, starts them, executes an external command to change the host notification time period, verifies the time period value in the database for the host, and stops the engine and broker. The process is repeated for different use_grpc values.
- [x] **BEEXTCMD17**: external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo3.0 configures the service check time period for a specific service on a host using the external command CHANGE_SVC_CHECK_TIMEPERIOD.
- [x] **BEEXTCMD18**: external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo2.0 configures the service check time period for a specific service on a host using the external command CHANGE_SVC_CHECK_TIMEPERIOD.
- [x] **BEEXTCMD19**: external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo3.0  service notification time period is changed using the Change Svc Notification Timeperiod external command for the specified service and host with a specified value after the initial host state is raised, a loop is executed up to 30 times, where it queries and retrieves the notification_period of the specified service and host from the database, and the result should be equal to 24x7.
- [x] **BEEXTCMD2**: using bbdo2.0 the script configures the engine and broker, starts them, executes external commands to change the normal service check interval, verifies the interval values in the database, and stops the engine and broker. The process is repeated for different use_grpc values.
- [x] **BEEXTCMD20**: external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo2.0  service notification time period is changed using the Change Svc Notification Timeperiod external command for the specified service and host with a specified value after the initial host state is raised, a loop is executed up to 30 times, where it queries and retrieves the notification_period of the specified service and host from the database, and the result should be equal to 24x7.
- [x] **BEEXTCMD21**: external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS and ENABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo3.0
- [x] **BEEXTCMD22**: external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS and ENABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo2.0
- [x] **BEEXTCMD23**: external command DISABLE_HOST_CHECK and ENABLE_HOST_CHECK on bbdo3.0
- [x] **BEEXTCMD24**: external command DISABLE_HOST_CHECK and ENABLE_HOST_CHECK on bbdo2.0
- [x] **BEEXTCMD25**: external command DISABLE_HOST_EVENT_HANDLER and ENABLE_HOST_EVENT_HANDLER on bbdo3.0
- [x] **BEEXTCMD26**: external command DISABLE_HOST_EVENT_HANDLER and ENABLE_HOST_EVENT_HANDLER on bbdo2.0
- [x] **BEEXTCMD27**: external command DISABLE_HOST_FLAP_DETECTION and ENABLE_HOST_FLAP_DETECTION on bbdo3.0
- [x] **BEEXTCMD28**: external command DISABLE_HOST_FLAP_DETECTION and ENABLE_HOST_FLAP_DETECTION on bbdo2.0
- [x] **BEEXTCMD29**: external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo3.0
- [x] **BEEXTCMD3**: external using bbdo3.0 the script configures the engine and broker, starts them, executes external commands to change the normal host check interval, verifies the interval value in the database, and stops the engine and broker. The process is repeated for different use_grpc values.
- [x] **BEEXTCMD30**: external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo2.0
- [x] **BEEXTCMD31**: external command DISABLE_HOST_SVC_CHECKS and ENABLE_HOST_SVC_CHECKS on bbdo3.0
- [x] **BEEXTCMD32**: external command DISABLE_HOST_SVC_CHECKS and ENABLE_HOST_SVC_CHECKS on bbdo2.0
- [x] **BEEXTCMD33**: external command DISABLE_HOST_SVC_NOTIFICATIONS and ENABLE_HOST_SVC_NOTIFICATIONS on bbdo3.0
- [x] **BEEXTCMD34**: external command DISABLE_HOST_SVC_NOTIFICATIONS and ENABLE_HOST_SVC_NOTIFICATIONS on bbdo2.0
- [x] **BEEXTCMD35**: external command DISABLE_PASSIVE_HOST_CHECKS and ENABLE_PASSIVE_HOST_CHECKS on bbdo3.0
- [x] **BEEXTCMD36**: external command DISABLE_PASSIVE_HOST_CHECKS and ENABLE_PASSIVE_HOST_CHECKS on bbdo2.0
- [x] **BEEXTCMD37**: external command DISABLE_PASSIVE_SVC_CHECKS and ENABLE_PASSIVE_SVC_CHECKS on bbdo3.0
- [x] **BEEXTCMD38**: external command DISABLE_PASSIVE_SVC_CHECKS and ENABLE_PASSIVE_SVC_CHECKS on bbdo2.0
- [x] **BEEXTCMD39**: external command START_OBSESSING_OVER_HOST and STOP_OBSESSING_OVER_HOST on bbdo3.0
- [x] **BEEXTCMD4**: using bbdo2.0 the script configures the engine and broker, starts them, executes external commands to change the normal host check interval, verifies the interval value in the database, and stops the engine and broker. The process is repeated for different use_grpc values.
- [x] **BEEXTCMD40**: external command START_OBSESSING_OVER_HOST and STOP_OBSESSING_OVER_HOST on bbdo2.0
- [x] **BEEXTCMD41**: external command START_OBSESSING_OVER_SVC and STOP_OBSESSING_OVER_SVC on bbdo3.0
- [x] **BEEXTCMD42**: external command START_OBSESSING_OVER_SVC and STOP_OBSESSING_OVER_SVC on bbdo2.0
- [x] **BEEXTCMD5**: external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo3.0 this code configures the engine and broker, starts them, executes external commands to change the retry service check interval, verifies the interval value in the database, and stops the engine and broker. The process is repeated for different use_grpc values.
- [x] **BEEXTCMD6**: external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo2.0 this code configures the engine and broker, starts them, executes external commands to change the retry service check interval, verifies the interval value in the database, and stops the engine and broker. The process is repeated for different use_grpc values.
- [x] **BEEXTCMD7**: external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo3.0 this code configures the engine and broker, starts them, executes external commands to change the retry host check interval, verifies the interval value in the database, and stops the engine and broker. The process is repeated for different use_grpc values.
- [x] **BEEXTCMD8**: external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo2.0 this code configures the engine and broker, starts them, executes external commands to change the retry host check interval, verifies the interval value in the database, and stops the engine and broker. The process is repeated for different use_grpc values.
- [x] **BEEXTCMD9**: external command CHANGE_MAX_SVC_CHECK_ATTEMPTS on bbdo3.0 executes external commands to change the maximum service check attempts, verifies the interval value in the database for both services and resources, and stops the engine and broker. The process is repeated for different use_grpc values.
- [x] **BEEXTCMD_COMPRESS_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and compressed grpc
- [x] **BEEXTCMD_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and grpc
- [x] **BEEXTCMD_GRPC2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 and grpc
- [x] **BEEXTCMD_GRPC3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 and grpc
- [x] **BEEXTCMD_GRPC4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 and grpc
- [x] **BEEXTCMD_REVERSE_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and reversed gRPC
- [x] **BEEXTCMD_REVERSE_GRPC2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 and grpc reversed
- [x] **BEEXTCMD_REVERSE_GRPC3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 and grpc reversed
- [x] **BEEXTCMD_REVERSE_GRPC4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 and grpc reversed
- [x] **BEHOSTCHECK**: external command CHECK_SERVICE_RESULT 
- [x] **BEHS1**: store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
- [x] **BEINSTANCE**: Instance to bdd 
- [x] **BEINSTANCESTATUS**: Instance status to bdd 
- [x] **BEPBBEE1**: central-module configured with bbdo_version 3.0 but not others. Unable to establish connection.
- [x] **BEPBBEE2**: bbdo_version 3 not compatible with sql/storage
- [x] **BEPBBEE3**: bbdo_version 3 generates new bbdo protobuf service status messages.
- [x] **BEPBBEE4**: bbdo_version 3 generates new bbdo protobuf host status messages.
- [x] **BEPBBEE5**: bbdo_version 3 generates new bbdo protobuf service messages.
- [x] **BEPBCVS**: bbdo_version 3 communication of custom variables.
- [x] **BEPBRI1**: bbdo_version 3 use pb_resource new bbdo protobuf ResponsiveInstance message.
- [x] **BERD1**: Starting/stopping Broker does not create duplicated events.
- [x] **BERD2**: Starting/stopping Engine does not create duplicated events.
- [x] **BERDUC1**: Starting/stopping Broker does not create duplicated events in usual cases
- [x] **BERDUC2**: Starting/stopping Engine does not create duplicated events in usual cases
- [x] **BERDUC3U1**: Starting/stopping Broker does not create duplicated events in usual cases with unified_sql and BBDO 3.0
- [x] **BERDUC3U2**: Starting/stopping Engine does not create duplicated events in usual cases with unified_sql and BBDO 3.0
- [x] **BERDUCA300**: Starting/stopping Engine is stopped ; it should emit a stop event and receive an ack event with events to clean from broker.
- [x] **BERDUCA301**: Starting/stopping Engine is stopped ; it should emit a stop event and receive an ack event with events to clean from broker with bbdo 3.0.1.
- [x] **BERDUCU1**: Starting/stopping Broker does not create duplicated events in usual cases with unified_sql
- [x] **BERDUCU2**: Starting/stopping Engine does not create duplicated events in usual cases with unified_sql
- [x] **BERES1**: store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
- [x] **BESERVCHECK**: external command CHECK_SERVICE_RESULT 
- [x] **BESS1**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
- [x] **BESS2**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
- [x] **BESS3**: Start-Stop Broker/Engine - Engine started first - Engine stopped first
- [x] **BESS4**: Start-Stop Broker/Engine - Engine started first - Broker stopped first
- [x] **BESS5**: Start-Stop Broker/engine - Engine debug level is set to all, it should not hang
- [x] **BESSBQ1**: A very bad queue file is written for broker. Broker and Engine are then started, Broker must read the file raising an error because of that file and then get data sent by Engine.
- [x] **BESS_CRYPTED_GRPC1**: Start-Stop grpc version Broker/Engine - well configured
- [x] **BESS_CRYPTED_GRPC2**: Start-Stop grpc version Broker/Engine only server crypted
- [x] **BESS_CRYPTED_GRPC3**: Start-Stop grpc version Broker/Engine only engine crypted
- [x] **BESS_CRYPTED_REVERSED_GRPC1**: Start-Stop grpc version Broker/Engine - well configured
- [x] **BESS_CRYPTED_REVERSED_GRPC2**: Start-Stop grpc version Broker/Engine only engine server crypted
- [x] **BESS_CRYPTED_REVERSED_GRPC3**: Start-Stop grpc version Broker/Engine only engine crypted
- [x] **BESS_ENGINE_DELETE_HOST**: once engine and cbd started, stop and restart cbd, delete an host and reload engine, cbd mustn't core
- [x] **BESS_GRPC1**: Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped first
- [x] **BESS_GRPC2**: Start-Stop grpc version Broker/Engine - Broker started first - Engine stopped first
- [x] **BESS_GRPC3**: Start-Stop grpc version Broker/Engine - Engine started first - Engine stopped first
- [x] **BESS_GRPC4**: Start-Stop grpc version Broker/Engine - Engine started first - Broker stopped first
- [x] **BESS_GRPC5**: Start-Stop grpc version Broker/engine - Engine debug level is set to all, it should not hang
- [x] **BESS_GRPC_COMPRESS1**: Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped last compression activated
- [x] **BETAG1**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Broker is started before.
- [x] **BETAG2**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
- [x] **BEUTAG1**: Engine is configured with some tags. When broker receives them through unified_sql stream, it stores them in the centreon_storage.tags table. Broker is started before.
- [x] **BEUTAG10**: some services are configured with tags on two pollers. Then tags are removed from some of them and in centreon_storage, we can observe resources_tags table updated.
- [x] **BEUTAG11**: some services are configured with tags on two pollers. Then several tags are removed, and we can observe resources_tags table updated.
- [x] **BEUTAG12**: Engine is configured with some tags. Group tags tag2, tag6 are set to hosts 1 and 2. Category tags tag4 and tag8 are added to hosts 2, 3, 4. The resources and resources_tags tables are well filled. The tag6 and tag8 are removed and resources_tags is also well updated.
- [x] **BEUTAG2**: Engine is configured with some tags. A new service is added with a tag. Broker should make the relations.
- [x] **BEUTAG3**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
- [x] **BEUTAG4**: Engine is configured with some tags. Group tags tag9, tag13 are set to services 1 and 3. Category tags tag3 and tag11 are added to services 1, 3, 5 and 6. The centreon_storage.resources and resources_tags tables are well filled.
- [x] **BEUTAG5**: Engine is configured with some tags. Group tags tag2, tag6 are set to hosts 1 and 2. Category tags tag4 and tag8 are added to hosts 2, 3, 4. The resources and resources_tags tables are well filled.
- [x] **BEUTAG6**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.resources_tags table. Engine is started before.
- [x] **BEUTAG7**: some services are configured and deleted with tags on two pollers.
- [x] **BEUTAG8**: Services have tags provided by templates.
- [x] **BEUTAG9**: hosts have tags provided by templates.
- [x] **BE_NOTIF_OVERFLOW**: bbdo 2.0 notification number =40000. make an overflow => notification_number null in db
- [x] **BE_TIME_NULL_SERVICE_RESOURCE**: With BBDO 3, time must be set to NULL on 0 in services, hosts and resources tables.
- [x] **BRCS1**: Broker reverse connection stopped
- [x] **BRCTS1**: Broker reverse connection too slow
- [x] **BRCTSMN**: Broker connected to map with neb filter
- [x] **BRCTSMNS**: Broker connected to map with neb and storages filters
- [x] **BRGC1**: Broker good reverse connection
- [x] **BRRDCDDID1**: RRD metrics deletion from index ids with rrdcached.
- [x] **BRRDCDDIDDB1**: RRD metrics deletion from index ids with a query in centreon_storage with rrdcached.
- [x] **BRRDCDDIDU1**: RRD metrics deletion from index ids with unified sql output with rrdcached.
- [x] **BRRDCDDM1**: RRD metrics deletion from metric ids with rrdcached.
- [x] **BRRDCDDMDB1**: RRD metrics deletion from metric ids with a query in centreon_storage and rrdcached.
- [x] **BRRDCDDMID1**: RRD deletion of non existing metrics and indexes with rrdcached
- [x] **BRRDCDDMIDU1**: RRD deletion of non existing metrics and indexes with rrdcached
- [x] **BRRDCDDMU1**: RRD metric deletion on table metric with unified sql output with rrdcached
- [x] **BRRDCDRB1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with storage/sql sql output and rrdcached.
- [x] **BRRDCDRBDB1**: RRD metric rebuild with a query in centreon_storage and unified sql with rrdcached
- [x] **BRRDCDRBU1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with unified_sql output and rrdcached.
- [x] **BRRDCDRBUDB1**: RRD metric rebuild with a query in centreon_storage and unified sql with rrdcached
- [x] **BRRDDID1**: RRD metrics deletion from index ids.
- [x] **BRRDDIDDB1**: RRD metrics deletion from index ids with a query in centreon_storage.
- [x] **BRRDDIDU1**: RRD metrics deletion from index ids with unified sql output.
- [x] **BRRDDM1**: RRD metrics deletion from metric ids.
- [x] **BRRDDMDB1**: RRD metrics deletion from metric ids with a query in centreon_storage.
- [x] **BRRDDMID1**: RRD deletion of non existing metrics and indexes
- [x] **BRRDDMIDU1**: RRD deletion of non existing metrics and indexes
- [x] **BRRDDMU1**: RRD metric deletion on table metric with unified sql output
- [x] **BRRDRBDB1**: RRD metric rebuild with a query in centreon_storage and unified sql
- [x] **BRRDRBUDB1**: RRD metric rebuild with a query in centreon_storage and unified sql
- [x] **BRRDRM1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with storage/sql sql output.
- [x] **BRRDRMU1**: RRD metric rebuild with gRPC API. 3 indexes are selected then a message to rebuild them is sent. This is done with unified_sql output.
- [x] **BRRDWM1**: We are working with BBDO3. This test checks protobuf metrics and status are sent to cbd RRD.
- [x] **EBBPS1**: 1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table resources.
- [x] **EBBPS2**: 1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table services.
- [x] **EBDP1**: Four new pollers are started and then we remove Poller3.
- [x] **EBDP2**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
- [x] **EBDP3**: Three new pollers are started, then they are killed. It is still possible to remove Poller2 if removed from the configuration.
- [x] **EBDP4**: Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by broker.
- [x] **EBDP5**: Four new pollers are started and then we remove Poller3.
- [x] **EBDP6**: Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
- [x] **EBDP7**: Three new pollers are started, then they are killed. It is still possible to remove Poller2 if removed from the configuration.
- [x] **EBDP8**: Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by broker.
- [x] **EBNHG1**: New host group with several pollers and connections to DB
- [x] **EBNHG4**: New host group with several pollers and connections to DB with broker and rename this hostgroup
- [x] **EBNHGU1**: New host group with several pollers and connections to DB with broker configured with unified_sql
- [x] **EBNHGU2**: New host group with several pollers and connections to DB with broker configured with unified_sql
- [x] **EBNHGU3**: New host group with several pollers and connections to DB with broker configured with unified_sql
- [x] **EBNHGU4**: New host group with several pollers and connections to DB with broker and rename this hostgroup
- [x] **EBNSG1**: New service group with several pollers and connections to DB
- [x] **EBNSGU1**: New service group with several pollers and connections to DB with broker configured with unified_sql
- [x] **EBNSGU2**: New service group with several pollers and connections to DB with broker configured with unified_sql
- [x] **EBNSVC1**: New services with several pollers
- [x] **EBSAU2**: New services with action_url with more than 2000 characters
- [x] **EBSN3**: New services with notes with more than 500 characters
- [x] **EBSNU1**: New services with notes_url with more than 2000 characters
- [x] **ENRSCHE1**: Verify that next check of a rescheduled host is made at last_check + interval_check
- [x] **LOGV2DB1**: log-v2 disabled old log enabled check broker sink
- [x] **LOGV2DB2**: log-v2 disabled old log disabled check broker sink
- [x] **LOGV2DF1**: log-v2 disabled old log enabled check logfile sink
- [x] **LOGV2DF2**: log-v2 disabled old log disabled check logfile sink
- [x] **LOGV2EB1**: Checking broker sink when log-v2 is enabled and legacy logs are disabled.
- [x] **LOGV2EB2**: log-v2 enabled old log enabled check broker sink
- [x] **LOGV2EBU1**: Checking broker sink when log-v2 is enabled and legacy logs are disabled with bbdo3.
- [x] **LOGV2EBU2**: Check Broker sink with log-v2 enabled and legacy log enabled with BBDO3.
- [x] **LOGV2EF1**: log-v2 enabled  old log disabled check logfile sink
- [x] **LOGV2EF2**: log-v2 enabled old log enabled check logfile sink
- [x] **LOGV2FE2**: log-v2 enabled old log enabled check logfile sink

### Ccc
- [x] **BECCC1**: ccc without port fails with an error message
- [x] **BECCC2**: ccc with -p 51001 connects to central cbd gRPC server.
- [x] **BECCC3**: ccc with -p 50001 connects to centengine gRPC server.
- [x] **BECCC4**: ccc with -p 51001 -l returns the available functions from Broker gRPC server
- [x] **BECCC5**: ccc with -p 51001 -l GetVersion returns an error because we can't execute a command with -l.
- [x] **BECCC6**: ccc with -p 51001 GetVersion{} calls the GetVersion command
- [x] **BECCC7**: ccc with -p 51001 GetVersion{"idx":1} returns an error because the input message is wrong.
- [x] **BECCC8**: ccc with -p 50001 EnableServiceNotifications{"names":{"host_name": "host_1", "service_name": "service_1"}} works and returns an empty message.

### Connector perl
- [x] **test use connector perl exist script**: test exist script
- [x] **test use connector perl multiple script**: test script multiple
- [x] **test use connector perl unknown script**: test unknown script

### Connector ssh
- [x] **Test6Hosts**: as 127.0.0.x point to the localhost address we will simulate check on 6 hosts
- [x] **TestBadPwd**: test bad password
- [x] **TestBadUser**: test unknown user

### Engine
- [x] **EFHC1**: Engine is configured with hosts and we force checks on one host 5 times on bbdo2
- [x] **EFHC2**: Engine is configured with hosts and we force checks on one host 5 times on bbdo2
- [x] **EFHCU1**: Engine is configured with hosts and we force checks on one host 5 times on bbdo3. Bbdo3 has no impact on this behavior. resources table is cleared before starting broker.
- [x] **EFHCU2**: Engine is configured with hosts and we force checks on one host 5 times on bbdo3. Bbdo3 has no impact on this behavior.
- [x] **EPC1**: Check with perl connector
- [x] **ESS1**: Start-Stop (0s between start/stop) 5 times one instance of engine and no coredump
- [x] **ESS2**: Start-Stop (300ms between start/stop) 5 times one instance of engine and no coredump
- [x] **ESS3**: Start-Stop (0s between start/stop) 5 times three instances of engine and no coredump
- [x] **ESS4**: Start-Stop (300ms between start/stop) 5 times three instances of engine and no coredump

### Migration
- [x] **MIGRATION**: Migration bbdo2 => sql/storage => unified_sql => bbdo3

### Severities
- [x] **BESEV1**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Broker is started before.
- [x] **BESEV2**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Engine is started before.
- [x] **BETUHSEV1**: Hosts have severities provided by templates.
- [x] **BETUSEV1**: Services have severities provided by templates.
- [x] **BEUHSEV1**: Four hosts have a severity added. Then we remove the severity from host 1. Then we change severity 10 to severity 8 for host 3.
- [x] **BEUHSEV2**: Seven hosts are configured with a severity on two pollers. Then we remove severities from the first and second hosts of the first poller but only the severity from the first host of the second poller.
- [x] **BEUSEV1**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Broker is started before.
- [x] **BEUSEV2**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Engine is started before.
- [x] **BEUSEV3**: Four services have a severity added. Then we remove the severity from service 1. Then we change severity 11 to severity7 for service 3.
- [x] **BEUSEV4**: Seven services are configured with a severity on two pollers. Then we remove severities from the first and second services of the first poller but only the severity from the first service of the second poller. Then only severities no more used should be removed from the database.

