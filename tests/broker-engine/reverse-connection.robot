*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes
Test Teardown	Save logs If Failed

Documentation	Centreon Broker and Engine communication with or without compression
Library	Process
Library	OperatingSystem
Library	DateTime
Library	Collections
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py

*** Test Cases ***
BRGC1
	[Documentation]	Broker good reverse connection
	[Tags]	Broker	map	reverse connection
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central_map
	Config Broker	module

	Log To Console	Compression set to
	Broker Config Log	central	bbdo	info
	Broker Config Log	module0	bbdo	info
	Remove File  ${VarRoot}/lib/centreon-broker/*queue*
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected

	Run Reverse Bam	${50}	${0.2}

	Kindly Stop Broker
	Stop Engine

	${content}=	Create List	New incoming connection 'centreon-broker-master-map-2'	file: end of file '${VarRoot}/lib/centreon-broker//central-broker-master.queue.centreon-broker-master-map-2' reached, erasing it
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${result}=	Find In Log With Timeout	${log}	${start}	${content}	40
	Should Be True	${result}	msg=Connection to map has failed.
	File Should Not Exist	${VarRoot}/lib/centreon-broker/central-broker-master.queue.centreon-broker-master-map*	msg=There should not exist queue map files.


BRCTS1
	[Documentation]	Broker reverse connection too slow
	[Tags]	Broker	map	reverse connection
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central_map
	Config Broker	module

	Broker Config Log	central	bbdo	info
	Broker Config Log	module0	bbdo	info
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected

	Run Reverse Bam	${150}	${10}

	Kindly Stop Broker
	Stop Engine

	${content}=	Create List	New incoming connection 'centreon-broker-master-map-2'	file: end of file '${VarRoot}/lib/centreon-broker//central-broker-master.queue.centreon-broker-master-map-2' reached, erasing it
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${result}=	Find In Log With Timeout	${log}	${start}	${content}	40
	Should Be True	${result}	msg=Connection to map has failed
	File Should Not Exist	${VarRoot}/lib/centreon-broker/central-broker-master.queue.centreon-broker-master-map*	msg=There should not exist queue map files.


BRCS1
	[Documentation]	Broker reverse connection stopped
	[Tags]	Broker	map	reverse connection stopped
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central_map
	Config Broker	module

	Broker Config Log	central	bbdo	info
	Broker Config Log	module0	bbdo	info
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected

	Kindly Stop Broker
	Stop Engine

	${content}=	Create List	New incoming connection 'centreon-broker-master-map-2'	file: end of file '${VarRoot}/lib/centreon-broker//central-broker-master.queue.centreon-broker-master-map-2' reached, erasing it
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${result}=	Find In Log With Timeout	${log}	${start}	${content}	40
	Should Not Be True	${result}	msg=Connection to map has failed
	File Should Not Exist	${VarRoot}/lib/centreon-broker/central-broker-master.queue.centreon-broker-master-map-2	msg=There should not exist queue map files.
