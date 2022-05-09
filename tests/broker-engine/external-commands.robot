*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes

Documentation	Centreon Broker and Engine progressively add services
Library	DatabaseLibrary
Library	Process
Library	OperatingSystem
Library	DateTime
Library	Collections
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py

*** Test Cases ***
BEEXTCMD1
	[Documentation]	external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo3.0
	[Tags]	Broker	Engine	services	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Add Item	module0	bbdo_version	3.0.0
	Broker Config Add Item	central	bbdo_version	3.0.0
	Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	sql	debug
	Config Broker Sql Output	central	unified_sql
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
	${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
	Change Normal Svc Check Interval	host_1	service_1	10

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

	FOR	${index}	IN RANGE	30
		Log To Console	select s.check_interval from services s,hosts h where s.description='service_1' and h.name='host_1' 
		${output}=	Query	select s.check_interval from services s,hosts h where s.description='service_1' and h.name='host_1'
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "((10.0,),)"
	END
	Should Be Equal As Strings	${output}	((10.0,),)
	Stop Engine
	Kindly Stop Broker

BEEXTCMD2
	[Documentation]	external command CHANGE_NORMAL_SVC_CHECK_INTERVAL on bbdo2.0
	[Tags]	Broker	Engine	services	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	sql	debug
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
	${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
	Change Normal Svc Check Interval	host_1	service_1	15

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

	FOR	${index}	IN RANGE	30
		Log To Console	select s.check_interval from services s,hosts h where s.description='service_1' and h.name='host_1' 
		${output}=	Query	select s.check_interval from services s,hosts h where s.description='service_1' and h.name='host_1'
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "((15.0,),)"
	END
	Should Be Equal As Strings	${output}	((15.0,),)
	Stop Engine
	Kindly Stop Broker

BEEXTCMD3
	[Documentation]	external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo3.0
	[Tags]	Broker	Engine	host	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Add Item	module0	bbdo_version	3.0.0
	Broker Config Add Item	central	bbdo_version	3.0.0
	Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	core	error
	Broker Config Log	central	sql	debug
	Broker Config Log	module0	neb	trace
	Config Broker Sql Output	central	unified_sql
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
	${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
	Change Normal Host Check Interval	host_1	10

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

	FOR	${index}	IN RANGE	30
		Log To Console	select check_interval from hosts where name='host_1' 
		${output}=	Query	select check_interval from hosts where name='host_1' 
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "((10.0,),)"
	END
	Should Be Equal As Strings	${output}	((10.0,),)
	Stop Engine
	Kindly Stop Broker

BEEXTCMD4
	[Documentation]	external command CHANGE_NORMAL_HOST_CHECK_INTERVAL on bbdo2.0
	[Tags]	Broker	Engine	host	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	core	error
	Broker Config Log	central	sql	debug
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
	${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
	Change Normal Host Check Interval	host_1	15

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

	FOR	${index}	IN RANGE	30
		Log To Console	select check_interval from hosts where name='host_1' 
		${output}=	Query	select check_interval from hosts where name='host_1' 
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "((15.0,),)"
	END
	Should Be Equal As Strings	${output}	((15.0,),)
	Stop Engine
	Kindly Stop Broker




BEEXTCMD5
	[Documentation]	external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo3.0
	[Tags]	Broker	Engine	services	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Add Item	module0	bbdo_version	3.0.0
	Broker Config Add Item	central	bbdo_version	3.0.0
	Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	sql	debug
	Config Broker Sql Output	central	unified_sql
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
	${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
	Change Retry Svc Check Interval	host_1	service_1	15

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

	FOR	${index}	IN RANGE	30
		Log To Console	select s.retry_interval from services s,hosts h where s.description='service_1' and h.name='host_1' 
		${output}=	Query	select s.retry_interval from services s,hosts h where s.description='service_1' and h.name='host_1'
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "((15.0,),)"
	END
	Should Be Equal As Strings	${output}	((15.0,),)
	Stop Engine
	Kindly Stop Broker

BEEXTCMD6
	[Documentation]	external command CHANGE_RETRY_SVC_CHECK_INTERVAL on bbdo2.0
	[Tags]	Broker	Engine	services	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	sql	debug
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
	${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
	Change Retry Svc Check Interval	host_1	service_1	10

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

	FOR	${index}	IN RANGE	30
		Log To Console	select s.retry_interval from services s,hosts h where s.description='service_1' and h.name='host_1' 
		${output}=	Query	select s.retry_interval from services s,hosts h where s.description='service_1' and h.name='host_1'
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "((10.0,),)"
	END
	Should Be Equal As Strings	${output}	((10.0,),)
	Stop Engine
	Kindly Stop Broker

BEEXTCMD7
	[Documentation]	external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo3.0
	[Tags]	Broker	Engine	host	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Add Item	module0	bbdo_version	3.0.0
	Broker Config Add Item	central	bbdo_version	3.0.0
	Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	core	error
	Broker Config Log	central	sql	debug
	Broker Config Log	module0	neb	trace
	Config Broker Sql Output	central	unified_sql
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
	${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
	Change Retry Host Check Interval	host_1	15

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

	FOR	${index}	IN RANGE	30
		Log To Console	select retry_interval from hosts where name='host_1' 
		${output}=	Query	select retry_interval from hosts where name='host_1' 
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "((15.0,),)"
	END
	Should Be Equal As Strings	${output}	((15.0,),)
	Stop Engine
	Kindly Stop Broker

BEEXTCMD8
	[Documentation]	external command CHANGE_RETRY_HOST_CHECK_INTERVAL on bbdo2.0
	[Tags]	Broker	Engine	host	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	core	error
	Broker Config Log	central	sql	debug
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
	${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
	Change Retry Host Check Interval	host_1	10

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

	FOR	${index}	IN RANGE	30
		Log To Console	select retry_interval from hosts where name='host_1' 
		${output}=	Query	select retry_interval from hosts where name='host_1' 
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "((10.0,),)"
	END
	Should Be Equal As Strings	${output}	((10.0,),)
	Stop Engine
	Kindly Stop Broker

BEEXTCMD9
	[Documentation]	external command CHANGE_MAX_SVC_CHECK_ATTEMPTS on bbdo3.0
	[Tags]	Broker	Engine	services	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Add Item	module0	bbdo_version	3.0.0
	Broker Config Add Item	central	bbdo_version	3.0.0
	Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	sql	debug
	Config Broker Sql Output	central	unified_sql
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
	${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
	Change Max Svc Check Attempts	host_1	service_1	15

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

	FOR	${index}	IN RANGE	30
		Log To Console	select s.max_check_attempts from services s,hosts h where s.description='service_1' and h.name='host_1' 
		${output}=	Query	select s.max_check_attempts from services s,hosts h where s.description='service_1' and h.name='host_1'
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "((15,),)"
	END
	Should Be Equal As Strings	${output}	((15,),)

	FOR	${index}	IN RANGE	30
		Log To Console	select max_check_attempts from resources where name='service_1' and parent_name='host_1' 
		${output}=	Query	select max_check_attempts from resources where name='service_1' and parent_name='host_1'
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "((15,),)"
	END
	Should Be Equal As Strings	${output}	((15,),)
	Stop Engine
	Kindly Stop Broker

BEEXTCMD10
	[Documentation]	external command CHANGE_MAX_SVC_CHECK_ATTEMPTS on bbdo2.0
	[Tags]	Broker	Engine	services	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	sql	debug
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
	${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
	Change Max Svc Check Attempts	host_1	service_1	10

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

	FOR	${index}	IN RANGE	30
		Log To Console	select s.max_check_attempts from services s,hosts h where s.description='service_1' and h.name='host_1' 
		${output}=	Query	select s.max_check_attempts from services s,hosts h where s.description='service_1' and h.name='host_1'
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "((10,),)"
	END
	Should Be Equal As Strings	${output}	((10,),)
	Stop Engine
	Kindly Stop Broker

BEEXTCMD11
	[Documentation]	external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo3.0
	[Tags]	Broker	Engine	host	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Add Item	module0	bbdo_version	3.0.0
	Broker Config Add Item	central	bbdo_version	3.0.0
	Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	core	error
	Broker Config Log	central	sql	debug
	Broker Config Log	module0	neb	trace
	Config Broker Sql Output	central	unified_sql
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
	${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
	Change Max Host Check Attempts	host_1	15

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

	FOR	${index}	IN RANGE	30
		Log To Console	select max_check_attempts from hosts where name='host_1' 
		${output}=	Query	select max_check_attempts from hosts where name='host_1' 
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "((15,),)"
	END
	Should Be Equal As Strings	${output}	((15,),)

	FOR	${index}	IN RANGE	30
		Log To Console	select max_check_attempts from resources where name='host_1' 
		${output}=	Query	select max_check_attempts from resources where name='host_1' 
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "((15,),)"
	END
	Should Be Equal As Strings	${output}	((15,),)
	Stop Engine
	Kindly Stop Broker

BEEXTCMD12
	[Documentation]	external command CHANGE_MAX_HOST_CHECK_ATTEMPTS on bbdo2.0
	[Tags]	Broker	Engine	host	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	core	error
	Broker Config Log	central	sql	debug
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
	${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
	Change Max Host Check Attempts	host_1	10

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

	FOR	${index}	IN RANGE	30
		Log To Console	select max_check_attempts from hosts where name='host_1' 
		${output}=	Query	select max_check_attempts from hosts where name='host_1' 
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "((10,),)"
	END
	Should Be Equal As Strings	${output}	((10,),)

	Stop Engine
	Kindly Stop Broker

BEEXTCMD13
	[Documentation]	external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo3.0
	[Tags]	Broker	Engine	host	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Add Item	module0	bbdo_version	3.0.0
	Broker Config Add Item	central	bbdo_version	3.0.0
	Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	core	error
	Broker Config Log	central	sql	debug
	Broker Config Log	module0	neb	trace
	Config Broker Sql Output	central	unified_sql
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
	${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
	Change Host Check Timeperiod	host_1	24x6

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

	FOR	${index}	IN RANGE	30
		Log To Console	select check_period from hosts where name='host_1' 
		${output}=	Query	select check_period from hosts where name='host_1' 
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "(('24x6',),)"
	END
	Should Be Equal As Strings	${output}	(('24x6',),)

	Stop Engine
	Kindly Stop Broker

BEEXTCMD14
	[Documentation]	external command CHANGE_HOST_CHECK_TIMEPERIOD on bbdo2.0
	[Tags]	Broker	Engine	host	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	core	error
	Broker Config Log	central	sql	debug
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
	${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
	Change Host Check Timeperiod	host_1	24x7

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

	FOR	${index}	IN RANGE	30
		Log To Console	select check_period from hosts where name='host_1' 
		${output}=	Query	select check_period from hosts where name='host_1' 
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "(('24x7',),)"
	END
	Should Be Equal As Strings	${output}	(('24x7',),)

	Stop Engine
	Kindly Stop Broker

BEEXTCMD15
	[Documentation]	external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo3.0
	[Tags]	Broker	Engine	host	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Add Item	module0	bbdo_version	3.0.0
	Broker Config Add Item	central	bbdo_version	3.0.0
	Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	core	error
	Broker Config Log	central	sql	debug
	Broker Config Log	module0	neb	trace
	Config Broker Sql Output	central	unified_sql
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
	${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
	Change Host Notification Timeperiod	host_1	24x7

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

	FOR	${index}	IN RANGE	30
		Log To Console	select notification_period from hosts where name='host_1' 
		${output}=	Query	select notification_period from hosts where name='host_1' 
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "(('24x7',),)"
	END
	Should Be Equal As Strings	${output}	(('24x7',),)

	Stop Engine
	Kindly Stop Broker

BEEXTCMD16
	[Documentation]	external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo2.0
	[Tags]	Broker	Engine	host	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	core	error
	Broker Config Log	central	sql	debug
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
	${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
	Change Host Notification Timeperiod	host_1	24x6

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

	FOR	${index}	IN RANGE	30
		Log To Console	select notification_period from hosts where name='host_1' 
		${output}=	Query	select notification_period from hosts where name='host_1' 
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "(('24x6',),)"
	END
	Should Be Equal As Strings	${output}	(('24x6',),)

	Stop Engine
	Kindly Stop Broker

BEEXTCMD17
	[Documentation]	external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo3.0
	[Tags]	Broker	Engine	host	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Add Item	module0	bbdo_version	3.0.0
	Broker Config Add Item	central	bbdo_version	3.0.0
	Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	core	error
	Broker Config Log	central	sql	debug
	Broker Config Log	module0	neb	trace
	Config Broker Sql Output	central	unified_sql
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
	${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
	Change Svc Check Timeperiod	host_1	service_1	24x6

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

	FOR	${index}	IN RANGE	30
		Log To Console	select s.check_period from services s,hosts h where s.description='service_1' and h.name='host_1'
		${output}=	Query	select s.check_period from services s,hosts h where s.description='service_1' and h.name='host_1' 
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "(('24x6',),)"
	END
	Should Be Equal As Strings	${output}	(('24x6',),)

	Stop Engine
	Kindly Stop Broker

BEEXTCMD18
	[Documentation]	external command CHANGE_SVC_CHECK_TIMEPERIOD on bbdo2.0
	[Tags]	Broker	Engine	host	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	core	error
	Broker Config Log	central	sql	debug
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
	${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
	Change Svc Check Timeperiod	host_1	service_1	24x7

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

	FOR	${index}	IN RANGE	30
		Log To Console	select s.check_period from services s,hosts h where s.description='service_1' and h.name='host_1'
		${output}=	Query	select s.check_period from services s,hosts h where s.description='service_1' and h.name='host_1' 
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "(('24x7',),)"
	END
	Should Be Equal As Strings	${output}	(('24x7',),)

	Stop Engine
	Kindly Stop Broker

BEEXTCMD19
	[Documentation]	external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo3.0
	[Tags]	Broker	Engine	host	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Add Item	module0	bbdo_version	3.0.0
	Broker Config Add Item	central	bbdo_version	3.0.0
	Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	core	error
	Broker Config Log	central	sql	debug
	Broker Config Log	module0	neb	trace
	Config Broker Sql Output	central	unified_sql
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
	${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
	Change Svc Notification Timeperiod	host_1	service_1	24x7

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

	FOR	${index}	IN RANGE	30
		Log To Console	select s.notification_period from services s,hosts h where s.description='service_1' and h.name='host_1'
		${output}=	Query	select s.notification_period from services s,hosts h where s.description='service_1' and h.name='host_1'
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "(('24x7',),)"
	END
	Should Be Equal As Strings	${output}	(('24x7',),)

	Stop Engine
	Kindly Stop Broker

BEEXTCMD20
	[Documentation]	external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo2.0
	[Tags]	Broker	Engine	host	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	core	error
	Broker Config Log	central	sql	debug
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
	${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
	Change Svc Notification Timeperiod	host_1	service_1	24x6

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

	FOR	${index}	IN RANGE	30
		Log To Console	select s.notification_period from services s,hosts h where s.description='service_1' and h.name='host_1' 
		${output}=	Query	select s.notification_period from services s,hosts h where s.description='service_1' and h.name='host_1' 
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "(('24x6',),)"
	END
	Should Be Equal As Strings	${output}	(('24x6',),)

	Stop Engine
	Kindly Stop Broker

BEEXTCMD21
	[Documentation]	external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo3.0
	[Tags]	Broker	Engine	host	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Add Item	module0	bbdo_version	3.0.0
	Broker Config Add Item	central	bbdo_version	3.0.0
	Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	core	error
	Broker Config Log	central	sql	debug
	Broker Config Log	module0	neb	trace
	Config Broker Sql Output	central	unified_sql
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
	${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
	Change Svc Notification Timeperiod	host_1	service_1	24x7

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

	FOR	${index}	IN RANGE	30
		Log To Console	select s.notification_period from services s,hosts h where s.description='service_1' and h.name='host_1'
		${output}=	Query	select s.notification_period from services s,hosts h where s.description='service_1' and h.name='host_1'
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "(('24x7',),)"
	END
	Should Be Equal As Strings	${output}	(('24x7',),)

	Stop Engine
	Kindly Stop Broker

BEEXTCMD22
	[Documentation]	external command CHANGE_SVC_NOTIFICATION_TIMEPERIOD on bbdo2.0
	[Tags]	Broker	Engine	host	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	core	error
	Broker Config Log	central	sql	debug
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
	${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
	Change Svc Notification Timeperiod	host_1	service_1	24x6

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

	FOR	${index}	IN RANGE	30
		Log To Console	select s.notification_period from services s,hosts h where s.description='service_1' and h.name='host_1' 
		${output}=	Query	select s.notification_period from services s,hosts h where s.description='service_1' and h.name='host_1' 
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "(('24x6',),)"
	END
	Should Be Equal As Strings	${output}	(('24x6',),)

	Stop Engine
	Kindly Stop Broker

BEEXTCMD23
	[Documentation]	external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo3.0
	[Tags]	Broker	Engine	host	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Add Item	module0	bbdo_version	3.0.0
	Broker Config Add Item	central	bbdo_version	3.0.0
	Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	core	error
	Broker Config Log	central	sql	debug
	Broker Config Log	module0	neb	trace
	Config Broker Sql Output	central	unified_sql
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
	${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
	Change Host Notification Timeperiod	host_1	24x7

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

	FOR	${index}	IN RANGE	30
		Log To Console	select notification_period from hosts where name='host_1' 
		${output}=	Query	select notification_period from hosts where name='host_1' 
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "(('24x7',),)"
	END
	Should Be Equal As Strings	${output}	(('24x7',),)

	Stop Engine
	Kindly Stop Broker

BEEXTCMD24
	[Documentation]	external command CHANGE_HOST_NOTIFICATION_TIMEPERIOD on bbdo2.0
	[Tags]	Broker	Engine	host	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	core	error
	Broker Config Log	central	sql	debug
	Clear Retention
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
	${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
	Change Host Notification Timeperiod	host_1	24x6

	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

	FOR	${index}	IN RANGE	30
		Log To Console	select notification_period from hosts where name='host_1' 
		${output}=	Query	select notification_period from hosts where name='host_1' 
		Log To Console	${output}
		Sleep	1s
		EXIT FOR LOOP IF	"${output}" == "(('24x6',),)"
	END
	Should Be Equal As Strings	${output}	(('24x6',),)

	Stop Engine
	Kindly Stop Broker

BEEXTCMD25
	[Documentation]	external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo3.0
	[Tags]	Broker	Engine	host	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Add Item	module0	bbdo_version	3.0.0
	Broker Config Add Item	central	bbdo_version	3.0.0
	Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	core	error
	Broker Config Log	central	sql	debug
	Broker Config Log	module0	neb	trace
	Config Broker Sql Output	central	unified_sql
	Clear Retention
	FOR	${use_grpc}	IN RANGE	0  1
		${start}=	Get Current Date
		Start Broker
		Start Engine
		${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
		${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
		Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
		Disable Host And child Notifications	${use_grpc}	host_1

		Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

		FOR	${index}	IN RANGE	30
			Log To Console	select notify from hosts where name='host_1' 
			${output}=	Query	select notify from hosts where name='host_1'  
			Log To Console	${output}
			Sleep	1s
			EXIT FOR LOOP IF	"${output}" == "((0,),)"
		END
		Should Be Equal As Strings	${output}	((0,),)

		FOR	${index}	IN RANGE	30
			Log To Console	select notifications_enabled from resources where name='host_1' 
			${output}=	Query	select notifications_enabled from resources where name='host_1' 
			Log To Console	${output}
			Sleep	1s
			EXIT FOR LOOP IF	"${output}" == "((0,),)"
		END
		Should Be Equal As Strings	${output}	((0,),)

		Stop Engine
		Kindly Stop Broker
	END

BEEXTCMD26
	[Documentation]	external command DISABLE_HOST_AND_CHILD_NOTIFICATIONS on bbdo2.0
	[Tags]	Broker	Engine	host	extcmd
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	core	error
	Broker Config Log	central	sql	debug
	Clear Retention
	FOR	${use_grpc}	IN RANGE	0  1
		${start}=	Get Current Date
		Start Broker
		Start Engine
		${content}=	Create List	INITIAL SERVICE STATE: host_1;service_1;
		${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
		Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.
		Disable Host And child Notifications	${use_grpc}	host_1

		Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}

		FOR	${index}	IN RANGE	30
			Log To Console	select notify from hosts where name='host_1' 
			${output}=	Query	select notify from hosts where name='host_1'  
			Log To Console	${output}
			Sleep	1s
			EXIT FOR LOOP IF	"${output}" == "((0,),)"
		END
		Should Be Equal As Strings	${output}	((0,),)

		FOR	${index}	IN RANGE	30
			Log To Console	select notifications_enabled from resources where name='host_1' 
			${output}=	Query	select notifications_enabled from resources where name='host_1' 
			Log To Console	${output}
			Sleep	1s
			EXIT FOR LOOP IF	"${output}" == "((0,),)"
		END
		Should Be Equal As Strings	${output}	((0,),)

		Stop Engine
		Kindly Stop Broker
	END

