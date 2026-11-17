*** Settings ***
Documentation       test gorgone autodiscovery module
Resource            ${CURDIR}${/}..${/}..${/}resources${/}import.resource
Test Timeout        220s


*** Test Cases ***
check autodiscovery ${communication_mode}
    [Documentation]    Check engine autodiscovery module.
    ${central}=    Set Variable    ${communication_mode}_gorgone_central_discovery
    ${poller}=    Set Variable    ${communication_mode}_gorgone_poller2_discovery
    @{process_list}    Create List    ${central}    ${poller}
    [Teardown]    Test Teardown    @{process_list}
    Test Setup    ${communication_mode}    @{process_list}

    # Service discovery command is constructed by gorgone, so we need to check vault is honored by gorgone.
    Test Service Disco
    # host discovery command is constructed by php which honor vault himself.
    Test Host disco    ${poller}
    Test Service Disco change rules are not interpreted as code    ${central}
    # This should always be last as the autodiscovery module will DIE trying to parse the output of the injection.
    Test Service Disco don't interpret bash    ${central}

    Examples:    communication_mode   --
        ...    push_zmq
        ...    push_zmq_uid
        ...    pullwss
        ...    pullwss_uid
        ...    pull

*** Keywords ***
Test Service Disco
    ${response}=    POST  http://127.0.0.1:8085/api/centreon/autodiscovery/services    data={}
    Log To Console    ${response.json()}
    Dictionary Should Not Contain Key  ${response.json()}    error    api/centreon/statistics/engine api call resulted in an error : ${response.json()}
    Check Row Count    SELECT * FROM service WHERE service_description like 'Disk-%';    equal    4    alias=conf    retry_timeout=5    retry_pause=1
    Check Row Count    SELECT service_description FROM service WHERE service_description = 'Disk-/home';    equal    1    alias=conf    retry_timeout=5    retry_pause=1

Test Service Disco don't interpret bash
    [Documentation]    the 3rd service discovery is disabled by default, so we can test it separately and check there is no bash injection possible.
    [Arguments]    ${poller_name}
    ${start_date}=   Get Current Date    increment=-1s

    ${http_body}=    Get File    ${ROOT_CONFIG}${/}autodiscovery/service-injection-http-body.json
    ${response}=    POST  http://127.0.0.1:8085/api/centreon/autodiscovery/services    data=${http_body}
    Log To Console    ${response.json()}

    ${query}    Create List    internal message: .PUTLOG.*"stdout":"toto ;touch /tmp/robotInjectionAutodiscoverychecker
    ${logs_poller}    Ctn Find In Log With Timeout    log=/var/log/centreon-gorgone/${poller_name}/gorgoned.log    content=${query}    date=${start_date}    timeout=10    regex=True
    Should Be True    ${logs_poller}    Didn't found the service injection command in the poller logs
    File Should Not Exist    /tmp/robotInjectionAutodiscoverychecker    File should not have been created by the autodiscovery service.

