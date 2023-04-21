*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes
Test Teardown	Save logs If Failed

Documentation	Engine/Broker tests on bbdo_version 3.0.0 and protobuf bbdo embedded events.
Library	DatabaseLibrary
Library	Process
Library	DateTime
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
	${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	30
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

BE_TIME_NULL_SERVICE_RESOURCE
	[Documentation]	bbdo 3.0 times must be null and not zero
	[Tags]	Broker	Engine	protobuf	bbdo
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker Sql Output	central	unified_sql
	Config BBDO3  1

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	Execute SQL String  DELETE FROM services
	Execute SQL String  DELETE FROM resources
	Execute SQL String  DELETE FROM hosts

	Clear Retention

	Start Broker
	Start Engine

	FOR	${index}	IN RANGE	300
		${output}=	Query	SELECT r.last_status_change, s.last_hard_state_change, s.last_notification, s.next_notification , s.last_state_change, s.last_time_critical, s.last_time_ok, s.last_time_unknown, s.last_time_warning, h.last_hard_state_change, h.last_notification, h.next_host_notification, h.last_state_change, h.last_time_down, h.last_time_unreachable, h.last_time_up FROM services s, resources r, hosts h WHERE h.host_id=1 AND s.service_id=1 AND r.id=1 AND r.parent_id=1
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "((None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None),)"
	END
	Should Be Equal As Strings	${output}	((None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None),)
	Stop Engine
	Kindly Stop Broker
