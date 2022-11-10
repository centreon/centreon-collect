*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	BAM Setup
Test Teardown	Save logs If Failed

Documentation	Centreon Broker and BAM with bbdo version 3.0.1
Library	Process
Library	DatabaseLibrary
Library	DateTime
Library	OperatingSystem
Library	../resources/Broker.py
Library	../resources/Engine.py


*** Test Cases ***
BAPBSTATUS
	[Documentation]	With bbdo version 3.0.1, a BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. 
	[Tags]	Broker	downtime	engine	bam
	Clear Commands Status
	Config Broker	module
	Config Broker	central
	Config Broker	rrd
	Broker Config Log	central	bam	trace
	Broker Config Log	central	sql	trace
    Config BBDO3	${1}
	Config Engine	${1}

	Clone Engine Config To DB
	Add Bam Config To Engine

	@{svc}=	Set Variable	${{ [("host_16", "service_314")] }}
	Create BA With Services	test	worst	${svc}
	Add Bam Config To Broker	central
	# Command of service_314 is set to critical
	${cmd_1}=	Get Command Id	314
	Log To Console	service_314 has command id ${cmd_1}
	Set Command Status	${cmd_1}	2
	Start Broker
	${start}=	Get Current Date
	Start Engine
	# Let's wait for the initial service states.
    ${content}=	Create List	INITIAL SERVICE STATE: host_50;service_1000;
    ${result}=	Find In Log with Timeout	${engineLog0}	${start}	${content}	60
    Should Be True	${result}	msg=An Initial service state on service (50, 1000) should be raised before we can start external commands.

	# KPI set to critical
	Repeat Keyword	3 times	Process Service Check Result	host_16	service_314	2	output critical for 314

	${result}=	Check Service Status With Timeout	host_16	service_314	2	60	HARD
	Should Be True	${result}	msg=The service (host_16,service_314) is not CRITICAL as expected

	# The BA should become critical
	${result}=	Check Ba Status With Timeout	test	2	60
	Should Be True	${result}	msg=The BA ba_1 is not CRITICAL as expected

    Connect To Database	pymysql	${DBNameConf}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	${output}=	Query	SELECT current_level, acknowledged, downtime, in_downtime, current_status FROM mod_bam WHERE name='test'
	Should Be Equal As Strings	${output}	((100.0, 0.0, 0.0, 0, 2),)

	Stop Engine
	Kindly Stop Broker

*** Keywords ***
BAM Setup
	Stop Processes
