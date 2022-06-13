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
- [x] **BDBU1**: Access denied when database name exists but is not the good one for unified sql output
- [x] **BDBU3**: Access denied when database name does not exist for unified sql output
- [x] **BDBU5**: cbd does not crash if the unified sql db_host is wrong
- [x] **BDBU7**: Access denied when database user password is wrong for unified sql
- [x] **BDBU10**: Connection should be established when user password is good for unified sql
- [x] **BDBMU1**: start broker/engine with unified sql and then start MariaDB => connection is established
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
- [x] **BCL1**: Starting broker with option '-s foobar' should return an error
- [x] **BCL2**: Starting broker with option '-s 5' should work

### Broker/database
- [x] **NetworkDbFail1**: network failure test between broker and database (shutting down connection for 100ms)
- [x] **NetworkDbFail2**: network failure test between broker and database (shutting down connection for 1s)
- [x] **NetworkDbFail3**: network failure test between broker and database (shutting down connection for 10s)
- [x] **NetworkDbFail4**: network failure test between broker and database (shutting down connection for 30s)
- [x] **NetworkDbFail5**: network failure test between broker and database (shutting down connection for 60s)
- [x] **NetworkDBFail6**: network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s
- [x] **NetworkDBFailU6**: network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s (with unified_sql)
- [x] **NetworkDBFail7**: network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s
- [x] **NetworkDBFailU7**: network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s (with unified_sql)

