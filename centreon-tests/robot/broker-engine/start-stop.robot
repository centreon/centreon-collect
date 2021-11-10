*** Settings ***
Resource	../ressources/ressources.robot
Suite Setup	Clean Before Test
Suite Teardown    Clean After Test

Documentation	Centreon Broker and Engine start/stop tests
Library	Process
Library	OperatingSystem
Library	../engine/Engine.py
Library	../broker/Broker.py
Library	Common.py

*** Test cases ***
BESS1: Start-Stop Broker/Engine - Broker started first - Broker stopped first
	[Tags]	Broker	Engine	start-stop
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}
	Stop Broker
	Stop Engine

BESS2: Start-Stop Broker/Engine - Broker started first - Engine stopped first
	[Tags]	Broker	Engine	start-stop
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}
	Stop Engine
	Stop Broker

BESS3: Start-Stop Broker/Engine - Engine started first - Engine stopped first
	[Tags]	Broker	Engine	start-stop
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
	Start Engine
	Start Broker
	${result}=	Check Connections
	Should Be True	${result}
	Stop Engine
	Stop Broker

BESS4: Start-Stop Broker/Engine - Engine started first - Broker stopped first
	[Tags]	Broker	Engine	start-stop
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
	Start Engine
	Start Broker
	${result}=	Check Connections
	Should Be True	${result}
	Stop Broker
	Stop Engine

BESS5: Start-Stop Broker/engine - Engine debug level is set to all, it should not hang
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
	Stop Broker
	Stop Engine


*** Keywords ***

Check Connections
	${count}=	Get Engines Count
	${pid1}=	Get Process Id	b1
	FOR	${idx}	IN RANGE	0	${count}
		${alias}=	Catenate	SEPARATOR=	e	${idx}
		${pid2}=	Get Process Id	${alias}
		${retval}=	Check Connection	5669	${pid1}	${pid2}
		Return From Keyword If	${retval} == ${False}	${False}
	END
	${pid2}=	Get Process Id	b2
	${retval}=	Check Connection	5670	${pid1}	${pid2}
	[Return]	${retval}

