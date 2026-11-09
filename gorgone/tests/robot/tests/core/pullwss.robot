*** Settings ***
Documentation       Start and stop gorgone in pullwss mode

Resource            ${CURDIR}${/}..${/}..${/}resources${/}import.resource

Suite Setup         Start Mockoon    ${ROOT_CONFIG}..${/}resources/web-api-mockoon.json
Suite Teardown      Stop Mockoon

Test Timeout        300s

*** Variables ***
@{process_list}    pullwss_gorgone_poller_2_simple    pullwss_gorgone_central_simple

*** Test Cases ***
check one poller can connect to a central multiples times in a row
    [Teardown]    Stop Gorgone And Remove Gorgone Config
    ...    @{process_list}
    ...   sql_file=${ROOT_CONFIG}database${/}delete_pollers.sql

    @{process_list}    Set Variable    ${mode}_gorgone_central_simple    ${mode}_gorgone_poller_2_simple
    Log To Console    \nStarting the gorgone setup
    Setup Two Gorgone Instances    communication_mode=${mode}    central_name=${mode}_gorgone_central_simple    poller_name=${mode}_gorgone_poller_2_simple

    Ctn Check No Error In Logs    ${mode}_gorgone_poller_2_simple

    Restart Poller And Check Connection    ${mode}_gorgone_poller_2_simple    conn_number=2

    Examples:    mode   --
        ...    pullwss
        ...    pullwss_uid

check a remote server can hold two poller connection ${mode}
    [Tags]    remote
    [Teardown]    Stop Gorgone And Remove Gorgone Config
    ...    @{process_list}
    ...    sql_file=${ROOT_CONFIG}database${/}delete_pollers.sql

    @{process_list}    Set Variable    ${mode}_gorgone_poller_2_simple    ${mode}_gorgone_central_simple    ${mode}_gorgone_remote_simple    ${mode}_gorgone_poller_4_simple
    Log To Console    \nStarting the gorgone setup

    Setup Remote And Poller And Central Instances    central_name=${mode}_gorgone_central_simple    poller_name=${mode}_gorgone_poller_2_simple    remote_name=${mode}_gorgone_remote_simple
    Ctn Check No Error In Logs    ${mode}_gorgone_poller_2_simple

    Restart Poller And Check Connection    ${mode}_gorgone_poller_2_simple    conn_number=2    api_port=${Rapi_port}    com_port=${Rcom_port}
    
    Setup New Poller    ${mode}_gorgone_poller_4_simple    comm_port=${Rcom_port}    api_port=${Rapi_port}

    Restart Poller And Check Connection    ${mode}_gorgone_poller_4_simple    conn_number=4    api_port=${Rapi_port}    com_port=${Rcom_port}

    Restart Poller And Check Connection    ${mode}_gorgone_poller_2_simple    conn_number=4    api_port=${Rapi_port}    com_port=${Rcom_port}


    Examples:    mode   --
        ...    pullwss
        ...    pullwss_uid

check two poller can connect to a central
    [Teardown]    Stop Gorgone And Remove Gorgone Config
    ...    @{process_list}
    ...    sql_file=${ROOT_CONFIG}database${/}delete_pollers.sql

    @{process_list}    Set Variable    ${mode}_gorgone_poller_2_simple    ${mode}_gorgone_central_simple    ${mode}_gorgone_poller_4_simple
    Log To Console    \nStarting the gorgone setup

    Setup Two Gorgone Instances    communication_mode=${mode}    central_name=${mode}_gorgone_central_simple    poller_name=${mode}_gorgone_poller_2_simple
    # let's add a 3rd poller...

    Setup New Poller    ${mode}_gorgone_poller_4_simple
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
        ...    1    pullwss        poller-ok-expdate:myPollerToken
        ...    2    pullwss_uid    poller-ok-expdate:myPollerToken
        ...    3    pullwss        poller-ok-noexpdate:myPollerToken
        ...    4    pullwss_uid    poller-ok-noexpdate:myPollerToken

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

    Examples:    id    mode       env_token                                 message_central    --
        ...      1     pullwss    Central-1:centralCMAtoken                 [proxy-httpserver] invalid token
        ...      2     pullwss    poller-revoked-expdate:myPollerToken      [proxy-httpserver] invalid token
        ...      3     pullwss    poller-revoked-noexpdate:myPollerToken    [proxy-httpserver] invalid token
        ...      4     pullwss    poller-expired:myPollerToken              [proxy-httpserver] invalid token
        ...      5     pullwss    do_not_exists:myPollerToken               [proxy-httpserver] cannot get token

