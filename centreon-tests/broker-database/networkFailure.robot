*** Settings ***
Resource			../resources/resources.robot
#Suite Setup			Clean Before Suite
#Suite Teardown		Clean After Suite
#Test Setup			Stop Processes

Documentation		Centreon Broker database connection failure
Library				Process
Library				DateTime
Library				OperatingSystem
Library				../resources/BrokerDatabase.py

*** Test Cases ***
NetworkDbFail1
	[Documentation]		network failure test between broker and database (shutting down connection for 100ms)
	[Tags]				Broker		Database	Network
	Network Failure		interval=100ms
NetworkDbFail2
	[Documentation]		network failure test between broker and database (shutting down connection for 1s)
	[Tags]				Broker		Database	Network
	Network Failure		interval=1s
NetworkDbFail3
	[Documentation]		network failure test between broker and database (shutting down connection for 10s)
	[Tags]				Broker		Database 	Network
	Network Failure		interval=10s
NetworkDbFail4
	[Documentation]		network failure test between broker and database (shutting down connection for 30s)
	[Tags]				Broker		Database 	Network
	Network Failure		interval=30s
NetworkDbFail5
	[Documentation]		network failure test between broker and database (shutting down connection for 60s)
	[Tags]				Broker		Database 	Network
	Network Failure		interval=1m

*** Keywords ***
#Disable Eth Connection
#	Run	iptables -A INPUT -p tcp --dport 3306 -j DROP
#	Run	iptables -A OUTPUT -p tcp --dport 3306 -j DROP
#	Run	iptables -A FORWARD -p tcp --dport 3306 -j DROP
#Enable Eth Connection
#	Run	iptables -F
#	Run	iptables -X
Disable Sleep Enable
	[Arguments]		${interval}
	Disable Eth Connection On Port	port=3306
	Sleep   		${interval}
	Reset Eth Connection
Network Failure
	[Arguments]		${interval}
	#Config Engine	${1}
    #Config Broker  	central
	#Config Broker	module
	#Config Broker	rrd
	#Broker Config Output Set        central     central-broker-master-sql       db_host     127.0.0.1
    ${start}=		Get Current Date
	#Start mysql
    #Start Broker
	#Start Engine
	#${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	#${content}=			"SQL: performing mysql_ping."
	#${result}=			Find In Log		${log}		${start}	${content}
	#Should Be True		${result}
	#Disable Sleep Enable	interval=${interval}
	#${result}=		Do When Catch In Log	${log}		${start}	${content}		Disable Sleep Enable	${interval}
	${result}=		Do When Catch In Log	""			${start}	""				Disable Sleep Enable	${interval}
    Should Not Be Equal		${result}	False		msg=timeout after 5 minutes (NetworkFailure with sleep = ${interval})
	#Stop Broker
	#Stop Engine
    #Stop Mysql