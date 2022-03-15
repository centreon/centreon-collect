*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes

Documentation	Centreon Broker database connection failure
Library		Process
Library		DateTime
Library		OperatingSystem
Library		../resources/Common.py
Library		../resources/Broker.py
Library		../resources/Engine.py

*** Test Cases ***
NetworkDbFail1
	[Documentation]	network failure test between broker and database (shutting down connection for 100ms)
	[Tags]	Broker	Database	Network
	Network Failure	interval=100ms

NetworkDbFail2
	[Documentation]	network failure test between broker and database (shutting down connection for 1s)
	[Tags]	Broker	Database	Network
	Network Failure	interval=1s

NetworkDbFail3
	[Documentation]	network failure test between broker and database (shutting down connection for 10s)
	[Tags]	Broker	Database	Network
	Network Failure	interval=10s

NetworkDbFail4
	[Documentation]	network failure test between broker and database (shutting down connection for 30s)
	[Tags]	Broker	Database	Network
	Network Failure	interval=30s

NetworkDbFail5
	[Documentation]	network failure test between broker and database (shutting down connection for 60s)
	[Tags]	Broker	Database	Network
	Network Failure	interval=1m

NetworkDBFail6
	[Documentation]	network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s
        [Tags]	Broker	Database	Network	unstable
	Config Engine	${1}
        Config Broker	central
	Broker Config Output Set	central	central-broker-master-sql	db_host	127.0.0.1
	Broker Config Output set	central	central-broker-master-sql	connections_count	5
	Broker Config Output Set	central	central-broker-master-perfdata	db_host	127.0.0.1
	Broker Config Output set	central	central-broker-master-perfdata	connections_count	5
	Broker Config Log	central	sql	trace
        Config Broker	rrd
        Config Broker	module
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Broker and Engine are not connected
	${content}=	Create List	run query: SELECT
	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	40
	Should Be True	${result}	msg=No SELECT done by broker in the DB
        Disable Eth Connection On Port	port=3306
        Sleep	1m
        Reset Eth Connection
        ${content}=	Create List	0 events acknowledged
	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	40
        Stop Engine
        Kindly Stop Broker

NetworkDBFailU6
	[Documentation]	network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s (with unified_sql)
        [Tags]	Broker	Database	Network	unified_sql	unstable
	Reset Eth Connection
	Config Engine	${1}
        Config Broker	central
	Config Broker Sql Output	central	unified_sql
	Broker Config Output Set	central	central-broker-unified-sql	db_host	127.0.0.1
	Broker Config Output set	central	central-broker-unified-sql	connections_count	5
	Broker Config Log	central	sql	trace
        Config Broker	rrd
        Config Broker	module
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Broker and Engine are not connected
	${content}=	Create List	run query: SELECT
	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	40
	Should Be True	${result}	msg=No SELECT done by broker in the DB
        Disable Eth Connection On Port	port=3306
        Sleep	1m
        Reset Eth Connection
        ${content}=	Create List	0 events acknowledged
	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	40
        Stop Engine
        Kindly Stop Broker

NetworkDBFail7
	[Documentation]	network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s
        [Tags]	Broker	Database	Network
	Config Engine	${1}
        Config Broker	central
        Reset Eth Connection
	Broker Config Output Set	central	central-broker-master-sql	db_host	127.0.0.1
	Broker Config Output set	central	central-broker-master-sql	connections_count	5
	Broker Config Output Set	central	central-broker-master-perfdata	db_host	127.0.0.1
	Broker Config Output set	central	central-broker-master-perfdata	connections_count	5
	Broker Config Log	central	sql	trace
        Config Broker	rrd
        Config Broker	module
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Broker and Engine are not connected
	${content}=	Create List	run query: SELECT
	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	40
	Should Be True	${result}	msg=No SELECT done by broker in the DB
        FOR	${i}	IN	0	5
         Disable Eth Connection On Port	port=3306
         Sleep	10s
         Reset Eth Connection
         Sleep	10s
        END
        ${content}=	Create List	0 events acknowledged
	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	60
	Should Be True	${result}	msg=There are still events in the queue.
        Stop Engine
        Kindly Stop Broker

NetworkDBFailU7
	[Documentation]	network failure test between broker and database: we wait for the connection to be established and then we shut down the connection for 60s (with unified_sql)
        [Tags]	Broker	Database	Network	unified_sql
        Reset Eth Connection
	Config Engine	${1}
        Config Broker	central
	Config Broker Sql Output	central	unified_sql
	Broker Config Output Set	central	central-broker-unified-sql	db_host	127.0.0.1
	Broker Config Output set	central	central-broker-unified-sql	connections_count	5
	Broker Config Log	central	sql	trace
        Config Broker	rrd
        Config Broker	module
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Broker and Engine are not connected
	${content}=	Create List	run query: SELECT
	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	40
	Should Be True	${result}	msg=No SELECT done by broker in the DB
        FOR	${i}	IN	0	5
         Disable Eth Connection On Port	port=3306
         Sleep	10s
         Reset Eth Connection
         Sleep	10s
        END
        ${content}=	Create List	0 events acknowledged
	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	60
	Should Be True	${result}	msg=There are still events in the queue.
        Stop Engine
        Kindly Stop Broker

*** Keywords ***
Disable Sleep Enable
	[Arguments]		${interval}
	Disable Eth Connection On Port	port=3306
	Sleep			${interval}
	Reset Eth Connection

Network Failure
	[Arguments]	${interval}
	Reset Eth Connection
	Config Engine	${1}
	Config Broker	module
	Config Broker	rrd
	Config Broker	central
	Broker Config Output Set	central	central-broker-master-sql	db_host	127.0.0.1
	Broker Config Output set	central	central-broker-master-sql	connections_count	5
	Broker Config Output Set	central	central-broker-master-perfdata	db_host	127.0.0.1
	Broker Config Output set	central	central-broker-master-perfdata	connections_count	5
	Broker Config Log	central	sql	trace
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${content}=	Create List	SQL: performing mysql_ping
        ${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	50
        Should Be True	${result}	msg=We should have a call to mysql_ping every 30s on inactive connections.
	Disable Sleep Enable	${interval}
	${end}=	Get Current Date
	${content}=	Create List	mysql_connection: commit
	${result}=	Find In Log With Timeout	${centralLog}	${end}	${content}	80
	Should Be True	${result}	msg=timeout after network to be restablished (network failure duration : ${interval})
        Kindly Stop Broker
	Stop Engine

*** Variables ***
