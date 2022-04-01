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
BESEV1
	[Documentation]	Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Broker is started before.
	[Tags]	Broker	Engine	protobuf	bbdo	severities
	#Clear DB	severities
	Config Engine	${1}
	Create Severities File	${0}	${20}
	Config Engine Add Cfg File	${0}	severities.cfg
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Broker Config Log	module	neb	debug
	Broker Config Log	central	sql	debug
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	check severity With Timeout	severity20	5	1	30
	Should Be True	${result}	msg=severity20 should be of level 5 with icon_id 1
	${result}=	check severity With Timeout	severity1	1	5	30
	Should Be True	${result}	msg=severity1 should be of level 1 with icon_id 5
	Stop Engine
	Stop Broker

BESEV2
	[Documentation]	Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Engine is started before.
	[Tags]	Broker	Engine	protobuf	bbdo	severities
	#Clear DB	severities
	Config Engine	${1}
	Create Severities File	${0}	${20}
	Config Engine Add Cfg File	${0}	severities.cfg
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Broker Config Log	module	neb	debug
	Broker Config Log	central	sql	debug
	Clear Retention
	${start}=	Get Current Date
	Start Engine
	Sleep	1s
	Start Broker
	${result}=	check severity With Timeout	severity20	5	1	30
	Should Be True	${result}	msg=severity20 should be of level 5 with icon_id 1
	${result}=	check severity With Timeout	severity1	1	5	30
	Should Be True	${result}	msg=severity1 should be of level 1 with icon_id 5
	Stop Engine
	Stop Broker

#BESEV3
#	[Documentation]	Engine is configured with some severities (same as before). Engine and broker are started like in BESV2, severities.cfg is changed and engine reloaded. Broker updates the DB. Several severities are removed and Broker updates correctly the DB.
#	[Tags]	Broker	Engine	protobuf	bbdo	severities
#	#Clear DB	severities
#	Config Engine	${1}
#	Engine Config Set Value	${0}	log_legacy_enabled	${0}
#	Engine Config Set Value	${0}	log_v2_enabled	${1}
#	Engine Config Set Value	${0}	log_level_config	debug
#	Create Severities File	${0}	${20}
#	Config Engine Add Cfg File	${0}	severities.cfg
#	Config Broker	central
#	Config Broker	rrd
#	Config Broker	module
#	Broker Config Log	module	neb	debug
#	Broker Config Log	central	sql	debug
#	Clear Retention
#	${start}=	Get Current Date
#	Start Engine
#	Start Broker
#	${result}=	check severity With Timeout	severity20	5	1	30
#	Should Be True	${result}	msg=severity20 should be of level 5 with icon_id 1
#	${result}=	check severity With Timeout	severity1	1	5	30
#	Should Be True	${result}	msg=severity1 should be of level 1 with icon_id 5
#
#	Create Severities File	${0}	${20}	2
#	Reload Engine
#	Reload Broker
#	${result}=	check severity With Timeout	severity21	5	1	30
#	Should Be True	${result}	msg=severity21 should be of level 5 with icon_id 1
#	${result}=	check severity With Timeout	severity2	1	5	30
#	Should Be True	${result}	msg=severity2 should be of level 1 with icon_id 5
#
#	Create Severities File	${0}	${10}
#	Reload Engine
#	Reload Broker
#	${result}=	check severity With Timeout	severity10	5	1	30
#	Should Be True	${result}	msg=severity21 should be of level 5 with icon_id 1
#	${result}=	check severity With Timeout	severity1	1	5	30
#	Should Be True	${result}	msg=severity2 should be of level 1 with icon_id 5
#
#	log to console	Waiting for 10 severities now.
#	# Now, we should have 10 severities
#	${result}=	Check Severities Count	10	120
#	Should Be True	${result}	msg=We should only have 10 severities.
#
#	Stop Engine
#	Stop Broker

