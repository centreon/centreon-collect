*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes

Documentation	Engine/Broker tests on bbdo_version 3.0.0 and protobuf bbdo embedded events.
Library	Process
Library	DateTime
Library	OperatingSystem
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py
Library	../resources/specific-duplication.py

*** Test Cases ***
BEPBBEE1
	[Documentation]	central-module configured with bbdo_version 3.0 but not others. Unable to establish connection.
	[Tags]	Broker	Engine	protobuf	bbdo
	Config Engine	${1}
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
        Broker Config Add Item	module	bbdo_version	3.0.0
	Broker Config Log	module	bbdo	debug
	Broker Config Log	central	bbdo	debug
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	[bbdo] [error] BBDO: peer is using protocol version 3.0.0 whereas we're using protocol version 2.0.0
	${result}=	Find In Log with timeout	${centralLog}	${start}	${content}	30
	Should Be True	${result}	msg=Message about not matching bbdo versions not available
	Stop Engine
	Stop Broker

BEPBBEE2
	[Documentation]	bbdo_version 3 not compatible with sql/storage
	[Tags]	Broker	Engine	protobuf	bbdo
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
        Broker Config Add Item	module	bbdo_version	3.0.0
        Broker Config Add Item	central	bbdo_version	3.0.0
        Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	sql	debug
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	[config] [error] Configuration check error: bbdo versions >= 3.0.0 need the unified_sql module to be configured.
	${result}=	Find In Log with timeout	${centralLog}	${start}	${content}	30
	Should Be True	${result}	msg=Message about a missing config of unified_sql not available.
	Stop Engine

BEPBBEE3
	[Documentation]	bbdo_version 3 generates new bbdo protobuf service status messages.
	[Tags]	Broker	Engine	protobuf	bbdo
        Remove File	/tmp/pbservicestatus.log
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
        Broker Config Add Item	module	bbdo_version	3.0.0
        Broker Config Add Item	central	bbdo_version	3.0.0
        Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	sql	debug
	Config Broker Sql Output	central	unified_sql
	Broker Config Add Lua Output	central	test-protobuf	${SCRIPTS}test-pbservicestatus.lua
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
        Wait Until Created	/tmp/pbservicestatus.log	1m
	Stop Engine
	Stop Broker

BEPBBEE4
	[Documentation]	bbdo_version 3 generates new bbdo protobuf host status messages.
	[Tags]	Broker	Engine	protobuf	bbdo
        Remove File	/tmp/pbhoststatus.log
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
        Broker Config Add Item	module	bbdo_version	3.0.0
        Broker Config Add Item	central	bbdo_version	3.0.0
        Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	sql	debug
	Config Broker Sql Output	central	unified_sql
	Broker Config Add Lua Output	central	test-protobuf	${SCRIPTS}test-pbhoststatus.lua
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
        Wait Until Created	/tmp/pbhost.log	1m
	Stop Engine
	Stop Broker

BEPBBEE5
	[Documentation]	bbdo_version 3 generates new bbdo protobuf service messages.
	[Tags]	Broker	Engine	protobuf	bbdo
        Remove File	/tmp/pbservice.log
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
        Broker Config Add Item	module	bbdo_version	3.0.0
        Broker Config Add Item	central	bbdo_version	3.0.0
        Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	sql	debug
	Config Broker Sql Output	central	unified_sql
	Broker Config Add Lua Output	central	test-protobuf	${SCRIPTS}test-pbservice.lua
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
        Wait Until Created	/tmp/pbservice.log	1m
	Stop Engine
	Stop Broker