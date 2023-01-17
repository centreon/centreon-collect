*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes

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
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Add Item	module0	bbdo_version	3.0.0
	Broker Config Add Item	central	bbdo_version	3.0.0
	Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	sql	debug
	${nu}=	Evaluate	2000*"X"
	Engine Config set value in services	0	service_1	notes_url	${nu}
	Clear Retention
	Start Broker
	Start Engine

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	FOR    ${index}    IN RANGE    60
	 ${output}=	Query	SELECT notes_url FROM services WHERE description='service_1'
	 Sleep	1s
	 EXIT FOR LOOP IF	"${output}" == "(('${nu}',),)"
	END
	Should Be Equal As Strings	${output}	(('${nu}',),)
	Stop Engine
	Kindly Stop Broker

EBSAU2
	[Documentation]	New services with action_url with more than 2000 characters
	[Tags]	Broker	Engine	services	protobuf
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Add Item	module0	bbdo_version	3.0.0
	Broker Config Add Item	central	bbdo_version	3.0.0
	Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	sql	debug
	${au}=	Evaluate	2000*"Y"
	Engine Config set value in services	0	service_2	action_url	${au}
	Clear Retention
	Start Broker
	Start Engine

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	FOR    ${index}    IN RANGE    60
	 ${output}=	Query	SELECT action_url FROM services WHERE description='service_2'
	 Sleep	1s
	 EXIT FOR LOOP IF	"${output}" == "(('${au}',),)"
	END
	Should Be Equal As Strings	${output}	(('${au}',),)
	Stop Engine
	Kindly Stop Broker

EBSN3
	[Documentation]	New services with notes with more than 500 characters
	[Tags]	Broker	Engine	services	protobuf
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Add Item	module0	bbdo_version	3.0.0
	Broker Config Add Item	central	bbdo_version	3.0.0
	Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	sql	debug
	${n}=	Evaluate	500*"Z"
	Engine Config set value in services	0	service_3	notes	${n}
	Clear Retention
	Start Broker
	Start Engine

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	FOR    ${index}    IN RANGE    60
	 ${output}=	Query	SELECT notes FROM services WHERE description='service_3'
	 Sleep	1s
	 EXIT FOR LOOP IF	"${output}" == "(('${n}',),)"
	END
	Should Be Equal As Strings	${output}	(('${n}',),)
	Stop Engine
	Kindly Stop Broker
