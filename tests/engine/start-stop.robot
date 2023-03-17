*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown    Clean After Suite
Test Setup	Stop Processes

Documentation	Centreon Engine only start/stop tests
Library	Process
Library	OperatingSystem
Library	../resources/Broker.py
Library	../resources/Engine.py

*** Test Cases ***
ESS1
	[Documentation]	Start-Stop (0s between start/stop) 5 times one instance of engine and no coredump
	[Tags]	Engine	start-stop
	Config Engine	${1}
	Config Broker  module 
	Repeat Keyword	5 times	Start Stop Instances	0

ESS2
	[Documentation]	Start-Stop (300ms between start/stop) 5 times one instance of engine and no coredump
	[Tags]	Engine	start-stop
	Config Engine	${1}
	Config Broker  module 
	Repeat Keyword	5 times	Start Stop Instances	300ms

ESS3
	[Documentation]	Start-Stop (0s between start/stop) 5 times three instances of engine and no coredump
	[Tags]	Engine	start-stop
	Config Engine	${3}
	Config Broker  module  ${3} 
	Repeat Keyword	5 times	Start Stop Instances	300ms

ESS4
	[Documentation]	Start-Stop (300ms between start/stop) 5 times three instances of engine and no coredump
	[Tags]	Engine	start-stop
	Config Engine	${3}
	Config Broker  module  ${3}
	Repeat Keyword	5 times	Start Stop Instances	300ms

*** Keywords ***

Start Stop Instances
	[Arguments]	${interval}
        Start Engine
	Sleep	${interval}
        Stop Engine
