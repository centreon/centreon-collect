*** Settings ***
Documentation	Centreon Broker only start/stop tests
Library	Process
Library	OperatingSystem
Library	ConfigEngine.py

*** Test cases ***
Start-Stop one instance of engine and no coredump
	[Tags]	Engine	start-stop
	Remove Logs
	Config Engine	${1}

*** Keywords ***
Remove Logs
	Remove Files	${ENGINE_LOG}${/}centengine.log ${ENGINE_LOG}${/}centengine.debug

*** Variables ***
${ENGINE_LOG}	/var/log/centreon-engine
