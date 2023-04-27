*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Downtimes Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes
Test Teardown	Save logs If Failed

Documentation	Centreon Broker and Engine progressively add services
Library	Process
Library	DatabaseLibrary
Library	OperatingSystem
Library	DateTime
Library	Collections
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py

*** Test Cases ***
BEDTMASS1
	[Documentation]	New services with several pollers are created. Then downtimes are set on all configured hosts. This action results on 1050 downtimes if we also count impacted services. Then all these downtimes are removed. This test is done with BBDO 3.0.0
	[Tags]	Broker	Engine	services	protobuf
	Config Engine	${3}	${50}	${20}
	Engine Config Set Value	${0}	log_level_functions	trace
	Engine Config Set Value	${1}	log_level_functions	trace
	Engine Config Set Value	${2}	log_level_functions	trace
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${3}
	Broker Config Log	central	sql	debug
	Broker Config Log	module0	neb	debug
	Broker Config Log	module1	neb	debug
	Broker Config Log	module2	neb	debug

        Broker Config Add Item	module0	bbdo_version	3.0.0
        Broker Config Add Item	module1	bbdo_version	3.0.0
        Broker Config Add Item	module2	bbdo_version	3.0.0
        Broker Config Add Item	central	bbdo_version	3.0.0
        Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	sql	debug
	Config Broker Sql Output	central	unified_sql
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	# Let's wait for the initial service states.
        ${content}=	Create List	INITIAL SERVICE STATE: host_50;service_1000;
        ${result}=	Find In Log with Timeout	${engineLog2}	${start}	${content}	60
        Should Be True	${result}	msg=An Initial service state on service (50, 1000) should be raised before we can start external commands.

	# It's time to schedule downtimes
	FOR	${i}	IN RANGE	${17}
	 ${host0}=	Catenate	SEPARATOR=	host_	${i + 1}
	 ${host1}=	Catenate	SEPARATOR=	host_	${i + 18}
	 ${host2}=	Catenate	SEPARATOR=	host_	${i + 35}
	 Schedule host downtime	${0}	${host0}	${3600}
	 Schedule host downtime	${1}	${host1}	${3600}
	 Schedule host downtime	${2}	${host2}	${3600}
	END

	${result}=	check number of downtimes	${1050}	${start}	${60}
	Should be true	${result}	msg=We should have 1050 downtimes enabled.

	# It's time to delete downtimes
	FOR	${i}	IN RANGE	${17}
	 ${host0}=	Catenate	SEPARATOR=	host_	${i + 1}
	 ${host1}=	Catenate	SEPARATOR=	host_	${i + 18}
	 ${host2}=	Catenate	SEPARATOR=	host_	${i + 35}
	 Delete host downtimes	${0}	${host0}
	 Delete host downtimes	${1}	${host1}
	 Delete host downtimes	${2}	${host2}
	END

	${result}=	check number of downtimes	${0}	${start}	${60}
	Should be true	${result}	msg=We should have no downtime enabled.

	Stop Engine
	Kindly Stop Broker

BEDTMASS2
	[Documentation]	New services with several pollers are created. Then downtimes are set on all configured hosts. This action results on 1050 downtimes if we also count impacted services. Then all these downtimes are removed. This test is done with BBDO 2.0
	[Tags]	Broker	Engine	services	protobuf
	Config Engine	${3}	${50}	${20}
	Engine Config Set Value	${0}	log_level_functions	trace
	Engine Config Set Value	${1}	log_level_functions	trace
	Engine Config Set Value	${2}	log_level_functions	trace
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${3}
	Broker Config Log	central	sql	debug
	Broker Config Log	module0	neb	debug
	Broker Config Log	module1	neb	debug
	Broker Config Log	module2	neb	debug

	Broker Config Log	central	sql	debug
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	# Let's wait for the initial service states.
        ${content}=	Create List	INITIAL SERVICE STATE: host_50;service_1000;
        ${result}=	Find In Log with Timeout	${engineLog2}	${start}	${content}	60
        Should Be True	${result}	msg=An Initial service state on service (50, 1000) should be raised before we can start external commands.

	# It's time to schedule downtimes
	FOR	${i}	IN RANGE	${17}
	 ${host0}=	Catenate	SEPARATOR=	host_	${i + 1}
	 ${host1}=	Catenate	SEPARATOR=	host_	${i + 18}
	 ${host2}=	Catenate	SEPARATOR=	host_	${i + 35}
	 Schedule host downtime	${0}	${host0}	${3600}
	 Schedule host downtime	${1}	${host1}	${3600}
	 Schedule host downtime	${2}	${host2}	${3600}
	END

	${result}=	check number of downtimes	${1050}	${start}	${60}
	Should be true	${result}	msg=We should have 1050 downtimes enabled.

	# It's time to delete downtimes
	FOR	${i}	IN RANGE	${17}
	 ${host0}=	Catenate	SEPARATOR=	host_	${i + 1}
	 ${host1}=	Catenate	SEPARATOR=	host_	${i + 18}
	 ${host2}=	Catenate	SEPARATOR=	host_	${i + 35}
	 Delete host downtimes	${0}	${host0}
	 Delete host downtimes	${1}	${host1}
	 Delete host downtimes	${2}	${host2}
	END

	${result}=	check number of downtimes	${0}	${start}	${60}
	Should be true	${result}	msg=We should have no downtime enabled.

	Stop Engine
	Kindly Stop Broker

