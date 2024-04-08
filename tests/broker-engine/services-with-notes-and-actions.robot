*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Ctn Clean Before Suite
Suite Teardown	Ctn Clean After Suite
Test Setup	Ctn Stop Processes
Test Teardown	Ctn Save Logs If Failed

Documentation	Centreon Broker and Engine progressively add services
Library	DatabaseLibrary
Library	Process
Library	OperatingSystem
Library	DateTime
Library	Collections
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py

*** Test Cases ***
EBSNU1
	[Documentation]	New services with notes_url with more than 2000 characters
	[Tags]	Broker	Engine	services	protobuf
	Ctn Config Engine	${1}	${50}	${20}
	Ctn Config Broker	rrd
	Ctn Config Broker	central
	Ctn Config Broker	module	${1}
	Ctn Broker Config Add Item	module0	bbdo_version	3.0.1
	Ctn Broker Config Add Item	central	bbdo_version	3.0.1
	Ctn Broker Config Add Item	rrd	bbdo_version	3.0.1
	Ctn Broker Config Log	central	sql	debug
	Ctn Broker Config Log	central	bbdo	debug
	Ctn Config Broker Sql Output	central	unified_sql
	${nu}=	Evaluate	2000*"X"
	Ctn Engine Config Set Value In Services	0	service_1	notes_url	${nu}
	Ctn Clear Retention
	Ctn Start Broker
	Ctn Start Engine

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	FOR    ${index}    IN RANGE    60
	 ${output}=	Query	SELECT notes_url FROM services WHERE description='service_1'
	 Sleep	1s
	 EXIT FOR LOOP IF	"${output}" == "(('${nu}',),)"
	END
	Should Be Equal As Strings	${output}	(('${nu}',),)
	FOR    ${index}    IN RANGE    60
	 ${output}=	Query	SELECT notes_url FROM resources WHERE name='service_1'
	 Sleep	1s
	 EXIT FOR LOOP IF	"${output}" == "(('${nu}',),)"
	END
	Should Be Equal As Strings	${output}	(('${nu}',),)
	Ctn Stop Engine
	Ctn Kindly Stop Broker

EBSAU2
	[Documentation]	New services with action_url with more than 2000 characters
	[Tags]	Broker	Engine	services	protobuf
	Ctn Config Engine	${1}	${50}	${20}
	Ctn Config Broker	rrd
	Ctn Config Broker	central
	Ctn Config Broker	module	${1}
	Ctn Broker Config Add Item	module0	bbdo_version	3.0.1
	Ctn Broker Config Add Item	central	bbdo_version	3.0.1
	Ctn Broker Config Add Item	rrd	bbdo_version	3.0.1
	Ctn Broker Config Log	central	sql	debug
	Ctn Broker Config Log	central	bbdo	debug
	Ctn Config Broker Sql Output	central	unified_sql
	${au}=	Evaluate	2000*"Y"
	Ctn Engine Config Set Value In Services	0	service_2	action_url	${au}
	Ctn Clear Retention
	Ctn Start Broker
	Ctn Start Engine

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	FOR    ${index}    IN RANGE    60
	 ${output}=	Query	SELECT action_url FROM services WHERE description='service_2'
	 Sleep	1s
	 EXIT FOR LOOP IF	"${output}" == "(('${au}',),)"
	END
	Should Be Equal As Strings	${output}	(('${au}',),)
	FOR    ${index}    IN RANGE    60
	 ${output}=	Query	SELECT action_url FROM resources WHERE name='service_2'
	 Sleep	1s
	 EXIT FOR LOOP IF	"${output}" == "(('${au}',),)"
	END
	Should Be Equal As Strings	${output}	(('${au}',),)
	Ctn Stop Engine
	Ctn Kindly Stop Broker

EBSN3
	[Documentation]	New services with notes with more than 500 characters
	[Tags]	Broker	Engine	services	protobuf
	Ctn Config Engine	${1}	${50}	${20}
	Ctn Config Broker	rrd
	Ctn Config Broker	central
	Ctn Config Broker	module	${1}
	Ctn Broker Config Add Item	module0	bbdo_version	3.0.1
	Ctn Broker Config Add Item	central	bbdo_version	3.0.1
	Ctn Broker Config Add Item	rrd	bbdo_version	3.0.1
	Ctn Broker Config Log	central	sql	debug
	Ctn Broker Config Log	central	bbdo	debug
	Ctn Config Broker Sql Output	central	unified_sql
	${n}=	Evaluate	500*"Z"
	Ctn Engine Config Set Value In Services	0	service_3	notes	${n}
	Ctn Clear Retention
	Ctn Start Broker
	Ctn Start Engine

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	FOR    ${index}    IN RANGE    60
	 ${output}=	Query	SELECT notes FROM services WHERE description='service_3'
	 Sleep	1s
	 EXIT FOR LOOP IF	"${output}" == "(('${n}',),)"
	END
	Should Be Equal As Strings	${output}	(('${n}',),)
	FOR    ${index}    IN RANGE    60
	 ${output}=	Query	SELECT notes FROM resources WHERE name='service_3'
	 Sleep	1s
	 EXIT FOR LOOP IF	"${output}" == "(('${n}',),)"
	END
	Should Be Equal As Strings	${output}	(('${n}',),)
	Ctn Stop Engine
	Ctn Kindly Stop Broker
