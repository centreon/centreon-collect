*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes

Documentation	Engine/Broker tests on tags.
Library	Process
Library	DateTime
Library	OperatingSystem
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py
Library	../resources/specific-duplication.py

*** Test Cases ***
BETAG1
	[Documentation]	Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Broker is started before.
	[Tags]	Broker	Engine	protobuf	bbdo	tags
	Config Engine	${1}
	Create Tags File	${0}	${20}
	Config Engine Add Cfg File	${0}	tags.cfg
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Broker Config Log	module0	neb	debug
	Broker Config Log	central	sql	debug
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	check tag With Timeout	tag20	3	30
	Should Be True	${result}	msg=tag20 should be of type 3
	${result}=	check tag With Timeout	tag1	0	30
	Should Be True	${result}	msg=tag1 should be of type 0
	Stop Engine
	Kindly Stop Broker

BETAG2
	[Documentation]	Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
	[Tags]	Broker	Engine	protobuf	bbdo	tags
	Config Engine	${1}
	Create Tags File	${0}	${20}
	Config Engine Add Cfg File	${0}	tags.cfg
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Broker Config Log	module0	neb	debug
	Broker Config Log	central	sql	debug
	Clear Retention
	${start}=	Get Current Date
	Start Engine
	Sleep	1s
	Start Broker
	${result}=	check tag With Timeout	tag20	3	30
	Should Be True	${result}	msg=tag20 should be of type 3
	${result}=	check tag With Timeout	tag1	0	30
	Should Be True	${result}	msg=tag1 should be of type 0
	Stop Engine
	Kindly Stop Broker
