*** Settings ***
Documentation       test gorgone autodiscovery module
Library             DatabaseLibrary
Resource            ${CURDIR}${/}..${/}..${/}resources${/}import.resource
Test Timeout        220s


*** Test Cases ***
check autodiscovery ${communication_mode}
    [Documentation]    Check engine autodiscovery module
    ${central}=    Set Variable    ${communication_mode}_gorgone_central_discovery
    ${poller}=    Set Variable    ${communication_mode}_gorgone_poller2_discovery
    @{process_list}    Create List    ${central}    ${poller}
    #[Teardown]    Test Teardown

    @{central_config}    Create List    ${ROOT_CONFIG}autodiscovery.yaml    ${ROOT_CONFIG}actions.yaml
    @{poller_config}    Create List    ${ROOT_CONFIG}actions.yaml
    Setup Two Gorgone Instances    central_config=${central_config}    communication_mode=${communication_mode}    central_name=${central}    poller_name=${poller}    poller_config=${poller_config}
    Gorgone Execute Sql    ${ROOT_CONFIG}db-insert-autodiscovery.sql
    ${response}=    POST  http://127.0.0.1:8085/api/centreon/autodiscovery/services    data={}
    Log To Console    ${response.json()}
    Dictionary Should Not Contain Key  ${response.json()}    error    api/centreon/statistics/engine api call resulted in an error : ${response.json()}

    Log To Console    engine statistic are being retrived. Gorgone sent a log token : ${response.json()}


    Examples:    communication_mode   --
        ...    push_zmq

*** Keywords ***

Ctn Wait For Log
    [Documentation]    We can't make a single call because we don't know which will finish first 
    ...    (even if it will often be the central node). So we check first for the central log, then for the poller node
    ...    from the starting point of the log. In the search, the lib search for the first log, and once it's found
    ...    start searching the second log from the first log position.
    [Arguments]    ${logfile}    ${date}

    ${log_central}    Create List    poller 1 engine data was integrated in rrd and sql database.
    ${result_central}    Ctn Find In Log With Timeout    log=${logfile}    content=${log_central}    date=${date}    regex=1    timeout=60
    Should Be True    ${result_central}    Didn't found the logs : ${result_central}

    ${log_poller2}    Create List    poller 2 engine data was integrated in rrd and sql database.
    ${result_poller2}    Ctn Find In Log With Timeout    log=${logfile}    content=${log_poller2}    date=${date}    regex=1    timeout=60
    Should Be True    ${result_poller2}    Didn't found the poller log on Central : ${result_poller2}

Ctn Gorgone Check Poller Engine Stats Are Present
    [Arguments]    ${poller_id}=
    
    &{Service Check Latency}=           Create Dictionary 	  Min=0.102    Max=0.955    Average=0.550
    &{Host Check Latency}=              Create Dictionary 	  Min=0.020    Max=0.868    Average=0.475
    &{Service Check Execution Time}=    Create Dictionary 	  Min=0.001    Max=0.332    Average=0.132
    &{Host Check Execution Time}=       Create Dictionary 	  Min=0.030    Max=0.152    Average=0.083
    
    &{data_check}    Create Dictionary    Service Check Latency=&{Service Check Latency}
    ...   Host Check Execution Time=&{Host Check Execution Time}
    ...   Host Check Latency=&{Host Check Latency}
    ...   Service Check Execution Time=&{Service Check Execution Time}

    FOR    ${stat_label}    ${stat_data}    IN    &{data_check}
        
        FOR    ${stat_key}    ${stat_value}    IN    &{stat_data}
            Check Row Count    SELECT instance_id FROM nagios_stats WHERE stat_key = '${stat_key}' AND stat_value = '${stat_value}' AND stat_label = '${stat_label}' AND instance_id='${poller_id}';    equal    1    alias=storage    retry_timeout=5    retry_pause=1
        END
    END

Ctn Gorgone Force Engine Statistics Retrieve
    ${response}=    GET  http://127.0.0.1:8085/api/centreon/statistics/engine
    Log To Console    ${response.json()}
    Dictionary Should Not Contain Key  ${response.json()}    error    api/centreon/statistics/engine api call resulted in an error : ${response.json()}

    Log To Console    engine statistic are being retrived. Gorgone sent a log token : ${response.json()}

Test Teardown
    Stop Gorgone And Remove Gorgone Config    @{process_list}    sql_file=${ROOT_CONFIG}db_delete_poller.sql
    Gorgone Execute Sql    ${ROOT_CONFIG}db-delete-autodiscovery.sql
