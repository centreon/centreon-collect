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
BCL1
	[Documentation]	Starting broker with option '-s foobar' should return an error
	[Tags]	Broker	start-stop
	Ctn Config Broker	central
	Ctn Start Broker With Args	-s foobar
	${result}=	Ctn Wait For broker
	${expected}=	Evaluate	"The option -s expects a positive integer" in """${result}"""
	Should be True	${expected}	msg=expected error 'The option -s expects a positive integer'

BCL2
	[Documentation]	Starting broker with option '-s5' should work
	[Tags]	Broker	start-stop
	Ctn Config Broker	central
	${start}=	Get Current Date	exclude_millis=True
	Sleep	1s
	Ctn Start Broker With Args	-s5	${EtcRoot}/centreon-broker/central-broker.json
	${table}=	Create List	Starting the TCP thread pool of 5 threads
	${logger_res}=	Ctn Find In Log With Timeout	${centralLog}	${start}	${table}	30
	Should be True	${logger_res}	msg=Didn't found 5 threads in ${VarRoot}/log/centreon-broker/central-broker-master.log
	Ctn Stop Broker With Args

BCL3
	[Documentation]	Starting broker with options '-D' should work and activate diagnose mode
	[Tags]	Broker	start-stop
	Ctn Config Broker	central
	${start}=	Get Current Date	exclude_millis=True
	Sleep	1s
	Ctn Start Broker With Args	-D	${EtcRoot}/centreon-broker/central-broker.json
	${result}=	Ctn Wait For broker
	${expected}=	Evaluate	"diagnostic:" in """${result}"""
	Should be True	${expected}	msg=diagnostic mode didn't launch

BCL4
	[Documentation]	Starting broker with options '-s2' and '-D' should work.
	[Tags]	Broker	start-stop
	Ctn Config Broker	central
	Ctn Start Broker With Args	-s2	-D	${EtcRoot}/centreon-broker/central-broker.json
	${result}=	Ctn Wait For broker
	${expected}=	Evaluate	"diagnostic:" in """${result}"""
	Should be True	${expected}	msg=diagnostic mode didn't launch

*** Keywords ***
Ctn Start Broker With Args
	[Arguments]	@{options}
	log to console	@{options}
	Start Process	/usr/sbin/cbd	@{options}	alias=b1	stdout=/tmp/output.txt

Ctn Wait For broker
	Wait For Process	b1
	${result}=	Get File	/tmp/output.txt
	Remove File	/tmp/output.txt
	[Return]	${result}

Ctn Stop Broker With Args
	Send Signal To Process	SIGTERM	b1
	${result}=	Wait For Process	b1	timeout=60s	on_timeout=kill
	Should Be Equal As Integers	${result.rc}	0