Test Service Disco change rules are not interpreted as code
    [Documentation]    The regexp, replacement and modifier of a rule transformation are data.
    ...    The rule 4 (disabled by default) mixes legitimate transformations with injection attempts on every field.
    ...    Legitimate ones must still be applied, injected ones must be rejected without running any code.
    [Arguments]    ${central_name}
    @{markers}    Create List    Modifier    EFlag    Replace    Regexp    Delimiter
    FOR    ${marker}    IN    @{markers}
        Remove File    /tmp/robotChangeVars${marker}
    END
    ${start_date}=   Get Current Date    increment=-1s

    ${http_body}=    Get File    ${ROOT_CONFIG}${/}autodiscovery/service-change-vars-http-body.json
    ${response}=    POST  http://127.0.0.1:8085/api/centreon/autodiscovery/services    data=${http_body}
    Log To Console    ${response.json()}
    Dictionary Should Not Contain Key  ${response.json()}    error    api/centreon/autodiscovery/services api call resulted in an error : ${response.json()}

    # '$1' backreference on an attribute, then 'g' and 'i' flags on the service name: 'Sec-/var/lib/docker' -> 'Safe-VAR-lib_docker'.
    Check Row Count    SELECT * FROM service WHERE service_description like 'Safe-%';    equal    5    alias=conf    retry_timeout=15    retry_pause=1
    Check Row Count    SELECT * FROM service WHERE service_description = 'Safe-VAR-lib_docker';    equal    1    alias=conf
    Check Row Count    SELECT * FROM service WHERE service_description = 'Safe-_run_user_1001';    equal    1    alias=conf
    Check Row Count    SELECT * FROM service WHERE service_description like 'Sec-%';    equal    0    alias=conf

    # services are created after every transformation has been applied: no marker can show up later.
    @{run_injections}    Create List
    FOR    ${marker}    IN    @{markers}
        ${exists}    Run Keyword And Return Status    File Should Exist    /tmp/robotChangeVars${marker}
        IF    ${exists}    Append To List    ${run_injections}    ${marker}
    END
    Should Be Empty    ${run_injections}    Perl code injected in rule transformations has been run by gorgone: ${run_injections}

    ${query}    Create List    change: invalid modifier 'e'    change: invalid regexp
    ${logs_central}    Ctn Find In Log With Timeout    log=/var/log/centreon-gorgone/${central_name}/gorgoned.log    content=${query}    date=${start_date}    timeout=10
    Should Be True    ${logs_central}    Rejected transformations should be logged in /var/log/centreon-gorgone/${central_name}/gorgoned.log


Test Host disco
    [Arguments]    ${poller_name}
    ${http_body}=    Get File    ${ROOT_CONFIG}${/}autodiscovery/host-http-body.json
    ${start_date}=   Get Current Date    increment=-1s
    ${response}=    POST  http://127.0.0.1:8085/api/nodes/1/centreon/autodiscovery/hosts   data=${http_body}
    Check Row Count    select * from mod_host_disco_host;    equal    9    alias=conf    retry_timeout=180    retry_pause=5
    # check the poller made the call and not the central.
    ${query}    Create List    .COMMAND. .discovery_10.*"command":"echo '{."discovered_items
    ${logs_poller}    Ctn Find In Log With Timeout    log=/var/log/centreon-gorgone/${poller_name}/gorgoned.log    content=${query}    date=${start_date}    timeout=10    regex=True
    Should Be True    ${logs_poller}    Didn't found the logs in the poller file: /var/log/centreon-gorgone/${poller_name}/gorgoned.log

Test Teardown
    [Arguments]    @{process_list}
    Gorgone Execute Sql    ${ROOT_CONFIG}autodiscovery${/}db-delete-autodiscovery.sql
    Stop Gorgone And Remove Gorgone Config    @{process_list}    sql_file=${ROOT_CONFIG}database/delete_pollers.sql
    Stop Mockoon

Test Setup
    [Arguments]    ${communication_mode}    @{process_list}
    Start Mockoon    ${ROOT_CONFIG}..${/}resources/web-api-mockoon.json
    Setup Vault

    Ctn Init Tests
    Gorgone Execute Sql    ${ROOT_CONFIG}autodiscovery${/}db-delete-autodiscovery.sql

    @{central_config}    Create List    ${ROOT_CONFIG}autodiscovery${/}configuration-autodiscovery.yaml    ${ROOT_CONFIG}actions.yaml
    @{poller_config}    Create List    ${ROOT_CONFIG}actions.yaml
    Setup Two Gorgone Instances    central_config=${central_config}    communication_mode=${communication_mode}    central_name=${process_list}[0]    poller_name=${process_list}[1]    poller_config=${poller_config}
    # this file depend on the nagios_server table, which is created by the gorgone setup.

    Gorgone Execute Sql    ${ROOT_CONFIG}autodiscovery${/}db-insert-autodiscovery.sql
    Gorgone Execute Sql    ${ROOT_CONFIG}autodiscovery${/}db-insert-autodiscovery-2.sql

    Connect To Database    pymysql    ${DBNAME}    ${DBUSER}    ${DBPASSWORD}    ${DBHOST}    ${DBPORT}
    ...    alias=conf    autocommit=True
