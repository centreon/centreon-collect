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
BGRPCSS1
	[Documentation]	Start-Stop two instances of broker configured with grpc stream and no coredump
	[Tags]	Broker	start-stop	grpc
	Config Broker	central
	Config Broker	rrd
	Change Broker tcp output to grpc	central
	Change Broker tcp input to grpc	rrd
	Repeat Keyword	5 times	Start Stop Service	0

BGRPCSS2
	[Documentation]	Start/Stop 10 times broker configured with grpc stream with 300ms interval and no coredump
	[Tags]	Broker	start-stop	grpc
	Config Broker	central
	Change Broker tcp output to grpc	central
	Repeat Keyword	10 times	Start Stop Instance	300ms

BGRPCSS3
	[Documentation]	Start-Stop one instance of broker configured with grpc stream and no coredump
	[Tags]	Broker	start-stop	grpc
	Config Broker	central
	Change Broker tcp output to grpc	central
	Repeat Keyword	5 times	Start Stop Instance	0

BGRPCSS4
	[Documentation]	Start/Stop 10 times broker configured with grpc stream with 1sec interval and no coredump
	[Tags]	Broker	start-stop	grpc
	Config Broker	central
	Change Broker tcp output to grpc	central
	Repeat Keyword	10 times	Start Stop Instance	1s

BGRPCSS5
	[Documentation]	Start-Stop with reversed connection on grpc acceptor with only one instance and no deadlock
	[Tags]	Broker	start-stop	grpc
	Config Broker	central
	Change Broker tcp output to grpc	central
	Broker Config Output Set	central	centreon-broker-master-rrd	one_peer_retention_mode	yes
	Broker Config Output Remove	central	centreon-broker-master-rrd	host
	Repeat Keyword	5 times	Start Stop Instance	1s

BGRPCSSU1
	[Documentation]	Start-Stop with unified_sql two instances of broker with grpc stream and no coredump
	[Tags]	Broker	start-stop	unified_sql	grpc
	Config Broker	central
	Config Broker	rrd
	Config Broker Sql Output	central	unified_sql
	Change Broker tcp output to grpc	central
	Change Broker tcp input to grpc	rrd
	Repeat Keyword	5 times	Start Stop Service	0

BGRPCSSU2
	[Documentation]	Start/Stop with unified_sql 10 times broker configured with grpc stream with 300ms interval and no coredump
	[Tags]	Broker	start-stop	unified_sql	grpc
	Config Broker	central
	Config Broker Sql Output	central	unified_sql
	Change Broker tcp output to grpc	central
	Repeat Keyword	10 times	Start Stop Instance	300ms

BGRPCSSU3
	[Documentation]	Start-Stop with unified_sql one instance of broker configured with grpc and no coredump
	[Tags]	Broker	start-stop	unified_sql	grpc
	Config Broker	central
	Change Broker tcp output to grpc	central
	Config Broker Sql Output	central	unified_sql
	Repeat Keyword	5 times	Start Stop Instance	0

BGRPCSSU4
	[Documentation]	Start/Stop with unified_sql 10 times broker configured with grpc stream with 1sec interval and no coredump
	[Tags]	Broker	start-stop	unified_sql	grpc
	Config Broker	central
	Change Broker tcp output to grpc	central
	Config Broker Sql Output	central	unified_sql
	Repeat Keyword	10 times	Start Stop Instance	1s

BGRPCSSU5
	[Documentation]	Start-Stop with unified_sql with reversed connection on grpc acceptor with only one instance and no deadlock
	[Tags]	Broker	start-stop	unified_sql	grpc
	Config Broker	central
	Config Broker Sql Output	central	unified_sql
	Broker Config Output Set	central	centreon-broker-master-rrd	one_peer_retention_mode	yes
	Broker Config Output Remove	central	centreon-broker-master-rrd	host
	Change Broker tcp output to grpc	central
	Repeat Keyword	5 times	Start Stop Instance	1s

*** Keywords ***
Start Stop Service
	[Arguments]	${interval}
	Start Process	/usr/sbin/cbd	/etc/centreon-broker/central-broker.json	alias=b1
	Start Process	/usr/sbin/cbd	/etc/centreon-broker/central-rrd.json	alias=b2
	Sleep	${interval}
	Send Signal To Process	SIGTERM	b1
	${result}=	Wait For Process	b1	timeout=60s	on_timeout=kill
	Should Be True	${result.rc} == -15 or ${result.rc} == 0	msg=Broker service badly stopped
	Send Signal To Process	SIGTERM	b2
	${result}=	Wait For Process	b2	timeout=60s	on_timeout=kill
	Should Be True	${result.rc} == -15 or ${result.rc} == 0	msg=Broker service badly stopped

Start Stop Instance
	[Arguments]	${interval}
	Start Process	/usr/sbin/cbd	/etc/centreon-broker/central-broker.json	alias=b1
	Sleep	${interval}
	Send Signal To Process	SIGTERM	b1
	${result}=	Wait For Process	b1	timeout=60s	on_timeout=kill
	Should Be True	${result.rc} == -15 or ${result.rc} == 0	msg=Broker instance badly stopped

