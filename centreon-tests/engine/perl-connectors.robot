*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown    Clean After Suite
Test Setup	Stop Processes

Documentation	Centreon Engine test perl connectors
Library	Process
Library	OperatingSystem
Library	DateTime
Library	../resources/Engine.py
Library	../resources/Common.py

*** Test Cases ***
EPC1
	[Documentation]	Check with perl connector
	[Tags]	Engine	start-stop
	Config Engine	${1}
	Engine Config Set Value	${0}	log_level_commands	trace
	${start}=	Get Current Date

	Start Engine
	${content}=	Create List	connector::run: connector='Perl Connector'
	${result}=	Find In Log with timeout	${logEngine0}	${start}	${content}	60
	Should Be True	${result}	msg=Missing a message talking about 'Perl Connector'

	${content}=	Create List	connector::data_is_available
	${result}=	Find In Log with timeout	${logEngine0}	${start}	${content}	20
	Should Be True	${result}	msg=Missing a message telling data is available from the Perl connector

	Stop Engine
