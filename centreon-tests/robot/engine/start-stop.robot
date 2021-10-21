*** Settings ***
Documentation	Centreon Engine only start/stop tests
Library	Process
Library	OperatingSystem
Library	ConfigEngine.py

*** Test cases ***
ESS1: Start-Stop one instance of engine and no coredump
	[Tags]	Engine	start-stop
	Remove Logs
	Config Engine	${1}
	Start Stop Instances	0

ESS2: Start-Stop many instances of engine and no coredump
	[Tags]	Engine	start-stop
	Remove Logs
	Config Engine	${1}
	Repeat Keyword	5 times	Start Stop Instances	300ms

*** Keywords ***
Remove Logs
	Remove Files	${ENGINE_LOG}${/}centengine.log ${ENGINE_LOG}${/}centengine.debug

Start Stop Instances
	[Arguments]	${interval}
	${count}=	Get Instances Count
	FOR	${idx}	IN RANGE	0	${count}
		${alias}=	Catenate	SEPARATOR=	conf	${idx}
		${conf}=	Catenate	SEPARATOR=	/etc/centreon-engine/config	${idx}	/centengine.cfg
		Start Process	/usr/sbin/centengine	${conf}	alias=${alias}
	END
	Sleep	${interval}
	FOR	${idx}	IN RANGE	0	${count}
		${alias}=	Catenate	SEPARATOR=	conf	${idx}
		${result}=	Terminate Process	${alias}
		Should Be True	${result.rc} == -15 or ${result.rc} == 0
	END


*** Variables ***
${ENGINE_LOG}	/var/log/centreon-engine
