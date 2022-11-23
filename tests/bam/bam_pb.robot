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











BEPB_DIMENSION_BV_EVENT
	[Documentation]	bbdo_version 3 use pb_dimension_bv_event message.
	[Tags]	Broker	Engine	protobuf	bbdo
	Clear Commands Status
	Clear Retention

    Remove File     /tmp/all_lua_event.log
    Config BBDO3	${1}
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
    Broker Config Add Item	module0	bbdo_version	3.0.0
    Broker Config Add Item	central	bbdo_version	3.0.0
	Broker Config Log	central	bam	trace
	Broker Config Log	central	sql	trace
	Config Broker Sql Output	central	unified_sql
	
	Clone Engine Config To DB
	Add Bam Config To Broker	central

	Broker Config Add Lua Output	central	test-protobuf	${SCRIPTS}test-log-all-event.lua
	
	Connect To Database	pymysql	${DBNameConf}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	Execute SQL String	DELETE FROM mod_bam_ba_groups
	Execute SQL String	INSERT INTO mod_bam_ba_groups (id_ba_group, ba_group_name, ba_group_description) VALUES (574, 'virsgtr', 'description_grtmxzo')
	
	Start Broker  True
	Start Engine
    Wait Until Created	/tmp/all_lua_event.log	30s
	FOR	${index}	IN RANGE	10
		${grep_res}=  Grep File  /tmp/all_lua_event.log  "_type":393238, "category":6, "element":22, "bv_id":574, "bv_name":"virsgtr", "bv_description":"description_grtmxzo"
		Sleep	1s
		EXIT FOR LOOP IF	len("""${grep_res}""") > 0
	END

	Should Not Be Empty  ${grep_res}  msg=event not found

	Stop Engine
	Kindly Stop Broker  True

BEPB_DIMENSION_BA_BV_RELATION_EVENT
	[Documentation]	bbdo_version 3 use pb_dimension_ba_bv_relation_event message.
	[Tags]	Broker	Engine	protobuf	bbdo
	Clear Commands Status
	Clear Retention

    Remove File     /tmp/all_lua_event.log
    Config BBDO3	${1}
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
    Broker Config Add Item	module0	bbdo_version	3.0.0
    Broker Config Add Item	central	bbdo_version	3.0.0
	Broker Config Log	central	bam	trace
	Broker Config Log	central	sql	trace
	Config Broker Sql Output	central	unified_sql
	
	Clone Engine Config To DB
	Add Bam Config To Engine
	@{svc}=	Set Variable	${{ [("host_16", "service_314")] }}
	Create BA With Services	test	worst	${svc}

	Add Bam Config To Broker	central

	Broker Config Add Lua Output	central	test-protobuf	${SCRIPTS}test-log-all-event.lua
	
	Connect To Database	pymysql	${DBNameConf}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	Execute SQL String	INSERT INTO mod_bam_bagroup_ba_relation (id_ba, id_ba_group) VALUES (1, 456)
	
	Start Broker  True
	Start Engine
    Wait Until Created	/tmp/all_lua_event.log	30s
	FOR	${index}	IN RANGE	10
		${grep_res}=  Grep File  /tmp/all_lua_event.log  "_type":393239, "category":6, "element":23, "ba_id":1, "bv_id":456
		Sleep	1s
		EXIT FOR LOOP IF	len("""${grep_res}""") > 0
	END

	Should Not Be Empty  ${grep_res}  msg=event not found

	Stop Engine
	Kindly Stop Broker  True


*** Keywords ***
BAM Setup
	Stop Processes
