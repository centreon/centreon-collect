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


BESS_CRYPTED_GRPC1
	[Documentation]	Start-Stop grpc version Broker/Engine - well configured
	[Tags]	Broker	Engine	start-stop
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
	Copy File  ../broker/grpc/test/grpc_test_keys/ca_1234.crt  /tmp/
	Copy File  ../broker/grpc/test/grpc_test_keys/server_1234.key  /tmp/
	Copy File  ../broker/grpc/test/grpc_test_keys/server_1234.crt  /tmp/
	Change Broker tcp output to grpc	central
	Change Broker tcp output to grpc	module0
	Change Broker tcp input to grpc     central
	Change Broker tcp input to grpc     rrd
	Add Broker tcp output grpc crypto  module0  True  False
	Add Broker tcp input grpc crypto  central  True  False
	Remove Host from broker output  module0  central-module-master-output
	Add Host to broker output  module0  central-module-master-output  localhost
	FOR	${i}	IN RANGE	0	5
		Start Broker
		Start Engine
		${result}=	Check Connections
		Should Be True	${result}
		Stop Broker
		Stop Engine
	END

BESS_CRYPTED_GRPC2
	[Documentation]	Start-Stop grpc version Broker/Engine only server crypted
	[Tags]	Broker	Engine	start-stop
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
	Copy File  ../broker/grpc/test/grpc_test_keys/ca_1234.crt  /tmp/
	Copy File  ../broker/grpc/test/grpc_test_keys/server_1234.key  /tmp/
	Copy File  ../broker/grpc/test/grpc_test_keys/server_1234.crt  /tmp/
	Change Broker tcp output to grpc	central
	Change Broker tcp output to grpc	module0
	Change Broker tcp input to grpc     central
	Change Broker tcp input to grpc     rrd
	Add Broker tcp input grpc crypto  central  True  False
	FOR	${i}	IN RANGE	0	5
		Start Broker
		Start Engine
		Sleep  2s
		Stop Broker
		Stop Engine
	END

BESS_CRYPTED_GRPC3
	[Documentation]	Start-Stop grpc version Broker/Engine only engine crypted
	[Tags]	Broker	Engine	start-stop
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
	Copy File  ../broker/grpc/test/grpc_test_keys/ca_1234.crt  /tmp/
	Copy File  ../broker/grpc/test/grpc_test_keys/server_1234.key  /tmp/
	Copy File  ../broker/grpc/test/grpc_test_keys/server_1234.crt  /tmp/
	Change Broker tcp output to grpc	central
	Change Broker tcp output to grpc	module0
	Change Broker tcp input to grpc     central
	Change Broker tcp input to grpc     rrd
	Add Broker tcp output grpc crypto  module0  True  False
	FOR	${i}	IN RANGE	0	5
		Start Broker
		Start Engine
		Sleep  2s
		Stop Broker
		Stop Engine
	END

BESS_CRYPTED_REVERSED_GRPC1
	[Documentation]	Start-Stop grpc version Broker/Engine - well configured
	[Tags]	Broker	Engine	start-stop
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
	Copy File  ../broker/grpc/test/grpc_test_keys/ca_1234.crt  /tmp/
	Copy File  ../broker/grpc/test/grpc_test_keys/server_1234.key  /tmp/
	Copy File  ../broker/grpc/test/grpc_test_keys/server_1234.crt  /tmp/
	Change Broker tcp output to grpc	central
	Change Broker tcp output to grpc	module0
	Change Broker tcp input to grpc     central
	Change Broker tcp input to grpc     rrd
	Add Broker tcp output grpc crypto  module0  True  True
	Add Broker tcp input grpc crypto  central  True  True 
	Add Host to broker input  central  central-broker-master-input  localhost
	Remove Host from broker output  module0  central-module-master-output
	FOR	${i}	IN RANGE	0	5
		Start Broker
		Start Engine
		${result}=	Check Connections
		Should Be True	${result}
		Sleep  2s
		Stop Broker
		Stop Engine
	END

BESS_CRYPTED_REVERSED_GRPC2
	[Documentation]	Start-Stop grpc version Broker/Engine only engine server crypted
	[Tags]	Broker	Engine	start-stop
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
	Copy File  ../broker/grpc/test/grpc_test_keys/ca_1234.crt  /tmp/
	Copy File  ../broker/grpc/test/grpc_test_keys/server_1234.key  /tmp/
	Copy File  ../broker/grpc/test/grpc_test_keys/server_1234.crt  /tmp/
	Change Broker tcp output to grpc	central
	Change Broker tcp output to grpc	module0
	Change Broker tcp input to grpc     central
	Change Broker tcp input to grpc     rrd
	Add Broker tcp output grpc crypto  module0  True  True
	Add Host to broker input  central  central-broker-master-input  localhost
	Remove Host from broker output  module0  central-module-master-output
	FOR	${i}	IN RANGE	0	5
		Start Broker
		Start Engine
		Sleep  5s
		Stop Broker
		Stop Engine
	END

BESS_CRYPTED_REVERSED_GRPC3
	[Documentation]	Start-Stop grpc version Broker/Engine only engine crypted
	[Tags]	Broker	Engine	start-stop
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
	Copy File  ../broker/grpc/test/grpc_test_keys/ca_1234.crt  /tmp/
	Change Broker tcp output to grpc	central
	Change Broker tcp output to grpc	module0
	Change Broker tcp input to grpc     central
	Change Broker tcp input to grpc     rrd
	Add Broker tcp input grpc crypto  central  True  True 
	Add Host to broker input  central  central-broker-master-input  localhost
	Remove Host from broker output  module0  central-module-master-output
	FOR	${i}	IN RANGE	0	5
		Start Broker
		Start Engine
		Sleep  5s
		Stop Broker
		Stop Engine
	END

