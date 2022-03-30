*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes

Documentation	Engine/Broker tests on tags.
Library	Process
Library	DateTime
Library	OperatingSystem
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py
Library	../resources/specific-duplication.py

*** Test Cases ***
BETAG1
	[Documentation]	Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Broker is started before.
	[Tags]	Broker	Engine	protobuf	bbdo	tags
	Config Engine	${1}
	Create Tags File	${20}
	Config Engine Add Cfg File	tags.cfg
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Broker Config Log	module	neb	debug
	Broker Config Log	central	sql	debug
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	check tag With Timeout	tag20	4	30
	Should Be True	${result}	msg=tag20 should be of type 4
	${result}=	check tag With Timeout	tag1	1	30
	Should Be True	${result}	msg=tag1 should be of type 1
	Stop Engine
	Stop Broker

BETAG2
	[Documentation]	Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
	[Tags]	Broker	Engine	protobuf	bbdo	tags
	Config Engine	${1}
	Create Tags File	${20}
	Config Engine Add Cfg File	tags.cfg
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
	${result}=	check tag With Timeout	tag20	4	30
	Should Be True	${result}	msg=tag20 should be of type 4
	${result}=	check tag With Timeout	tag1	1	30
	Should Be True	${result}	msg=tag1 should be of type 1
	Stop Engine
	Stop Broker

#BETAG3
#	[Documentation]	Engine is configured with some tags (same as before). Engine and broker are started like in BESV2, tags.cfg is changed and engine reloaded. Broker updates the DB. Several tags are removed and Broker updates correctly the DB.
#	[Tags]	Broker	Engine	protobuf	bbdo	tags
#	Config Engine	${1}
#	Engine Config Set Value	${0}	log_legacy_enabled	${0}
#	Engine Config Set Value	${0}	log_v2_enabled	${1}
#	Engine Config Set Value	${0}	log_level_config	debug
#	Create Tags File	${20}
#	Config Engine Add Cfg File	tags.cfg
#	Config Broker	central
#	Config Broker	rrd
#	Config Broker	module
#	Broker Config Log	module	neb	debug
#	Broker Config Log	central	sql	debug
#	Clear Retention
#	${start}=	Get Current Date
#	Start Engine
#	Start Broker
#	${result}=	check tag With Timeout	tag20	4	30
#	Should Be True	${result}	msg=tag20 should be of type 4
#	${result}=	check tag With Timeout	tag1	1	30
#	Should Be True	${result}	msg=tag1 should be of type 1
#
#	Create Tags File	${20}	2
#	Reload Engine
#	Reload Broker
#	${result}=	check tag With Timeout	tag21	4	30
#	Should Be True	${result}	msg=tag21 should be of type 4
#	${result}=	check tag With Timeout	tag2	1	30
#	Should Be True	${result}	msg=tag2 should be of type 1
#
#	Create Tags File	${10}
#	Reload Engine
#	Reload Broker
#	${result}=	check tag With Timeout	tag10	2	30
#	Should Be True	${result}	msg=tag10 should be of type 2
#	${result}=	check tag With Timeout	tag1	1	30
#	Should Be True	${result}	msg=tag1 should be of type 1
#
#	log to console	Waiting for 10 tags now.
#	# Now, we should have 10 tags
#	${result}=	Check Tags Count	10	120
#	Should Be True	${result}	msg=We should only have 10 tags.
#
#	Stop Engine
#	Stop Broker

BEUTAG1
	[Documentation]	Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Broker is started before.
	[Tags]	Broker	Engine	protobuf	bbdo	tags
	Config Engine	${1}
	Create Tags File	${20}
	Config Engine Add Cfg File	tags.cfg
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
	${result}=	check tag With Timeout	tag20	4	30
	Should Be True	${result}	msg=tag20 should be of type 4
	${result}=	check tag With Timeout	tag1	1	30
	Should Be True	${result}	msg=tag1 should be of type 1
	Stop Engine
	Stop Broker