check poller token revocation
    [Teardown]    Run Keywords
    ...    Remove Environment Variable    GORGONE_TOKEN
    ...    AND
    ...    PUT    http://127.0.0.1:80/set-api-session-expired/false
    ...    AND
    ...    Stop Gorgone And Remove Gorgone Config    @{process_list}    sql_file=${ROOT_CONFIG}database${/}delete_pollers.sql

    Set Local Variable    ${revoked_url}    http://127.0.0.1:80/set-poller-config-is-revoked
    Set Local Variable    ${api_session_expired_url}    http://127.0.0.1:80/set-api-session-expired
    Set Local Variable    ${expired_url}    http://127.0.0.1:80/set-poller-config-is-expired
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
    Set Environment Variable    GORGONE_TOKEN    poller-config:myPollerToken
    ${response}    PUT    ${revoked_url}/false
    ${response}    PUT    ${expired_url}/false
    Setup Two Gorgone Instances    communication_mode=pullwss    central_name=pullwss_gorgone_central_simple    poller_name=pullwss_gorgone_poller_2_simple
    ...    central_replace_from=${{['@HTTPSERVER_TOKEN_CHECK_INTERVAL@']}}    central_replace_to=${{['5']}}
    Ctn Check No Error In Logs    pullwss_gorgone_poller_2_simple

    # the recurring revocation check fetches every token in one call and matches locally,
    # instead of one call per connected node (is_logged_websocket()'s own one-time per-node
    # login check still uses the single-token endpoint, unaffected by this).
    ${log_list_query}    Create List    GET /centreon/api/latest/administration/tokens?
    ${logs_central}    Ctn Find In Log With Timeout    log=${central_log_file}    content=${log_list_query}    date=${start_date}    timeout=${timeout}
    Should Be True    ${logs_central}
    ...    the recurring token check must list every token in one call, not one call per node : ${logs_central}

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
    # API session of the central expired (MON-213125): the token check gets a 401,
    # gorgone logs in again and gets the token, the poller stays connected
    ${start_date}    Get Current Date    increment=-1s
    ${response}    PUT    ${api_session_expired_url}/true
    ${log_relogin_query}    Create List
    ...    Recv header: HTTP/1.1 401 Unauthorized
    ...    Send header: POST /centreon/api/latest/login
    ${logs_central}    Ctn Find In Log With Timeout    log=${central_log_file}    content=${log_relogin_query}    date=${start_date}    timeout=${timeout}
    Should Be True    ${logs_central}    gorgone did not log in again after the 401 : ${logs_central}
    Check Poller Is Connected    port=${port}    expected_nb=2
    ${logs_central}    Ctn Find In Log With Timeout    log=${central_log_file}    content=${log_central_query}    date=${start_date}    timeout=${timeout}
    Should Not Be True    ${logs_central}    Did find the logs in the central file : ${logs_central}
    ${logs_poller}    Ctn Find In Log With Timeout    log=${poller_log_file}    content=${log_poller_query}    date=${start_date}    timeout=${timeout}
    Should Not Be True    ${logs_poller}    Did find the logs in the poller file : ${logs_poller}
    # Token revoked
    ${start_date}    Get Current Date    increment=-1s
    ${response}    PUT    ${revoked_url}/true
    Sleep    ${sleep}
    # The poller is disconnected
    Check Poller Is Connected    port=${port}    expected_nb=0
    ${logs_central}    Ctn Find In Log With Timeout    log=${central_log_file}    content=${log_central_query}    date=${start_date}    timeout=${timeout}
    Should Be True    ${logs_central}    Didn't find the logs in the central file : ${logs_central}
    ${logs_poller}    Ctn Find In Log With Timeout    log=${poller_log_file}    content=${log_poller_query}    date=${start_date}    timeout=${timeout}
    Should Be True    ${logs_poller}    Didn't find the logs in the poller file : ${logs_poller}
    # Token revoked
    ${response}    PUT    ${revoked_url}/false
    ${response}    PUT    ${expired_url}/null
    # Captured only now (after the PUTs return): a legitimate rejection (still revoked) logged
    # between an earlier capture and the PUT taking effect would otherwise count as a new one.
    ${central_lines}    Run    wc -l < ${central_log_file}
    ${poller_lines}    Run    wc -l < ${poller_log_file}
    Sleep    ${sleep}
    # The poller is connected
    Check Poller Is Connected    port=${port}    expected_nb=2
    # A line-count diff, not a date search: the backoff can land a rejected retry close enough to
    # the PUT above that second-precision timestamps can't reliably tell them apart.
    ${central_new}    Run    tail -n +$((${central_lines} + 1)) ${central_log_file} | grep 'invalid token' | wc -l
    Should Be Equal As Integers    ${central_new}    0    central rejected the token after the un-revoke
    ${poller_new}    Run    tail -n +$((${poller_lines} + 1)) ${poller_log_file} | grep -E 'invalid token|websocket closed with status' | wc -l
    Should Be Equal As Integers    ${poller_new}    0    poller saw a rejection after the un-revoke
    # Token revoked
    ${start_date}    Get Current Date    increment=-1s
    ${response}    PUT    ${expired_url}/true
    Sleep    ${sleep}
    # The poller is disconnected
    Check Poller Is Connected    port=${port}    expected_nb=0
    ${logs_central}    Ctn Find In Log With Timeout    log=${central_log_file}    content=${log_central_query}    date=${start_date}    timeout=${timeout}
    Should Be True    ${logs_central}    Didn't find the logs in the central file : ${logs_central}
    ${logs_poller}    Ctn Find In Log With Timeout    log=${poller_log_file}    content=${log_poller_query}    date=${start_date}    timeout=${timeout}
    Should Be True    ${logs_poller}    Didn't find the logs in the poller file : ${logs_poller}


