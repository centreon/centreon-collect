*** Settings ***
Documentation       Start and stop gorgone in pullwss mode

Resource            ${CURDIR}${/}..${/}..${/}resources${/}import.resource

Suite Setup         Start Mockoon    ${ROOT_CONFIG}..${/}resources/web-api-mockoon.json
Suite Teardown      Stop Mockoon

Test Timeout        250s

*** Variables ***
@{process_list}    pullwss_gorgone_poller_2_simple    pullwss_gorgone_central_simple

*** Test Cases ***
check one poller can connect to a central and gorgone central stop first
    [Teardown]    Stop Gorgone And Remove Gorgone Config
    ...    @{process_list}
    ...   sql_file=${ROOT_CONFIG}database${/}delete_pollers.sql

    @{process_list}    Set Variable    ${mode}_gorgone_central_simple    ${mode}_gorgone_poller_2_simple
    Log To Console    \nStarting the gorgone setup
    Setup Two Gorgone Instances    communication_mode=${mode}    central_name=${mode}_gorgone_central_simple    poller_name=${mode}_gorgone_poller_2_simple
    Ctn Check No Error In Logs    ${mode}_gorgone_poller_2_simple
    Examples:    mode   --
        ...    pullwss
        ...    pullwss_uid

check a remote server can hold a poller connection
    [Tags]    remote
    [Teardown]    Stop Gorgone And Remove Gorgone Config
    ...    @{process_list}
    ...    sql_file=${ROOT_CONFIG}database${/}delete_pollers.sql

    @{process_list}    Set Variable    ${mode}_gorgone_poller_2_simple    ${mode}_gorgone_central_simple    ${mode}_gorgone_remote_simple
    Log To Console    \nStarting the gorgone setup

    Setup Remote And Poller And Central Instances    central_name=${mode}_gorgone_central_simple    poller_name=${mode}_gorgone_poller_2_simple    remote_name=${mode}_gorgone_remote_simple
    Ctn Check No Error In Logs    ${mode}_gorgone_poller_2_simple

    Examples:    mode   --
        ...    pullwss
        ...    pullwss_uid

check two poller can connect to a central
    [Teardown]    Stop Gorgone And Remove Gorgone Config
    ...    @{process_list}
    ...    sql_file=${ROOT_CONFIG}database${/}delete_pollers.sql

    @{process_list}    Set Variable    ${mode}_gorgone_poller_2_simple    ${mode}_gorgone_central_simple    ${mode}_gorgone_poller_4_simple
    Log To Console    \nStarting the gorgone setup


    ${poller_config}=    Set Variable    ${pullwss_poller_config}
    ${id}=    Set Variable    4
    ${fromname}=    Set Variable    @POLLERID@
    IF    '${mode}' == 'pullwss_uid'
        ${id}=    Set Variable    499123456
    END
     @{poller_pullwss_config}=    Create List  ${gorgone_core_config}    ${poller_config}

    Setup Two Gorgone Instances    communication_mode=${mode}    central_name=${mode}_gorgone_central_simple    poller_name=${mode}_gorgone_poller_2_simple
    # let's add a 3rd poller...

     Setup Gorgone Config
     ...    ${poller_pullwss_config}
     ...    gorgone_name=${mode}_gorgone_poller_4_simple
     ...    replace_from=${{[str($fromname)]}}
     ...    replace_to=${{[str($id)]}}
     Start Gorgone    debug    ${mode}_gorgone_poller_4_simple
     # wait until gorgone http server bind the http api port.
     Wait Until Port Is Bind    8085
     Check Poller Is Connected    port=8086    expected_nb=4
     Check Poller Communicate     4

    Ctn Check No Error In Logs    ${mode}_gorgone_poller_2_simple
    Ctn Check No Error In Logs    ${mode}_gorgone_poller_4_simple

    Examples:    mode   --
        ...    pullwss_uid
        ...    pullwss

