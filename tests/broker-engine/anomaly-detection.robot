*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes
Test Teardown	Save logs If Failed

Documentation	Centreon Broker and Engine anomaly detection


Library	DateTime
Library	Process
Library	OperatingSystem
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py


*** Test Cases ***
ANO_NOFILE
	[Documentation]	an anomaly detection without threshold file must be in unknown state
	[Tags]  Broker	Engine  Anomaly
	Config Engine	${1}	${50}	${20}
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	sql	debug
	Config Broker Sql Output	central	unified_sql
	${serv_id}=  Create Anomaly Detection  ${0}  ${1}  ${1}  metric
	Remove File  /tmp/anomaly_threshold.json
	Clear Retention
	Clear Db  services
	Start Broker
	Start Engine
	Process Service Check result	host_1	anomaly_${serv_id}	2	taratata
	Check Service Status With Timeout  host_1  anomaly_${serv_id}  3  30

ANO_TOO_OLD_FILE
	[Documentation]	an anomaly detection with an oldest threshold file must be in unknown state
	[Tags]  Broker	Engine  Anomaly
	Config Engine	${1}	${50}	${20}
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	sql	debug
	Config Broker Sql Output	central	unified_sql
	${serv_id}=  Create Anomaly Detection  ${0}  ${1}  ${1}  metric
	${predict_data} =  Evaluate  [[0,0,2],[1648812678,0,3]]
	Create Anomaly Threshold File  /tmp/anomaly_threshold.json  ${1}  ${serv_id}  metric  ${predict_data}
	Clear Retention
	Clear Db  services
	Start Broker
	Start Engine
	Process Service Check result	host_1	anomaly_${serv_id}	2	taratata|metric=70%;50;75
	Check Service Status With Timeout  host_1  anomaly_${serv_id}  3  30


ANO_OUT_LOWER_THAN_LIMIT
	[Documentation]	an anomaly detection with a perfdata lower than lower limit make a critical state
	[Tags]  Broker	Engine  Anomaly
	Config Engine	${1}	${50}	${20}
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	sql	debug
	Config Broker Sql Output	central	unified_sql
	${serv_id}=  Create Anomaly Detection  ${0}  ${1}  ${1}  metric
	${predict_data} =  Evaluate  [[0,50,52],[2648812678,50,63]]
	Create Anomaly Threshold File  /tmp/anomaly_threshold.json  ${1}  ${serv_id}  metric  ${predict_data}
	Clear Retention
	Clear Db  services
	Start Broker
	Start Engine
	Process Service Check result	host_1	anomaly_${serv_id}	2	taratata|metric=20%;50;75
	Check Service Status With Timeout  host_1  anomaly_${serv_id}  2  30

ANO_OUT_UPPER_THAN_LIMIT
	[Documentation]	an anomaly detection with a perfdata upper than upper limit make a critical state
	[Tags]  Broker	Engine  Anomaly
	Config Engine	${1}	${50}	${20}
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	sql	debug
	Config Broker Sql Output	central	unified_sql
	${serv_id}=  Create Anomaly Detection  ${0}  ${1}  ${1}  metric
	${predict_data} =  Evaluate  [[0,50,52],[2648812678,50,63]]
	Create Anomaly Threshold File  /tmp/anomaly_threshold.json  ${1}  ${serv_id}  metric  ${predict_data}
	Clear Retention
	Clear Db  services
	Start Broker
	Start Engine
	Process Service Check result	host_1	anomaly_${serv_id}	2	taratata|metric=80%;50;75
	Check Service Status With Timeout  host_1  anomaly_${serv_id}  2  30

AOUTLU1
	[Documentation]	an anomaly detection with a perfdata upper than upper limit make a critical state with bbdo 3
	[Tags]  Broker	Engine	Anomaly	bbdo
	Config Engine	${1}	${50}	${20}
	Config Broker	central
	Config Broker	rrd
	Config Broker	module	${1}
	Broker Config Log	central	sql	debug
	Config Broker Sql Output	central	unified_sql
	Broker Config Add Item	module0	bbdo_version	3.0.1
	Broker Config Add Item	rrd	bbdo_version	3.0.1
	Broker Config Add Item	central	bbdo_version	3.0.1
	${serv_id}=  Create Anomaly Detection  ${0}  ${1}  ${1}  metric
	${predict_data} =  Evaluate  [[0,50,52],[2648812678,50,63]]
	Create Anomaly Threshold File  /tmp/anomaly_threshold.json  ${1}  ${serv_id}  metric  ${predict_data}
	Clear Retention
	Clear Db  services
	Start Broker
	${start}=	Get Current Date
	Start Engine
	# Let's wait for the initial service states.
	${content}=	Create List	INITIAL SERVICE STATE: host_50;service_1000;
	${result}=	Find In Log with Timeout	${engineLog0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial service state on service (50, 1000) should be raised before we can start external commands.

	Process Service Check result	host_1	anomaly_${serv_id}	2	taratata|metric=80%;50;75
	Check Service Status With Timeout  host_1  anomaly_${serv_id}  2  30
	Stop Engine
	Kindly Stop Broker
	${lst}=	Create List	1	0	4
	${result}=	Check Types in resources	${lst}
	Should Be True	${result}	msg=The table 'resources' should contain rows of types SERVICE, HOST and ANOMALY_DETECTION.
