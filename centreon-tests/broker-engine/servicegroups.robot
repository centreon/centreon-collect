*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes

Documentation	Centreon Broker and Engine add servicegroup
Library	Process
Library	OperatingSystem
Library	DateTime
Library	Collections
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py

*** Test Cases ***
EBNSG1
	[Documentation]	New service group with several pollers and connections to DB
	[Tags]	Broker	Engine	servicegroup
	Config Engine	${3}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${3}

	Broker Config Log	central	sql	info
	Broker Config Output Set	central	central-broker-master-sql	connections_count	5
	Broker Config Output Set	central	central-broker-master-perfdata	connections_count	5

	${start}=	Get Current Date
	Start Broker
	Start Engine
	Add service Group	${0}	${1}	["host_1","service_1", "host_1","service_2","host_1", "service_3"]
	Config Engine Add Cfg File	${0}	servicegroups.cfg
	Sleep	3s

	Reload Broker
	Reload Engine

	${content}=	Create List	enabling membership of service (1, 3) to service group 1 on instance 1	enabling membership of service (1, 2) to service group 1 on instance 1	enabling membership of service (1, 1) to service group 1 on instance 1

	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	45
	Should Be True	${result}	msg=One of the new service groups not found in logs.
	Stop Engine
	Kindly Stop Broker