check httpserver doesn't drop Ping or Getlog messages under its own send traffic
    [Documentation]    Regression test for MON-214545: gorgone::class::module::event() drains the
    ...    internal socket as a side effect of send_internal_action() (the httpserver child calls it
    ...    for its own replies, e.g. the PROXYHEALTHCHECK ack). A PING or GETLOG dispatch still in
    ...    flight on that same socket at that moment used to be read and silently discarded there,
    ...    since the child handles those through its own proxy()/transmit_back() loop, not the
    ...    action_<name> convention event() looks for. check_interval/ping/synchistory_time are set
    ...    tight here so the two loops race often within the test window instead of relying on luck.
    [Teardown]    Stop Gorgone And Remove Gorgone Config    @{process_list}
    ...    sql_file=${ROOT_CONFIG}database${/}delete_pollers.sql

    Set Local Variable    ${central_log_file}    /var/log/centreon-gorgone/pullwss_gorgone_central_simple/gorgoned.log
    @{process_list}    Set Variable    pullwss_gorgone_central_simple    pullwss_gorgone_poller_2_simple
    Log To Console    \nStarting the gorgone setup
    Setup Two Gorgone Instances    communication_mode=pullwss    central_name=pullwss_gorgone_central_simple    poller_name=pullwss_gorgone_poller_2_simple
    ...    central_replace_from=${{['@PROXY_PING@', '@HTTPSERVER_CHECK_INTERVAL@', '@PROXY_SYNCHISTORY_TIME@', '@PROXY_SYNCHISTORY_TIMEOUT@']}}
    ...    central_replace_to=${{['2', '2', '3', '2']}}
    Ctn Check No Error In Logs    pullwss_gorgone_poller_2_simple

    ${start_lines}    Run    wc -l < ${central_log_file}
    Sleep    60s

    ${pongs}    Run    tail -n +$((${start_lines} + 1)) ${central_log_file} | grep 'Pong received from' | wc -l
    Should Be True    ${pongs} >= 10    too few successful Ping round trips during the window : ${pongs}
    ${setlogs}    Run    tail -n +$((${start_lines} + 1)) ${central_log_file} | grep 'Received setlogs for' | wc -l
    Should Be True    ${setlogs} >= 5    too few successful Getlog round trips during the window : ${setlogs}

    ${lost_ping}    Run    tail -n +$((${start_lines} + 1)) ${central_log_file} | grep 'Ping timeout from' | wc -l
    Should Be Equal As Integers    ${lost_ping}    0    a Ping was silently dropped : ${lost_ping}
    ${lost_getlog}    Run    tail -n +$((${start_lines} + 1)) ${central_log_file} | grep 'Getlog max timeout reached' | wc -l
    Should Be Equal As Integers    ${lost_getlog}    0    a Getlog was silently dropped : ${lost_getlog}


