*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes
Test Teardown	Save logs If Failed

Documentation	Engine/Broker tests on bbdo_version 3.0.0 and protobuf bbdo embedded events.
Library	Process
Library	String
Library	DateTime
Library	DatabaseLibrary
Library	OperatingSystem
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py
Library	../resources/specific-duplication.py

*** Test Cases ***
BERES1
	[Documentation]	store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
	[Tags]	Broker	Engine	protobuf	bbdo
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
        Broker Config Add Item	module0	bbdo_version	3.0.0
        Broker Config Add Item	central	bbdo_version	3.0.0
        Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	sql	trace
	Config Broker Sql Output	central	unified_sql
        Broker Config Output Set	central	central-broker-unified-sql	store_in_resources	yes
        Broker Config Output Set	central	central-broker-unified-sql	store_in_hosts_services	no
	Clear Retention
	${start}=	Get Current Date
        Sleep	1s
	Start Broker
	Start Engine
        ${content_not_present}=	Create List	processing host status event (host:	UPDATE hosts SET checked=i	processing service status event (host:	UPDATE services SET checked=
        ${content_present}=	Create List	UPDATE resources SET status=
        ${result}=	Find In log With Timeout	${centralLog}	${start}	${content_present}	60
        Should Be True	${result}	msg=no updates concerning resources available.
        FOR	${l}	IN	${content_not_present}
         ${result}=	Find In Log	${centralLog}	${start}	${content_not_present}
         Should Not Be True	${result[0]}	msg=There are updates of hosts/services table(s).
        END
	Stop Engine
	Kindly Stop Broker

BEHS1
	[Documentation]	store_in_resources is enabled and store_in_hosts_services is not. Only writes into resources should be done (except hosts/services events that continue to be written in hosts/services tables)
	[Tags]	Broker	Engine	protobuf	bbdo
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
        Broker Config Add Item	module0	bbdo_version	3.0.0
        Broker Config Add Item	central	bbdo_version	3.0.0
        Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	sql	trace
	Config Broker Sql Output	central	unified_sql
        Broker Config Output Set	central	central-broker-unified-sql	store_in_resources	no
        Broker Config Output Set	central	central-broker-unified-sql	store_in_hosts_services	yes
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
        ${content_present}=	Create List	UPDATE hosts SET checked=	UPDATE services SET checked=
        ${content_not_present}=	Create List	INSERT INTO resources	UPDATE resources SET	UPDATE tags	INSERT INTO tags	UPDATE severities	INSERT INTO severities
        ${result}=	Find In log With Timeout	${centralLog}	${start}	${content_present}	60
        Should Be True	${result}	msg=no updates concerning hosts/services available.
        FOR	${l}	IN	${content_not_present}
         ${result}=	Find In Log	${centralLog}	${start}	${content_not_present}
         Should Not Be True	${result[0]}	msg=There are updates of the resources table.
        END
	Stop Engine
	Kindly Stop Broker


BEINSTANCESTATUS
	[Documentation]	Instance status to bdd 
	[Tags]	Broker	Engine	
	Config Engine	${1}	${50}	${20}
	engine_config_set_value  0  enable_flap_detection  1  True
	engine_config_set_value  0  enable_notifications  0  True
	engine_config_set_value  0  execute_host_checks  0  True
	engine_config_set_value  0  execute_service_checks  0  True
	engine_config_set_value  0  global_host_event_handler  command_1  True
	engine_config_set_value  0  global_service_event_handler  command_2  True
	engine_config_set_value  0  instance_heartbeat_interval  1  True
	engine_config_set_value  0  obsess_over_hosts  1  True
	engine_config_set_value  0  obsess_over_services  1  True
	engine_config_set_value  0  accept_passive_host_checks  0  True
	engine_config_set_value  0  accept_passive_service_checks  0  True
	
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	sql	trace
	Broker Config Add Item	module0	bbdo_version	3.0.0
	Broker Config Add Item	central	bbdo_version	3.0.0
	Config Broker Sql Output	central	unified_sql
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	check_for_external_commands
	${result}=	Find In Log with Timeout	${engineLog0}	${start}	${content}	60
	Should Be True	${result}	msg=No check for external commands executed for 1mn.
	${result}=  check_field_db_value  SELECT global_host_event_handler FROM instances WHERE instance_id=1  command_1  30
	Should Be True	${result}	msg=global_host_event_handler not updated.
	${result}=  check_field_db_value  SELECT global_service_event_handler FROM instances WHERE instance_id=1  command_2  2
	Should Be True	${result}	msg=global_service_event_handler not updated.
	${result}=  check_field_db_value  SELECT flap_detection FROM instances WHERE instance_id=1  ${1}  3
	Should Be True	${result}	msg=flap_detection not updated.
	${result}=  check_field_db_value  SELECT notifications FROM instances WHERE instance_id=1  ${0}  3
	Should Be True	${result}	msg=notifications not updated.
	${result}=  check_field_db_value  SELECT active_host_checks FROM instances WHERE instance_id=1  ${0}  3
	Should Be True	${result}	msg=active_host_checks not updated.
	${result}=  check_field_db_value  SELECT active_service_checks FROM instances WHERE instance_id=1  ${0}  3
	Should Be True	${result}	msg=active_service_checks not updated.
	${result}=  check_field_db_value  SELECT check_hosts_freshness FROM instances WHERE instance_id=1  ${0}  3
	Should Be True	${result}	msg=check_hosts_freshness not updated.
	${result}=  check_field_db_value  SELECT check_services_freshness FROM instances WHERE instance_id=1  ${1}  3
	Should Be True	${result}	msg=check_services_freshness not updated.
	${result}=  check_field_db_value  SELECT obsess_over_hosts FROM instances WHERE instance_id=1  ${1}  3
	Should Be True	${result}	msg=obsess_over_hosts not updated.
	${result}=  check_field_db_value  SELECT obsess_over_services FROM instances WHERE instance_id=1  ${1}  3
	Should Be True	${result}	msg=obsess_over_services not updated.
	${result}=  check_field_db_value  SELECT passive_host_checks FROM instances WHERE instance_id=1  ${0}  3
	Should Be True	${result}	msg=passive_host_checks not updated.
	${result}=  check_field_db_value  SELECT passive_service_checks FROM instances WHERE instance_id=1  ${0}  3
	Should Be True	${result}	msg=passive_service_checks not updated.
	Stop Engine
	Kindly Stop Broker

BEINSTANCE
	[Documentation]	Instance to bdd 
	[Tags]	Broker	Engine	
	Config Engine	${1}	${50}	${20}

	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	sql	trace
	Broker Config Add Item	module0	bbdo_version	3.0.0
	Broker Config Add Item	central	bbdo_version	3.0.0
	Config Broker Sql Output	central	unified_sql
	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	Execute SQL String	DELETE FROM instances

	#as GetCurrent Date floor milliseconds to upper or lower integer, we substract 1s
	${start}=  get_round_current_date
	Start Broker
	Start Engine
	${engine_pid}=  Get Engine Pid  e0
	${result}=  check_field_db_value  SELECT pid FROM instances WHERE instance_id=1  ${engine_pid}  30
	Should Be True	${result}	msg=no correct engine pid in instances table.
	${result}=  check_field_db_value  SELECT engine FROM instances WHERE instance_id=1  Centreon Engine  3
	Should Be True	${result}	msg=no correct engine in instances table.
	${result}=  check_field_db_value  SELECT running FROM instances WHERE instance_id=1  ${1}  3
	Should Be True	${result}	msg=no correct running in instances table.
	${result}=  check_field_db_value  SELECT name FROM instances WHERE instance_id=1  Poller0  3
	Should Be True	${result}	msg=no correct name in instances table.
	${result}=  check_field_db_value  SELECT end_time FROM instances WHERE instance_id=1  ${0}  3
	Should Be True	${result}	msg=no correct end_time in instances table.
	@{bdd_start_time}=	Query	SELECT start_time FROM instances WHERE instance_id=1
	${now}=	get_round_current_date
	Should Be True  ${start} <= ${bdd_start_time[0][0]} and ${bdd_start_time[0][0]} <= ${now}  sg=no correct start_time in instances table.



BE_NOTIF_OVERFLOW
	[Documentation]	bbdo 2.0 notification number =40000. make an overflow => notification_number null in db
	[Tags]	Broker	Engine	protobuf	bbdo
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
    Broker Config Add Item	module0	bbdo_version	2.0.0
    Broker Config Add Item	central	bbdo_version	2.0.0
	Config Broker Sql Output	central	unified_sql
	Broker Config Log	central	sql	trace
	Broker Config Log	central  perfdata  trace

	Clear Retention

	Start Broker
	Start Engine

	${start}=	Get Current Date
	${content}=	Create List	INITIAL SERVICE STATE: host_16;service_314;
	${result}=	Find In Log with Timeout	${engineLog0}	${start}	${content}	30
	Should Be True	${result}	msg=An Initial host state on host_16 should be raised before we can start our external commands.

	set_svc_notification_number  host_16  service_314  40000
	Repeat Keyword	3 times	Process Service Check Result	host_16	service_314	2	output critical for 314
	${result}=	Check Service Status With Timeout	host_16	service_314	2	30
	Should Be True	${result}	msg=The service (host_16,service_314) is not CRITICAL as expected

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	${output}=	Query	SELECT s.notification_number FROM services s LEFT JOIN hosts h ON s.host_id=h.host_id WHERE h.name='host_16' AND s.description='service_314'
	Should Be True	${output[0][0]} == None  msg=notification_number is not null

	Stop Engine
	Kindly Stop Broker

