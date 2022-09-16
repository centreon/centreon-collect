*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes

Documentation	Centreon Broker only start/stop tests
Library	Process
Library	OperatingSystem
Library	../resources/Broker.py
Library	DateTime


*** Test Cases ***
BLDIS1
	[Documentation]	Start broker with core logs 'disabled'
	[Tags]	Broker	start-stop	log-v2
	Config Broker	central
        Broker Config Log	central	core	disabled
        Broker Config Log	central	sql	debug
        ${start}=	Get Current Date
	Start Broker
        ${content}=	Create List	[sql]
        ${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	30
        Should Be True	${result}	msg="No sql logs produced"

        ${content}=	Create List	[core]
        ${result}=	Find In Log	${centralLog}	${start}	${content}
        Should Be Equal	${result[0]}	${False}	msg="We should not have core logs"
        Kindly Stop Broker

BLEC1
	[Documentation]	Change live the core level log from trace to debug
	[Tags]	Broker	log-v2	grpc
	Config Broker	central
        Broker Config Log	central	core	trace
        Broker Config Log	central	sql	debug
        ${start}=	Get Current Date
	Start Broker
        ${result}=	Get Broker Log Level	51001	central	core
        Should Be Equal	${result}	trace
        Set Broker Log Level	51001	central	core	debug
        ${result}=	Get Broker Log Level	51001	central	core
        Should Be Equal	${result}	debug

BLEC2
	[Documentation]	Change live the core level log from trace to foo raises an error
	[Tags]	Broker	log-v2	grpc
	Config Broker	central
        Broker Config Log	central	core	trace
        Broker Config Log	central	sql	debug
        ${start}=	Get Current Date
	Start Broker
        ${result}=	Get Broker Log Level	51001	central	core
        Should Be Equal	${result}	trace
        ${result}=	Set Broker Log Level	51001	central	core	foo
        Should Be Equal	${result}	The 'foo' level is unknown

BLEC3
	[Documentation]	Change live the foo level log to trace raises an error
	[Tags]	Broker	log-v2	grpc
	Config Broker	central
        Broker Config Log	central	core	trace
        Broker Config Log	central	sql	debug
        ${start}=	Get Current Date
	Start Broker
        ${result}=	Set Broker Log Level	51001	central	foo	trace
        Should Be Equal	${result}	The 'foo' logger does not exist