check pullwss poller retries fast and backs off after a central restart
    [Documentation]    Regression test for MON-214281: when the central becomes unavailable and
    ...    comes back, the poller used to wait on a fixed 60s Mojo::IOLoop recurring timer before
    ...    retrying the pullwss websocket connection, reconnecting anywhere between 0 and 60s after
    ...    the central was back depending on the timer's phase. It now retries with a capped
    ...    exponential backoff (retry_delay_min/retry_delay_max config options). This poller uses a
    ...    tight retry_delay_min=1/retry_delay_max=4 so the reconnect assertion is deterministic:
    ...    the old fixed-60s code would fail this timeout most of the time, the fixed one passes
    ...    well within it every time.
    [Teardown]    Stop Gorgone And Remove Gorgone Config
    ...    gorgone_poller_2    gorgone_central
    ...    sql_file=${ROOT_CONFIG}database${/}delete_pollers.sql

    Ctn Init Tests
    Gorgone Fix Schema
    Gorgone Execute Sql    ${ROOT_CONFIG}database/insert_central.sql

    @{central_pullwss_config}=    Create List    ${pullwss_central_config}    ${gorgone_core_config}
    @{poller_pullwss_config}=    Create List    ${gorgone_core_config}    ${pullwss_poller_config}

    Setup Gorgone Config    ${central_pullwss_config}    gorgone_name=gorgone_central
    ...    sql_file=${ROOT_CONFIG}database/insert_pullwss_poller.sql
    Setup Gorgone Config    ${poller_pullwss_config}    gorgone_name=gorgone_poller_2
    ...    replace_from=${{['@RETRY_DELAY_MIN@', '@RETRY_DELAY_MAX@']}}    replace_to=${{['1', '4']}}

    Start Gorgone    debug    gorgone_central
    Wait Until Port Is Bind    ${Ccom_port}
    Start Gorgone    debug    gorgone_poller_2
    Wait Until Port Is Bind    8085
    Check Poller Is Connected    port=${Ccom_port}    expected_nb=2
    Check Poller Communicate     2
    Ctn Check No Error In Logs    gorgone_poller_2

    ${start_date}    Get Current Date
    Stop Gorgone And Remove Gorgone Config    gorgone_central

    ${log_disconnect_query}    Create List    [pullwss] websocket closed with status
    ${logs_poller}    Ctn Find In Log With Timeout
    ...    log=/var/log/centreon-gorgone/gorgone_poller_2/gorgoned.log
    ...    content=${log_disconnect_query}
    ...    date=${start_date}
    ...    timeout=15
    Should Be True    ${logs_poller}    poller did not notice the central went away : ${logs_poller}

    Start Gorgone    debug    gorgone_central
    Wait Until Port Is Bind    ${Ccom_port}
    Wait Until Port Is Bind    8085

    ${log_reconnect_query}    Create List    [pullwss] websocket connected
    ${logs_poller}    Ctn Find In Log With Timeout
    ...    log=/var/log/centreon-gorgone/gorgone_poller_2/gorgoned.log
    ...    content=${log_reconnect_query}
    ...    date=${start_date}
    ...    timeout=20
    Should Be True    ${logs_poller}
    ...    poller should reconnect quickly (capped backoff, max 4s here) well under the old fixed 60s retry period : ${logs_poller}

    Check Poller Is Connected    port=${Ccom_port}    expected_nb=2

