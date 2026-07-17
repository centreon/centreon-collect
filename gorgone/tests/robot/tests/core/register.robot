*** Settings ***
Documentation       check register can still override poller definition from database

Resource            ${CURDIR}${/}..${/}..${/}resources${/}import.resource
Test Timeout        220s

*** Variables ***
@{process_list}    edit    edit

*** Test Cases ***
poller defined as push in database should be overridden by register module to ${communication_mode}
    [Tags]    register
    [Teardown]    Stop Gorgone And Remove Gorgone Config    @{process_list}        sql_file=${ROOT_CONFIG}database${/}delete_pollers.sql

    ${central_name}    Set Variable    ${communication_mode}_gorgone_register_central
    ${poller_name}    Set Variable    ${communication_mode}_gorgone_register_poller_2
    @{process_list}    Set Variable    ${central_name}    ${poller_name}

    ${central_config}    Create List    ${ROOT_CONFIG}register_module_${communication_mode}.yaml    ${ROOT_CONFIG}register_${communication_mode}_node.yaml
    Setup Two Gorgone Instances
    ...    central_config=${central_config}
    ...    communication_mode=${communication_mode}
    ...    central_name=${central_name}
    ...    poller_name=${poller_name}
    ...    check_connection=False

    Check Poller Communicate     2

    Ctn Check No Error In Logs    ${poller_name}

    Examples:    communication_mode   --
        ...    pullwss
        ...    pull
