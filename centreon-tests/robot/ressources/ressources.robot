*** Settings ***
Library	Process
Library	OperatingSystem
Library	../broker-engine/Common.py

*** Keywords ***
Clean Before Test
	Remove Files	${ENGINE_LOG}${/}centengine.log ${ENGINE_LOG}${/}centengine.debug
	Remove Files	${BROKER_LOG}${/}central-broker-master.log	${BROKER_LOG}${/}central-rrd-master.log	${BROKER_LOG}${/}central-module-master.log
	Terminate All Processes	kill=True
	Kill Broker
	Kill Engine

Clean After Test
	# Remove Files	${ENGINE_LOG}${/}centengine.log ${ENGINE_LOG}${/}centengine.debug
	# Remove Files	${BROKER_LOG}${/}central-broker-master.log	${BROKER_LOG}${/}central-rrd-master.log	${BROKER_LOG}${/}central-module-master.log
	Terminate All Processes	kill=True

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


*** Variables ***
${BROKER_LOG}	/var/log/centreon-broker
${ENGINE_LOG}	/var/log/centreon-engine