*** Settings ***
Documentation       Start and stop gorgone in pullwss mode

Resource            ${CURDIR}${/}..${/}..${/}resources${/}import.resource
Test Timeout        220s

*** Variables ***
@{process_list}    pullwss_gorgone_poller_2_simple    pullwss_gorgone_central_simple

*** Test Cases ***
check one poller can connect to a central and gorgone central stop first
    [Teardown]    Stop Gorgone And Remove Gorgone Config    @{process_list}        sql_file=${ROOT_CONFIG}db_delete_poller.sql
    @{process_list}    Set Variable    pullwss_gorgone_central_simple    pullwss_gorgone_poller_2_simple
    Log To Console    \nStarting the gorgone setup
    Setup Two Gorgone Instances    communication_mode=pullwss    central_name=pullwss_gorgone_central_simple    poller_name=pullwss_gorgone_poller_2_simple
    Ctn Check No Error In Logs    pullwss_gorgone_poller_2
    Log To Console    End of tests.

check one poller can connect to a central and gorgone poller stop first
    [Teardown]    Stop Gorgone And Remove Gorgone Config    @{process_list}    sql_file=${ROOT_CONFIG}db_delete_poller.sql
    @{process_list}    Set Variable    pullwss_gorgone_poller_2_simple    pullwss_gorgone_central_simple
    Log To Console    \nStarting the gorgone setup

    Setup Two Gorgone Instances    communication_mode=pullwss    central_name=pullwss_gorgone_central_simple    poller_name=pullwss_gorgone_poller_2_simple
    Ctn Check No Error In Logs    pullwss_gorgone_poller_2
    Log To Console    End of tests.
