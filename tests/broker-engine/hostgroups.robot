*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Ctn Clean Before Suite
Suite Teardown	Ctn Clean After Suite
Test Setup	Ctn Stop Processes
Test Teardown	Ctn Save Logs If Failed

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
	Ctn Config Engine	${3}
	Ctn Config Broker	rrd
	Ctn Config Broker	central
	Ctn Config Broker	module	${3}

	Ctn Broker Config Log	central	sql	info
        Ctn Broker Config Output Set	central	central-broker-master-sql	connections_count	5
        Ctn Broker Config Output Set	central	central-broker-master-perfdata	connections_count	5
	${start}=	Get Current Date
	Ctn Start Broker
	Ctn Start Engine
	Ctn Add Host Group	${0}	${1}	["host_1", "host_2", "host_3"]

        Sleep	3s
	Ctn Reload Broker
	Ctn Reload Engine

	${content}=	Create List	enabling membership of host 3 to host group 1 on instance 1	enabling membership of host 2 to host group 1 on instance 1	enabling membership of host 1 to host group 1 on instance 1

	${result}=	Ctn Find In Log With Timeout	${centralLog}	${start}	${content}	45
	Should Be True	${result}	msg=One of the new host groups not found in logs.
	Ctn Stop Engine
	Ctn Kindly Stop Broker

EBNHGU1
	[Documentation]	New host group with several pollers and connections to DB with broker configured with unified_sql
	[Tags]	Broker	Engine	hostgroup	unified_sql
	Ctn Config Engine	${3}
	Ctn Config Broker	rrd
	Ctn Config Broker	central
	Ctn Config Broker	module	${3}

	Ctn Broker Config Log	central	sql	info
	Ctn Config Broker Sql Output	central	unified_sql
        Ctn Broker Config Output Set	central	central-broker-unified-sql	connections_count	5
	${start}=	Get Current Date
	Ctn Start Broker
	Ctn Start Engine
	Ctn Add Host Group	${0}	${1}	["host_1", "host_2", "host_3"]

        Sleep	3s
	Ctn Reload Broker
	Ctn Reload Engine

	${content}=	Create List	enabling membership of host 3 to host group 1 on instance 1	enabling membership of host 2 to host group 1 on instance 1	enabling membership of host 1 to host group 1 on instance 1

	${result}=	Ctn Find In Log With Timeout	${centralLog}	${start}	${content}	45
	Should Be True	${result}	msg=One of the new host groups not found in logs.
	Ctn Stop Engine
	Ctn Kindly Stop Broker

EBNHGU2
	[Documentation]	New host group with several pollers and connections to DB with broker configured with unified_sql
	[Tags]	Broker	Engine	hostgroup	unified_sql
	Ctn Config Engine	${3}
	Ctn Config Broker	rrd
	Ctn Config Broker	central
	Ctn Config Broker	module	${3}

	Ctn Broker Config Log	central	sql	info
	Ctn Config Broker Sql Output	central	unified_sql
	Ctn Broker Config Output Set	central	central-broker-unified-sql	connections_count	5
	Ctn Broker Config Add Item	module0	bbdo_version	3.0.0
	Ctn Broker Config Add Item	module1	bbdo_version	3.0.0
	Ctn Broker Config Add Item	module2	bbdo_version	3.0.0
	Ctn Broker Config Add Item	central	bbdo_version	3.0.0
	Ctn Broker Config Add Item	rrd	bbdo_version	3.0.0
	${start}=	Get Current Date
	Ctn Start Broker
	Ctn Start Engine
	Ctn Add Host Group	${0}	${1}	["host_1", "host_2", "host_3"]

        Sleep	3s
	Ctn Reload Broker
	Ctn Reload Engine

	${content}=	Create List	enabling membership of host 3 to host group 1 on instance 1	enabling membership of host 2 to host group 1 on instance 1

	${result}=	Ctn Find In Log With Timeout	${centralLog}	${start}	${content}	45
	Should Be True	${result}	msg=One of the new host groups not found in logs.
        Ctn Stop Engine
        Ctn Kindly Stop Broker

