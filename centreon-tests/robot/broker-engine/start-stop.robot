*** Settings ***
Documentation	Centreon Broker and Engine start/stop tests
Library	Process
Library	OperatingSystem
Library	../engine/Engine.py
Library	../broker/Broker.py
Library	Common.py

*** Test cases ***
BESS1: Start-Stop Broker/Engine - Broker first
	[Tags]	Broker	Engine	start-stop
	Remove Logs
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
	Start Broker
	Start Engine
	Sleep	5s
	${result}=	Check Connections
	Should Be True	${result}
	Stop Broker
	Stop Engine


*** Keywords ***
Remove Logs
	Remove Files	${ENGINE_LOG}${/}centengine.log ${ENGINE_LOG}${/}centengine.debug
	Remove Files	${BROKER_LOG}${/}central-broker-master.log	${BROKER_LOG}${/}central-rrd-master.log	${BROKER_LOG}${/}central-module-master.log

Start Broker
	Start Process	/usr/sbin/cbd	/etc/centreon-broker/central-broker.json	alias=b1
	Start Process	/usr/sbin/cbd	/etc/centreon-broker/central-rrd.json	alias=b2

Stop Broker
	${result}=	Terminate Process	b1
	Should Be True	${result.rc} == -15 or ${result.rc} == 0
	${result}=	Terminate Process	b2
	Should Be True	${result.rc} == -15 or ${result.rc} == 0

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
		Should Be True	${result.rc} == -15 or ${result.rc} == 0
	END

Check Connections
	${count}=	Get Engines Count
	${pid1}=	Get Process Id	b1
	FOR	${idx}	IN RANGE	0	${count}
		${alias}=	Catenate	SEPARATOR=	e	${idx}
		${pid2}=	Get Process Id	${alias}
		${retval}=	Check Connection	5669	${pid1}	${pid2}
		IF	${retval} == ${False}
			[Return]	${False}
		END
	END
	${pid2}=	Get Process Id	b2
	${retval}=	Check Connection	5670	${pid1}	${pid2}
	[Return]	${retval}

*** Variables ***
${BROKER_LOG}	/var/log/centreon-broker
${ENGINE_LOG}	/var/log/centreon-engine
