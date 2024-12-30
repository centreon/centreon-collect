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
    Ctn Config Engine    ${1}    ${5}    ${5}
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
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg

    Ctn Create Severities File    ${0}    ${10}
    Ctn Config Engine Add Cfg File    ${0}    severities.cfg

    # Hostgroups configuration
    Ctn Add Host Group    ${0}    ${1}    ["host_2","host_3"]
    Ctn Add Host Group    ${0}    ${2}    ["host_4"]
    
    # Servicegroups configuration
    Ctn Add Service Group    ${0}    1    ["host_1","service_1"]
    Ctn Add Service Group    ${0}    2    ["host_2","service_6"]

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
    ${tags}    Ctn Engine Config Extractor Tags    ${content}[tags]    1    ${0}
    ${severities}    Ctn Engine Config Extractor Tags    ${content}[severities]    1    ${0}
    Should Be Equal As Strings   ${tags}[tagName]    tag1
    Should Be Equal As Strings     ${severities}[severityName]   severity1

    #check hostgroups
    ${hostgroup}    Ctn Engine Config Extractor Hostgroup   ${content}[hostgroups]    hostgroup_1
    Should Be Equal As Numbers   ${hostgroup}[hostgroupId]    1

    ${hostgroup}    Ctn Engine Config Extractor Hostgroup   ${content}[hostgroups]    hostgroup_2
    Should Be Equal As Numbers   ${hostgroup}[hostgroupId]    2

    #check servicegroups
    ${servicegroup}    Ctn Engine Config Extractor servicegroup   ${content}[servicegroups]    servicegroup_1
    Should Be Equal As Numbers   ${servicegroup}[servicegroupId]    1

    ${servicegroup}    Ctn Engine Config Extractor servicegroup   ${content}[servicegroups]    servicegroup_2
    Should Be Equal As Numbers   ${servicegroup}[servicegroupId]    2

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

    # add tag 50 and severity 50
    Ctn Add Tag    0    50    tag50    servicegroup
    
    # add new hostgroup 
    Ctn Add Host Group    ${0}    ${3}    ["host_5"]
    Ctn Add Service Group    ${0}    ${3}    ["host_5","service_21"]

    # modify host group 1
    Ctn Engine Config Set Key Value In Cfg    0    hostgroup_1    alias    hostgroup1_changed    hostgroups.cfg
    Ctn Engine Config Set Key Value In Cfg    0    servicegroup_1    alias    servicegroup1_changed    servicegroups.cfg

    # delete host group 2
    Ctn Engine Config Del Block In Cfg    0    hostgroup    hostgroup_2    hostgroups.cfg
    Ctn Engine Config Del Block In Cfg    0    servicegroup    servicegroup_2    servicegroups.cfg

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

    Log To Console    Check the change in GPRC
    ${tags}    Ctn Engine Config Extractor Tags    ${content}[tags]    1    ${0}
    ${severities}    Ctn Engine Config Extractor Tags    ${content}[severities]    1    ${0}
    Should Be Equal As Strings   ${tags}[tagName]    tag1_changed
    Should Be Equal As Strings     ${severities}[severityName]   severity1_changed
    
    ${tags}    Ctn Engine Config Extractor Tags    ${content}[tags]    50    ${0}
    Should Be Equal As Strings   ${tags}[tagName]    tag50

    ${tags}    Ctn Engine Config Extractor Tags    ${content}[tags]    3    ${0}
    Should Be True   ${tags}==None   tag id:3 type:0 should have been deleted

    ${tags}    Ctn Engine Config Extractor Tags   ${content}[tags]    3    ${1}
    Should Be True   ${tags}==None   tag id:3 type:1 should have been deleted
    #check hostgroups
    ${hostgroup}    Ctn Engine Config Extractor Hostgroup   ${content}[hostgroups]    hostgroup_1
    Should Be Equal As Strings   ${hostgroup}[alias]    hostgroup1_changed

    ${hostgroup}    Ctn Engine Config Extractor Hostgroup   ${content}[hostgroups]    hostgroup_2
    Should Be True   ${hostgroup}==None   hostgroup_2 should have been deleted

    ${hostgroup}    Ctn Engine Config Extractor Hostgroup   ${content}[hostgroups]    hostgroup_3
    Should Be Equal As Numbers   ${hostgroup}[hostgroupId]    3

    #check servicegroups
    ${servicegroup}    Ctn Engine Config Extractor servicegroup   ${content}[servicegroups]    servicegroup_1
    Should Be Equal As Strings   ${servicegroup}[alias]    servicegroup1_changed

    ${servicegroup}    Ctn Engine Config Extractor servicegroup   ${content}[servicegroups]    servicegroup_2
    Should Be True   ${servicegroup}==None   servicegroup_2 should have been deleted

    ${servicegroup}    Ctn Engine Config Extractor servicegroup   ${content}[servicegroups]    servicegroup_3
    Should Be Equal As Numbers   ${servicegroup}[servicegroupId]    3

    
    Log To Console    Check the change in Db
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

    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT name FROM tags WHERE id = 50 and type = 0;
        Sleep    1s
        IF    "${output}" == "(('tag50',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('tag50',),)

    FOR    ${index}    IN RANGE    2
        ${output}    Query    SELECT name FROM tags WHERE id = 3 and type = 0;
        Sleep    1s
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()

    FOR    ${index}    IN RANGE    2
        ${output}    Query    SELECT name FROM tags WHERE id = 3 and type = 1;
        Sleep    1s
        IF    "${output}" == "(())"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()

    Ctn Stop Engine
    Ctn Kindly Stop Broker

    Disconnect From Database