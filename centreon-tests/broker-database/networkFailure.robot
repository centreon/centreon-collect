*** Settings ***
Documentation
Library		Process
Library		OperatingSystem
Library     ../broker/Broker.py
Library     ../broker-engine/Common.py

*** Keywords ***
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
Network Failure
	[Arguments]		${interval}
    Config Broker   central
	Broker Config Output Set        central     central-broker-master-sql       db_host     127.0.0.1
    Start mysql
    Start Broker
    Disable Eth Connection
	Sleep   ${interval}
	Enable Eth Connection
    Stop Broker
    Stop Mysql

*** Test Cases ***
Network Failure Test 1/10s
	Network Failure	interval=100ms
Network Failure Test 1s
	Network Failure	interval=1s
Network Failure Test 10s
	Network Failure	interval=10s
Network Failure Test 30s
	Network Failure	interval=30s
Network Failure Test 60s
	Network Failure	interval=1m

#start engine
#check mysql log
#mysql connection -> log before pings
#cut network right after mysql ping start log
#there is a function, provided broker start time, that can read
#	log and see if there is a specific log message