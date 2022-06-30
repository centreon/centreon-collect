# Centreon Tests

This sub-project contains functional tests for Centreon Broker, Engine and Connectors.

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
- [x] **BEBAMIDT1**: A BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. Then we set a downtime on this last one. An inherited downtime is set to the BA. The downtime is removed from the service, the inherited downtime is then deleted.
- [x] **BEBAMIDT2**: A BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. Then we set a downtime on this last one. An inherited downtime is set to the BA. Engine is restarted. Broker is restarted. The two downtimes are still there with no duplicates. The downtime is removed from the service, the inherited downtime is then deleted.

### Broker
- [x] **BLDIS1**: Start broker with core logs 'disabled'
- [x] **BFC1**: Start broker with invalid filters but one filter ok
- [x] **BFC2**: Start broker with only invalid filters on an output
- [x] **BDB1**: Access denied when database name exists but is not the good one for sql output
- [x] **BDB2**: Access denied when database name exists but is not the good one for storage output
- [x] **BDB3**: Access denied when database name does not exist for sql output
- [x] **BDB4**: Access denied when database name does not exist for storage and sql outputs
- [x] **BDB5**: cbd does not crash if the storage/sql db_host is wrong
- [x] **BDB6**: cbd does not crash if the sql db_host is wrong
- [x] **BDB7**: access denied when database user password is wrong for perfdata/sql
- [x] **BDB8**: access denied when database user password is wrong for perfdata/sql
- [x] **BDB9**: access denied when database user password is wrong for sql
- [x] **BDB10**: connection should be established when user password is good for sql/perfdata
- [x] **BEDB2**: start broker/engine and then start MariaDB => connection is established
- [x] **BEDB3**: start broker/engine, then stop MariaDB and then start it again. The gRPC API should give informations about SQL connections.
- [x] **BEDB4**: start broker/engine, then stop MariaDB and then start it again. The gRPC API should give informations about SQL connections.
- [x] **BDBM1**: start broker/engine and then start MariaDB => connection is established
- [x] **BSS1**: Start-Stop two instances of broker and no coredump
- [x] **BSS2**: Start/Stop 10 times broker with 300ms interval and no coredump
- [x] **BSS3**: Start-Stop one instance of broker and no coredump
- [x] **BSS4**: Start/Stop 10 times broker with 1sec interval and no coredump
- [x] **BSS5**: Start-Stop with reversed connection on TCP acceptor with only one instance and no deadlock
- [x] **BCL1**: Starting broker with option '-s foobar' should return an error
- [x] **BCL2**: Starting broker with option '-s5' should work
- [x] **BCL3**: Starting broker with options '-D' should work and activate diagnose mode
- [x] **BCL4**: Starting broker with options '-s2' and '-D' should work.

### Broker/database
- [x] **NetworkDbFail1**: network failure test between broker and database (shutting down connection for 100ms)
- [x] **NetworkDbFail2**: network failure test between broker and database (shutting down connection for 1s)
- [x] **NetworkDbFail3**: network failure test between broker and database (shutting down connection for 10s)
- [x] **NetworkDbFail4**: network failure test between broker and database (shutting down connection for 30s)
- [x] **NetworkDbFail5**: network failure test between broker and database (shutting down connection for 60s)
- [x] **NetworkDBFail6**: network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s
- [x] **NetworkDBFail7**: network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s

### Broker/engine
- [x] **BECC1**: Broker/Engine communication with compression between central and poller
- [x] **BRGC1**: Broker good reverse connection
- [x] **BRCTS1**: Broker reverse connection too slow
- [x] **BRCS1**: Broker reverse connection stopped
- [x] **BEDTMASS1**: New services with several pollers
- [x] **BEEXTCMD2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0
- [x] **BEEXTCMD4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0
- [x] **BEEXTCMD6**: external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo2.0
- [x] **BEEXTCMD8**: external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo2.0
- [x] **BEEXTCMD10**: external command CHANGE_MAX_SVC_CHECK_ATTEMPTS on bbdo2.0
- [x] **BEEXTCMD12**: external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo2.0
- [x] **BEEXTCMD16**: external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo2.0
- [x] **BEEXTCMD18**: external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo2.0
- [x] **EBNHG1**: New host group with several pollers and connections to DB
- [x] **EBNHG4**: New host group with several pollers and connections to DB with broker and rename this hostgroup
- [x] **EBSNU1**: New services with notes_url with more than 2000 characters
- [x] **EBSAU2**: New services with action_url with more than 2000 characters
- [x] **EBSN3**: New services with notes with more than 500 characters
- [x] **BERD1**: Starting/stopping Broker does not create duplicated events.
- [x] **BERD2**: Starting/stopping Engine does not create duplicated events.
- [x] **BERDUC1**: Starting/stopping Broker does not create duplicated events in usual cases
- [x] **BERDUC2**: Starting/stopping Engine does not create duplicated events in usual cases
- [x] **ENRSCHE1**: check next check of reschedule is last_check+interval_check
- [x] **EBNSG1**: New service group with several pollers and connections to DB
- [x] **EBNSVC1**: New services with several pollers
- [x] **BESS1**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
- [x] **BESS2**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
- [x] **BESS3**: Start-Stop Broker/Engine - Engine started first - Engine stopped first
- [x] **BESS4**: Start-Stop Broker/Engine - Engine started first - Broker stopped first
- [x] **BESS5**: Start-Stop Broker/engine - Engine debug level is set to all, it should not hang
- [x] **BECT1**: Broker/Engine communication with anonymous TLS between central and poller
- [x] **BECT2**: Broker/Engine communication with TLS between central and poller with key/cert
- [x] **BECT3**: Broker/Engine communication with anonymous TLS and ca certificate
- [x] **BECT4**: Broker/Engine communication with TLS between central and poller with key/cert and hostname forced

### Connector perl
- [x] **test use connector perl exist script**: test exist script
- [x] **test use connector perl unknown script**: test unknown script
- [x] **test use connector perl multiple script**: test script multiple

### Connector ssh
- [x] **TestBadUser**: test unknown user
- [x] **TestBadPwd**: test bad password
- [x] **Test6Hosts**: as 127.0.0.x point to the localhost address we will simulate check on 6 hosts

### Engine
- [x] **ESS1**: Start-Stop (0s between start/stop) 5 times one instance of engine and no coredump
- [x] **ESS2**: Start-Stop (300ms between start/stop) 5 times one instance of engine and no coredump
- [x] **ESS3**: Start-Stop (0s between start/stop) 5 times three instances of engine and no coredump
- [x] **ESS4**: Start-Stop (300ms between start/stop) 5 times three instances of engine and no coredump
- [x] **EFHC1**: Engine is configured with hosts and we force checks on one 5 times on bbdo2
- [x] **EFHC2**: Engine is configured with hosts and we force checks on one 5 times on bbdo2
- [x] **EPC1**: Check with perl connector