check one poller can connect to a central with env var ${id}
    [Teardown]    Run Keywords
    ...    Remove Environment Variable    GORGONE_TOKEN
    ...    AND
    ...    Stop Gorgone And Remove Gorgone Config    @{process_list}    sql_file=${ROOT_CONFIG}database${/}delete_pollers.sql

    @{process_list}    Set Variable    ${mode}_gorgone_central_simple    ${mode}_gorgone_poller_2_simple
    Log To Console    \nStarting the gorgone setup
    Set Environment Variable    GORGONE_TOKEN    ${env_token}
    Setup Two Gorgone Instances    communication_mode=${mode}    central_name=${mode}_gorgone_central_simple    poller_name=${mode}_gorgone_poller_2_simple
    Ctn Check No Error In Logs    ${mode}_gorgone_poller_2_simple
    Examples:    id    mode         env_token    --
        ...    1    pullwss        poller-1:myPollerToken
        ...    2    pullwss_uid    poller-1:myPollerToken

check one poller cannot connect to a central with env var ${id}
    [Teardown]    Run Keywords
    ...    Remove Environment Variable    GORGONE_TOKEN
    ...    AND
    ...    Stop Gorgone And Remove Gorgone Config    @{process_list}    sql_file=${ROOT_CONFIG}database${/}delete_pollers.sql

    ${start_date}    Get Current Date
    @{process_list}    Set Variable    ${mode}_gorgone_central_simple    ${mode}_gorgone_poller_2_simple
    Log To Console    \nStarting the gorgone setup
    Set Environment Variable    GORGONE_TOKEN    ${env_token}
    Setup Two Gorgone Instances    communication_mode=${mode}    central_name=${mode}_gorgone_central_simple    poller_name=${mode}_gorgone_poller_2_simple    check_connection=False
    Check Poller Is Connected    port=8086    expected_nb=0
    Ctn Check No Error In Logs    ${mode}_gorgone_poller_2_simple
    # we need to find the message that explains the connection refusal
    ${log_central_query}    Create List    ${message_central}
    ${logs_central}    Ctn Find In Log With Timeout    log=/var/log/centreon-gorgone/${mode}_gorgone_central_simple/gorgoned.log    content=${log_central_query}    date=${start_date}
    Should Be True    ${logs_central}    Didn't find the logs in the central file : ${logs_central}
    ${log_poller_query}    Create List
    ...    [pullwss] websocket message: {"code":500,"message":"invalid token"}
    ...    [pullwss] websocket closed with status 1005
    ${logs_poller}    Ctn Find In Log With Timeout    log=/var/log/centreon-gorgone/${mode}_gorgone_poller_2_simple/gorgoned.log    content=${log_poller_query}    date=${start_date}
    Should Be True    ${logs_poller}    Didn't find the logs in the poller file : ${logs_poller}

    Examples:    id    mode    env_token    message_central    --
        ...    1    pullwss    Central-1:centralCMAtoken      [proxy-httpserver] invalid token
        ...    2    pullwss    poller-2:myPollerToken         [proxy-httpserver] invalid token
        ...    3    pullwss    poller-3:myPollerToken         [proxy-httpserver] invalid token
        ...    4    pullwss    do_not_exists:myPollerToken    [proxy-httpserver] cannot get token

