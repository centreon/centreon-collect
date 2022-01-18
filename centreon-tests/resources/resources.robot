*** Settings ***
Library	Process
Library	OperatingSystem
Library	Common.py

*** Keywords ***
Clean Before Suite
        Stop Processes
        Clear Engine Logs
        Clear Broker Logs

Clean After Suite
	# Remove Files	${ENGINE_LOG}${/}centengine.log ${ENGINE_LOG}${/}centengine.debug
	# Remove Files	${BROKER_LOG}${/}central-broker-master.log	${BROKER_LOG}${/}central-rrd-master.log	${BROKER_LOG}${/}central-module-master.log
	Terminate All Processes	kill=True

Clear Engine Logs
	Remove Directory	${ENGINE_LOG}	Recursive=True
        Create Directory	${ENGINE_LOG}

Clear Broker Logs
	Remove Files	${BROKER_LOG}${/}central-broker-master.log	${BROKER_LOG}${/}central-rrd-master.log	${BROKER_LOG}${/}central-module-master.log*
	Remove Files	${BROKER_LOG}${/}central-module-master.log	${BROKER_LOG}${/}central-rrd-master.log	${BROKER_LOG}${/}central-module-master.log*
	Remove Files	${BROKER_LOG}${/}central-rrd-master.log	${BROKER_LOG}${/}central-rrd-master.log	${BROKER_LOG}${/}central-module-master.log*

Start Broker
	Start Process	/usr/sbin/cbd	/etc/centreon-broker/central-broker.json	alias=b1
	Start Process	/usr/sbin/cbd	/etc/centreon-broker/central-rrd.json	alias=b2

Reload Broker
	Send Signal To Process	SIGHUP	b1
	Send Signal To Process	SIGHUP	b2

Stop Broker
	${result}=	Terminate Process	b1	kill=False
	Should Be Equal As Integers	${result.rc}	0
	${result}=	Terminate Process	b2	kill=False
	Should Be Equal As Integers	${result.rc}	0

Stop Processes
	Terminate All Processes	kill=True
	Kill Broker
	Kill Engine

Start Engine
	${count}=	Get Engines Count
	FOR	${idx}	IN RANGE	0	${count}
		${alias}=	Catenate	SEPARATOR=	e	${idx}
		Log To Console	${alias}
		${conf}=	Catenate	SEPARATOR=	/etc/centreon-engine/config	${idx}	/centengine.cfg
		${log}=	Catenate	SEPARATOR=	/var/log/centreon-engine/config	${idx}
		${lib}=	Catenate	SEPARATOR=	/var/lib/centreon-engine/config	${idx}
                Create Directory	${log}
                Create Directory	${lib}
                Log To Console	Starting Engine ${idx}
		Start Process	/usr/sbin/centengine	${conf}	alias=${alias}
	END

Stop Engine
	${count}=	Get Engines Count
	FOR	${idx}	IN RANGE	0	${count}
		${alias}=	Catenate	SEPARATOR=	e	${idx}
		${result}=	Terminate Process	${alias}
		Should Be True	${result.rc} == -15 or ${result.rc} == 0	msg=Engine badly stopped with ${count} instances.
	END

Reload Engine
	${count}=	Get Engines Count
	FOR	${idx}	IN RANGE	0	${count}
		${alias}=	Catenate	SEPARATOR=	e	${idx}
		Send Signal To Process	SIGHUP	${alias}
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

Disable Eth Connection On Port
	[Arguments]	${port}
	RUN	iptables -A INPUT -p tcp --dport ${port} -j DROP
	RUN	iptables -A OUTPUT -p tcp --dport ${port} -j DROP
	RUN	iptables -A FORWARD -p tcp --dport ${port} -j DROP

Reset Eth Connection
	Run	iptables -F
	Run	iptables -X

*** Variables ***
${BROKER_LOG}	/var/log/centreon-broker
${ENGINE_LOG}	/var/log/centreon-engine
${SCRIPTS}	${CURDIR}${/}scripts${/}
