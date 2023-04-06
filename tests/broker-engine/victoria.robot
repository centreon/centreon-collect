*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes
Test Teardown	Save logs If Failed

Documentation	Centreon Broker victoria metrics tests
Library	Process
Library	OperatingSystem
Library	DateTime
Library  HttpCtrl.Server
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py

*** Test Cases ***
VICT_ONE_CHECK
  	[Documentation]	victoria metrics 
	[Tags]	Broker	Engine	victoria_metrics
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
    Config BBDO3  1
	Broker Config Log	central	victoria_metrics	trace
	Broker Config Log	central	perfdata	trace
	Config Broker Sql Output	central	unified_sql
    config_broker_victoria_output
	${start}=	Get Current Date
    Start Broker
    Start Engine
    Start Server        127.0.0.1   8000
    #wait all is started
    ${content}=	Create List	INITIAL SERVICE STATE: host_50;service_1000;	check_for_external_commands()
    ${result}=	Find In Log with Timeout	${engineLog0}	${start}	${content}	60
    Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.

    Process Service Check Result	host_16	service_314  2  taratata|metric=80%;50;75;5;99
    Wait For Request
    Reply By   200
    
    Stop Engine
    Kindly Stop Broker
    Stop Server



