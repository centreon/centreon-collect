*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes

Documentation	Centreon Broker and Engine add Hostgroup
Library	DatabaseLibrary
Library	Process
Library	OperatingSystem
Library	DateTime
Library	Collections
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py

*** Test Cases ***
LOGV2EB1
	[Documentation]	log-v2 enabled  hold log disabled check broker sink
	[Tags]	Broker	Engine	log-v2 sink broker
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module
	Engine Config Set Value	${0}	log_legacy_enabled	${0}
	Engine Config Set Value	${0}	log_v2_enabled	${1}

	${start}=	Get Current Date	 UTC	exclude_millis=yes
	${time_stamp}    Convert Date    ${start}    epoch	exclude_millis=yes
    ${time_stamp2}    evaluate    int(${time_stamp})

	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected

	${pid}=	Get Process Id	e0
	${content}=	Create List	[process] [info] [${pid}] Configuration loaded, main loop starting.

	${log}=	Catenate	SEPARATOR=	${ENGINE_LOG}	/config0/centengine.log
	${result1}=	Find In Log With Timeout	${log}	${start}	${content}	15	
	Should Be True	${result1}

	Sleep	5s

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	Log To Console	after connection
	${output}=	Query	select count(*) from logs where output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2};
	Log To Console	${output}
	Should Be Equal As Strings	${output}	((1,),)
	Stop Engine
	Stop Broker

LOGV2DB1
	[Documentation]	log-v2 disabled hold log enabled check broker sink
	[Tags]	Broker	Engine	log-v2 sink broker
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module
	Engine Config Set Value	${0}	log_legacy_enabled	${1}
	Engine Config Set Value	${0}	log_v2_enabled	${0}

	${start}=	Get Current Date	 UTC	exclude_millis=yes
	${time_stamp}    Convert Date    ${start}    epoch	exclude_millis=yes
    ${time_stamp2}    evaluate    int(${time_stamp})

	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected

	${pid}=	Get Process Id	e0
	${content_v2}=	Create List	[process] [info] [${pid}] Configuration loaded, main loop starting.
	${content_hold}=	Create List	[${pid}] Configuration loaded, main loop starting.

	${log}=	Catenate	SEPARATOR=	${ENGINE_LOG}	/config0/centengine.log
	${result1}=	Find In Log With Timeout	${log}	${start}	${content_v2}	15	
	${result2}=	Find In Log With Timeout	${log}	${start}	${content_hold}	15
	Should Not Be True	${result1}
	Should Be True	${result2}	
	
	Sleep	10s

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	Log To Console	after connection
	${output}=	Query	select count(*) from logs where output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2};
	Log To Console	${output}
	Should Be Equal As Strings	${output}	((1,),)
	Stop Engine
	Stop Broker

LOGV2DB2
	[Documentation]	log-v2 disabled hold log disabled check broker sink
	[Tags]	Broker	Engine	log-v2 sink broker
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module
	Engine Config Set Value	${0}	log_legacy_enabled	${0}
	Engine Config Set Value	${0}	log_v2_enabled	${0}

	${start}=	Get Current Date	 UTC	exclude_millis=yes
	${time_stamp}    Convert Date    ${start}    epoch	exclude_millis=yes
    ${time_stamp2}    evaluate    int(${time_stamp})
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected

	${pid}=	Get Process Id	e0
	${content_v2}=	Create List	[process] [info] [${pid}] Configuration loaded, main loop starting.
	${content_hold}=	Create List	[${pid}] Configuration loaded, main loop starting.

	${log}=	Catenate	SEPARATOR=	${ENGINE_LOG}	/config0/centengine.log
	${result1}=	Find In Log With Timeout	${log}	${start}	${content_v2}	15	
	${result2}=	Find In Log With Timeout	${log}	${start}	${content_hold}	15
	Should Not Be True	${result1}
	Should Not Be True	${result2}

	Sleep	10s
	
	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	Log To Console	after connection
	${output}=	Query	select count(*) from logs where output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2};
	Log To Console	${output}
	Should Be Equal As Strings	${output}	((0,),)
	Stop Engine
	Stop Broker

