*** Settings ***
Resource	../ressources/ressources.robot
Suite Setup	Clean Before Test
Suite Teardown    Clean After Test

Documentation	Centreon Broker only start/stop tests
Library	Process
Library	OperatingSystem
Library	Broker.py


*** Test cases ***
BSS1: Start-Stop two instances of broker and no coredump
	[Tags]	Broker	start-stop
	Config Broker	central
	Repeat Keyword	5 times	Start Stop Service	0

BSS3: Start-Stop one instance of broker and no coredump
	[Tags]	Broker	start-stop
	Config Broker	central
	Repeat Keyword	5 times	Start Stop Instance	0

BSS5: Start-Stop with reversed connection on TCP acceptor with only one instance and no deadlock
	[Tags]	Broker	start-stop
	Config Broker	central
	Broker Config Output Set	central	centreon-broker-master-rrd	one_peer_retention_mode	yes
	Broker Config Output Remove	central	centreon-broker-master-rrd	host
	Repeat Keyword	5 times	Start Stop Instance	1s

BSS2: Start/Stop 10 times broker with 300ms interval and no coredump
	[Tags]	Broker	start-stop
	Config Broker	central
	Repeat Keyword	10 times	Start Stop Instance	300ms

BSS4: Start/Stop 10 times broker with 1sec interval and no coredump
	[Tags]	Broker	start-stop
	Config Broker	central
	Repeat Keyword	10 times	Start Stop Instance	1s

*** Keywords ***
Start Stop Service
	[Arguments]	${interval}
	Config Broker	central
	Config Broker	rrd
	Start Process	/usr/sbin/cbd	/etc/centreon-broker/central-broker.json	alias=b1
	Start Process	/usr/sbin/cbd	/etc/centreon-broker/central-rrd.json	alias=b2
	Sleep	${interval}
	${result1}=	Terminate Process	b1
	Should Be Equal As Integers	${result1.rc}	0
	${result2}=	Terminate Process	b2
	Should Be Equal As Integers	${result2.rc}	0

Start Stop Instance
	[Arguments]	${interval}
	Start Process	/usr/sbin/cbd	/etc/centreon-broker/central-broker.json
	Sleep	${interval}
	${result}=	Terminate Process
	Should Be True	${result.rc} == -15 or ${result.rc} == 0

