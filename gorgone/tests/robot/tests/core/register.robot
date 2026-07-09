*** Settings ***
Documentation       check register can still override poller definition from database

Resource            ${CURDIR}${/}..${/}..${/}resources${/}import.resource
Test Timeout        220s

*** Variables ***
@{process_list}    edit    edit

*** Test Cases ***
poller defined in push in database should be overridden by register module to pullwss
    [Tags]    register
    [Teardown]    Stop Gorgone And Remove Gorgone Config    @{process_list}        sql_file=${ROOT_CONFIG}database${/}delete_pollers.sql

    ${central_name}    Set Variable    ${mode}_gorgone_register_central
    ${poller_name}    Set Variable    ${mode}_gorgone_register_poller_2
    @{process_list}    Set Variable    ${central_name}    ${poller_name}
    Log To Console    \nStarting the gorgone setup
    ${central_config}    Create List    ${ROOT_CONFIG}register_module.yaml    ${ROOT_CONFIG}register_pull_node.yaml

    Ctn Init Tests
    # This change to the schema should be integrated by php team, for now we will check everything work with a temporary fix
    # this should not stay after merge on develop
    Gorgone Fix Schema
    # add central node to the database, the poller will be added after.
    Gorgone Execute Sql    ${ROOT_CONFIG}database/insert_central.sql

    @{central_pull_config}=    Copy List    ${central_config}
    Append To List    ${central_pull_config}    ${pull_central_config}    ${gorgone_core_config}

    @{poller_pull_config}    Create List    ${pull_poller_config}

    Setup Gorgone Config    ${central_pull_config}    gorgone_name=${central_name}    sql_file=${ROOT_CONFIG}database/insert_push_poller.sql
    Setup Gorgone Config    ${poller_pull_config}     gorgone_name=${poller_name}

    Start Gorgone    debug    ${central_name}
    Wait Until Port Is Bind    5556
    Start Gorgone    debug    ${poller_name}
    # wait until gorgone http server bind the http api port.
    Wait Until Port Is Bind    8085
    Check Poller Is Connected    port=5556    expected_nb=2
    Check Poller Communicate     2


    Ctn Check No Error In Logs    ${poller_name}

    Examples:    mode   --
        ...    pull
