*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes

Documentation	Centreon Broker RRD metric deletion
Library  DatabaseLibrary
Library	Process
Library	OperatingSystem
Library	DateTime
Library	Collections
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py

*** Test Cases ***
BRRDDM1
	[Documentation]	RRD metric deletion on table metric
	[Tags]	RRD metric deletion on table metric
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module
	Broker Config Log	rrd	rrd	debug
	Broker Config Log	rrd	core	error

	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected
	${metric}=	Get Metric To Delete
	Log To Console	metric to delete ${metric}

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	Log To Console	after connection
	Execute Sql String	UPDATE metrics SET to_delete=1 WHERE metric_id=${metric}

	Reload Broker
	Sleep  6m

	Stop Broker
	Stop Engine

	${content1}=	Create List	remove graph request for metric ${metric}
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-rrd-master.log
	${result}=	Find In Log	${log}	${start}	${content1}
	Should Be True	${result}
	File Should Not Exist	/var/lib/centreon/metrics/${metric}.rrd

BRRDDID1
	[Documentation]	RRD metric deletion on table index_data
	[Tags]	RRD metric deletion on table index_data
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module
	Broker Config Log	rrd	rrd	debug
	Broker Config Log	rrd	core	error

	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected

	${metric}=	Get Metric To Delete
	Log To Console	metric to delete ${metric}

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	Log To Console	after connection
	Execute Sql String	UPDATE index_data i LEFT JOIN metrics m ON i.id=m.index_id SET i.to_delete=1 WHERE m.metric_id=${metric}

	Reload Broker
	Sleep  6m

	Stop Broker
	Stop Engine

	${content1}=	Create List	remove graph request for metric ${metric}
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-rrd-master.log
	${result}=	Find In Log	${log}	${start}	${content1}
	Should Be True	${result}
	File Should Not Exist	/var/lib/centreon/metrics/${metric}.rrd

BRRDDMID1
	[Documentation]	RRD metric deletion on table metric and index_data
	[Tags]	RRD metric deletion on table metric and index_data
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module
	Broker Config Log	rrd	rrd	debug
	Broker Config Log	rrd	core	error

	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected

	${metric}=	Get Metric To Delete
	Log To Console	metric to delete ${metric}

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	Log To Console	after connection
	Execute Sql String	UPDATE index_data i LEFT JOIN metrics m ON i.id=m.index_id SET i.to_delete=1, m.to_delete=1 WHERE m.metric_id=${metric}

	Reload Broker
	Sleep  6m

	Stop Broker
	Stop Engine

	${content1}=	Create List	remove graph request for metric ${metric}
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-rrd-master.log
	${result}=	Find In Log	${log}	${start}	${content1}
	Should Be True	${result}
	File Should Not Exist	/var/lib/centreon/metrics/${metric}.rrd

*** Variables ***
${DBName}	centreon_storage
${DBHost}	localhost
${DBUser}	centreon
${DBPass}	centreon
${DBPort}	3306
