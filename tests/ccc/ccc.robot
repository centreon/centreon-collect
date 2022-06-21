*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes

Documentation	ccc tests with engine and broker
Library	Process
Library	DateTime
Library	OperatingSystem
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py

*** Test Cases ***
BECCC1
	[Documentation]	ccc without port fails with an error message
	[Tags]	Broker	Engine	ccc
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
	Clear Retention
	${start}=	Get Current Date
        Sleep	1s
	Start Broker
	Start Engine
	Sleep	3s
	Start Process	/usr/bin/ccc	stderr=/tmp/output.txt
	FOR	${i}	IN RANGE	10
	 Wait Until Created	/tmp/output.txt
	 ${content}=	Get File	/tmp/output.txt
	 EXIT FOR LOOP IF	len("${content.strip()}") > 0
	 Sleep	1s
	END
	should be equal as strings	${content.strip()}	You must specify a port for the connection to the gRPC server
	Stop Engine
	Kindly Stop Broker
	Remove File	/tmp/output.txt

BECCC2
	[Documentation]	ccc with -p 51001 connects to central cbd gRPC server.
	[Tags]	Broker	Engine	protobuf	bbdo
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
        Broker Config Add Item	module0	bbdo_version	3.0.0
        Broker Config Add Item	central	bbdo_version	3.0.0
        Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	sql	trace
	Config Broker Sql Output	central	unified_sql
        Broker Config Output Set	central	central-broker-unified-sql	store_in_resources	yes
        Broker Config Output Set	central	central-broker-unified-sql	store_in_hosts_services	no
	Clear Retention
	${start}=	Get Current Date
        Sleep	1s
	Start Broker
	Start Engine
	Sleep	3s
	Start Process	/usr/bin/ccc	-p 51001	stdout=/tmp/output.txt
	FOR	${i}	IN RANGE	10
	 Wait Until Created	/tmp/output.txt
	 ${content}=	Get File	/tmp/output.txt
	 EXIT FOR LOOP IF	len("${content.strip()}") > 0
	 Sleep	1s
	END
	Should Be Equal As Strings	${content.strip()}	Connected to a Broker 22.10.0 gRPC server
	Stop Engine
	Kindly Stop Broker
	Remove File	/tmp/output.txt

BECCC3
	[Documentation]	ccc with -p 50001 connects to centengine gRPC server.
	[Tags]	Broker	Engine	protobuf	bbdo
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
        Broker Config Add Item	module0	bbdo_version	3.0.0
        Broker Config Add Item	central	bbdo_version	3.0.0
        Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	sql	trace
	Config Broker Sql Output	central	unified_sql
        Broker Config Output Set	central	central-broker-unified-sql	store_in_resources	yes
        Broker Config Output Set	central	central-broker-unified-sql	store_in_hosts_services	no
	Clear Retention
	${start}=	Get Current Date
        Sleep	1s
	Start Broker
	Start Engine
	Sleep	3s
	Start Process	/usr/bin/ccc	-p 50001	stdout=/tmp/output.txt
	FOR	${i}	IN RANGE	10
	 Wait Until Created	/tmp/output.txt
	 ${content}=	Get File	/tmp/output.txt
	 EXIT FOR LOOP IF	len("${content.strip()}") > 0
	 Sleep	1s
	END
	Should Be Equal As Strings	${content.strip()}	Connected to an Engine 22.10.0 gRPC server
	Stop Engine
	Kindly Stop Broker
	Remove File	/tmp/output.txt