check constatus reflects a poller reconnection quickly after a central restart
    [Documentation]    Regression test for MON-214293: after a central restart, constatus
    ...    (ping_ok) used to only reflect a reconnected poller once the central's own
    ...    randomized first ping completed - up to proxy's ping_interval away, observed
    ...    around 65s in k3d with the 60s default - rejecting legitimate exports as if the
    ...    poller were still disconnected. A completed REGISTERNODES handshake now forces
    ...    an immediate ping. This poller sets ping=600s so a false pass can't come from
    ...    the central's own schedule firing by coincidence within the test's timeout.
    [Teardown]    Stop Gorgone And Remove Gorgone Config
    ...    gorgone_poller_2    gorgone_central
    ...    sql_file=${ROOT_CONFIG}database${/}delete_pollers.sql

    Ctn Init Tests
    Gorgone Fix Schema
    Gorgone Execute Sql    ${ROOT_CONFIG}database/insert_central.sql

    @{central_pullwss_config}=    Create List    ${pullwss_central_config}    ${gorgone_core_config}
    @{poller_pullwss_config}=    Create List    ${gorgone_core_config}    ${pullwss_poller_config}

    Setup Gorgone Config    ${central_pullwss_config}    gorgone_name=gorgone_central
    ...    sql_file=${ROOT_CONFIG}database/insert_pullwss_poller.sql
    ...    replace_from=${{['@PROXY_PING@']}}    replace_to=${{['600']}}
    Setup Gorgone Config    ${poller_pullwss_config}    gorgone_name=gorgone_poller_2

    Start Gorgone    debug    gorgone_central
    Wait Until Port Is Bind    ${Ccom_port}
    Start Gorgone    debug    gorgone_poller_2
    Wait Until Port Is Bind    8085
    Check Poller Is Connected    port=${Ccom_port}    expected_nb=2
    Check Poller Communicate     2

    ${before}    Get Current Date
    Stop Gorgone And Remove Gorgone Config    gorgone_central
    Start Gorgone    debug    gorgone_central
    Wait Until Port Is Bind    ${Ccom_port}
    Wait Until Port Is Bind    8085

    ${log_relogged}    Create List    httpserver client 2 is logged
    ${status}    Ctn Find In Log With Timeout
    ...    log=/var/log/centreon-gorgone/gorgone_central/gorgoned.log
    ...    content=${log_relogged}
    ...    date=${before}
    ...    timeout=20
    Should Be True    ${status}    poller never re-registered with the restarted central: ${status}

    ${ping_ok}    Set Variable    ${0}
    FOR    ${i}    IN RANGE    10
        ${response}    GET    http://127.0.0.1:8085/api/internal/constatus
        ${ping_ok}    Set Variable    ${response.json()}[data][2][ping_ok]
        IF    ${ping_ok} > 0    BREAK
        Sleep    1s
    END
    Should Be True    ${ping_ok} > 0
    ...    constatus still doesn't show the poller connected shortly after reconnection (ping_ok=${ping_ok})

*** Keywords ***
Restart Poller And Check Connection
    [Arguments]    ${poller_name}    ${conn_number}=2    ${api_port}=${Capi_port}    ${com_port}=8086
    Stop Gorgone And Remove Gorgone Config
    ...    ${poller_name}

    Start Gorgone    debug    ${poller_name}
    Check Poller Is Connected    port=${com_port}    expected_nb=${conn_number}    api_port=${api_port}
    Check Poller Communicate     ${conn_number}    api_port=${api_port}

Setup New Poller
    [Arguments]    ${poller_name}    ${conn_number}=4    ${mode}=pullwss    ${api_port}=${Capi_port}    ${comm_port}=8086
    ${poller_config}=    Set Variable    ${pullwss_poller_config}
    ${id}=    Set Variable    4
    ${fromname}=    Set Variable    @POLLERID@
    IF    '${mode}' == 'pullwss_uid'
        ${id}=    Set Variable    499123456
    END
    @{poller_pullwss_config}=    Create List  ${gorgone_core_config}    ${poller_config}
    Setup Gorgone Config
    ...    ${poller_pullwss_config}
    ...    gorgone_name=${poller_name}
    ...    replace_from=${{[str($fromname), '@PULLWSS_PORT@']}}
    ...    replace_to=${{[str($id), str(${comm_port})]}}
    Start Gorgone    debug    ${poller_name}
    # wait until gorgone http server bind the http api port.
    Wait Until Port Is Bind   ${api_port}
    Check Poller Is Connected    port=${comm_port}    expected_nb=4
    Check Poller Communicate     4    api_port=${api_port}

