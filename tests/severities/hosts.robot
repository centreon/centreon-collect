*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes

Documentation	Engine/Broker tests on severities.
Library	Process
Library	DateTime
Library	OperatingSystem
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py
Library	../resources/specific-duplication.py

*** Test Cases ***
BEUHSEV1
	[Documentation]	Four hosts have a severity added. Then we remove the severity from host 1. Then we change severity 10 to severity8 for host 3.
	[Tags]	Broker	Engine	protobuf	bbdo	severities
	#Clear DB	severities
	Config Engine	${1}
	Create Severities File	${0}	${20}
	Config Engine Add Cfg File	${0}	severities.cfg
	Add Severity To Hosts	0	10	[1, 2, 3, 4]
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Config Broker Sql Output	central	unified_sql
	Broker Config Add Item	module	bbdo_version	3.0.0
	Broker Config Add Item	central	bbdo_version	3.0.0
	Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	module	neb	debug
	Broker Config Log	central	sql	trace
	Clear Retention
	${start}=	Get Current Date
	Start Engine
	Start Broker
	Sleep	2s

	${result}=	check host severity With Timeout	1	10	60
	Should Be True	${result}	msg=Host 1 should have severity_id=10

	Remove Severities From Hosts	${0}
	Add Severity To Hosts	0	10	[2, 4]
	Add Severity To Hosts	0	8	[3]
	Reload Engine
	Reload Broker
	${result}=	check host severity With Timeout	3	8	60
	Should Be True	${result}	msg=Host 3 should have severity_id=8
	${result}=	check host severity With Timeout	1	None	60
	Should Be True	${result}	msg=Host 1 should have no severity

	Stop Engine
	Kindly Stop Broker

BEUHSEV2
	[Documentation]	Seven services are configured with a severity on two pollers. Then we remove severities from the first and second services of the first poller but only the severity from the first service of the second poller. Then only severities no more used should be removed from the database.
	[Tags]	Broker	Engine	protobuf	bbdo	severities
	Config Engine	${2}
	Create Severities File	${0}	${20}
	Create Severities File	${1}	${20}
	Config Engine Add Cfg File	${0}	severities.cfg
	Config Engine Add Cfg File	${1}	severities.cfg
	Engine Config Set Value	${0}	log_level_config	debug
	Engine Config Set Value	${1}	log_level_config	debug
	Add Severity To Services	0	19	[2, 4]
	Add Severity To Services	0	17	[3, 5]
	Add Severity To Services	1	19	[501, 502]
	Add Severity To Services	1	17	[503]
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Config Broker Sql Output	central	unified_sql
	Broker Config Add Item	module	bbdo_version	3.0.0
	Broker Config Add Item	central	bbdo_version	3.0.0
	Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	module	neb	debug
	Broker Config Log	central	sql	trace
	Clear Retention
	${start}=	Get Current Date
	Start Engine
	Start Broker
	Sleep	5s
	# We need to wait a little before reloading Engine
	${result}=	check service severity With Timeout	1	2	19	60
	Should Be True	${result}	msg=First step: Service (1, 2) should have severity_id=19

	${result}=	check service severity With Timeout	1	4	19	60
	Should Be True	${result}	msg=First step: Service (1, 4) should have severity_id=19

	${result}=	check service severity With Timeout	26	501	19	60
	Should Be True	${result}	msg=First step: Service (26, 501) should have severity_id=19

	${result}=	check service severity With Timeout	26	502	19	60
	Should Be True	${result}	msg=First step: Service (26, 502) should have severity_id=19

	${result}=	check service severity With Timeout	1	3	17	60
	Should Be True	${result}	msg=First step: Service (1, 3) should have severity_id=17

	${result}=	check service severity With Timeout	1	5	17	60
	Should Be True	${result}	msg=First step: Service (1, 5) should have severity_id=17

	${result}=	check service severity With Timeout	26	503	17	60
	Should Be True	${result}	msg=First step: Service (26, 503) should have severity_id=17

	Remove Severities From Services	${0}
	Create Severities File	${0}	${18}
	Create Severities File	${1}	${18}
	Add Severity To Services	1	17	[503]
	Reload Engine
	Reload Broker
	Sleep	3s
	${result}=	check service severity With Timeout	26	503	17	60
	Should Be True	${result}	msg=Second step: Service (26, 503) should have severity_id=17

	${result}=	check service severity With Timeout	1	4	None	60
	Should Be True	${result}	msg=Second step: Service (1, 4) should have severity_id=None

	${result}=	check service severity With Timeout	1	3	None	60
	Should Be True	${result}	msg=Second step: Service (1, 3) should have severity_id=17

	${result}=	check service severity With Timeout	1	5	None	60
	Should Be True	${result}	msg=Second step: Service (1, 5) should have severity_id=17

	Stop Engine
	Kindly Stop Broker

BETUHSEV1
	[Documentation]	Services have severities provided by templates.
	[Tags]	Broker	Engine	protobuf	bbdo	severities
	Config Engine	${2}
	Create Severities File	${0}	${20}
	Create Severities File	${1}	${20}
        Create Template File	${0}	service	severity	[1, 3]
        Create Template File	${1}	service	severity	[3, 5]

	Config Engine Add Cfg File	${0}	severities.cfg
	Config Engine Add Cfg File	${1}	severities.cfg
	Config Engine Add Cfg File	${0}	serviceTemplates.cfg
	Config Engine Add Cfg File	${1}	serviceTemplates.cfg
	Engine Config Set Value	${0}	log_level_config	debug
	Engine Config Set Value	${1}	log_level_config	debug
	Add Template To Services	0	service_template_1	[1, 2, 3, 4]
	Add Template To Services	0	service_template_2	[5, 6, 7, 8]
	Add Template To Services	1	service_template_1	[501, 502]
	Add Template To Services	1	service_template_2	[503, 504]
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Config Broker Sql Output	central	unified_sql
	Broker Config Add Item	module	bbdo_version	3.0.0
	Broker Config Add Item	central	bbdo_version	3.0.0
	Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	module	neb	debug
	Broker Config Log	central	sql	trace
	Clear Retention
	${start}=	Get Current Date
	Start Engine
	Start Broker
	Sleep	5s
	# We need to wait a little before reloading Engine
	${result}=	check service severity With Timeout	1	2	1	60
	Should Be True	${result}	msg=First step: Service (1, 2) should have severity_id=1

	${result}=	check service severity With Timeout	1	4	1	60
	Should Be True	${result}	msg=First step: Service (1, 4) should have severity_id=1

	${result}=	check service severity With Timeout	1	5	3	60
	Should Be True	${result}	msg=First step: Service (1, 5) should have severity_id=3

	${result}=	check service severity With Timeout	26	502	3	60
	Should Be True	${result}	msg=First step: Service (26, 502) should have severity_id=3

	${result}=	check service severity With Timeout	26	503	5	60
	Should Be True	${result}	msg=First step: Service (26, 503) should have severity_id=5

	Stop Engine
	Kindly Stop Broker

