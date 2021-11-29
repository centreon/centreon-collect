*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown    Clean After Suite
Test Setup	Stop Processes

Documentation	Centreon Broker tests on dublicated data that could come from retention when centengine or cbd are restarted
Library	Process
Library	DateTime
Library	OperatingSystem
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py
Library	../resources/specific-duplication.py

*** Test Cases ***
BERD1
	[Documentation]	Starting/stopping Broker does not create duplicated events.
	[Tags]	Broker	Engine	start-stop	duplicate	retention
	Config Engine	${1}
	Config Broker	central
        Broker Config Clear Outputs Except	central	["ipv4"]
        Broker Config Add Lua Output	central	test-doubles	${SCRIPTS}test-doubles-c.lua
	Broker Config Log	central	lua	debug
	Config Broker	module
        Broker Config Add Lua Output	module	test-doubles	${SCRIPTS}test-doubles.lua
	Broker Config Log	module	lua	debug
	Config Broker	rrd
        Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	lua: starting internal thread.
	${logCbd}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${logModule}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-module-master.log
	${result}=	Find In Log with timeout	${logCbd}	${start}	${content}	30
	Should Be True	${result}	msg=Lua not started in cbd
	${result}=	Find In Log with timeout	${logModule}	${start}	${content}	30
	Should Be True	${result}	msg=Lua not started in centengine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected.
        Sleep	5s
	Stop Broker
        Sleep	5s
        Clear Cache
        Start Broker
        Sleep	25s
	Stop Engine
        Stop Broker
	${result}=	Compare Md5	/tmp/lua-engine.log	/tmp/lua.log
	Should Be True  ${result}   msg=Contents of /tmp/lua.log and /tmp/lua-engine.log do not match.

