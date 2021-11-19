*** Settings ***
Resource	../ressources/ressources.robot
Suite Setup	Clean Before Test
Suite Teardown    Clean After Test

Documentation	Centreon Engine test perl connectors
Library	Process
Library	OperatingSystem
Library	Engine.py
Library	DateTime
Library	../engine/Engine.py
Library	../broker-engine/Common.py

*** Test cases ***
EPC1: Check with perl connector
	[Tags]	Engine	start-stop
	Config Engine	${1}
	Engine Config Set Value	${0}	debug_level	${256}
	${start}=	Get Current Date

	Start Engine
	${log}=	Catenate	SEPARATOR=	${ENGINE_LOG}	/config0/centengine.debug
	${content}=	Create List	connector::run: connector='Perl Connector'
	${result}=	Find In Log with timeout	${log}	${start}	${content}	20
	Should Be True	${result}

	${content}=	Create List	connector::data_is_available
	${result}=	Find In Log with timeout	${log}	${start}	${content}	20
	Should Be True	${result}

	Stop Engine
