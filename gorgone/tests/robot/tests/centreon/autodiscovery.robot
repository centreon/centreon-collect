*** Settings ***
Documentation       test gorgone autodiscovery module
Resource            ${CURDIR}${/}..${/}..${/}resources${/}import.resource
Test Timeout        220s


*** Test Cases ***
check autodiscovery ${communication_mode}
    [Documentation]    Check engine autodiscovery module
    ${central}=    Set Variable    ${communication_mode}_gorgone_central_discovery
    ${poller}=    Set Variable    ${communication_mode}_gorgone_poller2_discovery
    @{process_list}    Create List    ${central}    ${poller}
    [Teardown]    Test Teardown    @{process_list}
    Test Setup    ${communication_mode}    @{process_list}

    Test Service Disco
    Test Host disco    ${poller}
    Test Service Disco do interpret bash    ${central}

    Examples:    communication_mode   --
        ...    push_zmq
        #...    pullwss
        #...    pull

*** Keywords ***
Test Service Disco
    ${response}=    POST  http://127.0.0.1:8085/api/centreon/autodiscovery/services    data={}
    Log To Console    ${response.json()}
    Dictionary Should Not Contain Key  ${response.json()}    error    api/centreon/statistics/engine api call resulted in an error : ${response.json()}
    Check Row Count    SELECT * FROM service WHERE service_description like 'Disk-%';    equal    4    alias=conf    retry_timeout=5    retry_pause=1
    Check Row Count    SELECT service_description FROM service WHERE service_description = 'Disk-/home';    equal    1    alias=conf    retry_timeout=5    retry_pause=1

Test Service Disco do interpret bash
    [Documentation]    the 3rd service discovery is disabled by default, so we can test it separately and check there is bash interpolation possible.
    [Arguments]    ${poller_name}
    ${start_date}=   Get Current Date    increment=-1s

    ${http_body}=    Get File    ${ROOT_CONFIG}${/}autodiscovery/service-injection-http-body.json
    ${response}=    POST  http://127.0.0.1:8085/api/centreon/autodiscovery/services    data=${http_body}
    Log To Console    ${response.json()}

    ${query}    Create List    internal message: .PUTLOG.*"stdout":"toto"
    ${logs_poller}    Ctn Find In Log With Timeout    log=/var/log/centreon-gorgone/${poller_name}/gorgoned.log    content=${query}    date=${start_date}    timeout=10    regex=True
    Should Be True    ${logs_poller}    Didn't found the service injection command in the poller logs
    File Should Exist    /tmp/robotInjectionAutodiscoverychecker    File should not have been created by the autodiscovery service.

Test Host disco
    [Arguments]    ${poller_name}
    ${http_body}=    Get File    ${ROOT_CONFIG}${/}autodiscovery/host-http-body.json
    ${start_date}=   Get Current Date    increment=-1s
    ${response}=    POST  http://127.0.0.1:8085/api/nodes/1/centreon/autodiscovery/hosts   data=${http_body}
    Check Row Count    select * from mod_host_disco_host;    equal    9    alias=conf    retry_timeout=60    retry_pause=5
    # check the poller made the call and not the central.
    ${query}    Create List    .COMMAND. .discovery_10.*"command":"echo '{."discovered_items
    ${logs_poller}    Ctn Find In Log With Timeout    log=/var/log/centreon-gorgone/${poller_name}/gorgoned.log    content=${query}    date=${start_date}    timeout=10    regex=True
    Should Be True    ${logs_poller}    Didn't found the logs in the poller file: /var/log/centreon-gorgone/${poller_name}/gorgoned.log

Test Teardown
    [Arguments]    @{process_list}
    Gorgone Execute Sql    ${ROOT_CONFIG}autodiscovery${/}db-delete-autodiscovery.sql
    Stop Gorgone And Remove Gorgone Config    @{process_list}    sql_file=${ROOT_CONFIG}db_delete_poller.sql
    Stop Mockoon

Test Setup
    [Arguments]    ${communication_mode}    @{process_list}
    Start Mockoon    ${ROOT_CONFIG}..${/}resources/web-api-mockoon.json

    Gorgone Execute Sql    ${ROOT_CONFIG}autodiscovery${/}db-delete-autodiscovery.sql

    @{central_config}    Create List    ${ROOT_CONFIG}autodiscovery${/}configuration-autodiscovery.yaml    ${ROOT_CONFIG}actions.yaml
    @{poller_config}    Create List    ${ROOT_CONFIG}actions.yaml
    Setup Two Gorgone Instances    central_config=${central_config}    communication_mode=${communication_mode}    central_name=${process_list}[0]    poller_name=${process_list}[1]    poller_config=${poller_config}
    # this file depend on the nagios_server table, which is created by the gorgone setup.

    Gorgone Execute Sql    ${ROOT_CONFIG}autodiscovery${/}db-insert-autodiscovery.sql
    Gorgone Execute Sql    ${ROOT_CONFIG}autodiscovery${/}db-insert-autodiscovery-2.sql

    Connect To Database    pymysql    ${DBNAME}    ${DBUSER}    ${DBPASSWORD}    ${DBHOST}    ${DBPORT}
    ...    alias=conf    autocommit=True
