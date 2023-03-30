*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes
Test Teardown	Save logs If Failed

Documentation	Centreon Broker and Engine progressively add services
Library	Process
Library	OperatingSystem
Library	DateTime
Library	Collections
Library	DatabaseLibrary
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py

*** Test Cases ***
EBBPS1
	[Documentation]	1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table resources.
	[Tags]	Broker	Engine	services	unified_sql
	Config Engine	${1}	${1}	${1000}
	# We want all the services to be passive to avoid parasite checks during our test.
	Set Services passive	${0}	service_.*
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Add Item	module0	bbdo_version	3.0.1
	Broker Config Add Item	central	bbdo_version	3.0.1
	Broker Config Add Item	rrd	bbdo_version	3.0.1
	Broker Config Log	central	core	error
	Broker Config Log	central	tcp	error
	Broker Config Log	central	sql	trace
	Config Broker Sql Output	central	unified_sql
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1000;
	${result}=	Find In Log with Timeout	${engineLog0}	${start}	${content}	30
	Should Be True	${result}	msg=An Initial service state on host_1:service_1000 should be raised before we can start external commands.
	FOR	${i}	IN RANGE	${1000}
	  Process Service Check result	host_1	service_${i+1}	1	warning${i}
	END
	${content}=	Create List	connected to 'MariaDB' Server	Unified sql stream supports column-wise binding in prepared statements
	${result}=	Find In Log with timeout	${centralLog}	${start}	${content}	30
	Should Be True	${result}	msg=Prepared statements should be supported with this version of MariaDB.

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	${date}=	Get Current Date  result_format=epoch
	Log To Console    date=${date}
	FOR	${index}	IN RANGE	60
	  ${output}=	Query	SELECT count(*) FROM resources WHERE name like 'service\_%' and parent_name='host_1' and status <> 1
	  Log To Console	${output}
	  Sleep	1s
	  EXIT FOR LOOP IF	"${output}" == "((0,),)"
	END
	Should Be Equal As Strings	${output}	((0,),)

	FOR	${i}	IN RANGE	${1000}
	  Process Service Check result	host_1	service_${i+1}	2	warning${i}
	  IF	${i} % 200 == 0
	    Log to Console	Stopping Broker
	    Kindly Stop Broker
	    Log to Console	Waiting for 5s
	    Sleep	5s
	    Log to Console	Restarting Broker
	    Start Broker
	  END
	END
	${content}=	Create List	connected to 'MariaDB' Server	Unified sql stream supports column-wise binding in prepared statements
	${result}=	Find In Log with timeout	${centralLog}	${start}	${content}	30
	Should Be True	${result}	msg=Prepared statements should be supported with this version of MariaDB.

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	${date}=	Get Current Date  result_format=epoch
	Log To Console    date=${date}
	FOR	${index}	IN RANGE	60
	  ${output}=	Query	SELECT count(*) FROM resources WHERE name like 'service\_%' and parent_name='host_1' and status <> 2
	  Log To Console	${output}
	  Sleep	1s
	  EXIT FOR LOOP IF	"${output}" == "((0,),)"
	END
	Should Be Equal As Strings	${output}	((0,),)
	Stop Engine
	Kindly Stop Broker

EBBPS2
	[Documentation]	1000 service check results are sent to the poller. The test is done with the unified_sql stream, no service status is lost, we find the 1000 results in the database: table services.
	[Tags]	Broker	Engine	services	unified_sql
	Config Engine	${1}	${1}	${1000}
	# We want all the services to be passive to avoid parasite checks during our test.
	Set Services passive	${0}	service_.*
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Add Item	module0	bbdo_version	3.0.1
	Broker Config Add Item	central	bbdo_version	3.0.1
	Broker Config Add Item	rrd	bbdo_version	3.0.1
	Broker Config Log	central	core	error
	Broker Config Log	central	tcp	error
	Broker Config Log	central	sql	trace
	Config Broker Sql Output	central	unified_sql
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1000;
	${result}=	Find In Log with Timeout	${engineLog0}	${start}	${content}	30
	Should Be True	${result}	msg=An Initial service state on host_1:service_1000 should be raised before we can start external commands.
	FOR	${i}	IN RANGE	${1000}
	  Process Service Check result	host_1	service_${i+1}	1	warning${i}
	END
	${content}=	Create List	connected to 'MariaDB' Server	Unified sql stream supports column-wise binding in prepared statements
	${result}=	Find In Log with timeout	${centralLog}	${start}	${content}	30
	Should Be True	${result}	msg=Prepared statements should be supported with this version of MariaDB.

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	${date}=	Get Current Date  result_format=epoch
	Log To Console    date=${date}
	FOR	${index}	IN RANGE	60
	  ${output}=	Query	SELECT count(*) FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description LIKE 'service\_%' AND s.state <> 1
	  Log To Console	${output}
	  Sleep	1s
	  EXIT FOR LOOP IF	"${output}" == "((0,),)"
	END
	Should Be Equal As Strings	${output}	((0,),)

	FOR	${i}	IN RANGE	${1000}
	  Process Service Check result	host_1	service_${i+1}	2	warning${i}
	  IF	${i} % 200 == 0
	    Log to Console	Stopping Broker
	    Kindly Stop Broker
	    Log to Console	Waiting for 5s
	    Sleep	5s
	    Log to Console	Restarting Broker
	    Start Broker
	  END
	END
	${content}=	Create List	connected to 'MariaDB' Server	Unified sql stream supports column-wise binding in prepared statements
	${result}=	Find In Log with timeout	${centralLog}	${start}	${content}	30
	Should Be True	${result}	msg=Prepared statements should be supported with this version of MariaDB.

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	${date}=	Get Current Date  result_format=epoch
	Log To Console    date=${date}
	FOR	${index}	IN RANGE	60
	  ${output}=	Query	SELECT count(*) FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_1' AND s.description LIKE 'service\_%' AND s.state <> 2
	  Log To Console	${output}
	  Sleep	1s
	  EXIT FOR LOOP IF	"${output}" == "((0,),)"
	END
	Should Be Equal As Strings	${output}	((0,),)
	Stop Engine
	Kindly Stop Broker
