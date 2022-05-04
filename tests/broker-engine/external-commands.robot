*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes

Documentation	Centreon Broker and Engine progressively add services
Library	Process
Library	OperatingSystem
Library	DateTime
Library	Collections
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py

*** Test Cases ***
BEEXTCMD1
	[Documentation]	Passive check on services
	[Tags]	Broker	Engine	services	protobuf
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
        Broker Config Add Item	module0	bbdo_version	3.0.0
        Broker Config Add Item	central	bbdo_version	3.0.0
        Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	sql	debug
	Config Broker Sql Output	central	unified_sql
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
        FOR	${i}	IN RANGE	10
         Sleep	1s
         Process Service Check Result	host_1	service_1	0	output|extcmd=12
        END
	Stop Engine
	Kindly Stop Broker
