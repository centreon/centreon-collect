*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes

Documentation	Centreon Broker and BAM
Library	Process
Library	OperatingSystem
Library	../resources/Broker.py
Library	../resources/Engine.py


*** Test Cases ***
BEBAMIDT1
	[Documentation]	A BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. Then we set a downtime on this last one. An inherited downtime is set to the BA. The downtime is removed from the service, the inherited downtime is then deleted.
	[Tags]	Broker	downtime	engine	bam
	Clear Commands Status
	Config Broker	module
	Config Broker	central
	Broker Config Log	central	bam	trace
	Config Broker	rrd
	Config Engine	${1}

	Clone Engine Config To DB
	Add Bam Config To Engine

	@{svc}=	Set Variable	${{ [("host_16", "service_314")] }}
	Create BA With Services	test	worst	${svc}
	Add Bam Config To Broker	central
	# Command of service_314 is set to critical
	${cmd_1}=	Get Command Id	314
	Log To Console	service_314 has command id ${cmd_1}
	Set Command Status	${cmd_1}	2
	Start Broker
	Start Engine
	Sleep	5s

	# KPI set to critical
	Repeat Keyword	3 times	Process Service Check Result	host_16	service_314	2	output critical for 314
	${result}=	Check Service Status With Timeout	host_16	service_314	2	60
	Should Be True	${result}	msg=The service (host_16,service_314) is not CRITICAL as expected

	# The BA should become critical
	${result}=	Check Ba Status With Timeout	test	2	60
	Should Be True	${result}	msg=The BA ba_1 is not CRITICAL as expected

	# A downtime is put on service_314
	Schedule Service Downtime	host_16	service_314	3600
	${result}=	Check Service Downtime With Timeout	host_16	service_314	1	60
	Should Be True	${result}	msg=The service (host_16, service_314) is not in downtime as it should be
	${result}=	Check Service Downtime With Timeout	_Module_BAM_1	ba_1	1	60
	Should Be True	${result}	msg=The BA ba_1 is not in downtime as it should

	# The downtime is deleted
	Delete Service Downtime	host_16	service_314
	${result}=	Check Service Downtime With Timeout	host_16	service_314	0	60
	Should Be True	${result}	msg=The service (host_16, service_314) is in downtime and should not.
	${result}=	Check Service Downtime With Timeout	_Module_BAM_1	ba_1	0	60
	Should Be True	${result}	msg=The BA ba_1 is in downtime as it should not

	Stop Engine
	Stop Broker

BEBAMIDT2
	[Documentation]	A BA of type 'worst' with one service is configured. The BA is in critical state, because of its service. Then we set a downtime on this last one. An inherited downtime is set to the BA. Engine is restarted. Broker is restarted. The two downtimes are still there with no duplicates. The downtime is removed from the service, the inherited downtime is then deleted.
	[Tags]	Broker	downtime	engine	bam	start	stop
	Clear Commands Status
	Config Broker	central
	Broker Config Log	central	bam	trace
	Config Broker	rrd
	Config Engine	${1}
	Clone Engine Config To DB
	Add Bam Config To Engine
	@{svc}=	Set Variable	${{ [("host_16", "service_314")] }}
	Create BA With Services	test	worst	${svc}
	Add Bam Config To Broker	central
	# Command of service_314 is set to critical
	${cmd_1}=	Get Command Id	314
	Log To Console	service_314 has command id ${cmd_1}
	Set Command Status	${cmd_1}	2
	Start Broker
	Start Engine
	Sleep	3s

	# KPI set to critical
	Repeat Keyword	3 times	Process Service Check Result	host_16	service_314	2	output critical for 314
	${result}=	Check Service Status With Timeout	host_16	service_314	2	60
	Should Be True	${result}	msg=The service (host_16,service_314) is not CRITICAL as expected

	# The BA should become critical
	${result}=	Check Ba Status With Timeout	test	2	60
	Should Be True	${result}	msg=The BA ba_1 is not CRITICAL as expected

	# A downtime is put on service_314
	Schedule Service Downtime	host_16	service_314	3600
	${result}=	Check Service Downtime With Timeout	host_16	service_314	1	60
	Should Be True	${result}	msg=The service (host_16, service_314) is not in downtime as it should be
	${result}=	Check Service Downtime With Timeout	_Module_BAM_1	ba_1	1	60
	Should Be True	${result}	msg=The BA ba_1 is not in downtime as it should

	FOR	${i}	IN RANGE	5
	  # Engine is restarted
	  Stop Engine
	  Start Engine
	  Sleep	3s
	  # Broker is restarted
	  Stop Broker
	  Start Broker
	END

	Sleep	5s
	# There are still two downtimes: the one on the ba and the one on the kpi.
	${result}=	Number Of Downtimes is	2
	Should Be True	${result}	msg=We should only have only two downtimes

	# The downtime is deleted
	Delete Service Downtime	host_16	service_314
	${result}=	Check Service Downtime With Timeout	host_16	service_314	0	60
	Should Be True	${result}	msg=The service (host_16, service_314) is in downtime and should not.
	${result}=	Check Service Downtime With Timeout	_Module_BAM_1	ba_1	0	60
	Should Be True	${result}	msg=The BA ba_1 is in downtime as it should not

	# We should have no more downtime
	${result}=	Number Of Downtimes is	0
	Should Be True	${result}	msg=We should have no more downtime

	Stop Engine
	Stop Broker
