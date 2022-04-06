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
BFC1
	[Documentation]	Start broker with invalid filters but one filter ok
	[Tags]	Broker	start-stop	log-v2
	Config Broker	central
        Broker Config Log	central	config	info
        Broker Config Log	central	sql	error
        Broker Config Log	central	core	error
        Broker Config Output Set Json	central	central-broker-master-sql	filters	{"category": ["neb", "foo", "bar"]}
        ${start}=	Get Current Date
	Start Broker
        ${content}=	Create List	Error in category elements: 'foo' is not a known category	Error in category elements: 'bar' is not a known category	Filters applied on endpoint: neb
        ${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	30
        Should Be True	${result}	msg="Only neb filter should be applied on sql output"

        Kindly Stop Broker

BFC2
	[Documentation]	Start broker with only invalid filters on an output
	[Tags]	Broker	start-stop	log-v2
	Config Broker	central
        Broker Config Log	central	config	info
        Broker Config Log	central	sql	error
        Broker Config Log	central	core	error
        Broker Config Output Set Json	central	central-broker-master-sql	filters	{"category": ["doe", "foo", "bar"]}
        ${start}=	Get Current Date
	Start Broker
        ${content}=	Create List	Error in category elements: 'doe' is not a known category	Error in category elements: 'bar' is not a known category	Filters applied on endpoint: all
        ${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	30
        Should Be True	${result}	msg="Only neb filter should be applied on sql output"

        Kindly Stop Broker
