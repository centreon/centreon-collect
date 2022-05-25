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
