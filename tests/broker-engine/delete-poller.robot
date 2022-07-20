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
        ${result}=	Find In Log with Timeout	${engineLog3}	${start}	${content}	60
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
        ${result}=	Find In Log with Timeout	${engineLog2}	${start}	${content}	60
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
        ${result}=	Find In Log with Timeout	${engineLog1}	${start}	${content}	60
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
        ${result}=	Find In Log with Timeout	${engineLog2}	${start}	${content}	60
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
        ${result}=	Find In Log with Timeout	${engineLog1}	${start}	${content}	60
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

EBDP4
	[Documentation]	Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by broker.
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
	Broker Config Log	central	core	error
	Broker Config Log	central	sql	trace
	Broker Config Log	module3	neb	trace
	Broker Config Flush log	central	0
	Config Broker Sql Output	central	unified_sql
	${start}=	Get Current Date
	Start Broker
	Start Engine

	# Let's wait for the initial service states.
        ${content}=	Create List	INITIAL SERVICE STATE: host_50;service_1000;
        ${result}=	Find In Log with Timeout	${engineLog3}	${start}	${content}	60
        Should Be True	${result}	msg=An Initial service state on service (50, 1000) should be raised before we can start external commands.

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	FOR    ${index}    IN RANGE    60
	 ${output}=	Query	SELECT instance_id FROM instances WHERE name='Poller3'
	 Sleep	1s
	 EXIT FOR LOOP IF	"${output}" != "()"
	END
	Should Be Equal As Strings	${output}	((4,),)

	# Let's brutally kill the poller
	${content}=	Create List	processing poller event (id: 4, name: Poller3, running:
	${result}=	Find In log with timeout	${centralLog}	${start}	${content}	60
        Should Be True	${result}	msg=We want the poller 4 event before stopping broker
	Kindly Stop Broker
	Clear Broker Logs

	# Generation of many service status but kept in memory on poller3.
	FOR	${i}	IN RANGE	200
	 Process Service Check result	host_40	service_781	2	service_781 should fail	config3
	 Process Service Check result	host_40	service_782	1	service_782 should fail	config3
	END
	${content}=	Create List	SERVICE ALERT: host_40;service_781;CRITICAL	SERVICE ALERT: host_40;service_782;WARNING
	${result}=	Find In log with timeout	${engineLog3}	${start}	${content}	60
        Should Be True	${result}	msg=Service alerts about service 781 and 782 should be raised

	${content}=	Create List	callbacks: service (40, 781) has no perfdata	service (40, 782) has no perfdata
	${result}=	Find In log with timeout	${moduleLog3}	${start}	${content}	60
        Should Be True	${result}	msg=pb service status on services (40, 781) and (40, 782) should be generated
	Stop Engine

	# Because poller3 is going to be removed, we move its memory file to poller0, 1 and 2.
	Move File	/var/lib/centreon-engine/central-module-master3.memory.central-module-master-output	/var/lib/centreon-engine/central-module-master0.memory.central-module-master-output

	# Poller3 is removed from the engine configuration but still there in centreon_storage DB
	Config Engine	${3}	${39}	${20}

	# Restart Broker
	Start Broker
	Remove Poller	51001	Poller3
	FOR    ${index}    IN RANGE    60
	 ${output}=	Query	SELECT instance_id FROM instances WHERE name='Poller3'
	 Sleep	1s
	 EXIT FOR LOOP IF	"${output}" == "()"
	END
	Should Be Equal As Strings	${output}	()

	Start Engine
	# Let's wait for the initial service states.
        ${content}=	Create List	INITIAL SERVICE STATE: host_13;service_260;
        ${result}=	Find In Log with Timeout	${engineLog0}	${start}	${content}	60
        Should Be True	${result}	msg=An Initial service state on service (13, 260) should be raised before we can start external commands.

	${content}=	Create List	service status (40, 781) thrown away because host 40 is not known by any poller
	log to console	date ${start}
	${result}=	Find in Log With Timeout	${centralLog}	${start}	${content}	60
	Should be True	${result}	msg=No message about these two wrong service status.
	Stop Engine
	Kindly Stop Broker

EBDP5
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
        ${result}=	Find In Log with Timeout	${engineLog3}	${start}	${content}	60
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

	Remove Poller by id	51001	${4}
	Sleep	6s

	Stop Engine
	Kindly Stop Broker

	FOR    ${index}    IN RANGE    60
	 ${output}=	Query	SELECT instance_id FROM instances WHERE name='Poller3'
	 Sleep	1s
	 EXIT FOR LOOP IF	"${output}" == "()"
	END
	Should Be Equal As Strings	${output}	()

EBDP6
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
        ${result}=	Find In Log with Timeout	${engineLog2}	${start}	${content}	60
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
        ${result}=	Find In Log with Timeout	${engineLog1}	${start}	${content}	60
        Should Be True	${result}	msg=An Initial service state on service (50, 1000) should be raised before we can start external commands.

	Remove Poller by id	51001	${3}

	Stop Engine
	Kindly Stop Broker

	FOR    ${index}    IN RANGE    60
	 ${output}=	Query	SELECT instance_id FROM instances WHERE name='Poller2'
	 Sleep	1s
	 log to console	Output= ${output}
	 EXIT FOR LOOP IF	"${output}" == "()"
	END
	Should Be Equal As Strings	${output}	()

EBDP7
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
        ${result}=	Find In Log with Timeout	${engineLog2}	${start}	${content}	60
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
        ${result}=	Find In Log with Timeout	${engineLog1}	${start}	${content}	60
        Should Be True	${result}	msg=An Initial service state on service (50, 1000) should be raised before we can start external commands.

	Remove Poller by id	51001	${3}

	Stop Engine
	Kindly Stop Broker

	FOR    ${index}    IN RANGE    60
	 ${output}=	Query	SELECT instance_id FROM instances WHERE name='Poller2'
	 Sleep	1s
	 log to console	Output= ${output}
	 EXIT FOR LOOP IF	"${output}" == "()"
	END
	Should Be Equal As Strings	${output}	()

EBDP8
	[Documentation]	Four new pollers are started and then we remove Poller3 with its hosts and services. All service status/host status are then refused by broker.
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
	Broker Config Log	central	core	error
	Broker Config Log	central	sql	trace
	Broker Config Log	module3	neb	trace
	Broker Config Flush log	central	0
	Config Broker Sql Output	central	unified_sql
	${start}=	Get Current Date
	Start Broker
	Start Engine

	# Let's wait for the initial service states.
        ${content}=	Create List	INITIAL SERVICE STATE: host_50;service_1000;
        ${result}=	Find In Log with Timeout	${engineLog3}	${start}	${content}	60
        Should Be True	${result}	msg=An Initial service state on service (50, 1000) should be raised before we can start external commands.

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	FOR    ${index}    IN RANGE    60
	 ${output}=	Query	SELECT instance_id FROM instances WHERE name='Poller3'
	 Sleep	1s
	 EXIT FOR LOOP IF	"${output}" != "()"
	END
	Should Be Equal As Strings	${output}	((4,),)

	# Let's brutally kill the poller
	${content}=	Create List	processing poller event (id: 4, name: Poller3, running:
	${result}=	Find In log with timeout	${centralLog}	${start}	${content}	60
        Should Be True	${result}	msg=We want the poller 4 event before stopping broker
	Kindly Stop Broker
	Clear Broker Logs

	# Generation of many service status but kept in memory on poller3.
	FOR	${i}	IN RANGE	200
	 Process Service Check result	host_40	service_781	2	service_781 should fail	config3
	 Process Service Check result	host_40	service_782	1	service_782 should fail	config3
	END
	${content}=	Create List	SERVICE ALERT: host_40;service_781;CRITICAL	SERVICE ALERT: host_40;service_782;WARNING
	${result}=	Find In log with timeout	${engineLog3}	${start}	${content}	60
        Should Be True	${result}	msg=Service alerts about service 781 and 782 should be raised

	${content}=	Create List	callbacks: service (40, 781) has no perfdata	service (40, 782) has no perfdata
	${result}=	Find In log with timeout	${moduleLog3}	${start}	${content}	60
        Should Be True	${result}	msg=pb service status on services (40, 781) and (40, 782) should be generated
	Stop Engine

	# Because poller3 is going to be removed, we move its memory file to poller0, 1 and 2.
	Move File	/var/lib/centreon-engine/central-module-master3.memory.central-module-master-output	/var/lib/centreon-engine/central-module-master0.memory.central-module-master-output

	# Poller3 is removed from the engine configuration but still there in centreon_storage DB
	Config Engine	${3}	${39}	${20}

	# Restart Broker
	Start Broker
	Remove Poller by id	51001	${4}
	FOR    ${index}    IN RANGE    60
	 ${output}=	Query	SELECT instance_id FROM instances WHERE name='Poller3'
	 Sleep	1s
	 EXIT FOR LOOP IF	"${output}" == "()"
	END
	Should Be Equal As Strings	${output}	()

	Start Engine
	# Let's wait for the initial service states.
        ${content}=	Create List	INITIAL SERVICE STATE: host_13;service_260;
        ${result}=	Find In Log with Timeout	${engineLog0}	${start}	${content}	60
        Should Be True	${result}	msg=An Initial service state on service (13, 260) should be raised before we can start external commands.

	${content}=	Create List	service status (40, 781) thrown away because host 40 is not known by any poller
	log to console	date ${start}
	${result}=	Find in Log With Timeout	${centralLog}	${start}	${content}	60
	Should be True	${result}	msg=No message about these two wrong service status.
	Stop Engine
	Kindly Stop Broker