### Broker/engine
- [x] **BEPBBEE1**: central-module configured with bbdo_version 3.0 but not others. Unable to establish connection.
- [x] **BEPBBEE2**: bbdo_version 3 not compatible with sql/storage
- [x] **BEPBBEE3**: bbdo_version 3 generates new bbdo protobuf service status messages.
- [x] **BEPBBEE4**: bbdo_version 3 generates new bbdo protobuf host status messages.
- [x] **BEPBBEE5**: bbdo_version 3 generates new bbdo protobuf service messages.
- [x] **BECC1**: Broker/Engine communication with compression between central and poller
- [x] **EBNHG1**: New host group with several pollers and connections to DB
- [x] **EBNHGU1**: New host group with several pollers and connections to DB with broker configured with unified_sql
- [x] **EBNHGU2**: New host group with several pollers and connections to DB with broker configured with unified_sql
- [x] **EBNHGU3**: New host group with several pollers and connections to DB with broker configured with unified_sql
- [x] **EBNHG4**: New host group with several pollers and connections to DB with broker and rename this hostgroup
- [x] **EBNHGU4**: New host group with several pollers and connections to DB with broker and rename this hostgroup
- [x] **LOGV2EB1**: log-v2 enabled  old log disabled check broker sink
- [x] **LOGV2DB1**: log-v2 disabled old log enabled check broker sink
- [x] **LOGV2DB2**: log-v2 disabled old log disabled check broker sink
- [x] **LOGV2EB2**: log-v2 enabled old log enabled check broker sink
- [x] **LOGV2EF1**: log-v2 enabled  old log disabled check logfile sink
- [x] **LOGV2DF1**: log-v2 disabled old log enabled check logfile sink
- [x] **LOGV2DF2**: log-v2 disabled old log disabled check logfile sink
- [x] **LOGV2EF2**: log-v2 enabled old log enabled check logfile sink
- [x] **LOGV2BE2**: log-v2 enabled old log enabled check broker sink is equal
- [x] **LOGV2FE2**: log-v2 enabled old log enabled check logfile sink
- [x] **BERES1**: store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
- [x] **BEHS1**: store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
- [x] **BERD1**: Starting/stopping Broker does not create duplicated events.
- [x] **BERD2**: Starting/stopping Engine does not create duplicated events.
- [x] **BERDUC1**: Starting/stopping Broker does not create duplicated events in usual cases
- [x] **BERDUCU1**: Starting/stopping Broker does not create duplicated events in usual cases with unified_sql
- [x] **BERDUC2**: Starting/stopping Engine does not create duplicated events in usual cases
- [x] **BERDUCU2**: Starting/stopping Engine does not create duplicated events in usual cases with unified_sql
- [x] **BERDUC3U1**: Starting/stopping Broker does not create duplicated events in usual cases with unified_sql and BBDO 3.0
- [x] **BERDUC3U2**: Starting/stopping Engine does not create duplicated events in usual cases with unified_sql and BBDO 3.0
- [x] **BRGC1**: Broker good reverse connection
- [x] **BRCTS1**: Broker reverse connection too slow
- [x] **BRCS1**: Broker reverse connection stopped
- [x] **BRRDDM1**: RRD metrics deletion from metric ids.
- [x] **BRRDDID1**: RRD metrics deletion from index ids.
- [x] **BRRDDMID1**: RRD deletion of non existing metrics and indexes
- [x] **BRRDDMU1**: RRD metric deletion on table metric with unified sql output
- [x] **BRRDDIDU1**: RRD metrics deletion from index ids with unified sql output.
- [x] **BRRDDMIDU1**: RRD deletion of non existing metrics and indexes
- [x] **BRRDRM1**: RRD metric rebuild with gRPC API and unified sql
- [x] **BRRDRMU1**: RRD metric rebuild with gRPC API and unified sql
- [x] **ENRSCHE1**: check next check of reschedule is last_check+interval_check
- [x] **EBNSG1**: New service group with several pollers and connections to DB
- [x] **EBNSGU1**: New service group with several pollers and connections to DB with broker configured with unified_sql
- [x] **EBNSGU2**: New service group with several pollers and connections to DB with broker configured with unified_sql
- [x] **EBNSVC1**: New services with several pollers
- [x] **BERD1**: Starting/stopping Broker does not create duplicated events.
- [x] **BERD2**: Starting/stopping Engine does not create duplicated events.
- [x] **BERDUC1**: Starting/stopping Broker does not create duplicated events in usual cases
- [x] **BERDUCU1**: Starting/stopping Broker does not create duplicated events in usual cases with unified_sql
- [x] **BERDUC2**: Starting/stopping Engine does not create duplicated events in usual cases
- [x] **BERDUCU2**: Starting/stopping Engine does not create duplicated events in usual cases with unified_sql
- [x] **BERDUC3U1**: Starting/stopping Broker does not create duplicated events in usual cases with unified_sql and BBDO 3.0
- [x] **BERDUC3U2**: Starting/stopping Engine does not create duplicated events in usual cases with unified_sql and BBDO 3.0
- [x] **BEDTMASS1**: New services with several pollers
- [x] **BEDTMASS2**: New services with several pollers
- [x] **BESS1**: Start-Stop Broker/Engine - Broker started first - Broker stopped first
- [x] **BESS2**: Start-Stop Broker/Engine - Broker started first - Engine stopped first
- [x] **BESS3**: Start-Stop Broker/Engine - Engine started first - Engine stopped first
- [x] **BESS4**: Start-Stop Broker/Engine - Engine started first - Broker stopped first
- [x] **BESS5**: Start-Stop Broker/engine - Engine debug level is set to all, it should not hang
- [x] **BESS_GRPC1**: Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped first
- [x] **BESS_GRPC2**: Start-Stop grpc version Broker/Engine - Broker started first - Engine stopped first
- [x] **BESS_GRPC3**: Start-Stop grpc version Broker/Engine - Engine started first - Engine stopped first
- [x] **BESS_GRPC4**: Start-Stop grpc version Broker/Engine - Engine started first - Broker stopped first
- [x] **BESS_GRPC5**: Start-Stop grpc version Broker/engine - Engine debug level is set to all, it should not hang
- [x] **BESS_GRPC_COMPRESS1**: Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped first compression activated
- [x] **BECT1**: Broker/Engine communication with anonymous TLS between central and poller
- [x] **BECT2**: Broker/Engine communication with TLS between central and poller with key/cert
- [x] **BECT3**: Broker/Engine communication with anonymous TLS and ca certificate
- [x] **BECT4**: Broker/Engine communication with TLS between central and poller with key/cert and hostname forced
- [x] **BECT_GRPC1**: Broker/Engine communication with anonymous TLS between central and poller
- [x] **BECT_GRPC2**: Broker/Engine communication with TLS between central and poller with key/cert
- [x] **BECT_GRPC3**: Broker/Engine communication with anonymous TLS and ca certificate
- [x] **BECT_GRPC4**: Broker/Engine communication with TLS between central and poller with key/cert and hostname forced
- [x] **BEDTMASS1**: New services with several pollers
- [x] **BEDTMASS2**: New services with several pollers
- [x] **BETAG1**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Broker is started before.
- [x] **BETAG2**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
- [x] **BEUTAG1**: Engine is configured with some tags. When broker receives them through unified_sql stream, it stores them in the centreon_storage.tags table. Broker is started before.
- [x] **BEUTAG2**: Engine is configured with some tags. A new service is added with a tag. Broker should make the relations.
- [x] **BEUTAG3**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
- [x] **BEUTAG4**: Engine is configured with some tags. Group tags tag9, tag13 are set to services 1 and 3. Category tags tag3 and tag11 are added to services 1, 3, 5 and 6. The centreon_storage.resources and resources_tags tables are well filled.
- [x] **BEUTAG5**: Engine is configured with some tags. Group tags tag2, tag6 are set to hosts 1 and 2. Category tags tag4 and tag8 are added to hosts 2, 3, 4. The resources and resources_tags tables are well filled.
- [x] **BEUTAG6**: Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.resources_tags table. Engine is started before.
- [x] **BEUTAG7**: some services are configured and deleted with tags on two pollers.
- [x] **BEUTAG8**: Services have tags provided by templates.
- [x] **BEUTAG9**: hosts have tags provided by templates.
- [x] **BEUTAG10**: some services are configured with tags on two pollers. Then tags are removed from some of them and in centreon_storage, we can observe resources_tags table updated.
- [x] **BEUTAG11**: some services are configured with tags on two pollers. Then several tags are removed, and we can observe resources_tags table updated.
- [x] **BEUTAG12**: Engine is configured with some tags. Group tags tag2, tag6 are set to hosts 1 and 2. Category tags tag4 and tag8 are added to hosts 2, 3, 4. The resources and resources_tags tables are well filled. The tag6 and tag8 are removed and resources_tags is also well updated.
- [x] **BEEXTCMD1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0
- [x] **BEEXTCMD2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0
- [x] **BEEXTCMD3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0
- [x] **BEEXTCMD4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0
- [x] **BEEXTCMD5**: external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo3.0
- [x] **BEEXTCMD6**: external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo2.0
- [x] **BEEXTCMD7**: external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo3.0
- [x] **BEEXTCMD8**: external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo2.0
- [x] **BEEXTCMD9**: external command CHANGE_MAX_SVC_CHECK_ATTEMPTS on bbdo3.0
- [x] **BEEXTCMD10**: external command CHANGE_MAX_SVC_CHECK_ATTEMPTS on bbdo2.0
- [x] **BEEXTCMD11**: external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo3.0
- [x] **BEEXTCMD12**: external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo2.0
- [x] **BEEXTCMD13**: external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo3.0
- [x] **BEEXTCMD14**: external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo2.0
- [x] **BEEXTCMD15**: external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo3.0
- [x] **BEEXTCMD16**: external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo2.0
- [x] **BEEXTCMD17**: external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo3.0
- [x] **BEEXTCMD18**: external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo2.0
- [x] **BEEXTCMD19**: external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo3.0
- [x] **BEEXTCMD20**: external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo2.0
- [x] **BEEXTCMD21**: external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS and ENABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo3.0
- [x] **BEEXTCMD22**: external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS and ENABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo2.0
- [x] **BEEXTCMD23**: external command DISABLE_HOST_CHECK and ENABLE_HOST_CHECK on bbdo3.0
- [x] **BEEXTCMD24**: external command DISABLE_HOST_CHECK and ENABLE_HOST_CHECK on bbdo2.0
- [x] **BEEXTCMD25**: external command DISABLE_HOST_EVENT_HANDLER and ENABLE_HOST_EVENT_HANDLER on bbdo3.0
- [x] **BEEXTCMD26**: external command DISABLE_HOST_EVENT_HANDLER and ENABLE_HOST_EVENT_HANDLER on bbdo2.0
- [x] **BEEXTCMD27**: external command DISABLE_HOST_FLAP_DETECTION and ENABLE_HOST_FLAP_DETECTION on bbdo3.0
- [x] **BEEXTCMD28**: external command DISABLE_HOST_FLAP_DETECTION and ENABLE_HOST_FLAP_DETECTION on bbdo2.0
- [x] **BEEXTCMD29**: external command DISABLE_HOST_NOTIFICATIONS and ENABLE_HOST_NOTIFICATIONS on bbdo3.0
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
- [x] **BEEXTCMD40**: external command START_OBSESSING_OVER_HOST and STOP_OBSESSING_OVER_HOST on bbdo2.0
- [x] **BEEXTCMD41**: external command START_OBSESSING_OVER_SVC and STOP_OBSESSING_OVER_SVC on bbdo3.0
- [x] **BEEXTCMD42**: external command START_OBSESSING_OVER_SVC and STOP_OBSESSING_OVER_SVC on bbdo2.0
- [x] **BEEXTCMD_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and grpc
- [x] **BEEXTCMD_GRPC2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 and grpc
- [x] **BEEXTCMD_GRPC3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 and grpc
- [x] **BEEXTCMD_GRPC4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 and grpc
- [x] **BEEXTCMD_REVERSE_GRPC1**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0 and reversed gRPC
- [x] **BEEXTCMD_REVERSE_GRPC2**: external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0 and grpc reversed
- [x] **BEEXTCMD_REVERSE_GRPC3**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0 and grpc reversed
- [x] **BEEXTCMD_REVERSE_GRPC4**: external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0 and grpc reversed
- [x] **EBSNU1**: New services with notes_url with more than 2000 characters
- [x] **EBSAU2**: New services with action_url with more than 2000 characters
- [x] **EBSN3**: New services with notes with more than 500 characters

