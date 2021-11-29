*** Settings ***
Documentation
Library		Process
Library		DateTime
Library		OperatingSystem
Library		BrokerDatabase.py
Library     ../broker/Broker.py
Library		../engine/Engine.py
Library     ../broker-engine/Common.py

*** Variables ***
${BROKER_LOG}	/var/log/centreon-broker

*** Keywords ***
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
		Should Be Equal As Integers	${result.rc}	0
	END
Start Broker
	Start Process	/usr/sbin/cbd	/etc/centreon-broker/central-broker.json    alias=b1
	Start Process	/usr/sbin/cbd	/etc/centreon-broker/central-rrd.json	    alias=b2
Stop Broker
	${result}=  Terminate Process   b1
	Should Be Equal As Integers		${result.rc}	0
	${result}=	Terminate Process   b2
	Should Be Equal As Integers		${result.rc}	0
Disable Eth Connection
	Run	iptables -A INPUT -p tcp --dport 3306 -j DROP
	Run	iptables -A OUTPUT -p tcp --dport 3306 -j DROP
	Run	iptables -A FORWARD -p tcp --dport 3306 -j DROP
Enable Eth Connection
	Run	iptables -F
	Run	iptables -X
Disable Sleep Enable
	[Arguments]		${interval}
	Disable Eth Connection
	Sleep   ${interval}
	Enable Eth Connection
Network Failure
	[Arguments]		${interval}
	Config Engine	${1}
    Config Broker  central
	#Config Broker	module
	#Config Broker	rrd
	Broker Config Output Set        central     central-broker-master-sql       db_host     127.0.0.1
    ${start}=		Get Current Date
	Start mysql
    Start Broker
	Start Engine
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${content}=		"SQL: performing mysql_ping."
	#${result}=			Find In Log		${log}		${start}	${content}
	#Should Be True		${result}
	#Disable Sleep Enable	interval=${interval}
	${result}=		Do When Catch In Log	${log}		${start}	${content}		Disable Sleep Enable	${interval}
	#${result}=		Do When Catch In Log	""		${start}	""		Disable Sleep Enable	${interval}
    Should Not Be Equal		${result}	False		msg=timeout after 5 minutes (NetworkFailure with ${interval})
	Stop Broker
	Stop Engine
    Stop Mysql

*** Test Cases ***
NetworkDbFail1
	[Documentation]		network failure test between broker and database (shutting down connection for 100ms)
	[Tags]	Broker	Database
	Network Failure	interval=100ms
NetworkDbFail2
	[Documentation]		network failure test between broker and database (shutting down connection for 1s)
	[Tags]	Broker	Database
	Network Failure	interval=1s
NetworkDbFail3
	[Documentation]		network failure test between broker and database (shutting down connection for 10s)
	[Tags]	Broker	Database
	Network Failure	interval=10s
NetworkDbFail4
	[Documentation]		network failure test between broker and database (shutting down connection for 30s)
	[Tags]	Broker	Database
	Network Failure	interval=30s
NetworkDbFail5
	[Documentation]		network failure test between broker and database (shutting down connection for 60s)
	[Tags]	Broker	Database
	Network Failure	interval=1m