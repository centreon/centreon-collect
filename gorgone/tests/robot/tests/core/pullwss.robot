*** Settings ***
Documentation       Start and stop gorgone in pullwss mode

Resource            ${CURDIR}${/}..${/}..${/}resources${/}import.resource
Test Timeout        620s

*** Variables ***
${central_process}    pullwss_gorgone_central_simple
${poller_process}    pullwss_gorgone_poller_2_simple
@{process_list}    ${central_process}    ${poller_process}

*** Test Cases ***
check ${id} poller get disconnected if token change in DB.
    [Teardown]    Stop Gorgone And Remove Gorgone Config    @{process_list}        sql_file=${ROOT_CONFIG}database/delete_pollers.sql
    Log To Console    \nStarting the gorgone setup
    Setup Two Gorgone Instances    communication_mode=pullwss    central_name=${central_process}    poller_name=${poller_process}
    Ctn Check No Error In Logs    ${poller_process}
    Sleep    3
    Execute Sql String    ${sql_string}
    ${start_date}    Get Current Date    increment=-1s
    ${api_resp}    POST    http://127.0.0.1:8085/api/centreon/nodes/sync    data={}

    ${log_poller2_query}    Create List    websocket message: {"code":500,"message":"authentication failed"}
    ${logs_poller}    Ctn Find In Log With Timeout
    ...    log=/var/log/centreon-gorgone/${poller_process}/gorgoned.log
    ...    content=${log_poller2_query}
    ...    date=${start_date}
    ...    timeout=65
    Should Be True    ${logs_poller}    can't find connexion failure log

    
    Examples:    id    sql_string   --
        ...    1    UPDATE nagios_server SET gorgone_auth_token = 'wrongToken' WHERE id = 2
        ...    2    DELETE FROM nagios_server WHERE id = 2


#pullwss use token from DB or conf
#    [Documentation]    check a central honor the token in the yaml configuration
#    # by default robot don't set a global token in the configuration file, so let's do it manually to change the configuration to set an empty token, and add the token in the db for the poller.
#    # this is a copy paste from Setup Two Gorgone Instances with a sed to change configuration
#    [Setup]    Start Central With Token In Yaml    secret_token
#
#    [Template]    Run A Poller
#    secret_token_db    secret_token_db    True
#    secret_token_db    secret_token    False
#    secret_token_db    secret_token_wrong    False
#    ${EMPTY}           ${EMPTY}    False
#    ${EMPTY}           secret_token    True
#    ${EMPTY}    secret_token_wrong    False
#    [Teardown]    Stop Gorgone And Remove Gorgone Config    @{process_list}    sql_file=${ROOT_CONFIG}database/delete_pollers.sql
#
#pullwss use token from DB conf token empty
#    [Setup]    Start Central With Token In Yaml    ${EMPTY}
#
#    [Template]    Run A Poller
#    secret_token_db    secret_token_db       True
#    secret_token_db    secret_token          False
#    secret_token_db    secret_token_wrong    False
#    ${EMPTY}           ${EMPTY}              False
#    ${EMPTY}           secret_token          False
#    ${EMPTY}           secret_token_wrong    False
#    [Teardown]    Stop Gorgone And Remove Gorgone Config    @{process_list}    sql_file=${ROOT_CONFIG}database/delete_pollers.sql

*** Keywords ***
Run A Poller
    [Documentation]    TODO
    [Arguments]    ${db_token}    ${poller_token}    ${connect}

    Log To Console    \nStarting the gorgone setup
    Connect To Database    pymysql    ${DBNAME}    ${DBUSER}    ${DBPASSWORD}    ${DBHOST}    ${DBPORT}
    ...    alias=conf    autocommit=True

    ${result}    Run    sed -i -e 's/token: .*/token: "${poller_token}"/g' /etc/centreon-gorgone/${poller_process}/config.d/pullwss_poller_config.yaml

    Execute Sql String    UPDATE nagios_server SET gorgone_auth_token = '${db_token}' WHERE id = 2
    ${api_resp}=    POST    http://127.0.0.1:8085/api/centreon/nodes/sync    data={}
    Sleep    0.5
    Start Gorgone    debug    ${poller_process}
    ${start_date}    Get Current Date    increment=-1s
    IF    ${connect} == False
        # this poller Should not be able to connect.
        ${log_poller2_query}    Create List    websocket message: {"code":500,"message":"authentication failed"}
        ${logs_poller}    Ctn Find In Log With Timeout    log=/var/log/centreon-gorgone/${poller_process}/gorgoned.log    content=${log_poller2_query}    date=${start_date}    timeout=30
    ELSE
        # check poller communicate with the central.
        Check Poller Is Connected    port=8086    expected_nb=2
        Check Poller Communicate     2
    END

    ${result}    Terminate Process    ${poller_process}
    Should Be True    ${result.rc} == -15 or ${result.rc} == 0    Gorgone ${poller_process} badly stopped, code returned is ${result.rc}.


Start central With Token In Yaml
    [Documentation]    todo
    [Arguments]    ${token}

    ${result}    Run    perl /usr/local/bin/gorgone_key_generation.pl
    # add central node to the database
    Gorgone Execute Sql    ${ROOT_CONFIG}database/insert_central.sql

    @{central_pullwss_config}=    Create List

    Append To List    ${central_pullwss_config}    ${pullwss_central_config}    ${gorgone_core_config}
    @{poller_pullwss_config}=    Create List
    Append To List    ${poller_pullwss_config}    ${gorgone_core_config}    ${pullwss_poller_config}

    Setup Gorgone Config    ${central_pullwss_config}    gorgone_name=${central_process}    sql_file=${ROOT_CONFIG}database/insert_pullwss_poller.sql
    Setup Gorgone Config    ${poller_pullwss_config}     gorgone_name=${poller_process}

    # let's remove the token from the central conf to force db check
    ${result}    Run    sed -i -e 's/# token: "secret_token"/token: "${token}" #/g' /etc/centreon-gorgone/${central_process}/config.d/pullwss_central_config.yaml
    Start Gorgone    debug    ${central_process}
    Wait Until Port Is Bind    8086
    Wait Until Port Is Bind    8085
