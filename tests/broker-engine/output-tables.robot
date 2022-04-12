*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes

Documentation	Engine/Broker tests on bbdo_version 3.0.0 and protobuf bbdo embedded events.
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
        Broker Config Add Item	module	bbdo_version	3.0.0
        Broker Config Add Item	central	bbdo_version	3.0.0
        Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	sql	trace
	Config Broker Sql Output	central	unified_sql
        Broker Config Output Set	central	central-broker-unified-sql	store_in_resources	yes
        Broker Config Output Set	central	central-broker-unified-sql	store_in_hosts_services	no
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
        ${content_not_present}=	Create List	processing host status event (host:	UPDATE hosts SET checked=i	processing service status event (host:	UPDATE services SET checked=
        ${content_present}=	Create List	UPDATE resources SET status=	INSERT INTO resources (id
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
        Broker Config Add Item	module	bbdo_version	3.0.0
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