EBNHGU3
       [Documentation]	New host group with several pollers and connections to DB with broker configured with unified_sql
       [Tags]	Broker	Engine	hostgroup	unified_sql
       Ctn Config Engine	${4}
       Ctn Config Broker	rrd
       Ctn Config Broker	central
       Ctn Config Broker	module	${4}

	Ctn Broker Config Log	central	sql	info
	Ctn Config Broker Sql Output	central	unified_sql
	Ctn Broker Config Output Set	central	central-broker-unified-sql	connections_count	5
	Ctn Broker Config Add Item	module0	bbdo_version	3.0.0
	Ctn Broker Config Add Item	module1	bbdo_version	3.0.0
	Ctn Broker Config Add Item	module2	bbdo_version	3.0.0
	Ctn Broker Config Add Item	module3	bbdo_version	3.0.0
	Ctn Broker Config Add Item	central	bbdo_version	3.0.0
	Ctn Broker Config Add Item	rrd	bbdo_version	3.0.0
	Ctn Broker Config Log	central	sql	debug


	${start}=	Get Current Date
	Ctn Start Broker
	Ctn Start Engine
	Ctn Add Host Group	${0}	${1}	["host_1", "host_2", "host_3"]
	Ctn Add Host Group	${1}	${1}	["host_21", "host_22", "host_23"]
	Ctn Add Host Group	${2}	${1}	["host_31", "host_32", "host_33"]
	Ctn Add Host Group	${3}	${1}	["host_41", "host_42", "host_43"]

	Sleep	3s
	Ctn Reload Broker
	Ctn Reload Engine

        ${result}=	Ctn Check Number Of Relations Between Hostgroup And Hosts	1	12	30
	Should Be True	${result}	msg=We should have 12 hosts members of host 1.

	Ctn Config Engine Remove Cfg File	${0}	hostgroups.cfg

	Sleep	3s
	Ctn Reload Broker
	Ctn Reload Engine
        ${result}=	Ctn Check Number Of Relations Between Hostgroup And Hosts	1	9	30
	Should Be True	${result}	msg=We should have 12 hosts members of host 1.

	Ctn Stop Engine
	Ctn Kindly Stop Broker

EBNHG4
	[Documentation]	New host group with several pollers and connections to DB with broker and rename this hostgroup
	[Tags]	Broker	Engine	hostgroup
	Ctn Config Engine	${3}
	Ctn Config Broker	rrd
	Ctn Config Broker	central
	Ctn Config Broker	module	${3}

	Ctn Broker Config Log	central	sql	info
	Ctn Broker Config Output Set	central	central-broker-master-sql	connections_count	5
	Ctn Broker Config Output Set	central	central-broker-master-perfdata	connections_count	5
	${start}=	Get Current Date
	Ctn Start Broker
	Ctn Start Engine
	Sleep	3s
	Ctn Add Host Group	${0}	${1}	["host_1", "host_2", "host_3"]

	Ctn Reload Broker
	Ctn Reload Engine

	${content}=	Create List	enabling membership of host 3 to host group 1 on instance 1	enabling membership of host 2 to host group 1

	${result}=	Ctn Find In Log With Timeout	${centralLog}	${start}	${content}	45
	Should Be True	${result}	msg=One of the new host groups not found in logs.

	Ctn Rename Host Group	${0}	${1}	test	["host_1", "host_2", "host_3"]

	Sleep	10s
	${start}=	Get Current Date
        Log to Console	Step-1
	Ctn Reload Broker
        Log to Console	Step0
	Ctn Reload Engine

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

	Ctn Stop Engine
	Ctn Kindly Stop Broker

EBNHGU4
	[Documentation]	New host group with several pollers and connections to DB with broker and rename this hostgroup
	[Tags]	Broker	Engine	hostgroup
	Ctn Config Engine	${3}
	Ctn Config Broker	rrd
	Ctn Config Broker	central
	Ctn Config Broker	module	${3}

	Ctn Broker Config Log	central	sql	info
	Ctn Config Broker Sql Output	central	unified_sql
	Ctn Broker Config Output Set	central	central-broker-unified-sql	connections_count	5
	${start}=	Get Current Date
	Ctn Start Broker
	Ctn Start Engine
	Sleep	3s
	Ctn Add Host Group	${0}	${1}	["host_1", "host_2", "host_3"]

	Ctn Reload Broker
	Ctn Reload Engine

	${content}=	Create List	enabling membership of host 3 to host group 1 on instance 1	enabling membership of host 2 to host group 1

	${result}=	Ctn Find In Log With Timeout	${centralLog}	${start}	${content}	45
	Should Be True	${result}	msg=One of the new host groups not found in logs.

	Ctn Rename Host Group	${0}	${1}	test	["host_1", "host_2", "host_3"]

	Sleep	10s
	${start}=	Get Current Date
        Log to Console	Step-1
	Ctn Reload Broker
        Log to Console	Step0
	Ctn Reload Engine

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

	Ctn Stop Engine
	Ctn Kindly Stop Broker
