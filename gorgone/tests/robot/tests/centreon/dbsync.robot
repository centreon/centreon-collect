*** Settings ***
Documentation       check gorgone can send many log, even over the pullwss message size limit.
# Check every communication_mode although only pullwss had a problem.

Resource            ${CURDIR}${/}..${/}..${/}resources${/}import.resource
Test Timeout        400s
*** Variables ***
@{process_list}    pullwss_gorgone_poller_2    pullwss_gorgone_central


*** Test Cases ***
send many log by ${communication_mode}, expect all of them on the central
    [Tags]    overflow
    [Teardown]    Stop Gorgone And Remove Gorgone Config    @{process_list}    sql_file=${ROOT_CONFIG}db_delete_poller.sql
    @{process_list}    Set Variable    pullwss_gorgone_poller_2    pullwss_gorgone_central
    Log To Console    \nStarting the gorgone setup

    Setup Two Gorgone Instances    communication_mode=pullwss    central_name=pullwss_gorgone_central    poller_name=pullwss_gorgone_poller_2
    #Ctn Check No Error In Logs    pullwss_gorgone_poller_2

    Connect To Database
    ...    sqlite3
    ...    database=/etc/centreon-gorgone/${process_list}[1]/history.sdb
    ...    alias=sqlite_central

    Connect To Database
    ...    sqlite3
    ...    database=/etc/centreon-gorgone/${process_list}[0]/history.sdb
    ...    alias=sqlite_poller
    Execute SQL String    DELETE FROM gorgone_history    alias=sqlite_central
    Execute SQL String    DELETE FROM gorgone_history    alias=sqlite_poller
    Execute SQL String    VACUUM    alias=sqlite_poller
    ${log_size}    Set Variable    200
    ${log_count}    Set Variable    3000
    ${nb_log_central}=    Set Variable    0

    FOR    ${i}    IN RANGE    3
        ${log_count}=    Evaluate    ${log_count} + 1000
        ${nb_log_central}=    Evaluate    ${nb_log_central} + ${log_count}
        ${token}=    Create Many Sqlite Log    @{process_list}    log_size=${log_size}    log_count=${log_count}
        Sleep    6s
        Get Log From Central    @{process_list}    token=${token}    log_count=${log_count}
    END

    ${output}=    Query     SELECT count(*) FROM gorgone_history    alias=sqlite_central
    ${row_count}=    Set Variable    ${output}[0][0]
    Should Be True    ${row_count} >= ${nb_log_central}    message=${row_count} logs in the central, expected at least ${nb_log_central}.
    Should Be True    ${row_count} < ${nb_log_central} + 20    message=${row_count} logs in the central, expected around ${nb_log_central}.
    Log To Console    End of tests.
    Examples:    communication_mode   --
        ...    push_zmq
        ...    pull
        ...    pullwss

*** Keywords ***
Get Log From Central
    [Documentation]    This use the api to request logs from the poller, then wait in the database for every logs.
    [Arguments]    @{process_list}    ${token}    ${log_count}=10

    ${log_nb}    Ctn Get Api Log Count With Timeout    token=${token}    node_path=nodes/2/    timeout=1
    Check Row Count    SELECT * FROM gorgone_history WHERE token = '${token}'    ==    ${log_count}    retry_timeout=40s    retry_pause=1s    alias=sqlite_central
    ${log_nb}    Ctn Get Api Log Count With Timeout    token=${token}    timeout=1

    Should Be Equal As Numbers    ${log_nb}    ${log_count}

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
