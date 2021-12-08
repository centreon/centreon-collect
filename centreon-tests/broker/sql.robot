*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes

Documentation	Centreon Broker Mariadb access
Library	Process
Library	OperatingSystem
Library	DateTime
Library	../resources/Broker.py
Library	../resources/Engine.py
Library	../resources/Common.py

*** Test Cases ***
BDB1
	[Documentation]	Access denied when database name exists but is not the good one for sql output
	[Tags]	Broker	sql
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Broker Config Output set	central	central-broker-master-sql	db_name	centreon
	FOR	${i}	IN RANGE	0	5
	 ${start}=	Get Current Date
	 Start Broker
	 ${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	 ${content}=	Create List	Table 'centreon.instances' doesn't exist
	 ${result}=	Find In Log with timeout	${log}	${start}	${content}	30
	 Should Be True	${result}
	 Stop Broker
	END

BDB2
	[Documentation]	Access denied when database name exists but is not the good one for storage output
	[Tags]	Broker	sql
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Broker Config Output set	central	central-broker-master-perfdata	db_name	centreon
	FOR	${i}	IN RANGE	0	5
	 ${start}=	Get Current Date
	 Start Broker
	 ${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	 ${content}=	Create List	Unable to connect to the database
	 ${result}=	Find In Log with timeout	${log}	${start}	${content}	20
	 Should Be True	${result}
	 Stop Broker
	END

BDB3
	[Documentation]	Access denied when database name does not exist for sql output
	[Tags]	Broker	sql
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Broker Config Output set	central	central-broker-master-sql	db_name	centreon1
	FOR	${i}	IN RANGE	0	5
	 ${start}=	Get Current Date
	 Start Broker
	 ${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	 ${content}=	Create List	global error: mysql_connection: error while starting connection
	 ${result}=	Find In Log with timeout	${log}	${start}	${content}	30
	 Should Be True	${result}
	 Stop Broker
	END

BDB4
	[Documentation]	Access denied when database name does not exist for storage output
	[Tags]	Broker	sql
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Broker Config Output set	central	central-broker-master-perfdata	db_name	centreon1
	FOR	${i}	IN RANGE	0	5
	 ${start}=	Get Current Date
	 Start Broker
	 ${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	 ${content}=	Create List	Unable to connect to the database
	 ${result}=	Find In Log with timeout	${log}	${start}	${content}	20
	 Should Be True	${result}
	 Stop Broker
	END

BDB5
	[Documentation]	cbd does not crash if the storage db_host is wrong
	[Tags]	Broker	sql
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Broker Config Output set	central	central-broker-master-perfdata	db_host	1.2.3.4
	FOR	${i}	IN RANGE	0	5
	 ${start}=	Get Current Date
	 Start Broker
	 ${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	 ${content}=	Create List	Unable to connect to the database
	 ${result}=	Find In Log with timeout	${log}	${start}	${content}	50
	 Should Be True	${result}
	 Stop Broker
	END

BDB6
	[Documentation]	cbd does not crash if the sql db_host is wrong
	[Tags]	Broker	sql
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Broker Config Output set	central	central-broker-master-sql	db_host	1.2.3.4
	FOR	${i}	IN RANGE	0	5
	 ${start}=	Get Current Date
	 Start Broker
	 ${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	 ${content}=	Create List	error while starting connection
	 ${result}=	Find In Log with timeout	${log}	${start}	${content}	20
	 Should Be True	${result}
	 Stop Broker
	END

BDB7
	[Documentation]	access denied when database user password is wrong for perfdata/sql
	[Tags]	Broker	sql
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Broker Config Output set	central	central-broker-master-sql	db_password	centreon1
	Broker Config Output set	central	central-broker-master-perfdata	db_password	centreon1
	${start}=	Get Current Date
	Start Broker
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${content}=	Create List	mysql_connection: error while starting connection
	${result}=	Find In Log with timeout	${log}	${start}	${content}	20
	Should Be True	${result}
	Stop Broker

BDB8
	[Documentation]	access denied when database user password is wrong for perfdata
	[Tags]	Broker	sql
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Broker Config Output set	central	central-broker-master-perfdata	db_password	centreon1
	${start}=	Get Current Date
	Start Broker
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${content}=	Create List	mysql_connection: error while starting connection
	${result}=	Find In Log with timeout	${log}	${start}	${content}	20
	Should Be True	${result}
	Stop Broker

BDB9
	[Documentation]	access denied when database user password is wrong for sql
	[Tags]	Broker	sql
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Broker Config Output set	central	central-broker-master-sql	db_password	centreon1
	${start}=	Get Current Date
	Start Broker
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${content}=	Create List	mysql_connection: error while starting connection
	${result}=	Find In Log with timeout	${log}	${start}	${content}	20
	Should Be True	${result}
	Stop Broker

BDB10
	[Documentation]	connection should be established when user password is good for sql/perfdata
	[Tags]	Broker	sql
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Broker Config Log	central	sql	debug
	Broker Config Log	module	sql	debug
	${start}=	Get Current Date
	Start Broker
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${content}=	Create List	sql stream initialization	storage stream initialization
	${result}=	Find In Log with timeout	${log}	${start}	${content}	40
	Should Be True	${result}
	Stop Broker

BEDB2
	[Documentation]	start broker/engine and then start MariaDB => connection is established
	[Tags]	Broker	sql	start-stop
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Config Engine	${1}
	${start}=	Get Current Date
	Stop Mysql
	Start Broker
	Start Engine
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${content}=	Create List	Unable to connect to the database
	${result}=	Find In Log with timeout	${log}	${start}	${content}	40
	Should Be True	${result}
	Start Mysql
	${result}=	Check Broker Stats exist	central	mysql manager	waiting tasks in connection 0
	Should Be True	${result}
	Stop Broker
	Stop Engine

BDBM1
	[Documentation]	start broker/engine and then start MariaDB => connection is established
	[Tags]	Broker	sql	start-stop
	@{lst}=	Create List	1	6
	FOR	${c}	IN	@{lst}
	 Config Broker	central
	 Broker Config Output set	central	central-broker-master-sql	connections_count	${c}
	 Broker Config Output set	central	central-broker-master-perfdata	connections_count	${c}
	 Config Broker	rrd
	 Config Broker	module
	 Config Engine	${1}
	 ${start}=	Get Current Date
	 Stop Mysql
	 Start Broker
	 Start Engine
	 ${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	 ${content}=	Create List	Unable to connect to the database
	 ${result}=	Find In Log with timeout	${log}	${start}	${content}	20
	 Should Be True	${result}
	 Start Mysql
	 ${result}=	Get Broker Stats Size	central	mysql manager
	 Should Be True	${result} >= ${c} + 1
	 Stop Broker
	 Stop Engine
	END

BDBU1
	[Documentation]	Access denied when database name exists but is not the good one for unified sql output
	[Tags]	Broker	sql	unified_sql
	Config Broker	central
	Config Broker Sql Output	central	unified_sql
	Config Broker	rrd
	Config Broker	module
	Broker Config Output set	central	central-broker-unified-sql	db_name	centreon
	FOR	${i}	IN RANGE	0	5
	 ${start}=	Get Current Date
	 Start Broker
	 ${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	 ${content}=	Create List	Table 'centreon.instances' doesn't exist
	 ${result}=	Find In Log with timeout	${log}	${start}	${content}	30
	 Should Be True	${result}
	 Stop Broker
	END

BDBU3
	[Documentation]	Access denied when database name does not exist for unified sql output
	[Tags]	Broker	sql	unified_sql
	Config Broker	central
	Config Broker Sql Output	central	unified_sql
	Config Broker	rrd
	Config Broker	module
	Broker Config Output set	central	central-broker-unified-sql	db_name	centreon1
	FOR	${i}	IN RANGE	0	5
	 ${start}=	Get Current Date
	 Start Broker
	 ${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	 ${content}=	Create List	global error: mysql_connection: error while starting connection
	 ${result}=	Find In Log with timeout	${log}	${start}	${content}	30
	 Should Be True	${result}
	 Stop Broker
	END

BDBU5
	[Documentation]	cbd does not crash if the unified sql db_host is wrong
	[Tags]	Broker	sql	unified_sql
	Config Broker	central
	Config Broker Sql Output	central	unified_sql
	Config Broker	rrd
	Config Broker	module
	Broker Config Output set	central	central-broker-unified-sql	db_host	1.2.3.4
	FOR	${i}	IN RANGE	0	5
	 ${start}=	Get Current Date
	 Start Broker
	 ${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	 ${content}=	Create List	error while starting connection
	 ${result}=	Find In Log with timeout	${log}	${start}	${content}	50
	 Should Be True	${result}	msg=Cannot find the message telling cbd is not connected to the database.
	 Stop Broker
	END

BDBU7
	[Documentation]	Access denied when database user password is wrong for unified sql
	[Tags]	Broker	sql	unified_sql
	Config Broker	central
	Config Broker Sql Output	central	unified_sql
	Config Broker	rrd
	Config Broker	module
	Broker Config Output set	central	central-broker-unified-sql	db_password	centreon1
	${start}=	Get Current Date
	Start Broker
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${content}=	Create List	mysql_connection: error while starting connection
	${result}=	Find In Log with timeout	${log}	${start}	${content}	20
	Should Be True	${result}	msg=Error concerning cbd not connected to the database is missing.
	Stop Broker

BDBU10
	[Documentation]	Connection should be established when user password is good for unified sql
	[Tags]	Broker	sql	unified_sql
	Config Broker	central
	Config Broker Sql Output	central	unified_sql
	Config Broker	rrd
	Config Broker	module
	Broker Config Log	central	sql	debug
	Broker Config Log	module	sql	debug
	${start}=	Get Current Date
	Start Broker
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${content}=	Create List	mysql_connection: commit
	${result}=	Find In Log with timeout	${log}	${start}	${content}	40
	Should Be True	${result}	msg=Log concerning a commit (connection ok) is missing.
	Stop Broker

BDBMU1
	[Documentation]	start broker/engine with unified sql and then start MariaDB => connection is established
	[Tags]	Broker	sql	start-stop	unified_sql
	@{lst}=	Create List	1	6
	FOR	${c}	IN	@{lst}
	 Config Broker	central
	 Config Broker Sql Output	central	unified_sql
	 Broker Config Output set	central	central-broker-unified-sql	connections_count	${c}
	 Broker Config Output set	central	central-broker-unified-sql	retry_interval	5
	 Config Broker	rrd
	 Config Broker	module
	 Config Engine	${1}
	 ${start}=	Get Current Date
	 Stop Mysql
	 Start Broker
	 Start Engine
	 ${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	 ${content}=	Create List	mysql_connection: error while starting connection
	 ${result}=	Find In Log with timeout	${log}	${start}	${content}	20
	 Should Be True	${result}	msg=Broker does not see any issue with the db while it is switched off
	 Start Mysql
	 ${result}=	Check Broker Stats exist	central	mysql manager	waiting tasks in connection 0	80
	 Should Be True	${result}	msg=No stats on mysql manager found
	 ${result}=	Get Broker Stats size	central	mysql manager	${60}
	 Should Be True	${result} >= ${c} + 1	msg=Broker mysql manager stats do not show the ${c} connections
	 Stop Broker
	 Stop Engine
	END