LOGV2EB2
	[Documentation]	log-v2 enabled hold log enabled check broker sink
	[Tags]	Broker	Engine	log-v2	sinkbroker
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module
	Engine Config Set Value	${0}	log_legacy_enabled	${1}
	Engine Config Set Value	${0}	log_v2_enabled	${1}

	${start}=	Get Current Date	 UTC	exclude_millis=yes
	${time_stamp}    Convert Date    ${start}    epoch	exclude_millis=yes
    ${time_stamp2}    evaluate    int(${time_stamp})
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected

	${pid}=	Get Process Id	e0
	${content_v2}=	Create List	[process] [info] [${pid}] Configuration loaded, main loop starting.
	${content_hold}=	Create List	[${pid}] Configuration loaded, main loop starting.

	${log}=	Catenate	SEPARATOR=	${ENGINE_LOG}	/config0/centengine.log
	${result1}=	Find In Log With Timeout	${log}	${start}	${content_v2}	15	
	${result2}=	Find In Log With Timeout	${log}	${start}	${content_hold}	15
	Should Be True	${result1}
	Should Be True	${result2}

	Sleep	10s

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	Log To Console	after connection
	${output}=	Query	select count(*) from logs where output="Configuration loaded, main loop starting." AND ctime>=${time_stamp2};
	Log To Console	${output}
	Should Be Equal As Strings	${output}	((2,),)

	Stop Engine
	Stop Broker

LOGV2EF1
	[Documentation]	log-v2 enabled  hold log disabled check logfile sink
	[Tags]	Broker	Engine	log-v2
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module
	Engine Config Set Value	${0}	log_legacy_enabled	${0}
	Engine Config Set Value	${0}	log_v2_enabled	${1}

	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected
	${pid}=	Get Process Id	e0
	${content_v2}=	Create List	[process] [info] [${pid}] Configuration loaded, main loop starting.

	${log}=	Catenate	SEPARATOR=	${ENGINE_LOG}	/config0/centengine.log
	${result1}=	Find In Log With Timeout	${log}	${start}	${content_v2}	15	
	Should Be True	${result1}
	Stop Engine
	Stop Broker

LOGV2DF1
	[Documentation]	log-v2 disabled hold log enabled check logfile sink
	[Tags]	Broker	Engine	log-v2
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module
	Engine Config Set Value	${0}	log_legacy_enabled	${1}
	Engine Config Set Value	${0}	log_v2_enabled	${0}

	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected
	${pid}=	Get Process Id	e0
	${content_hold}=	Create List	[${pid}] Configuration loaded, main loop starting.
	${content_v2}=	Create List	[process] [info] [${pid}] Configuration loaded, main loop starting.

	${log}=	Catenate	SEPARATOR=	${ENGINE_LOG}	/config0/centengine.log
	${result1}=	Find In Log With Timeout	${log}	${start}	${content_hold}	15	
	${result2}=	Find In Log With Timeout	${log}	${start}	${content_v2}	15	
	Should Be True	${result1}
	Should Not Be True	${result2}
	Stop Engine
	Stop Broker

LOGV2DF2
	[Documentation]	log-v2 disabled hold log disabled check logfile sink
	[Tags]	Broker	Engine	log-v2
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module
	Engine Config Set Value	${0}	log_legacy_enabled	${0}
	Engine Config Set Value	${0}	log_v2_enabled	${0}

	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected
	${pid}=	Get Process Id	e0
	${content_v2}=	Create List	[process] [info] [${pid}] Configuration loaded, main loop starting.
	${content_hold}=	Create List	[${pid}] Configuration loaded, main loop starting.

	${log}=	Catenate	SEPARATOR=	${ENGINE_LOG}	/config0/centengine.log
	${result1}=	Find In Log With Timeout	${log}	${start}	${content_v2}	15	
	${result2}=	Find In Log With Timeout	${log}	${start}	${content_hold}	15
	Should Not Be True	${result1}
	Should Not Be True	${result2}	
	Stop Engine
	Stop Broker

LOGV2EF2
	[Documentation]	log-v2 enabled hold log enabled check logfile sink
	[Tags]	Broker	Engine	log-v2
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module
	Engine Config Set Value	${0}	log_legacy_enabled	${1}
	Engine Config Set Value	${0}	log_v2_enabled	${1}

	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected
	${pid}=	Get Process Id	e0
	${content_v2}=	Create List	[process] [info] [${pid}] Configuration loaded, main loop starting.
	${content_hold}=	Create List	[${pid}] Configuration loaded, main loop starting.

	${log}=	Catenate	SEPARATOR=	${ENGINE_LOG}	/config0/centengine.log
	${result1}=	Find In Log With Timeout	${log}	${start}	${content_v2}	15	
	${result2}=	Find In Log With Timeout	${log}	${start}	${content_hold}	15
	Should Be True	${result1}
	Should Be True	${result2}	
	Stop Engine
	Stop Broker

*** Variables ***
${DBName}	centreon_storage
${DBHost}	localhost
${DBUser}	centreon
${DBPass}	centreon
${DBPort}	3306
