*** Settings ***
Documentation       Start and stop gorgone in pullwss mode

Resource            ${CURDIR}${/}..${/}..${/}resources${/}import.resource
Test Timeout        160s

*** Variables ***
@{process_list}    pullwss_gorgone_poller_2    pullwss_gorgone_central
${log_size}    100
${log_count}    3000
#6000
*** Test Cases ***
check one poller can connect to a central and gorgone central stop first
    [Teardown]    Stop Gorgone And Remove Gorgone Config    @{process_list}        sql_file=${ROOT_CONFIG}db_delete_poller.sql
    @{process_list}    Set Variable    pullwss_gorgone_central    pullwss_gorgone_poller_2
    Log To Console    \nStarting the gorgone setup
    Setup Two Gorgone Instances    communication_mode=pullwss    central_name=pullwss_gorgone_central    poller_name=pullwss_gorgone_poller_2
    Ctn Check No Error In Logs    pullwss_gorgone_poller_2
    Log To Console    End of tests.

check one poller can connect to a central and gorgone poller stop first
    [Teardown]    Stop Gorgone And Remove Gorgone Config    @{process_list}    sql_file=${ROOT_CONFIG}db_delete_poller.sql
    @{process_list}    Set Variable    pullwss_gorgone_poller_2    pullwss_gorgone_central
    Log To Console    \nStarting the gorgone setup

    Setup Two Gorgone Instances    communication_mode=pullwss    central_name=pullwss_gorgone_central    poller_name=pullwss_gorgone_poller_2
    Ctn Check No Error In Logs    pullwss_gorgone_poller_2
    Log To Console    End of tests.

gorgone can send many log, even over the pullwss message limit
    [Tags]    overflow
    [Teardown]    Stop Gorgone And Remove Gorgone Config    @{process_list}    sql_file=${ROOT_CONFIG}db_delete_poller.sql
    @{process_list}    Set Variable    pullwss_gorgone_poller_2    pullwss_gorgone_central
    Log To Console    \nStarting the gorgone setup

    Setup Two Gorgone Instances    communication_mode=pullwss    central_name=pullwss_gorgone_central    poller_name=pullwss_gorgone_poller_2
    Ctn Check No Error In Logs    pullwss_gorgone_poller_2

    Connect To Database
    ...    sqlite3
    ...    database=/etc/centreon-gorgone/${process_list}[1]/history.sdb
    ...    isolation_level=${None}
    ...    alias=sqlite_central

    Connect To Database
    ...    sqlite3
    ...    database=/etc/centreon-gorgone/${process_list}[0]/history.sdb
    ...    isolation_level=${None}
    ...    alias=sqlite_poller
    Execute SQL String    DELETE FROM gorgone_history    alias=sqlite_central
    Execute SQL String    DELETE FROM gorgone_history    alias=sqlite_poller
    Execute SQL String    VACUUM    alias=sqlite_poller

    FOR    ${i}    IN RANGE    3
        ${log_count}=    Evaluate    ${log_count} + 1000
        ${token}=    Create Many Sqlite Log    @{process_list}    log_size=${log_size}    log_count=${log_count}
        Get Log From Central    @{process_list}    token=${token}    log_count=${log_count}
    END
     Log To Console    End of tests.
*** Keywords ***
Get Log From Central
    [Documentation]    This use the api to request logs from the poller, then wait in the database for every logs.
    # TODO : the api should be updated because the max number of logs sent back is now limited. New "offset" option allows to get the rest of the log but not to increase the size of the answer.
    [Arguments]    @{process_list}    ${token}    ${log_count}=10

    ${status}    ${logs}    Ctn Get Api Log With Timeout    token=${token}    node_path=nodes/2/    timeout=1
    Check Row Count    SELECT * FROM gorgone_history WHERE token = '${token}'    ==    ${log_count}    retry_timeout=40s    retry_pause=5s    alias=sqlite_central

Create Many Sqlite Log
    [Arguments]    @{process_list}    ${log_size}=1000    ${log_count}=100000

    Log To Console    /etc/centreon-gorgone/${process_list}[0]/history.sdb
    ${token}=    Generate Random String    5
    Log To Console    token is : ${token}


    ${time}=    Get Current Date    result_format=epoch
    # this will simulate the fact that the log should never normally have the same timestamp for each log.
    # each log will be 0.0002s apart.
    ${time}=    Evaluate    ${time} - ${log_count} * 0.0002
    FOR    ${i}    IN RANGE    ${log_count}
        ${string}=    Generate Random String    ${log_size}
        ${time}=    Evaluate    ${time} + 0.0001
        Execute SQL String    INSERT INTO gorgone_history (token, code, etime, ctime, data) VALUES ('${token}', 2, ${time}, ${time}, 'This is test ${i}. ${string}');    alias=sqlite_poller

    END
    Log To Console    finished to create ${log_count} logs in poller db.
    RETURN    ${token}
