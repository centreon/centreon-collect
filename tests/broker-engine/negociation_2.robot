*** Settings ***
Documentation       Centreon Broker and Engine start/stop tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed

Library    Collections



*** Test Cases ***
BENOT01
    [Documentation]    To Do
    [Tags]    broker    engine    start-stop    MON-15671
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1    3.1.0
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    module0    core    error
    Ctn Broker Config Log    rrd    core    error
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    module0    bbdo    debug
    Ctn Broker Config Log    rrd    bbdo    debug

    Ctn Clear Db    severities
    Ctn Clear Db    tags


    ${home}=    Get Environment Variable    HOME

    ${file}=    Set Variable    ${HOME}/current-conf.prot
    ${file_exists}=    Run Keyword And Return Status    File Should Exist    ${file}
    Run Keyword If    ${file_exists}    Remove File    ${file}

    Ctn Remove Prot Files    ${VarRoot}/lib

    Remove Directory    ${VarRoot}/lib/centreon/config    recursive=${True}
    Remove Directory    ${VarRoot}/lib/centreon-broker/pollers-configuration    recursive=${True}
    Create Directory    ${VarRoot}/lib/centreon/config


    # configure the engine to negotiate with the central broker
    Ctn Engine Config Set Value    ${0}    broker_module    /usr/lib64/nagios/cbmod.so -c /tmp/etc/centreon-broker/central-module0.json -p /tmp/var/lib/centreon-engine/current-conf.prot    disambiguous=True
    # configure broker to look for the engine config
    Ctn Broker Config Add Item    central    cache_config_directory    ${VarRoot}/lib/centreon/config

    # remove any previous configuration

    Ctn Create Tags File    ${0}    ${10}
    Ctn Config Engine Add Cfg File    ${0}    tags.cfg

    Ctn Create Severities File    ${0}    ${10}
    Ctn Config Engine Add Cfg File    ${0}    severities.cfg

    # copy the configuration files to the tmp/var/lib/centreon/config/1 directory
    Copy Directory
    ...    ${EtcRoot}/centreon-engine/config0
    ...    ${VarRoot}/lib/centreon/config/1

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine   ${True}
    Ctn Wait For Engine To Be Ready    ${start}    1

    ${content}    Create List    BBDO: engine configuration sent to peer 'central-broker-master' with version
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that Engine is sending its configuration should be available in centengine.log

    ${content}    Create List
    ...    BBDO: Engine configuration needs to be updated
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that Engine needs an update of its configuration should be available.

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

    ${content}    Ctn Dump Conf Info Grpc
    ${tags}    Ctn Engine Config Extractor    ${content}[tags]    1    ${0}
    ${severities}    Ctn Engine Config Extractor    ${content}[severities]    1    ${0}
    Should Be Equal As Strings   ${tags}[tagName]    tag1
    Should Be Equal As Strings     ${severities}[severityName]   severity1

    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT name FROM tags WHERE id = 1 and type = 0;
        Sleep    1s
        IF    "${output}" == "(('tag1',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('tag1',),)

    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT name FROM severities WHERE id = 1 and type = 0;
        Sleep    1s
        IF    "${output}" == "(('severity1',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('severity1',),)

    Ctn Stop Engine

    # modify tag 1/2 and severity 1/2
    Ctn Engine Config Set Key Value In Cfg    0    1    tag_name    tag1_changed    tags.cfg
    Ctn Engine Config Set Key Value In Cfg    0    1    severity_name    severity1_changed    severities.cfg

    # delete tag 3 and severity 3
    Ctn Engine Config Del Block In Cfg    0    tag    3    tags.cfg
    Ctn Engine Config Del Block In Cfg    0    severity    3    severities.cfg

    # # add tag 50 and severity 50
    

    Remove Directory    ${VarRoot}/lib/centreon/config    recursive=${True}
    Create Directory    ${VarRoot}/lib/centreon/config

    Copy Directory
    ...    ${EtcRoot}/centreon-engine/config0
    ...    ${VarRoot}/lib/centreon/config/1

    ${start}    Get Current Date
    Ctn Start Engine   ${True}
    Ctn Wait For Engine To Be Ready    ${start}    1

    ${content}    Create List    BBDO: engine configuration sent to peer 'central-broker-master' with version
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that Engine is sending its configuration should be available in centengine.log

    ${content}    Create List
    ...    BBDO: Engine configuration needs to be updated
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling that Engine needs an update of its configuration should be available.

    ${content}    Ctn Dump Conf Info Grpc

    ${tags}    Ctn Engine Config Extractor    ${content}[tags]    1    ${0}
    ${severities}    Ctn Engine Config Extractor    ${content}[severities]    1    ${0}
    Should Be Equal As Strings   ${tags}[tagName]    tag1_changed
    Should Be Equal As Strings     ${severities}[severityName]   severity1_changed

    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT name FROM tags WHERE id = 1 and type = 0;
        Sleep    1s
        IF    "${output}" == "(('tag1_changed',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('tag1_changed',),)

    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT name FROM severities WHERE id = 1 and type = 0;
        Sleep    1s
        IF    "${output}" == "(('severity1_changed',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('severity1_changed',),)

    Ctn Stop Engine
    Ctn Kindly Stop Broker