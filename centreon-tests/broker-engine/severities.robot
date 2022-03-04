*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes

Documentation	Engine/Broker tests on severities.
Library	Process
Library	DateTime
Library	OperatingSystem
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py
Library	../resources/specific-duplication.py

*** Test Cases ***
BESEV1
	[Documentation]	Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table.
	[Tags]	Broker	Engine	protobuf	bbdo	severities
        Clear DB	severities
	Config Engine	${1}
        Create Severities File	${20}
        Config Engine Add Cfg File	severities.cfg
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Broker Config Log	module	neb	debug
	Broker Config Log	central	sql	debug
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
        ${result}=	check severity With Timeout	severity20	5	1	30
        Should Be True	${result}	msg=severity20 should be of level 5 with icon_id 1
        ${result}=	check severity With Timeout	severity1	1	5	30
        Should Be True	${result}	msg=severity1 should be of level 1 with icon_id 5
	Stop Engine
	Stop Broker

BESEV2
	[Documentation]	Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table.
	[Tags]	Broker	Engine	protobuf	bbdo	severities
        Clear DB	severities
	Config Engine	${1}
        Create Severities File	${20}
        Config Engine Add Cfg File	severities.cfg
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Broker Config Log	module	neb	debug
	Broker Config Log	central	sql	debug
	Clear Retention
	${start}=	Get Current Date
	Start Engine
        Sleep	1s
	Start Broker
        ${result}=	check severity With Timeout	severity20	5	1	30
        Should Be True	${result}	msg=severity20 should be of level 5 with icon_id 1
        ${result}=	check severity With Timeout	severity1	1	5	30
        Should Be True	${result}	msg=severity1 should be of level 1 with icon_id 5
	Stop Engine
	Stop Broker
