*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes

Documentation	Centreon Broker only start/stop tests
Library	Process
Library	OperatingSystem
Library	../resources/Broker.py


*** Test Cases ***
BSS1
	[Documentation]	Start-Stop two instances of broker and no coredump
	[Tags]	Broker	start-stop
	Config Broker	central
        Broker Config Log	central	core	error
        Broker Config Log	central	sql	trace
	Config Broker	rrd
	Repeat Keyword	5 times	Start Stop Service	0

BSS2
	[Documentation]	Start/Stop 10 times broker with 300ms interval and no coredump
	[Tags]	Broker	start-stop
	Config Broker	central
        Broker Config Log	central	sql	trace
        Broker Config Log	central	core	error
	Repeat Keyword	10 times	Start Stop Instance	300ms

BSS3
	[Documentation]	Start-Stop one instance of broker and no coredump
	[Tags]	Broker	start-stop
	Config Broker	central
        Broker Config Log	central	sql	trace
        Broker Config Log	central	core	error
	Repeat Keyword	5 times	Start Stop Instance	0

BSS4
	[Documentation]	Start/Stop 10 times broker with 1sec interval and no coredump
	[Tags]	Broker	start-stop
	Config Broker	central
        Broker Config Log	central	sql	trace
        Broker Config Log	central	core	error
	Repeat Keyword	10 times	Start Stop Instance	1s

BSS5
	[Documentation]	Start-Stop with reversed connection on TCP acceptor with only one instance and no deadlock
	[Tags]	Broker	start-stop
	Config Broker	central
        Broker Config Log	central	sql	trace
        Broker Config Log	central	core	error
	Broker Config Output Set	central	centreon-broker-master-rrd	one_peer_retention_mode	yes
	Broker Config Output Remove	central	centreon-broker-master-rrd	host
	Repeat Keyword	5 times	Start Stop Instance	1s

BSSU1
	[Documentation]	Start-Stop with unified_sql two instances of broker and no coredump
	[Tags]	Broker	start-stop	unified_sql
	Config Broker	central
	Config Broker	rrd
	Config Broker Sql Output	central	unified_sql
	Repeat Keyword	5 times	Start Stop Service	0

BSSU2
	[Documentation]	Start/Stop with unified_sql 10 times broker with 300ms interval and no coredump
	[Tags]	Broker	start-stop	unified_sql
	Config Broker	central
	Config Broker Sql Output	central	unified_sql
	Repeat Keyword	10 times	Start Stop Instance	300ms

BSSU3
	[Documentation]	Start-Stop with unified_sql one instance of broker and no coredump
	[Tags]	Broker	start-stop	unified_sql
	Config Broker	central
	Config Broker Sql Output	central	unified_sql
	Repeat Keyword	5 times	Start Stop Instance	0

BSSU4
	[Documentation]	Start/Stop with unified_sql 10 times broker with 1sec interval and no coredump
	[Tags]	Broker	start-stop	unified_sql
	Config Broker	central
	Config Broker Sql Output	central	unified_sql
	Repeat Keyword	10 times	Start Stop Instance	1s

BSSU5
	[Documentation]	Start-Stop with unified_sql with reversed connection on TCP acceptor with only one instance and no deadlock
	[Tags]	Broker	start-stop	unified_sql
	Config Broker	central
	Config Broker Sql Output	central	unified_sql
	Broker Config Output Set	central	centreon-broker-master-rrd	one_peer_retention_mode	yes
	Broker Config Output Remove	central	centreon-broker-master-rrd	host
	Repeat Keyword	5 times	Start Stop Instance	1s

*** Keywords ***
Start Stop Service
	[Arguments]	${interval}
	Start Process	/usr/sbin/cbd	/etc/centreon-broker/central-broker.json	alias=b1
	Start Process	/usr/sbin/cbd	/etc/centreon-broker/central-rrd.json	alias=b2
	Sleep	${interval}
	${result1}=	Terminate Process	b1
	Should Be Equal As Integers	${result1.rc}	0	msg=Broker b1 not correctly stopped (code ${result1.rc})
	${result2}=	Terminate Process	b2
	Should Be Equal As Integers	${result2.rc}	0	msg=Broker b2 not correctly stopped (code ${result2.rc})

Start Stop Instance
	[Arguments]	${interval}
	Start Process	/usr/sbin/cbd	/etc/centreon-broker/central-broker.json
	Sleep	${interval}
	${result}=	Terminate Process
	Should Be True	${result.rc} == -15 or ${result.rc} == 0	msg=Broker instance not correctly stopped (code ${result.rc})

