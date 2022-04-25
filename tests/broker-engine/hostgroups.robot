*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes

Documentation	Centreon Broker and Engine add Hostgroup
Library	Process
Library	OperatingSystem
Library	DateTime
Library	Collections
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py

*** Test Cases ***
EBNHG1
	[Documentation]	New host group with several pollers and connections to DB
	[Tags]	Broker	Engine	New host group
	Config Engine	${3}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module

	Broker Config Log	central	sql	info
        Broker Config Output Set	central	central-broker-master-sql	connections_count	5
        Broker Config Output Set	central	central-broker-master-perfdata	connections_count	5
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected
	Add Host Group	${0}	${1}	["host_1", "host_2", "host_3"]

	Reload Broker
	Reload Engine

	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected

	${content}=	Create List	enabling membership of host 3 to host group 1 on instance 1	enabling membership of host 2 to host group 1 on instance 1	enabling membership of host 1 to host group 1 on instance 1

	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${result}=	Find In Log With Timeout	${log}	${start}	${content}	45
	Should Be True	${result}	msg=One of the new host groups not found in logs.
	Stop Engine
	Stop Broker

EBNHGU1
	[Documentation]	New host group with several pollers and connections to DB with broker configured with unified_sql
	[Tags]	Broker	Engine	New host group	unified_sql
	Config Engine	${3}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module

	Broker Config Log	central	sql	info
	Config Broker Sql Output	central	unified_sql
        Broker Config Output Set	central	central-broker-unified-sql	connections_count	5
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected
	Add Host Group	${0}	${1}	["host_1", "host_2", "host_3"]

	Reload Broker
	Reload Engine

	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected

	${content}=	Create List	enabling membership of host 3 to host group 1 on instance 1	enabling membership of host 2 to host group 1 on instance 1	enabling membership of host 1 to host group 1 on instance 1

	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${result}=	Find In Log With Timeout	${log}	${start}	${content}	45
	Should Be True	${result}	msg=One of the new host groups not found in logs.
	Stop Engine
	Stop Broker

