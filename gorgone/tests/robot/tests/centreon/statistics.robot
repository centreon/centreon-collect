*** Settings ***
Documentation       test gorgone statistics module
Library             DatabaseLibrary
Resource            ${CURDIR}${/}..${/}..${/}resources${/}import.resource
Test Timeout        220s
Suite Setup         Suite Setup Statistics Module
Suite Teardown      Suite Teardown Statistic Module

*** Test Cases ***
check statistic module add all centengine data in db ${communication_mode}
    [Documentation]    Check engine statistics are correctly added in sql Database
    ${central}=    Set Variable    ${communication_mode}_gorgone_central_statistics
    ${poller}=    Set Variable    ${communication_mode}_gorgone_poller2_statistics
    @{process_list}    Create List    ${central}    ${poller}
    [Teardown]    Stop Gorgone And Remove Gorgone Config    @{process_list}    sql_file=${ROOT_CONFIG}db_delete_poller.sql

    ${date}    Get Current Date    increment=-1s
    @{central_config}    Create List    ${ROOT_CONFIG}statistics.yaml    ${ROOT_CONFIG}actions.yaml
    @{poller_config}    Create List    ${ROOT_CONFIG}actions.yaml
    Setup Two Gorgone Instances    central_config=${central_config}    communication_mode=${communication_mode}    central_name=${central}    poller_name=${poller}    poller_config=${poller_config}

    # we first test the module when there is no data in the table, we will test it again when
    # there is data in the table to be sure the data are correctly updated.
    Execute SQL String    DELETE FROM nagios_stats    alias=storage
    Check Row Count    SELECT * FROM nagios_stats    ==    0    alias=storage    assertion_message=there is still data in the nagios_stats table after the delete.

    Ctn Gorgone Force Engine Statistics Retrieve
    # statistics module send the GORGONE_ACTION_FINISH_OK once messages for the action module are sent.
    # It don't wait for the action module to send back data or for the processing of the response to be finished.
    # So I added a log each time a poller stat have finished to be processed. In this test I know
    # I have 2 log because there is the central and one poller.
    Ctn Wait For Log    /var/log/centreon-gorgone/${central}/gorgoned.log    ${date}
    
    Ctn Gorgone Check Poller Engine Stats Are Present    poller_id=1
    Ctn Gorgone Check Poller Engine Stats Are Present    poller_id=2

    # As the value we set in db are fake and hardcoded, we need to change the data before
    # running again the module to be sure data are correctly updated, instead of letting the last value persist.
    Execute SQL String    UPDATE nagios_stats SET stat_value=999;    alias=storage
    ${date2}    Get Current Date    increment=-1s
    Ctn Gorgone Force Engine Statistics Retrieve
    Ctn Wait For Log    /var/log/centreon-gorgone/${central}/gorgoned.log    ${date2}
    Ctn Gorgone Check Poller Engine Stats Are Present    poller_id=1
    Ctn Gorgone Check Poller Engine Stats Are Present    poller_id=2

    Examples:    communication_mode   --
        ...    push_zmq
        ...    pullwss

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

Suite Setup Statistics Module
    Set Centenginestat Binary
    Connect To Database    pymysql    ${DBNAME_STORAGE}    ${DBUSER}    ${DBPASSWORD}    ${DBHOST}    ${DBPORT}
    ...    alias=storage    autocommit=True

Set Centenginestat Binary
    [Documentation]    this keyword add a centenginestats file from the local directory to the /usr/sbin 
    ...    directory and make it executable. This allow to test the gorgone statistics module 
    ...    without installing centreon-engine and starting the service

    Copy File    /usr/sbin/centenginestats    /usr/sbin/centenginestats-back
    Copy File    ${CURDIR}${/}centenginestats    /usr/sbin/centenginestats
    Run    chmod 755 /usr/sbin/centenginestats
    
Suite Teardown Statistic Module
    Copy File    /usr/sbin/centenginestats-back    /usr/sbin/centenginestats