check poller token revocation
    [Teardown]    Run Keywords
    ...    Remove Environment Variable    GORGONE_TOKEN
    ...    AND
    ...    Stop Gorgone And Remove Gorgone Config    @{process_list}    sql_file=${ROOT_CONFIG}database${/}delete_pollers.sql

    Set Local Variable    ${revoked_url}    http://127.0.0.1:80/set-poller-4-is-revoked
    Set Local Variable    ${expired_url}    http://127.0.0.1:80/set-poller-4-is-expired
    Set Local Variable    ${port}    8086
    Set Local Variable    ${timeout}    11
    Set Local Variable    ${sleep}    6
    Set Local Variable    ${central_log_file}    /var/log/centreon-gorgone/pullwss_gorgone_central_simple/gorgoned.log
    Set Local Variable    ${poller_log_file}    /var/log/centreon-gorgone/pullwss_gorgone_poller_2_simple/gorgoned.log
    ${log_poller_query}    Create List
    ...    [pullwss] websocket message: {"code":500,"message":"invalid token"}
    ...    [pullwss] websocket closed with status 1005

    ${start_date}    Get Current Date
    @{process_list}    Set Variable    pullwss_gorgone_central_simple    pullwss_gorgone_poller_2_simple
    Log To Console    \nStarting the gorgone setup
    Set Environment Variable    GORGONE_TOKEN    poller-4:myPollerToken
    ${response}    PUT    ${revoked_url}/false
    ${response}    PUT    ${expired_url}/false
    Setup Two Gorgone Instances    communication_mode=pullwss    central_name=pullwss_gorgone_central_simple    poller_name=pullwss_gorgone_poller_2_simple
    Ctn Check No Error In Logs    pullwss_gorgone_poller_2_simple
    ${log_central_query}    Create List    [proxy-httpserver] invalid token
    # The poller is connected
    Check Poller Is Connected    port=${port}    expected_nb=2
    ${logs_central}    Ctn Find In Log With Timeout    log=${central_log_file}    content=${log_central_query}    date=${start_date}    timeout=${timeout}
    Should Not Be True    ${logs_central}    Did find the logs in the central file : ${logs_central}
    ${logs_poller}    Ctn Find In Log With Timeout    log=${poller_log_file}    content=${log_poller_query}    date=${start_date}    timeout=${timeout}
    Should Not Be True    ${logs_poller}    Did find the logs in the poller file : ${logs_poller}
    ${start_date}    Get Current Date
    Sleep    ${sleep}
    # Still connected
    Check Poller Is Connected    port=${port}    expected_nb=2
    ${logs_central}    Ctn Find In Log With Timeout    log=${central_log_file}    content=${log_central_query}    date=${start_date}    timeout=${timeout}
    Should Not Be True    ${logs_central}    Did find the logs in the central file : ${logs_central}
    ${logs_poller}    Ctn Find In Log With Timeout    log=${poller_log_file}    content=${log_poller_query}    date=${start_date}    timeout=${timeout}
    Should Not Be True    ${logs_poller}    Did find the logs in the poller file : ${logs_poller}
    # Token revoked
    ${response}    PUT    ${revoked_url}/true
    ${start_date}    Get Current Date
    Sleep    ${sleep}
    # The poller is disconnected
    Check Poller Is Connected    port=${port}    expected_nb=0
    ${logs_central}    Ctn Find In Log With Timeout    log=${central_log_file}    content=${log_central_query}    date=${start_date}    timeout=${timeout}
    Should Be True    ${logs_central}    Didn't find the logs in the central file : ${logs_central}
    ${logs_poller}    Ctn Find In Log With Timeout    log=${poller_log_file}    content=${log_poller_query}    date=${start_date}    timeout=${timeout}
    Should Be True    ${logs_poller}    Didn't find the logs in the poller file : ${logs_poller}
    # Token revoked
    ${response}    PUT    ${revoked_url}/false
    ${start_date}    Get Current Date
    Sleep    ${sleep}
    # The poller is connected
    Check Poller Is Connected    port=${port}    expected_nb=2
    ${logs_central}    Ctn Find In Log With Timeout    log=${central_log_file}    content=${log_central_query}    date=${start_date}    timeout=${timeout}
    Should Not Be True    ${logs_central}    Did find the logs in the central file : ${logs_central}
    ${logs_poller}    Ctn Find In Log With Timeout    log=${poller_log_file}    content=${log_poller_query}    date=${start_date}    timeout=${timeout}
    Should Not Be True    ${logs_poller}    Did find the logs in the poller file : ${logs_poller}
    # Token revoked
    ${response}    PUT    ${expired_url}/true
    ${start_date}    Get Current Date
    Sleep    ${sleep}
    # The poller is disconnected
    Check Poller Is Connected    port=${port}    expected_nb=0
    ${logs_central}    Ctn Find In Log With Timeout    log=${central_log_file}    content=${log_central_query}    date=${start_date}    timeout=${timeout}
    Should Be True    ${logs_central}    Didn't find the logs in the central file : ${logs_central}
    ${logs_poller}    Ctn Find In Log With Timeout    log=${poller_log_file}    content=${log_poller_query}    date=${start_date}    timeout=${timeout}
    Should Be True    ${logs_poller}    Didn't find the logs in the poller file : ${logs_poller}