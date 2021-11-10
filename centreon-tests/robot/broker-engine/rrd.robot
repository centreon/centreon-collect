*** Settings ***
Resource	../ressources/ressources.robot
Suite Setup	Clean Before Test
Suite Teardown	Clean After Test

Documentation	Centreon Broker RRD metric deletion
Library  DatabaseLibrary
Library	Process
Library	OperatingSystem
Library	DateTime
Library	Collections
Library	../engine/Engine.py
Library	../broker/Broker.py
Library	Common.py

*** Test cases ***
BRRDDM1: RRD metric deletion
	[Tags]	Broker	Engine	compression	tcp
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module
	# Broker Config Log	central	perfdata	debug
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
	
	Sighup Broker
	Sleep  6m
	
	Stop Broker
	Stop Engine

	${content1}=	Create List	remove graph request for metric ${metric}
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-rrd-master.log
	${result}=	Find In Log	${log}	${start}	${content1}
	Should Be True	${result}


*** Keywords ***

Check Connections
	${count}=	Get Engines Count
	${pid1}=	Get Process Id	b1
	FOR	${idx}	IN RANGE	0	${count}
		${alias}=	Catenate	SEPARATOR=	e	${idx}
		${pid2}=	Get Process Id	${alias}
		${retval}=	Check Connection	5669	${pid1}	${pid2}
		Return from Keyword If	${retval} == ${False}	${False}
	END
	${pid2}=	Get Process Id	b2
	${retval}=	Check Connection	5670	${pid1}	${pid2}
	[Return]	${retval}

*** Variables ***
&{ext}	yes=COMPRESSION	no=	auto=COMPRESSION
@{choices}	yes	no	auto
${DBName}	centreon_storage
${DBHost}	localhost
${DBUser}	centreon
${DBPass}	centreon
${DBPort}	3306
