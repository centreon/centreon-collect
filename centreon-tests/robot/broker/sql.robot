*** Settings ***
Documentation	Centreon Broker Mariadb access
Library	Process
Library	OperatingSystem
Library	DateTime
Library	Broker.py
Library	../engine/Engine.py
Library	../broker-engine/Common.py

*** Test cases ***
BDB1: Access denied when database name exists but is not the good one for sql output
	[Tags]	Broker	sql
	#Remove Logs
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Broker Config Output set	central	central-broker-master-sql	db_name	centreon
	FOR	${i}	IN RANGE	0	5
	${start}=	Get Current Date
	Start Broker
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${content}=	Create List	Table 'centreon.instances' doesn't exist
	${result}=	Find In Log with timeout	${log}	${start}	${content}	20
	Should Be True	${result}
	Stop Broker
	END

BDB2: Access denied when database name exists but is not the good one for storage output
	[Tags]	Broker	sql
	#Remove Logs
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

BDB3: Access denied when database name does not exist for sql output
	[Tags]	Broker	sql
	#Remove Logs
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Broker Config Output set	central	central-broker-master-sql	db_name	centreon1
	FOR	${i}	IN RANGE	0	5
	 ${start}=	Get Current Date
	 Start Broker
	 ${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	 ${content}=	Create List	global error: mysql_connection: error while starting connection
	 ${result}=	Find In Log with timeout	${log}	${start}	${content}	20
	 Should Be True	${result}
	 Stop Broker
	END

BDB4: Access denied when database name does not exist for storage output
	[Tags]	Broker	sql
	Remove Logs
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

BDB5: cbd does not crash if the storage db_host is wrong
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
	 ${result}=	Find In Log with timeout	${log}	${start}	${content}	20
	 Should Be True	${result}
	 Stop Broker
	END

BDB6: cbd does not crash if the sql db_host is wrong
	[Tags]	Broker	sql
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Broker Config Output set	central	central-broker-master-sql	db_host	1.2.3.4
	FOR	${i}	IN RANGE	0	5
	 ${start}=	Get Current Date
	 Start Broker
	 ${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	 ${content}=	Create List	Unable to initialize the storage connection to the database
	 ${result}=	Find In Log with timeout	${log}	${start}	${content}	20
	 Should Be True	${result}
	 Stop Broker
	END

BDB7: access denied when database user password is wrong for perfdata/sql
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

BDB8: access denied when database user password is wrong for perfdata
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

BDB9: access denied when database user password is wrong for sql
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

BDB10: connection should be established when user password is good for sql/perfdata
	[Tags]	Broker	sql
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	${start}=	Get Current Date
	Start Broker
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${content}=	Create List	sql stream initialization	storage stream initialization
	${result}=	Find In Log with timeout	${log}	${start}	${content}	20
	Should Be True	${result}
	Stop Broker

BEDB2: start broker/engine and then start MariaDB => connection is established
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
	${result}=	Find In Log with timeout	${log}	${start}	${content}	20
	Should Be True	${result}
	Start Mysql
	${result}=	Check Broker Stats exists	central	mysql manager	waiting tasks in connection 0
	Should Be True	${result}
	Stop Broker
	Stop Engine

BDBM1: start broker/engine and then start MariaDB => connection is established
	[Tags]	Broker	sql	start-stop
	@{lst}=	Create List	4	6
	FOR	${c}	IN	@{lst}
	 Config Broker	central
	 Broker Config Output set	central	central-broker-master-sql	connections_count	6
	 Broker Config Output set	central	central-broker-master-perfdata	connections_count	6
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
	 ${result}=	Check Broker Stats Size	central	mysql manager
	 Should Be True	${result} >= ${c}
	 Stop Broker
	 Stop Engine
	END

*** Keywords ***
Start Broker
	Start Process	/usr/sbin/cbd	/etc/centreon-broker/central-broker.json	alias=b1
	Start Process	/usr/sbin/cbd	/etc/centreon-broker/central-rrd.json	alias=b2

Stop Broker
	${result}=	Terminate Process	b1
	Should Be Equal As Integers	${result.rc}	0
	${result}=	Terminate Process	b2
	Should Be Equal As Integers	${result.rc}	0

Start Engine
	${count}=	Get Engines Count
	FOR	${idx}	IN RANGE	0	${count}
		${alias}=	Catenate	SEPARATOR=	e	${idx}
		${conf}=	Catenate	SEPARATOR=	/etc/centreon-engine/config	${idx}	/centengine.cfg
		Start Process	/usr/sbin/centengine	${conf}	alias=${alias}
	END

Stop Engine
	${count}=	Get Engines Count
	FOR	${idx}	IN RANGE	0	${count}
		${alias}=	Catenate	SEPARATOR=	e	${idx}
		${result}=	Terminate Process	${alias}
		Log To Console	value of result=${result.rc}
		Should Be Equal As Integers	${result.rc}	0
	END

Remove Logs
	Remove Files	${BROKER_LOG}${/}central-broker-master.log	${BROKER_LOG}${/}central-rrd-master.log ${BROKER_LOG}${/}central-module-master.log

*** Variables ***
${BROKER_LOG}	/var/log/centreon-broker
