*** Settings ***
Library	Process
Library	OperatingSystem

*** Keywords ***
Remove Logs
	Remove Files	${ENGINE_LOG}${/}centengine.log ${ENGINE_LOG}${/}centengine.debug
	Remove Files	${BROKER_LOG}${/}central-broker-master.log	${BROKER_LOG}${/}central-rrd-master.log	${BROKER_LOG}${/}central-module-master.log

Stop All Broker
	Terminate process   "cbd"	true

Stop All Engine
    Terminate process   "cbd"	true

*** Variables ***
${BROKER_LOG}	/var/log/centreon-broker
${ENGINE_LOG}	/var/log/centreon-engine