BEDTSVCREN1
	[Documentation]	A downtime is set on a service then the service is renamed. The downtime is still active on the renamed service. The downtime is removed from the renamed service and it is well removed.
	[Tags]	Broker	Engine	services	downtime
	Config Engine	${1}
	Engine Config Set Value	${0}	log_level_functions	trace
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	sql	debug
	Broker Config Log	module0	neb	debug

	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	# Let's wait for the check of external commands
        ${content}=	Create List	check_for_external_commands
        ${result}=	Find In Log with Timeout	${engineLog0}	${start}	${content}	60
        Should Be True	${result}	msg=No check for external commands executed for 1mn.

	# It's time to schedule a downtime
	Schedule service downtime	host_1	service_1	${3600}

	${result}=	check number of downtimes	${1}	${start}	${60}
	Should be true	${result}	msg=We should have 1 downtime enabled.

	# Let's rename the service service_1
	Rename Service	${0}	host_1	service_1	toto_1

	Reload Engine
	# Let's wait for the check of external commands
        ${content}=	Create List	check_for_external_commands
        ${result}=	Find In Log with Timeout	${engineLog0}	${start}	${content}	60
        Should Be True	${result}	msg=No check for external commands executed for 1mn.

	Delete service downtime full	${0}	host_1	toto_1

	${result}=	check number of downtimes	${0}	${start}	${60}
	Should be true	${result}	msg=We should have no downtime enabled.

	Stop Engine
	Kindly Stop Broker


BEDTSVCFIXED
	[Documentation]	A downtime is set on a service, the total number of downtimes is really 1 then we delete this downtime and the number of downtime is 0.
	[Tags]	Broker	Engine	downtime
	Config Engine	${1}
	Engine Config Set Value	${0}	log_level_functions	trace
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	sql	debug
	Broker Config Log	module0	neb	debug

	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
 	# Let's wait for the check of external commands
  ${content}=	Create List	check_for_external_commands
  ${result}=	Find In Log with Timeout	${engineLog0}	${start}	${content}	60
  Should Be True	${result}	msg=No check for external commands executed for 1mn.

	# It's time to schedule a downtime
	Schedule service downtime  host_1  service_1  ${3600}

	${result}=	check number of downtimes	${1}	${start}	${60}
	Should be true	${result}	msg=We should have 1 downtime enabled.

	Delete service downtime full	${0}	host_1	service_1

	${result}=	check number of downtimes	${0}	${start}	${60}
	Should be true	${result}	msg=We should have no downtime enabled.

	Stop Engine
	Kindly Stop Broker

BEDTHOSTFIXED
	[Documentation]	A downtime is set on a host, the total number of downtimes is really 21 (1 for the host and 20 for its 20 services) then we delete this downtime and the number is 0.
	[Tags]	Broker	Engine	downtime
	Config Engine	${1}
	Engine Config Set Value	${0}	log_level_functions	trace
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	sql	debug
	Broker Config Log	module0	neb	debug
	Config Broker Sql Output	central	unified_sql

	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	# Let's wait for the check of external commands
  ${content}=	Create List	check_for_external_commands
  ${result}=	Find In Log with Timeout	${engineLog0}	${start}	${content}	60
  Should Be True	${result}	msg=No check for external commands executed for 1mn.

	# It's time to schedule downtimes
	Schedule host fixed downtime	${0}	host_1	${3600}

	${result}=	check number of downtimes	${21}	${start}	${60}
	Should be true	${result}	msg=We should have 21 downtimes (1 host + 20 services) enabled.

	# It's time to delete downtimes
	Delete host downtimes	${0}	host_1

	${result}=	check number of downtimes	${0}	${start}	${60}
	Should be true	${result}	msg=We should have no downtime enabled.

	Stop Engine
	Kindly Stop Broker

*** Keywords ***
Clean Downtimes Before Suite
	Clean Before Suite
	
	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	${output}=	Execute SQL String	DELETE FROM downtimes WHERE deletion_time IS NULL
