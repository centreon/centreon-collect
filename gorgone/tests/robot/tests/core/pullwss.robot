*** Settings ***
Documentation       Start and stop gorgone in pullwss mode

Resource            ${CURDIR}${/}..${/}..${/}resources${/}import.resource

Suite Setup         Start Mockoon    ${ROOT_CONFIG}..${/}resources/web-api-mockoon.json
Suite Teardown      Stop Mockoon

Test Timeout        220s

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
        ${id}=    Set Variable    44992764
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

check one poller can connect to a central with env var
    [Teardown]    Stop Gorgone And Remove Gorgone Config
    ...    @{process_list}
    ...   sql_file=${ROOT_CONFIG}database${/}delete_pollers.sql

    @{process_list}    Set Variable    ${mode}_gorgone_central_simple    ${mode}_gorgone_poller_2_simple
    Log To Console    \nStarting the gorgone setup
    Set Environment Variable    GORGONE_TOKEN    poller-1:myPollerToken
    Setup Two Gorgone Instances    communication_mode=${mode}    central_name=${mode}_gorgone_central_simple    poller_name=${mode}_gorgone_poller_2_simple
    Ctn Check No Error In Logs    ${mode}_gorgone_poller_2_simple
    Examples:    mode   --
        ...    pullwss
        ...    pullwss_uid