*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Ctn Clean Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup	Ctn Stop Processes

Documentation	Centreon Engine only start/stop tests
Library	Process
Library	OperatingSystem
Library	../resources/Broker.py
Library	../resources/Engine.py

*** Test Cases ***
ESS1
	[Documentation]	Start-Stop (0s between start/stop) 5 times one instance of engine and no coredump
	[Tags]	Engine	start-stop
	Ctn Config Engine	${1}
	Ctn Config Broker    module
	Repeat Keyword	5 times	Ctn Start Stop Instances	0

ESS2
	[Documentation]	Start-Stop (300ms between start/stop) 5 times one instance of engine and no coredump
	[Tags]	Engine	start-stop
	Ctn Config Engine	${1}
	Ctn Config Broker    module
	Repeat Keyword	5 times	Ctn Start Stop Instances	300ms

ESS3
	[Documentation]	Start-Stop (0s between start/stop) 5 times three instances of engine and no coredump
	[Tags]	Engine	start-stop
	Ctn Config Engine	${3}
	Ctn Config Broker    module  ${3}
	Repeat Keyword	5 times	Ctn Start Stop Instances	300ms

ESS4
	[Documentation]	Start-Stop (300ms between start/stop) 5 times three instances of engine and no coredump
	[Tags]	Engine	start-stop
	Ctn Config Engine	${3}
	Ctn Config Broker    module  ${3}
	Repeat Keyword	5 times	Ctn Start Stop Instances	300ms

*** Keywords ***

Ctn Start Stop Instances
	[Arguments]	${interval}
        Ctn Start Engine
	Sleep	${interval}
        Ctn Stop Engine