BEUSEV1
	[Documentation]	Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Broker is started before.
	[Tags]	Broker	Engine	protobuf	bbdo	severities
	#Clear DB	severities
	Config Engine	${1}
	Create Severities File	${0}	${20}
	Config Engine Add Cfg File	${0}	severities.cfg
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Config Broker Sql Output	central	unified_sql
        Broker Config Add Item	module	bbdo_version	3.0.0
        Broker Config Add Item	central	bbdo_version	3.0.0
        Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	module	neb	debug
	Broker Config Log	central	sql	debug
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	check severity With Timeout	severity20	5	1	30
	Should Be True	${result}	msg=severity20 should be of level 5 with icon_id 1
	${result}=	check severity With Timeout	severity1	1	5	30
	Should Be True	${result}	msg=severity1 should be of level 1 with icon_id 5
	Stop Engine
	Stop Broker

BEUSEV2
	[Documentation]	Engine is configured with some severities. When broker receives them, it stores them in the centreon_storage.severities table. Engine is started before.
	[Tags]	Broker	Engine	protobuf	bbdo	severities
	#Clear DB	severities
	Config Engine	${1}
	Create Severities File	${0}	${20}
	Config Engine Add Cfg File	${0}	severities.cfg
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Config Broker Sql Output	central	unified_sql
        Broker Config Add Item	module	bbdo_version	3.0.0
        Broker Config Add Item	central	bbdo_version	3.0.0
        Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	module	neb	debug
	Broker Config Log	central	sql	debug
	Clear Retention
	${start}=	Get Current Date
	Start Engine
	Sleep	1s
	Start Broker
	${result}=	check severity With Timeout	severity20	5	1	30
	Should Be True	${result}	msg=severity20 should be of level 5 with icon_id 1
	${result}=	check severity With Timeout	severity1	1	5	30
	Should Be True	${result}	msg=severity1 should be of level 1 with icon_id 5
	Stop Engine
	Stop Broker

BEUSEV3
	[Documentation]	Four services have a severity added. Then we remove the severity from service 1. Then we change severity 11 to severity7 for service 3.
	[Tags]	Broker	Engine	protobuf	bbdo	severities
	#Clear DB	severities
	Config Engine	${1}
	Create Severities File	${0}	${20}
	Config Engine Add Cfg File	${0}	severities.cfg
        Add Severity To Services	0	11	[1, 2, 3, 4]
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

	${result}=	check service severity With Timeout	1	1	11	60
	Should Be True	${result}	msg=Service (1, 1) should have severity_id=11

        Remove Severities From Services	${0}
        Add Severity To Services	0	11	[2, 4]
        Add Severity To Services	0	7	[3]
        Reload Engine
        Reload Broker
	${result}=	check service severity With Timeout	1	3	7	60
	Should Be True	${result}	msg=Service (1, 3) should have severity_id=7
	${result}=	check service severity With Timeout	1	1	None	60
	Should Be True	${result}	msg=Service (1, 1) should have no severity

	Stop Engine
	Kindly Stop Broker

BEUSEV4
	[Documentation]	Four services are configured with a severity on two pollers. Then we remove severities from the first and second services of the first poller but only the severity from the first service of the second poller. Then only severities no more used should be removed from the database.
	[Tags]	Broker	Engine	protobuf	bbdo	severities
	Config Engine	${2}
	Create Severities File	${0}	${20}
	Create Severities File	${1}	${20}
	Config Engine Add Cfg File	${0}	severities.cfg
	Config Engine Add Cfg File	${1}	severities.cfg
#        Add Severity To Services	0	19	[2, 4]
#        Add Severity To Services	0	17	[3, 5]
#        Add Severity To Services	1	19	[501, 502]
#        Add Severity To Services	1	17	[503]
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
	# We need to wait a little before reloading Engine
        Sleep	50s
#	${result}=	check service severity With Timeout	1	2	19	60
#	Should Be True	${result}	msg=Service (1, 2) should have severity_id=19

