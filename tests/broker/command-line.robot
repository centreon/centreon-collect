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
BCL1
	[Documentation]	Starting broker with option '-s foobar' should return an error
	[Tags]	Broker	start-stop
	Config Broker	central
	Start Broker With Args	-s foobar
	${result}=	Wait For Broker
	${expected}=	Evaluate	"The option -s expects a positive integer" in """${result}"""
	Should be True	${expected}	msg=expected error 'The option -s expects a positive integer'

BCL2
	[Documentation]	Starting broker with option '-s 5' should work
	[Tags]	Broker	start-stop
	Config Broker	central
	${start}=	Get Current Date	exclude_millis=True
	Start Broker With Args	-s5	/etc/centreon-broker/central-broker.json
	${table}=	Create List	Starting the TCP thread pool of 5 threads
	Find in log with timeout	${centralLog}	${start}	${table}	30
	Stop Broker With Args

*** Keywords ***
Start Broker With Args
	[Arguments]	@{options}
	log to console	@{options}
	Start Process	/usr/sbin/cbd	@{options}	alias=b1	stdout=/tmp/output.txt

Wait For broker
	Wait For Process	b1
	${result}=	Get File	/tmp/output.txt
	Remove File	/tmp/output.txt
	[Return]	${result}

Stop Broker With Args
	Send Signal To Process	SIGTERM	b1
	${result}=	Wait For Process	b1	timeout=60s	on_timeout=kill
	Should Be Equal As Integers	${result.rc}	0



