*** Settings ***
Resource	../ressources/ressources.robot
# Test Setup	Stop All Broker
Suite Teardown    Terminate All Processes    kill=True

Documentation	Centreon Broker and Engine communication with or without compression
Library	Process
Library	OperatingSystem
Library	DateTime
Library	Collections
Library	../engine/Engine.py
Library	../broker/Broker.py
Library	Common.py

*** Test cases ***
BECC1: Broker/Engine communication with compression between central and poller
	[Tags]	Broker	Engine	compression	tcp
	Remove Logs
	Config Engine	${1}
	Config Broker	rrd
	FOR	${comp1}	IN	@{choices}
	FOR	${comp2}	IN	@{choices}
		Log To Console	Compression set to ${comp1} on central and to ${comp2} on module
		Config Broker	central
		Config Broker	module
		Broker Config Input set	central	central-broker-master-input	compression	${comp1}
		Broker Config Output set	module	central-module-master-output	compression	${comp2}
		Broker Config Log	central	bbdo	info
		Broker Config Log	module	bbdo	info
		${start}=	Get Current Date
		Start Broker
		Start Engine
		${result}=	Check Connections
		Should Be True	${result}	msg=Engine and Broker not connected
		Stop Broker
		Stop Engine
		${content1}=	Create List	we have extensions '${ext["${comp1}"]}' and peer has '${ext["${comp2}"]}'
		${content2}=	Create List	we have extensions '${ext["${comp2}"]}' and peer has '${ext["${comp1}"]}'
		IF	"${comp1}" == "yes" and "${comp2}" == "no"
			Insert Into List	${content1}	${-1}	extension 'COMPRESSION' is set to 'yes' in the configuration but cannot be activated because of peer configuration
		ELSE IF	"${comp1}" == "no" and "${comp2}" == "yes"
			Insert Into List	${content2}	${-1}	extension 'COMPRESSION' is set to 'yes' in the configuration but cannot be activated because of peer configuration
		END
		${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
		${result}=	Find In Log	${log}	${start}	${content1}
		Should Be True	${result}
		${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-module-master.log
		${result}=	Find In Log	${log}	${start}	${content2}
		Should Be True	${result}
	END
	END

*** Keywords ***
Remove Logs
	Remove Files	${ENGINE_LOG}${/}centengine.log ${ENGINE_LOG}${/}centengine.debug
	Remove Files	${BROKER_LOG}${/}central-broker-master.log	${BROKER_LOG}${/}central-rrd-master.log	${BROKER_LOG}${/}central-module-master.log

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
		Should Be Equal As Integers	${result.rc}	0
	END

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
${BROKER_LOG}	/var/log/centreon-broker
${ENGINE_LOG}	/var/log/centreon-engine
&{ext}	yes=COMPRESSION	no=	auto=COMPRESSION
@{choices}	yes	no	auto
