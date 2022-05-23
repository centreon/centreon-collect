*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown    Clean After Suite
Test Setup	Stop Processes

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
	Stop Broker
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
	Stop Engine
	Stop Broker

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
	Stop Engine
	Stop Broker

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
	Stop Broker
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
	Stop Broker
	Stop Engine

BESS_GRPC1
	[Documentation]	Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped first
	[Tags]	Broker	Engine	start-stop
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
	Change Broker tcp output to grpc	central
	Change Broker tcp output to grpc	module0
	Change Broker tcp input to grpc     central
	Change Broker tcp input to grpc     rrd
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}
	Stop Broker
	Stop Engine

BESS_GRPC2
	[Documentation]	Start-Stop grpc version Broker/Engine - Broker started first - Engine stopped first
	[Tags]	Broker	Engine	start-stop
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
	Change Broker tcp output to grpc	central
	Change Broker tcp output to grpc	module0
	Change Broker tcp input to grpc     central
	Change Broker tcp input to grpc     rrd
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}
	Stop Engine
	Stop Broker

BESS_GRPC3
	[Documentation]	Start-Stop grpc version Broker/Engine - Engine started first - Engine stopped first
	[Tags]	Broker	Engine	start-stop
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
	Change Broker tcp output to grpc	central
	Change Broker tcp output to grpc	module0
	Change Broker tcp input to grpc     central
	Change Broker tcp input to grpc     rrd
	Start Engine
	Start Broker
	${result}=	Check Connections
	Should Be True	${result}
	Stop Engine
	Stop Broker

BESS_GRPC4
	[Documentation]	Start-Stop grpc version Broker/Engine - Engine started first - Broker stopped first
	[Tags]	Broker	Engine	start-stop
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
	Change Broker tcp output to grpc	central
	Change Broker tcp output to grpc	module0
	Change Broker tcp input to grpc     central
	Change Broker tcp input to grpc     rrd
	Start Engine
	Start Broker
	${result}=	Check Connections
	Should Be True	${result}
	Stop Broker
	Stop Engine

BESS_GRPC5
	[Documentation]	Start-Stop grpc version Broker/engine - Engine debug level is set to all, it should not hang
	[Tags]	Broker	Engine	start-stop
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
	Engine Config Set Value	${0}	debug_level	${-1}
	Change Broker tcp output to grpc	central
	Change Broker tcp output to grpc	module0
	Change Broker tcp input to grpc     central
	Change Broker tcp input to grpc     rrd
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}
	Stop Broker
	Stop Engine

BESS_GRPC_COMPRESS1
	[Documentation]	Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped first compression activated
	[Tags]	Broker	Engine	start-stop
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
	Change Broker tcp output to grpc	central
	Change Broker tcp output to grpc	module0
	Change Broker tcp input to grpc     central
	Change Broker tcp input to grpc     rrd
	Change Broker Compression Output  module0  yes
	Change Broker Compression Output  central  yes
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}
	Stop Broker
	Stop Engine
