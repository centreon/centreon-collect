*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes

Documentation	Creation of 4 pollers and then deletion of Poller3.
Library	DatabaseLibrary
Library	Process
Library	OperatingSystem
Library	DateTime
Library	Collections
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py

*** Test Cases ***
EBDP1
	[Documentation]	Four new pollers are started and then we remove Poller3.
	[Tags]	Broker	Engine	grpc
	Config Engine	${4}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${4}
	Broker Config Add Item	module0	bbdo_version	3.0.1
	Broker Config Add Item	module1	bbdo_version	3.0.1
	Broker Config Add Item	module2	bbdo_version	3.0.1
	Broker Config Add Item	module3	bbdo_version	3.0.1
	Broker Config Add Item	central	bbdo_version	3.0.1
	Broker Config Add Item	rrd	bbdo_version	3.0.1
	Broker Config Log	central	sql	trace
	Config Broker Sql Output	central	unified_sql
	${start}=	Get Current Date
	Start Broker
	Start Engine

	# Let's wait for the initial service states.
        ${content}=	Create List	INITIAL SERVICE STATE: host_50;service_1000;
        ${result}=	Find In Log with Timeout	${logEngine3}	${start}	${content}	60
        Should Be True	${result}	msg=An Initial service state on service (50, 1000) should be raised before we can start external commands.

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	FOR    ${index}    IN RANGE    60
	 ${output}=	Query	SELECT instance_id FROM instances WHERE name='Poller3'
	 Sleep	1s
	 EXIT FOR LOOP IF	"${output}" != "()"
	END
	Should Be Equal As Strings	${output}	((4,),)

	Stop Engine
	Kindly Stop Broker
	# Poller3 is removed from the engine configuration but still there in centreon_storage DB
	Config Engine	${3}	${50}	${20}
	${start}=	Get Current Date
	Start Broker
	Start Engine

	Remove Poller	51001	Poller3	
	Sleep	6s

	Stop Engine
	Kindly Stop Broker

	FOR    ${index}    IN RANGE    60
	 ${output}=	Query	SELECT instance_id FROM instances WHERE name='Poller3'
	 Sleep	1s
	 EXIT FOR LOOP IF	"${output}" == "()"
	END
	Should Be Equal As Strings	${output}	()

EBDP2
	[Documentation]	Three new pollers are started, then they are killed. After a simple restart of broker, it is still possible to remove Poller2 if removed from the configuration.
	[Tags]	Broker	Engine	grpc
	Config Engine	${3}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${3}
	Broker Config Add Item	module0	bbdo_version	3.0.1
	Broker Config Add Item	module1	bbdo_version	3.0.1
	Broker Config Add Item	module2	bbdo_version	3.0.1
	Broker Config Add Item	central	bbdo_version	3.0.1
	Broker Config Add Item	rrd	bbdo_version	3.0.1
	Broker Config Log	central	sql	trace
	Config Broker Sql Output	central	unified_sql
	${start}=	Get Current Date
	Start Broker
	Start Engine

	# Let's wait for the initial service states.
        ${content}=	Create List	INITIAL SERVICE STATE: host_50;service_1000;
        ${result}=	Find In Log with Timeout	${logEngine2}	${start}	${content}	60
        Should Be True	${result}	msg=An Initial service state on service (50, 1000) should be raised before we can start external commands.

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	FOR    ${index}    IN RANGE    60
	 ${output}=	Query	SELECT instance_id FROM instances WHERE name='Poller2'
	 Sleep	1s
	 log to console	Output= ${output}
	 EXIT FOR LOOP IF	"${output}" != "()"
	END
	Should Be Equal As Strings	${output}	((3,),)

	# Let's brutally kill the poller
	Send Signal To Process	SIGKILL	e0
	Send Signal To Process	SIGKILL	e1
	Send Signal To Process	SIGKILL	e2
	Terminate Process	e0
	Terminate Process	e1
	Terminate Process	e2

	log to console	Reconfiguration of 2 pollers
	# Poller2 is removed from the engine configuration but still there in centreon_storage DB
	Config Engine	${2}	${50}	${20}
	${start}=	Get Current Date
	Kindly Stop Broker
	Clear Engine Logs
	Start Engine
	Start Broker

	# Let's wait for the initial service states.
        ${content}=	Create List	INITIAL SERVICE STATE: host_50;service_1000;
        ${result}=	Find In Log with Timeout	${logEngine1}	${start}	${content}	60
        Should Be True	${result}	msg=An Initial service state on service (50, 1000) should be raised before we can start external commands.

	Remove Poller	51001	Poller2

	Stop Engine
	Kindly Stop Broker

	FOR    ${index}    IN RANGE    60
	 ${output}=	Query	SELECT instance_id FROM instances WHERE name='Poller2'
	 Sleep	1s
	 log to console	Output= ${output}
	 EXIT FOR LOOP IF	"${output}" == "()"
	END
	Should Be Equal As Strings	${output}	()

EBDP3
	[Documentation]	Three new pollers are started, then they are killed. It is still possible to remove Poller2 if removed from the configuration.
	[Tags]	Broker	Engine	grpc
	Config Engine	${3}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${3}
	Broker Config Add Item	module0	bbdo_version	3.0.1
	Broker Config Add Item	module1	bbdo_version	3.0.1
	Broker Config Add Item	module2	bbdo_version	3.0.1
	Broker Config Add Item	central	bbdo_version	3.0.1
	Broker Config Add Item	rrd	bbdo_version	3.0.1
	Broker Config Log	central	sql	trace
	Config Broker Sql Output	central	unified_sql
	${start}=	Get Current Date
	Start Broker
	Start Engine

	# Let's wait for the initial service states.
        ${content}=	Create List	INITIAL SERVICE STATE: host_50;service_1000;
        ${result}=	Find In Log with Timeout	${logEngine2}	${start}	${content}	60
        Should Be True	${result}	msg=An Initial service state on service (50, 1000) should be raised before we can start external commands.

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	FOR    ${index}    IN RANGE    60
	 ${output}=	Query	SELECT instance_id FROM instances WHERE name='Poller2'
	 Sleep	1s
	 log to console	Output= ${output}
	 EXIT FOR LOOP IF	"${output}" != "()"
	END
	Should Be Equal As Strings	${output}	((3,),)

	# Let's brutally kill the poller
	Send Signal To Process	SIGKILL	e0
	Send Signal To Process	SIGKILL	e1
	Send Signal To Process	SIGKILL	e2
	Terminate Process	e0
	Terminate Process	e1
	Terminate Process	e2

	log to console	Reconfiguration of 2 pollers
	# Poller2 is removed from the engine configuration but still there in centreon_storage DB
	Config Engine	${2}	${50}	${20}
	${start}=	Get Current Date
	Clear Engine Logs
	Start Engine

	# Let's wait for the initial service states.
        ${content}=	Create List	INITIAL SERVICE STATE: host_50;service_1000;
        ${result}=	Find In Log with Timeout	${logEngine1}	${start}	${content}	60
        Should Be True	${result}	msg=An Initial service state on service (50, 1000) should be raised before we can start external commands.

	Remove Poller	51001	Poller2

	Stop Engine
	Kindly Stop Broker

	FOR    ${index}    IN RANGE    60
	 ${output}=	Query	SELECT instance_id FROM instances WHERE name='Poller2'
	 Sleep	1s
	 log to console	Output= ${output}
	 EXIT FOR LOOP IF	"${output}" == "()"
	END
	Should Be Equal As Strings	${output}	()
