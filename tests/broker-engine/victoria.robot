*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes
Test Teardown	Save logs If Failed

Documentation	Centreon Broker victoria metrics tests
Library  String
Library	Process
Library	OperatingSystem
Library	DateTime
Library  HttpCtrl.Server
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py

*** Test Cases ***
VICT_ONE_CHECK_METRIC
  	[Documentation]	victoria metrics metric output
	[Tags]	Broker	Engine	victoria_metrics
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
    Config BBDO3  1
    Clear Retention
	Broker Config Log	central	victoria_metrics	trace
	Broker Config Log	central	perfdata	trace
	Broker Config Source Log  central   1
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

    Process Service Check Result	host_16	service_314  0  taratata|metric_taratata=80%;50;75;5;99


    ${start}=	get_round_current_date
	${timeout}=	Get Current Date  result_format=epoch  increment=00:01:00
    ${now}=	Get Current Date  result_format=epoch
    WHILE  ${now} < ${timeout}
        Wait For Request  timeout=30
        ${body}=  Get Request Body
        Set Test Variable  ${metric_found}  False
        IF  ${body != None}
            ${body}=     Decode Bytes To String   ${body}   UTF-8
            ${metric_found}=  check_victoria_metric  ${body}  ${start}  unit=%  host_id=16  serv_id=314  host=host_16  serv=service_314  name=metric_taratata  val=80  min=5  max=99  
        END
        EXIT FOR LOOP IF  ${metric_found}

        Reply By   200
        ${now}=	Get Current Date  result_format=epoch
    END
    
    Should Be True  ${now} < ${timeout}

    Stop Engine
    Kindly Stop Broker
    Stop Server

VICT_ONE_CHECK_STATUS
  	[Documentation]	victoria metrics status output
	[Tags]	Broker	Engine	victoria_metrics
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
    Config BBDO3  1
    Clear Retention
	Broker Config Log	central	victoria_metrics	trace
	Broker Config Log	central	perfdata	trace
	Broker Config Source Log  central   1
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

    #service ok
    ${start}=	get_round_current_date
    Process Service Check Result	host_16	service_314  0  taratata|metric_taratata=80%;50;75;5;99

	${timeout}=	Get Current Date  result_format=epoch  increment=00:01:00
    ${now}=	Get Current Date  result_format=epoch
    WHILE  ${now} < ${timeout}
        Wait For Request  timeout=30
        ${body}=  Get Request Body
        Set Test Variable  ${status_found}  False
        IF  ${body != None}
            ${body}=     Decode Bytes To String   ${body}   UTF-8
            ${status_found}=  check_victoria_status  ${body}  ${start}  host_id=16  serv_id=314  host=host_16  serv=service_314  val=100 
        END
        EXIT FOR LOOP IF  ${status_found}

        Reply By   200
        ${now}=	Get Current Date  result_format=epoch
    END
    
    Should Be True  ${now} < ${timeout}

    #service warning
  	${start}=	get_round_current_date
    Repeat Keyword	3 times  Process Service Check Result	host_16	service_314  1  taratata|metric_taratata=80%;50;75;5;99

	${timeout}=	Get Current Date  result_format=epoch  increment=00:01:00
    ${now}=	Get Current Date  result_format=epoch
    WHILE  ${now} < ${timeout}
        Wait For Request  timeout=30
        ${body}=  Get Request Body
        Set Test Variable  ${status_found}  False
        IF  ${body != None}
            ${body}=     Decode Bytes To String   ${body}   UTF-8
            ${status_found}=  check_victoria_status  ${body}  ${start}  host_id=16  serv_id=314  host=host_16  serv=service_314  val=75 
        END
        EXIT FOR LOOP IF  ${status_found}

        Reply By   200
        ${now}=	Get Current Date  result_format=epoch
    END
    
    Should Be True  ${now} < ${timeout}

    #service critical

  	${start}=	get_round_current_date
    Repeat Keyword	3 times  Process Service Check Result	host_16	service_314  2  taratata|metric_taratata=80%;50;75;5;99

	${timeout}=	Get Current Date  result_format=epoch  increment=00:01:00
    ${now}=	Get Current Date  result_format=epoch
    WHILE  ${now} < ${timeout}
        Wait For Request  timeout=30
        ${body}=  Get Request Body
        Set Test Variable  ${status_found}  False
        IF  ${body != None}
            ${body}=     Decode Bytes To String   ${body}   UTF-8
            ${status_found}=  check_victoria_status  ${body}  ${start}  host_id=16  serv_id=314  host=host_16  serv=service_314  val=0 
        END
        EXIT FOR LOOP IF  ${status_found}

        Reply By   200
        ${now}=	Get Current Date  result_format=epoch
    END
    
    Should Be True  ${now} < ${timeout}


    Stop Engine
    Kindly Stop Broker
    Stop Server


VICT_ONE_CHECK_METRIC_AFTER_FAILURE
  	[Documentation]	victoria metrics metric output after victoria shutdown
	[Tags]	Broker	Engine	victoria_metrics
	Config Engine	${1}	${50}	${20}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module	${1}
    Config BBDO3  1
    Clear Retention
	Broker Config Log	central	victoria_metrics	trace
	Broker Config Log	central	perfdata	trace
	Broker Config Source Log  central   1
	Config Broker Sql Output	central	unified_sql
    config_broker_victoria_output
	${start}=	Get Current Date
    Start Broker
    Start Engine
    #wait all is started
    ${content}=	Create List	INITIAL SERVICE STATE: host_50;service_1000;	check_for_external_commands()
    ${result}=	Find In Log with Timeout	${engineLog0}	${start}	${content}	60
    Should Be True	${result}	msg=An Initial host state on host_1 should be raised before we can start our external commands.

    Process Service Check Result	host_16	service_314  0  taratata|metric_taratata=80%;50;75;5;99
    ${start}=	get_round_current_date

	${content}=	Create List  [victoria_metrics]  name: "metric_taratata"
    ${result}=	find_in_log_with_timeout	${centralLog}	${start}	${content}	60
    Should Be True	${result}	msg=victoria should add metric in a request

    Start Server        127.0.0.1   8000
	${timeout}=	Get Current Date  result_format=epoch  increment=00:01:00
    ${now}=	Get Current Date  result_format=epoch
    WHILE  ${now} < ${timeout}
        Wait For Request  timeout=30
        ${body}=  Get Request Body
        Set Test Variable  ${metric_found}  False
        IF  ${body != None}
            ${body}=     Decode Bytes To String   ${body}   UTF-8
            ${metric_found}=  check_victoria_metric  ${body}  ${start}  unit=%  host_id=16  serv_id=314  host=host_16  serv=service_314  name=metric_taratata  val=80  min=5  max=99  
        END
        EXIT FOR LOOP IF  ${metric_found}

        Reply By   200
        ${now}=	Get Current Date  result_format=epoch
    END
    
    Should Be True  ${now} < ${timeout}

    Stop Engine
    Kindly Stop Broker
    Stop Server