BEUTAG2
	[Documentation]	Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
	[Tags]	Broker	Engine	protobuf	bbdo	tags
	Config Engine	${1}
	Create Tags File	${20}
	Config Engine Add Cfg File	tags.cfg
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
	${result}=	check tag With Timeout	tag20	4	30
	Should Be True	${result}	msg=tag20 should be of type 4
	${result}=	check tag With Timeout	tag1	1	30
	Should Be True	${result}	msg=tag1 should be of type 1
	Stop Engine
	Stop Broker

#BEUTAG3
#	[Documentation]	Engine is configured with some tags (same as before). Engine and broker are started like in BESV2, tags.cfg is changed and engine reloaded. Broker updates the DB. Several tags are removed and Broker updates correctly the DB.
#	[Tags]	Broker	Engine	protobuf	bbdo	tags
#	Config Engine	${1}
#	Engine Config Set Value	${0}	log_legacy_enabled	${0}
#	Engine Config Set Value	${0}	log_v2_enabled	${1}
#	Engine Config Set Value	${0}	log_level_config	debug
#	Create Tags File	${20}
#	Config Engine Add Cfg File	tags.cfg
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
#	${result}=	check tag With Timeout	tag20	4	30
#	Should Be True	${result}	msg=tag20 should be of type 4
#	${result}=	check tag With Timeout	tag1	1	30
#	Should Be True	${result}	msg=tag1 should be of type 1
#
#	Create Tags File	${20}	2
#	Reload Engine
#	Reload Broker
#	${result}=	check tag With Timeout	tag21	4	30
#	Should Be True	${result}	msg=tag21 should be of type 4
#	${result}=	check tag With Timeout	tag2	1	30
#	Should Be True	${result}	msg=tag2 should be of type 1
#
#	Create Tags File	${10}
#	Reload Engine
#	Reload Broker
#	${result}=	check tag With Timeout	tag10	2	30
#	Should Be True	${result}	msg=tag21 should be of type 2
#	${result}=	check tag With Timeout	tag1	1	30
#	Should Be True	${result}	msg=tag2 should be of type 1
#
#	log to console	Waiting for 10 tags now.
#	# Now, we should have 10 tags
#	${result}=	Check Tags Count	10	120
#	Should Be True	${result}	msg=We should only have 10 tags.
#
#	Stop Engine
#	Stop Broker

BEUTAG3
	[Documentation]	Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.resources_tags table. Engine is started before.
	[Tags]	Broker	Engine	protobuf	bbdo	tags
	#Clear DB	tags
	Config Engine	${1}
	Create Tags File	${20}
	Config Engine Add Cfg File	tags.cfg
	Add Tags To Services	4,8,12	[1, 2, 3, 4]
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
	${result}=	check service tags With Timeout	1	1	8	60
	Should Be True	${result}	msg=Service (1, 1) should have tag_id=8
	Stop Engine
	Stop Broker

BEUTAG4
	[Documentation]	Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.resources_tags table. Engine is started before.
	[Tags]	Broker	Engine	protobuf	bbdo	tags
	#Clear DB	tags
	Config Engine	${1}
	Create Tags File	${20}
	Config Engine Add Cfg File	tags.cfg
	Add Tags To Host	3,7,11	[1, 2, 3, 4]
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
	${result}=	check host tags With Timeout	1	1	7	60
	Should Be True	${result}	msg=Host (1, 1) should have tag_id=7
	Stop Engine
	Stop Broker

BEUTAG5
	[Documentation]	Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.resources_tags table. Engine is started before.
	[Tags]	Broker	Engine	protobuf	bbdo	tags
	#Clear DB	tags
	Config Engine	${1}
	Create Tags File	${20}
	Config Engine Add Cfg File	tags.cfg
	Add Tags To Host	3,7,11	[1, 2, 3, 4]
	Add Tags To Services	4,8,12	[1, 2, 3, 4]
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
	${result}=	check host tags With Timeout	1	1	7	60
	Should Be True	${result}	msg=Host (1, 1) should have tag_id=7
	${result}=	check service tags With Timeout	1	1	8	60
	Should Be True	${result}	msg=Service (1, 1) should have tag_id=8
	Stop Engine
	Stop Broker
