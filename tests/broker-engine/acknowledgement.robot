*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes
Test Teardown	Save logs If Failed

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
BEACK1
	[Documentation]	Engine has a critical service. An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to OK. And the acknowledgement in database is deleted from engine but still open on the database.
	[Tags]	Broker	Engine	services	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	module0	neb	debug
	Broker Config Log	central	sql	debug

	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_50;service_1000;	check_for_external_commands()
	${result}=	Find In Log with Timeout	${engineLog0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial service state on (host_50,service_1000) should be raised before we can start our external commands.

	# Time to set the service to CRITICAL HARD.
	Process Service Check Result	host_1	service_1	2	(1;1) is critical
	${result}=	Check Service Status With Timeout	host_1	service_1	${2}	60	SOFT
	Should Be True	${result}	msg=Service (1;1) should be critical
	Repeat Keyword	2 times	Process Service Check Result	host_1	service_1	2	(1;1) is critical

	${result}=	Check Service Status With Timeout	host_1	service_1	${2}	60	HARD
	Should Be True	${result}	msg=Service (1;1) should be critical HARD
	${d}=	Get Current Date	result_format=epoch	exclude_millis=True
	Acknowledge Service Problem	host_1	service_1
	${ack_id}=	Check Acknowledgement With Timeout	host_1	service_1	${d}	2	60	HARD
	Should Be True	${ack_id} > 0	msg=No acknowledgement on service (1, 1).

	# Service_1 is set back to OK.
	Repeat Keyword	3 times	Process Service Check Result	host_1	service_1	0	(1;1) is OK
	${result}=	Check Service Status With Timeout	host_1	service_1	${0}	60	HARD
	Should Be True	${result}	msg=Service (1;1) should be OK HARD

	# Acknowledgement is deleted but this is not visible in database.

BEACK2
	[Documentation]	Configuration is made with BBDO3. Engine has a critical service. An external command is sent to acknowledge it. The centreon_storage.acknowledgements table is then updated with this acknowledgement. The service is newly set to OK. And the acknowledgement in database is deleted.
	[Tags]	Broker	Engine	services	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Config BBDO3	${1}
	Broker Config Log	module0	neb	debug
	Broker Config Log	central	sql	debug

	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_50;service_1000;	check_for_external_commands()
	${result}=	Find In Log with Timeout	${engineLog0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial service state on (host_50,service_1000) should be raised before we can start our external commands.

	# Time to set the service to CRITICAL HARD.
	Process Service Check Result	host_1	service_1	2	(1;1) is critical
	${result}=	Check Service Resource Status With Timeout	host_1	service_1	${2}	60	SOFT
	Should Be True	${result}	msg=Service (1;1) should be critical
	Repeat Keyword	2 times	Process Service Check Result	host_1	service_1	2	(1;1) is critical

	${result}=	Check Service Resource Status With Timeout	host_1	service_1	${2}	60	HARD
	Should Be True	${result}	msg=Service (1;1) should be critical HARD
	${d}=	Get Current Date	result_format=epoch	exclude_millis=True
	Acknowledge Service Problem	host_1	service_1
	${ack_id}=	Check Acknowledgement With Timeout	host_1	service_1	${d}	2	60	HARD
	Should Be True	${ack_id} > 0	msg=No acknowledgement on service (1, 1).

	# Service_1 is set back to OK.
	Repeat Keyword	3 times	Process Service Check Result	host_1	service_1	0	(1;1) is OK
	${result}=	Check Service Resource Status With Timeout	host_1	service_1	${0}	60	HARD
	Should Be True	${result}	msg=Service (1;1) should be OK HARD

	# Acknowledgement is deleted but this is not visible in database.