### Connector perl
- [x] **test use connector perl exist script**: test exist script
- [x] **test use connector perl unknown script**: test unknown script
- [x] **test use connector perl multiple script**: test script multiple

### Connector ssh
- [x] **TestBadUser**: test unknown user
- [x] **TestBadPwd**: test bad password
- [x] **Test6Hosts**: as 127.0.0.x point to the localhost address we will simulate check on 6 hosts

### Engine
- [x] **EFHC1**: Engine is configured with hosts and we force checks on one 5 times on bbdo2
- [x] **EFHC2**: Engine is configured with hosts and we force checks on one 5 times on bbdo2
- [x] **EFHCU1**: Engine is configured with hosts and we force checks on one 5 times on bbdo3. Bbdo3 has no impact on this behavior. resources table is cleared before starting broker.
- [x] **EFHCU2**: Engine is configured with hosts and we force checks on one 5 times on bbdo3. Bbdo3 has no impact on this behavior.
- [x] **EPC1**: Check with perl connector
- [x] **ESS1**: Start-Stop (0s between start/stop) 5 times one instance of engine and no coredump
- [x] **ESS2**: Start-Stop (300ms between start/stop) 5 times one instance of engine and no coredump
- [x] **ESS3**: Start-Stop (0s between start/stop) 5 times three instances of engine and no coredump
- [x] **ESS4**: Start-Stop (300ms between start/stop) 5 times three instances of engine and no coredump

### Migration
- [x] **MIGRATION**: Migration bbdo2 => sql/storage => unified_sql => bbdo3

### Severities
- [x] **BEUHSEV1**: Four hosts have a severity added. Then we remove the severity from host 1. Then we change severity 10 to severity8 for host 3.
- [x] **BEUHSEV2**: Seven hosts are configured with a severity on two pollers. Then we remove severities from the first and second hosts of the first poller but only the severity from the first host of the second poller.
- [x] **BETUHSEV1**: Hosts have severities provided by templates.
- [x] **BESEV1**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Broker is started before.
- [x] **BESEV2**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Engine is started before.
- [x] **BEUSEV1**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Broker is started before.
- [x] **BEUSEV2**: Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Engine is started before.
- [x] **BEUSEV3**: Four services have a severity added. Then we remove the severity from service 1. Then we change severity 11 to severity7 for service 3.
- [x] **BEUSEV4**: Seven services are configured with a severity on two pollers. Then we remove severities from the first and second services of the first poller but only the severity from the first service of the second poller. Then only severities no more used should be removed from the database.
- [x] **BETUSEV1**: Services have severities provided by templates.
