*** Settings ***
Resource	../ressources/ressources.robot
Suite Setup	Clean Before Test
Suite Teardown	Clean After Test

Documentation	Centreon Broker and Engine add Hostgroup
Library	Process
Library	OperatingSystem
Library	DateTime
Library	Collections
Library	../engine/Engine.py
Library	../broker/Broker.py
Library	Common.py

*** Test cases ***
New host group
	[Tags]	Broker	Engine	New host group
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module

	Broker Config Log	central	sql	info
	Broker Config Log	module	sql	info
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected
	Sleep  25s
	Add Host Group	${0}	["host_1", "host_2", "host_3"]

	Sleep  5s
	Sighup Broker
	Sighup engine
	Sleep  45s

	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected
	Stop Broker
	Stop Engine

	${content1}=	Create List	enabling membership of host 3 to host group 1 on instance 1
	${content2}=	Create List	enabling membership of host 2 to host group 1 on instance 1
	${content3}=	Create List	enabling membership of host 1 to host group 1 on instance 1

	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${result}=	Find In Log	${log}	${start}	${content1}
	Should Be True	${result}
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${result}=	Find In Log	${log}	${start}	${content2}
	Should Be True	${result}
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${result}=	Find In Log	${log}	${start}	${content3}
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
