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
	 ${content}=	Create List	error while loading caches: could not get list of deleted instances
	 ${result}=	Find In Log with timeout	${centralLog}	${start}	${content}	30
	 Should Be True	${result}	msg=An error message about the cache should arise.
	 Stop Broker
	END

BDB2
	[Documentation]	Access denied when database name exists but is not the good one for storage output
	[Tags]	Broker	sql
	Config Broker	central
        Broker Config Log	central	sql	info
	Config Broker	rrd
	Config Broker	module
	Broker Config Output set	central	central-broker-master-perfdata	db_name	centreon
	FOR	${i}	IN RANGE	0	5
	 ${start}=	Get Current Date
	 Start Broker
         ${content}=	Create List	mysql_connection: Table 'centreon.index_data' doesn't exist
	 ${result}=	Find In Log with timeout	${centralLog}	${start}	${content}	20
	 Should Be True	${result}	msg=An error message should be raised because of a database misconfiguration.
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
	 ${content}=	Create List	global error: mysql_connection: error while starting connection
	 ${result}=	Find In Log with timeout	${centralLog}	${start}	${content}	30
	 Should Be True	${result}	msg=No message about the database not connected.
	 Stop Broker
	END

BDB4
	[Documentation]	Access denied when database name does not exist for storage and sql outputs
	[Tags]	Broker	sql
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Broker Config Output set	central	central-broker-master-perfdata	db_name	centreon1
	Broker Config Output set	central	central-broker-master-sql	db_name	centreon1
	FOR	${i}	IN RANGE	0	5
	 ${start}=	Get Current Date
	 Start Broker
	 ${content}=	Create List	error while starting connection
	 ${result}=	Find In Log with timeout	${centralLog}	${start}	${content}	20
	 Should Be True	${result}	msg=No message about the fact that cbd is not correctly connected to the database.
	 Stop Broker
	END

BDB5
	[Documentation]	cbd does not crash if the storage/sql db_host is wrong
	[Tags]	Broker	sql
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Broker Config Output set	central	central-broker-master-perfdata	db_host	1.2.3.4
	Broker Config Output set	central	central-broker-master-sql	db_host	1.2.3.4
	FOR	${i}	IN RANGE	0	5
	 ${start}=	Get Current Date
	 Start Broker
	 ${content}=	Create List	error while starting connection
	 ${result}=	Find In Log with timeout	${centralLog}	${start}	${content}	50
	 Should Be True	${result}	msg=No message about the disconnection between cbd and the database
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
	 ${content}=	Create List	error while starting connection
	 ${result}=	Find In Log with timeout	${centralLog}	${start}	${content}	20
	 Should Be True	${result}	msg=No message about the disconnection between cbd and the database
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
	${content}=	Create List	mysql_connection: error while starting connection
	${result}=	Find In Log with timeout	${centralLog}	${start}	${content}	20
	Should Be True	${result}
	Stop Broker

BDB8
	[Documentation]	access denied when database user password is wrong for perfdata/sql
	[Tags]	Broker	sql
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Broker Config Output set	central	central-broker-master-perfdata	db_password	centreon1
	Broker Config Output set	central	central-broker-master-sql	db_password	centreon1
	${start}=	Get Current Date
	Start Broker
	${content}=	Create List	mysql_connection: error while starting connection
	${result}=	Find In Log with timeout	${centralLog}	${start}	${content}	20
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
	${content}=	Create List	mysql_connection: error while starting connection
	${result}=	Find In Log with timeout	${centralLog}	${start}	${content}	20
	Should Be True	${result}
	Stop Broker

BDB10
	[Documentation]	connection should be established when user password is good for sql/perfdata
	[Tags]	Broker	sql
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Broker Config Log	central	sql	debug
	${start}=	Get Current Date
	Start Broker
	${content}=	Create List	sql stream initialization	storage stream initialization
	${result}=	Find In Log with timeout	${centralLog}	${start}	${content}	40
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
	${content}=	Create List	error while starting connection
	${result}=	Find In Log with timeout	${centralLog}	${start}	${content}	40
	Should Be True	${result}	msg=Message about the disconnection between cbd and the database is missing
	Start Mysql
	${result}=	Check Broker Stats exist	central	mysql manager	waiting tasks in connection 0
	Should Be True	${result}	msg=Message about the connection to the database is missing.
	Stop Broker
	Stop Engine

BEDB3
	[Documentation]	start broker/engine, then stop MariaDB and then start it again. The gRPC API should give informations about SQL connections.
	[Tags]	Broker	sql	start-stop	grpc
	Config Broker	central
	Config Broker	rrd
	Config Broker	module
	Config Engine	${1}
	${start}=	Get Current Date
	Start Mysql
	Start Broker
	Start Engine
        FOR	${t}	IN RANGE	60
          ${result}=	Check Sql connections count with grpc	51001	${3}
          Exit For Loop If	${result}
        END
        Should Be True	${result}	msg=gRPC does not return 3 connections as expected
	Stop Mysql
        FOR	${t}	IN RANGE	60
          ${result}=	Check All Sql connections Down with grpc	51001
          Exit For Loop If	${result}
        END
        Should Be True	${result}	msg=Connections are not all down.

        Start Mysql
        FOR	${t}	IN RANGE	60
          ${result}=	Check Sql connections count with grpc	51001	${3}
          Exit For Loop If	${result}
        END
        Should Be True	${result}	msg=gRPC does not return 3 connections as expected
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
	 ${content}=	Create List	error while starting connection
	 ${result}=	Find In Log with timeout	${centralLog}	${start}	${content}	20
	 Should Be True	${result}	msg=Message about the disconnection between cbd and the database is missing
	 Start Mysql
	 ${result}=	Get Broker Stats Size	central	mysql manager
	 Should Be True	${result} >= ${c} + 1	msg=The stats file should contain at less ${c} + 1 connections to the database.
	 Stop Broker
	 Stop Engine
	END
