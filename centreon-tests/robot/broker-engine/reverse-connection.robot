*** Settings ***
Resource	../ressources/ressources.robot
Suite Setup	Clean Before Test
Suite Teardown	Clean After Test

Documentation	Centreon Broker and Engine communication with or without compression
Library	Process
Library	OperatingSystem
Library	DateTime
Library	Collections
Library	../engine/Engine.py
Library	../broker/Broker.py
Library	Common.py

*** Test cases ***
Broker reverse connection good
	[Tags]	Broker	map	reverse connection good
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central_map
	Config Broker	module
	
	Log To Console	Compression set to
	Broker Config Log	central	bbdo	info
	Broker Config Log	module	bbdo	info
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected

	Run Reverse Bam	${50}	${0.2}

	Stop Broker
	Stop Engine

	${content1}=	Create List	New incoming connection 'centreon-broker-master-map-2'
	${content2}=	Create List	file: end of file '/var/lib/centreon-broker//central-broker-master.queue.centreon-broker-master-map-2' reached, erasing it
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${result}=	Find In Log	${log}	${start}	${content1}
	Should Be True	${result}
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${result}=	Find In Log	${log}	${start}	${content2}
	Should Be True	${result}
	File Should Not Exist	/var/lib/centreon-broker/central-broker-master.queue.centreon-broker-master-map-2


Broker reverse connection too slow
	[Tags]	Broker	map	reverse connection too slow
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central_map
	Config Broker	module
	
	Broker Config Log	central	bbdo	info
	Broker Config Log	module	bbdo	info
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected

	Run Reverse Bam	${150}	${10}

	Stop Broker
	Stop Engine

	${content1}=	Create List	New incoming connection 'centreon-broker-master-map-2'
	${content2}=	Create List	file: end of file '/var/lib/centreon-broker//central-broker-master.queue.centreon-broker-master-map-2' reached, erasing it
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${result}=	Find In Log	${log}	${start}	${content1}
	Should Be True	${result}
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${result}=	Find In Log	${log}	${start}	${content2}
	Should Be True	${result}
	File Should Not Exist	/var/lib/centreon-broker/central-broker-master.queue.centreon-broker-master-map-2


Broker reverse connection stopped
	[Tags]	Broker	map	reverse connection stopped
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central_map
	Config Broker	module
	
	Broker Config Log	central	bbdo	info
	Broker Config Log	module	bbdo	info
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected

	Stop Broker
	Stop Engine

	${content1}=	Create List	New incoming connection 'centreon-broker-master-map-2'
	${content2}=	Create List	file: end of file '/var/lib/centreon-broker//central-broker-master.queue.centreon-broker-master-map-2' reached, erasing it
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${result}=	Find In Log	${log}	${start}	${content1}
	Should Not Be True	${result}
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${result}=	Find In Log	${log}	${start}	${content2}
	Should Not Be True	${result}
	File Should Not Exist	/var/lib/centreon-broker/central-broker-master.queue.centreon-broker-master-map-2


*** Keywords ***

Check Connections
	${count}=	Get Engines Count
	${pid1}=	Get Process Id	b1
	FOR	${idx}	IN RANGE	0	${count}
		${alias}=	Catenate	SEPARATOR=	e	${idx}
		${pid2}=	Get Process Id	${alias}
		${retval}=	Check Connection	5669	${pid1}	${pid2}
		Return from Keyword If	${retval} == ${False}	${False}
	END
	${pid2}=	Get Process Id	b2
	${retval}=	Check Connection	5670	${pid1}	${pid2}
	[Return]	${retval}