#	${result}=	check service severity With Timeout	1	4	19	60
#	Should Be True	${result}	msg=Service (1, 4) should have severity_id=19
#
#	${result}=	check service severity With Timeout	26	501	19	60
#	Should Be True	${result}	msg=Service (26, 501) should have severity_id=19
#
#	${result}=	check service severity With Timeout	26	502	19	60
#	Should Be True	${result}	msg=Service (26, 502) should have severity_id=19
#
#	${result}=	check service severity With Timeout	1	3	17	60
#	Should Be True	${result}	msg=Service (1, 3) should have severity_id=17
#
#	${result}=	check service severity With Timeout	1	5	17	60
#	Should Be True	${result}	msg=Service (1, 5) should have severity_id=17
#
#	${result}=	check service severity With Timeout	26	503	17	60
#	Should Be True	${result}	msg=Service (26, 503) should have severity_id=17
#
#        Remove Severities From Services	${0}
#	Create Severities File	${0}	${16}
#	Create Severities File	${1}	${18}
#        Add Severity To Services	1	17	[503]
#        Reload Engine
#        Reload Broker
#        Sleep	3s
#	${result}=	check service severity With Timeout	26	503	17	60
#	Should Be True	${result}	msg=Service (26, 503) should have severity_id=17
#
#	${result}=	check service severity With Timeout	1	4	None	60
#	Should Be True	${result}	msg=Service (1, 4) should have severity_id=19
#
#	${result}=	check service severity With Timeout	26	501	None	60
#	Should Be True	${result}	msg=Service (26, 501) should have severity_id=19
#
#	${result}=	check service severity With Timeout	26	502	None	60
#	Should Be True	${result}	msg=Service (26, 502) should have severity_id=19
#
#	${result}=	check service severity With Timeout	1	3	None	60
#	Should Be True	${result}	msg=Service (1, 3) should have severity_id=17
#
#	${result}=	check service severity With Timeout	1	5	None	60
#	Should Be True	${result}	msg=Service (1, 5) should have severity_id=17
#
#	${result}=	check severity existence uWith Timeout	19	0	False	60
#	Should Be True	${result}	msg=Service severity 19 should not exist anymore
#
#	${result}=	check severity existence With Timeout	17	0	True	60
#	Should Be True	${result}	msg=Service severity 17 should still exist

	Stop Engine
	Kindly Stop Broker

#BEUSEV3
#	[Documentation]	Engine is configured with some severities (same as before). Engine and broker are started like in BESV2, severities.cfg is changed and engine reloaded. Broker updates the DB. Several severities are removed and Broker updates correctly the DB.
#	[Tags]	Broker	Engine	protobuf	bbdo	severities
#	#Clear DB	severities
#	Config Engine	${1}
#	Engine Config Set Value	${0}	log_legacy_enabled	${0}
#	Engine Config Set Value	${0}	log_v2_enabled	${1}
#	Engine Config Set Value	${0}	log_level_config	debug
#	Create Severities File	${0}	${20}
#	Config Engine Add Cfg File	${0}	severities.cfg
#	Config Broker	central
#	Config Broker	rrd
#	Config Broker	module
#	Config Broker Sql Output	central	unified_sql
#        Broker Config Add Item	module	bbdo_version	3.0.0
#        Broker Config Add Item	central	bbdo_version	3.0.0
#        Broker Config Add Item	rrd	bbdo_version	3.0.0
#	Broker Config Log	module	neb	debug
#	Broker Config Log	central	sql	debug
#	Clear Retention
#	${start}=	Get Current Date
#	Start Engine
#	Start Broker
#	${result}=	check severity With Timeout	severity20	5	1	30
#	Should Be True	${result}	msg=severity20 should be of level 5 with icon_id 1
#	${result}=	check severity With Timeout	severity1	1	5	30
#	Should Be True	${result}	msg=severity1 should be of level 1 with icon_id 5
#
#	Create Severities File	${0}	${20}	2
#	Reload Engine
#	Reload Broker
#	${result}=	check severity With Timeout	severity21	5	1	30
#	Should Be True	${result}	msg=severity21 should be of level 5 with icon_id 1
#	${result}=	check severity With Timeout	severity2	1	5	30
#	Should Be True	${result}	msg=severity2 should be of level 1 with icon_id 5
#
#	Create Severities File	${0}	${10}
#	Reload Engine
#	Reload Broker
#	${result}=	check severity With Timeout	severity10	5	1	30
#	Should Be True	${result}	msg=severity21 should be of level 5 with icon_id 1
#	${result}=	check severity With Timeout	severity1	1	5	30
#	Should Be True	${result}	msg=severity2 should be of level 1 with icon_id 5
#
#	log to console	Waiting for 10 severities now.
#	# Now, we should have 10 severities
#	${result}=	Check Severities Count	10	120
#	Should Be True	${result}	msg=We should only have 10 severities.
#
#	Stop Engine
#	Stop Broker
