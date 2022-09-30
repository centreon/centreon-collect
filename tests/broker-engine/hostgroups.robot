*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes

Documentation	Centreon Broker and Engine add Hostgroup
Library	DatabaseLibrary
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
	[Tags]	Broker	Engine	hostgroup
	Config Engine	${3}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${3}

	Broker Config Log	central	sql	info
        Broker Config Output Set	central	central-broker-master-sql	connections_count	5
        Broker Config Output Set	central	central-broker-master-perfdata	connections_count	5
	${start}=	Get Current Date
	Start Broker
	Start Engine
	Add Host Group	${0}	${1}	["host_1", "host_2", "host_3"]

        Sleep	3s
	Reload Broker
	Reload Engine

	${content}=	Create List	enabling membership of host 3 to host group 1 on instance 1	enabling membership of host 2 to host group 1 on instance 1	enabling membership of host 1 to host group 1 on instance 1

	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	45
	Should Be True	${result}	msg=One of the new host groups not found in logs.
	Stop Engine
	Stop Broker

EBNHG4
	[Documentation]	New host group with several pollers and connections to DB with broker and rename this hostgroup
	[Tags]	Broker	Engine	hostgroup
	Config Engine	${3}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${3}

	Broker Config Log	central	sql	info
	Broker Config Output Set	central	central-broker-master-sql	connections_count	5
	Broker Config Output Set	central	central-broker-master-perfdata	connections_count	5
	${start}=	Get Current Date
	Start Broker
	Start Engine
	Sleep	3s
	Add Host Group	${0}	${1}	["host_1", "host_2", "host_3"]

	Reload Broker
	Reload Engine

	${content}=	Create List	enabling membership of host 3 to host group 1 on instance 1	enabling membership of host 2 to host group 1

	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	45
	Should Be True	${result}	msg=One of the new host groups not found in logs.

	Rename Host Group	${0}	${1}	test	["host_1", "host_2", "host_3"]

	Sleep	10s
	${start}=	Get Current Date
        Log to Console	Step-1
	Reload Broker
        Log to Console	Step0
	Reload Engine

        Log to Console	Step1
	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
        Log to Console	Step1
	FOR	${index}	IN RANGE	60
	 Log To Console	SELECT name FROM hostgroups WHERE hostgroup_id = ${1}
	 ${output}=	Query	SELECT name FROM hostgroups WHERE hostgroup_id = ${1}
	 Log To Console	${output}
	 Sleep	1s
	 EXIT FOR LOOP IF	"${output}" == "(('hostgroup_test',),)"
	END
	Should Be Equal As Strings	${output}	(('hostgroup_test',),)

	Stop Engine
	Kindly Stop Broker


EBNHG5
	[Documentation]	New host group with several pollers and connections to DB with broker and delete this hostgroup
	[Tags]	Broker	Engine	hostgroup
	Config Engine	${3}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${3}

	Broker Config Log	central	sql	info
	Broker Config Output Set	central	central-broker-master-sql	connections_count	5
	Broker Config Output Set	central	central-broker-master-perfdata	connections_count	5
	${start}=	Get Current Date
	Start Broker
	Start Engine
	Sleep	3s
	Add Host Group	${0}	${1}	["host_1", "host_2", "host_3"]

	Reload Broker
	Reload Engine

	${content}=	Create List	enabling membership of host 3 to host group 1 on instance 1	enabling membership of host 2 to host group 1

	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	45
	Should Be True	${result}	msg=One of the new host groups not found in logs.

	Sleep	10s
	${start}=	Get Current Date

	Config Engine	${3}
	Reload Broker
	Reload Engine

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	Sleep	5s
	FOR	${index}	IN RANGE	10
	 Log To Console	SELECT count(*) FROM hosts_hostgroups 
	 ${output}=	Query	SELECT count(*) FROM hosts_hostgroups 
	 Log To Console	${output}
	 Sleep	1s
	 EXIT FOR LOOP IF	"${output}" == "((0,),)"
	END
	Should Be Equal As Strings	${output}	((0,),)

	FOR	${index}	IN RANGE	60
	 Log To Console	SELECT count(*) FROM hostgroups 
	 ${output}=	Query	SELECT count(*) FROM hostgroups 
	 Log To Console	${output}
	 Sleep	1s
	 EXIT FOR LOOP IF	"${output}" == "((0,),)"
	END
	Should Be Equal As Strings	${output}	((0,),)

	Stop Engine
	Kindly Stop Broker
