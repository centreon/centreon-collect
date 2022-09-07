*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown    Clean After Suite
Test Setup	Stop Processes
Test Teardown	Save logs If Failed

Documentation	Centreon Broker and Engine start/stop tests
Library	Process
Library	OperatingSystem
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py

*** Test Cases ***
BESS1
	[Documentation]	Start-Stop Broker/Engine - Broker started first - Broker stopped first
	[Tags]	Broker	Engine	start-stop
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}
	Kindly Stop Broker
	Stop Engine

BESS2
	[Documentation]	Start-Stop Broker/Engine - Broker started first - Engine stopped first
	[Tags]	Broker	Engine	start-stop
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}
	${result}=  Check Poller Enabled In Database  1  5
	Should Be True	${result}
	Stop Engine
	${result}=  Check Poller Disabled In Database  1  5
	Should Be True	${result}
	Kindly Stop Broker

BESS3
	[Documentation]	Start-Stop Broker/Engine - Engine started first - Engine stopped first
	[Tags]	Broker	Engine	start-stop
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
	Start Engine
	Start Broker
	${result}=	Check Connections
	Should Be True	${result}
	${result}=  Check Poller Enabled In Database  1  10
	Should Be True	${result}
	Stop Engine
	${result}=  Check Poller Disabled In Database  1  5
	Should Be True	${result}
	Kindly Stop Broker

BESS4
	[Documentation]	Start-Stop Broker/Engine - Engine started first - Broker stopped first
	[Tags]	Broker	Engine	start-stop
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
	Start Engine
	Start Broker
	${result}=	Check Connections
	Should Be True	${result}
	${result}=  Check Poller Enabled In Database  1  10
	Should Be True	${result}
	Kindly Stop Broker
	Stop Engine

BESS5
	[Documentation]	Start-Stop Broker/engine - Engine debug level is set to all, it should not hang
	[Tags]	Broker	Engine	start-stop
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
	Engine Config Set Value	${0}	debug_level	${-1}
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}
	Kindly Stop Broker
	Stop Engine

