*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Ctn Clean Before Suite
Suite Teardown	Ctn Clean After Suite
Test Setup	Ctn Stop Processes

Documentation	Centreon Broker only start/stop tests
Library	Process
Library	OperatingSystem
Library	../resources/Broker.py
Library	DateTime


*** Test Cases ***
BLDIS1
	[Documentation]	Start broker with core logs 'disabled'
	[Tags]	Broker	start-stop	log-v2
	Ctn Config Broker	rrd
	Ctn Config Broker	central
        Ctn Broker Config Log	central	core	disabled
        Ctn Broker Config Log	central	sql	debug
        ${start}=	Get Current Date
	Ctn Start Broker
        ${content}=	Create List	[sql]
        ${result}=	Ctn Find In Log With Timeout	${centralLog}	${start}	${content}	30
        Should Be True	${result}	msg="No sql logs produced"

        ${content}=	Create List	[core]
        ${result}=	Ctn Find In Log With Timeout	${centralLog}	${start}	${content}	30
	Should Be Equal	${result}	${False}	msg="We should not have core logs"
        Ctn Kindly Stop Broker

BLEC1
	[Documentation]	Change live the core level log from trace to debug
	[Tags]	Broker	log-v2	grpc
	Ctn Config Broker	rrd
	Ctn Config Broker	central
        Ctn Broker Config Log	central	core	trace
        Ctn Broker Config Log	central	sql	debug
        ${start}=	Get Current Date
	Ctn Start Broker
        ${result}=	Ctn Get Broker Log Level	51001	central	core
        Should Be Equal	${result}	trace
        Ctn Set Broker Log Level	51001	central	core	debug
        ${result}=	Ctn Get Broker Log Level	51001	central	core
        Should Be Equal	${result}	debug

BLEC2
	[Documentation]	Change live the core level log from trace to foo raises an error
	[Tags]	Broker	log-v2	grpc
	Ctn Config Broker	rrd
	Ctn Config Broker	central
        Ctn Broker Config Log	central	core	trace
        Ctn Broker Config Log	central	sql	debug
        ${start}=	Get Current Date
	Ctn Start Broker
        ${result}=	Ctn Get Broker Log Level	51001	central	core
        Should Be Equal	${result}	trace
        ${result}=	Ctn Set Broker Log Level	51001	central	core	foo
        Should Be Equal	${result}	Enum LogLevelEnum has no value defined for name 'FOO'

BLEC3
	[Documentation]	Change live the foo level log to trace raises an error
	[Tags]	Broker	log-v2	grpc
	Ctn Config Broker	rrd
	Ctn Config Broker	central
        Ctn Broker Config Log	central	core	trace
        Ctn Broker Config Log	central	sql	debug
        ${start}=	Get Current Date
	Ctn Start Broker
        ${result}=	Ctn Set Broker Log Level	51001	central	foo	trace
        Should Be Equal	${result}	The 'foo' logger does not exist
