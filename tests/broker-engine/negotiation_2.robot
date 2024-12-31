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
    Ctn Config Engine    ${1}    ${10}    ${1}
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
    
   # configure broker to look for the engine config
    Ctn Broker Config Add Item    central    cache_config_directory    ${VarRoot}/lib/centreon/config

    # remove any previous configuration
    Ctn Remove Prot Files    ${VarRoot}/lib

    Remove Directory    ${VarRoot}/lib/centreon/config    recursive=${True}
    Remove Directory    ${VarRoot}/lib/centreon-broker/pollers-configuration    recursive=${True}
    Create Directory    ${VarRoot}/lib/centreon/config

    # copy the configuration files old to the tmp/var/lib/centreon/config/1 directory
    Remove Directory    ${EtcRoot}/centreon-engine/config0    recursive=${True}
    Copy Directory
    ...    /home/soufiane/c_config/old/config0
    ...    ${EtcRoot}/centreon-engine/config0
    
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

    Log To Console    Check configuration via grpc !!!!!!!
    # check tags/severities
    ${tags}    Ctn Engine Config Extractor Tags    ${content}[tags]    1    ${0}
    Should Be Equal As Strings   ${tags}[tagName]    tag1

    ${tags}    Ctn Engine Config Extractor Tags    ${content}[tags]    2    ${0}
    Should Be Equal As Strings   ${tags}[tagName]    tag5

    ${tags}    Ctn Engine Config Extractor Tags    ${content}[tags]    3    ${0}
    Should Be True   ${tags}==None    the tag shouldn't exist

    ${severities}    Ctn Engine Config Extractor Tags    ${content}[severities]    1    ${0}
    Should Be Equal As Strings     ${severities}[severityName]   severity1

    ${severities}    Ctn Engine Config Extractor Tags    ${content}[severities]    2    ${1}
    Should Be Equal As Strings     ${severities}[severityName]   severity2

    ${severities}    Ctn Engine Config Extractor Tags    ${content}[severities]    3    ${0}
    Should Be True    ${severities}==None    the severity shouldn't exist

    # check hostgroups
    ${hostgroup}    Ctn Engine Config Extractor Key    ${content}[hostgroups]    hostgroupName    hostgroup_1
    Should Be Equal As Numbers   ${hostgroup}[hostgroupId]    1

    ${hostgroup}    Ctn Engine Config Extractor Key   ${content}[hostgroups]    hostgroupName    hostgroup_2
    Should Be Equal As Numbers   ${hostgroup}[hostgroupId]    2

    ${hostgroup}    Ctn Engine Config Extractor Key   ${content}[hostgroups]    hostgroupName    hostgroup_3
    Should Be True   ${hostgroup}==None    the hostgroup shouldn't exist

    # check servicegroups
    ${servicegroup}    Ctn Engine Config Extractor Key   ${content}[servicegroups]    servicegroupName    servicegroup_1
    Should Be Equal As Numbers   ${servicegroup}[servicegroupId]    1

    ${servicegroup}    Ctn Engine Config Extractor Key   ${content}[servicegroups]    servicegroupName    servicegroup_2
    Should Be Equal As Numbers   ${servicegroup}[servicegroupId]    2

    ${servicegroup}    Ctn Engine Config Extractor Key   ${content}[servicegroups]    servicegroupName    servicegroup_3
    Should Be True   ${servicegroup}==None    the service shouldn't exist

    # check hosts

    Log To Console    Check the configuration in DB !!!!!!!
    
    # check tags / severities
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT name FROM tags WHERE id = 1 and type = 0;
        Sleep    1s
        IF    "${output}" == "(('tag1',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('tag1',),)

    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT name FROM tags WHERE id = 2 and type = 0;
        Sleep    1s
        IF    "${output}" == "(('tag5',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('tag5',),)

    FOR    ${index}    IN RANGE    2
        ${output}    Query    SELECT name FROM tags WHERE id = 3 and type = 0;
        Sleep    1s
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()

    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT name FROM severities WHERE id = 1 and type = 0;
        Sleep    1s
        IF    "${output}" == "(('severity1',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('severity1',),)

    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT name FROM severities WHERE id = 2 and type = 1;
        Sleep    1s
        IF    "${output}" == "(('severity2',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('severity2',),)

    FOR    ${index}    IN RANGE    2
        ${output}    Query    SELECT name FROM severities WHERE id = 3 and type = 0;
        Sleep    1s
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()

    # check hostgroups
    # check servicegroups
    # check hosts

    Ctn Stop Engine

    # remove old configuration
    Remove Directory    ${VarRoot}/lib/centreon/config    recursive=${True}
    Create Directory    ${VarRoot}/lib/centreon/config

    # copy configuration files new to tmp/var/lib/centreon/config/1 directory
    Remove Directory    ${EtcRoot}/centreon-engine/config0    recursive=${True}
    Copy Directory
    ...    /home/soufiane/c_config/new/config0
    ...    ${EtcRoot}/centreon-engine/
    Copy Directory
    ...    /home/soufiane/c_config/new/config0
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

    Log To Console    Check the change in GPRC !!!!!!!
    
    # check tags/severities
    ${tags}    Ctn Engine Config Extractor Tags    ${content}[tags]    1    ${0}
    Should Be Equal As Strings   ${tags}[tagName]    tag1_changed

    ${tags}    Ctn Engine Config Extractor Tags    ${content}[tags]    2    ${0}
    Should Be True   ${tags}==None    the tag shouldn't exist

    ${tags}    Ctn Engine Config Extractor Tags    ${content}[tags]    3    ${0}
    Should Be Equal As Strings   ${tags}[tagName]    tag9

    ${severities}    Ctn Engine Config Extractor Tags    ${content}[severities]    1    ${0}
    Should Be Equal As Strings     ${severities}[severityName]   severity1_changed

    ${severities}    Ctn Engine Config Extractor Tags    ${content}[severities]    2    ${1}
    Should Be True    ${severities}==None    the severity shouldn't exist

    ${severities}    Ctn Engine Config Extractor Tags    ${content}[severities]    3    ${0}
    Should Be Equal As Strings     ${severities}[severityName]   severity3

    # check hostgroups
    ${hostgroup}    Ctn Engine Config Extractor Key    ${content}[hostgroups]    hostgroupName    hostgroup_1
    Should Be Equal As Numbers   ${hostgroup}[hostgroupId]    1
    Should Be Equal As Strings   ${hostgroup}[alias]    hostgroup_1_changed  

    ${hostgroup}    Ctn Engine Config Extractor Key   ${content}[hostgroups]    hostgroupName    hostgroup_2
    Should Be True   ${hostgroup}==None    the hostgroup shouldn't exist

    ${hostgroup}    Ctn Engine Config Extractor Key   ${content}[hostgroups]    hostgroupName    hostgroup_3
    Should Be Equal As Numbers   ${hostgroup}[hostgroupId]    3

    # check servicegroups
    ${servicegroup}    Ctn Engine Config Extractor Key   ${content}[servicegroups]    servicegroupName    servicegroup_1
    Should Be Equal As Numbers   ${servicegroup}[servicegroupId]    1
    Should Be Equal As Strings   ${servicegroup}[alias]    servicegroup_1_changed

    ${servicegroup}    Ctn Engine Config Extractor Key   ${content}[servicegroups]    servicegroupName    servicegroup_2
    Should Be True   ${servicegroup}==None    the service shouldn't exist

    ${servicegroup}    Ctn Engine Config Extractor Key   ${content}[servicegroups]    servicegroupName    servicegroup_3
    Should Be Equal As Numbers   ${servicegroup}[servicegroupId]    3
    
    # check hosts
    Log To Console    Check the change in Db !!!!!!!

    # check tags/severities
    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT name FROM tags WHERE id = 1 and type = 0;
        Sleep    1s
        IF    "${output}" == "(('tag1_changed',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('tag1_changed',),)

    FOR    ${index}    IN RANGE    2
        ${output}    Query    SELECT name FROM tags WHERE id = 2 and type = 0;
        Sleep    1s
        IF    "${output}" == "()"    BREAK
    END
    Should Be Equal As Strings    ${output}    ()

    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT name FROM tags WHERE id = 3 and type = 0;
        Sleep    1s
        IF    "${output}" == "(('tag9',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('tag9',),)

    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT name FROM severities WHERE id = 1 and type = 0;
        Sleep    1s
        IF    "${output}" == "(('severity1_changed',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('severity1_changed',),)

    # FOR    ${index}    IN RANGE    2
    #     ${output}    Query    SELECT name FROM severities WHERE id = 2 and type = 0;
    #     Sleep    1s
    #     IF    "${output}" == "()"    BREAK
    # END
    # Should Be Equal As Strings    ${output}    ()

    FOR    ${index}    IN RANGE    60
        ${output}    Query    SELECT name FROM severities WHERE id = 3 and type = 0;
        Sleep    1s
        IF    "${output}" == "(('severity3',),)"    BREAK
    END
    Should Be Equal As Strings    ${output}    (('severity3',),)
    
    # check hostgroups
    # check servicegroups
    # check hosts

    Ctn Stop Engine
    Ctn Kindly Stop Broker

    Disconnect From Database