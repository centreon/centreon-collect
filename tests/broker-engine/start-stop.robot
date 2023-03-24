*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown    Clean After Suite
Test Setup	Stop Processes
Test Teardown	Save logs If Failed

Documentation	Centreon Broker and Engine start/stop tests
Library	Process
Library	DateTime
Library	OperatingSystem
Library	DateTime
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
	${result}=  Check Poller Enabled In Database  1  10
	Should Be True	${result}
	Stop Engine
	${result}=  Check Poller Disabled In Database  1  10
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
	${result}=  Check Poller Disabled In Database  1  10
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
	Kindly Stop Broker
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
	${result}=  Check Poller Enabled In Database  1  10
	Should Be True	${result}
	Stop Engine
	${result}=  Check Poller Disabled In Database  1  10
	Should Be True	${result}
	Kindly Stop Broker

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
	${result}=  Check Poller Enabled In Database  1  10
	Should Be True	${result}
	Stop Engine
	${result}=  Check Poller Disabled In Database  1  10
	Should Be True	${result}
	Kindly Stop Broker

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
	Kindly Stop Broker
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
	${result}=  Check Poller Enabled In Database  1  10
	Should Be True	${result}
	Stop Engine
	${result}=  Check Poller Disabled In Database  1  10
	Should Be True	${result}
	Kindly Stop Broker

BESS_GRPC_COMPRESS1
	[Documentation]	Start-Stop grpc version Broker/Engine - Broker started first - Broker stopped last compression activated
	[Tags]	Broker	Engine	start-stop
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
	Change Broker tcp output to grpc	central
	Change Broker tcp output to grpc	module0
	Change Broker tcp input to grpc     central
	Change Broker tcp input to grpc     rrd
	Change Broker Compression Output  module0  central-module-master-output  yes
	Change Broker Compression Input  central  centreon-broker-master-input  yes
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}
	${result}=  Check Poller Enabled In Database  1  10
	Should Be True	${result}
	Stop Engine
	${result}=  Check Poller Disabled In Database  1  10
	Should Be True	${result}
	Kindly Stop Broker


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
		${result}=  Check Poller Enabled In Database  1  10
		Should Be True	${result}
		Stop Engine
		${result}=  Check Poller Disabled In Database  1  10
		Should Be True	${result}
		Kindly Stop Broker
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
		Kindly Stop Broker
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
		Kindly Stop Broker
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
		Kindly Stop Broker
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
		Kindly Stop Broker
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
		Kindly Stop Broker
		Stop Engine
	END

BESS_ENGINE_DELETE_HOST
	[Documentation]	once engine and cbd started, stop and restart cbd, delete an host and reload engine, cbd mustn't core
	[Tags]	Broker	Engine	start-stop
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Clear Retention
	${start}=	Get Current Date
	Start Broker  True
	Start Engine
	${content}=	Create List	check_for_external_commands
	${result}=	Find In Log with Timeout	${engineLog0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
	Kindly Stop Broker  True
	Start Broker  True
	engine_config_remove_service_host  ${0}  host_16
	engine_config_remove_host  ${0}  host_16
	Reload Engine
	Sleep  2s
	Kindly Stop Broker  True
	Stop Engine

BESSBQ1
	[Documentation]	A very bad queue file is written for broker. Broker and Engine are then started, Broker must read the file raising an error because of that file and then get data sent by Engine.
	[Tags]	Broker	Engine	start-stop	queue
	Config Engine	${1}
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
        Broker Config Flush Log	central	0
	Broker Config Log	central	core	error
	Broker Config Log	central	bbdo	debug
	Broker Config Log	central	sql	trace
	Broker Config Log	central	core	debug
	Config Broker Sql Output	central	unified_sql
	Clear Retention
	Create bad queue	central-broker-master.queue.central-broker-master-sql
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	execute statement 306524174

	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	120
	Should Be True	${result}	msg=Services should be updated after the ingestion of the queue file
	Stop Engine
	Kindly Stop Broker

