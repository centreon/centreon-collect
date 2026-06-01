*** Settings ***
Documentation       Start and stop gorgone in pullwss mode

Resource            ${CURDIR}${/}..${/}..${/}resources${/}import.resource
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

check one poller can connect to a central and gorgone poller stop first
    [Teardown]    Stop Gorgone And Remove Gorgone Config
    ...    @{process_list}
    ...    sql_file=${ROOT_CONFIG}database${/}delete_pollers.sql

    @{process_list}    Set Variable    ${mode}_gorgone_poller_2_simple    ${mode}_gorgone_central_simple
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