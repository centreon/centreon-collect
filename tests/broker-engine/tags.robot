*** Settings ***
Documentation       Engine/Broker tests on tags.

Resource            ../resources/resources.robot
Library             DatabaseLibrary
Library             Process
Library             DateTime
Library             OperatingSystem
Library             ../resources/Engine.py
Library             ../resources/Broker.py
Library             ../resources/Common.py
Library             ../resources/specific-duplication.py

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Init Test
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BETAG1
    [Documentation]    Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Broker is started before.
    [Tags]    broker    engine    protobuf    bbdo    tags
    Ctn Config Engine    ${1}
    Ctn Create Tags File    ${0}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    Ctn Start Broker
    ${start}    Get Current Date
    Ctn Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ${result}    Ctn Check Tag With Timeout    tag20    3    30
    Should Be True    ${result}    tag20 should be of type 3
    ${result}    Ctn Check Tag With Timeout    tag1    0    30
    Should Be True    ${result}    tag1 should be of type 0
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BETAG2
    [Documentation]    Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
    [Tags]    broker    engine    protobuf    bbdo    tags
    Ctn Config Engine    ${1}
    Ctn Create Tags File    ${0}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    Sleep    1s
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ${result}    Ctn Check Tag With Timeout    tag20    3    30
    Should Be True    ${result}    tag20 should be of type 3
    ${result}    Ctn Check Tag With Timeout    tag1    0    30
    Should Be True    ${result}    tag1 should be of type 0
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEUTAG1
    [Documentation]    Engine is configured with some tags. When broker receives them through unified_sql stream, it stores them in the centreon_storage.tags table. Broker is started before.
    [Tags]    broker    engine    protobuf    bbdo    tags    unified_sql
    Ctn Config Engine    ${1}
    Ctn Create Tags File    ${0}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.0
    Ctn Broker Config Add Item    central    bbdo_version    3.0.0
    Ctn Broker Config Add Item    rrd    bbdo_version    3.0.0
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    Ctn Start Broker
    ${start}    Get Current Date
    Ctn Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ${result}    Ctn Check Tag With Timeout    tag20    3    30
    Should Be True    ${result}    tag20 should be of type 3
    ${result}    Ctn Check Tag With Timeout    tag1    0    30
    Should Be True    ${result}    tag1 should be of type 0
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEUTAG2
    [Documentation]    Engine is configured with some tags. A new service is added with a tag. Broker should make the relations.
    [Tags]    broker    engine    protobuf    bbdo    tags    unified_sql
    Ctn Config Engine    ${1}
    Ctn Create Tags File    ${0}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.0
    Ctn Broker Config Add Item    central    bbdo_version    3.0.0
    Ctn Broker Config Add Item    rrd    bbdo_version    3.0.0
    Ctn Broker Config Output Set    central    central-broker-unified-sql    connections_count    1
    Ctn Broker Config Output Set    central    central-broker-unified-sql    queries_per_transaction    1
    Ctn Broker Config Output Set    central    central-broker-unified-sql    read_timeout    1
    Ctn Broker Config Output Set    central    central-broker-unified-sql    retry_interval    5
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    error
    Ctn Clear Retention
    Ctn Start Broker
    ${start}    Get Current Date
    Ctn Start Engine

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ${svc}    Ctn Create Service    ${0}    1    1
    Ctn Add Tags To Services    ${0}    group_tags    4    [${svc}]

    Ctn Stop Engine
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Reload Broker

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ${result}    Ctn Check Resources Tags With Timeout    1    ${svc}    servicegroup    [4]    60
    Should Be True    ${result}    New service should have a service group tag of id 4.
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEUTAG3
    [Documentation]    Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.tags table. Engine is started before.
    [Tags]    broker    engine    protobuf    bbdo    tags    unified_sql
    Ctn Config Engine    ${1}
    Ctn Create Tags File    ${0}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.0
    Ctn Broker Config Add Item    central    bbdo_version    3.0.0
    Ctn Broker Config Add Item    rrd    bbdo_version    3.0.0
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    Sleep    1s
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ${result}    Ctn Check Tag With Timeout    tag20    3    30
    Should Be True    ${result}    tag20 should be of type 3
    ${result}    Ctn Check Tag With Timeout    tag1    0    30
    Should Be True    ${result}    tag1 should be of type 0
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEUTAG4
    [Documentation]    Engine is configured with some tags. Group tags tag9, tag13 are set to services 1 and 3. Category tags tag3 and tag11 are added to services 1, 3, 5 and 6. The centreon_storage.resources and resources_tags tables are well filled.
    [Tags]    broker    engine    protobuf    bbdo    tags    unified_sql
    # Clear Db    tags
    Ctn Config Engine    ${1}
    Ctn Create Tags File    ${0}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Add Tags To Services    ${0}    group_tags    4,5    [1, 3]
    Ctn Add Tags To Services    ${0}    category_tags    2,4    [3, 5, 6]
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.0
    Ctn Broker Config Add Item    central    bbdo_version    3.0.0
    Ctn Broker Config Add Item    rrd    bbdo_version    3.0.0
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Engine
    Sleep    1s
    Ctn Start Broker

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ${result}    Ctn Check Resources Tags With Timeout    1    1    servicegroup    [4, 5]    60
    Should Be True    ${result}    Service (1, 1) should have servicegroup tag ids 4 and 5
    ${result}    Ctn Check Resources Tags With Timeout    1    3    servicegroup    [4, 5]    60
    Should Be True    ${result}    Service (1, 3) should have servicegroup tag ids 4, 5
    ${result}    Ctn Check Resources Tags With Timeout    1    3    servicecategory    [2, 4]    60
    Should Be True    ${result}    Service (1, 3) should have servicecategory tag ids 2, 4
    ${result}    Ctn Check Resources Tags With Timeout    1    5    servicecategory    [2, 4]    60
    Should Be True    ${result}    Service (1, 5) should have servicecategory tag ids 2, 4
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEUTAG5
    [Documentation]    Engine is configured with some tags. Group tags tag2, tag6 are set to hosts 1 and 2. Category tags tag4 and tag8 are added to hosts 2, 3, 4. The resources and resources_tags tables are well filled.
    [Tags]    broker    engine    protobuf    bbdo    tags
    # Clear Db    tags
    Ctn Config Engine    ${1}
    Ctn Create Tags File    ${0}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Add Tags To Hosts    ${0}    group_tags    2,3    [1, 2]
    Ctn Add Tags To Hosts    ${0}    category_tags    2,3    [2, 3, 4]
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.0
    Ctn Broker Config Add Item    central    bbdo_version    3.0.0
    Ctn Broker Config Add Item    rrd    bbdo_version    3.0.0
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    Sleep    1s
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ${result}    Ctn Check Resources Tags With Timeout    0    1    hostgroup    [2,3]    60
    Should Be True    ${result}    Host 1 should have hostgroup tags 2 and 3
    ${result}    Ctn Check Resources Tags With Timeout    0    2    hostgroup    [2,3]    60
    Should Be True    ${result}    Host 2 should have hostgroup tags 2 and 3
    ${result}    Ctn Check Resources Tags With Timeout    0    2    hostcategory    [2, 3]    60
    Should Be True    ${result}    Host 2 should have hostcategory tags 2 and 3
    ${result}    Ctn Check Resources Tags With Timeout    0    3    hostcategory    [2, 3]    60
    Should Be True    ${result}    Host 3 should have hostcategory tags 2 and 3
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEUTAG6
    [Documentation]    Engine is configured with some tags. When broker receives them, it stores them in the centreon_storage.resources_tags table. Engine is started before.
    [Tags]    broker    engine    protobuf    bbdo    tags
    # Clear Db    tags
    Ctn Config Engine    ${1}
    Ctn Create Tags File    ${0}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Add Tags To Hosts    ${0}    group_tags    2,4    [1, 2, 3, 4]
    Ctn Add Tags To Hosts    ${0}    category_tags    1,5    [1, 2, 3, 4]
    Ctn Add Tags To Services    ${0}    group_tags    2,4    [1, 2, 3, 4]
    Ctn Add Tags To Services    ${0}    category_tags    3,5    [1, 2, 3, 4]
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.0
    Ctn Broker Config Add Item    central    bbdo_version    3.0.0
    Ctn Broker Config Add Item    rrd    bbdo_version    3.0.0
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    Sleep    1s
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ${result}    Ctn Check Resources Tags With Timeout    0    1    hostgroup    [2,4]    60
    Should Be True    ${result}    Host 1 should have hostgroup tag_id 2 and 4
    ${result}    Ctn Check Resources Tags With Timeout    0    1    hostcategory    [1,5]    60
    Should Be True    ${result}    Host 1 should have hostcategory tag_id 1 and 5
    ${result}    Ctn Check Resources Tags With Timeout    1    1    servicegroup    [2,4]    60
    Should Be True    ${result}    Service (1, 1) should have servicegroup tag_id 2 and 4.
    ${result}    Ctn Check Resources Tags With Timeout    1    1    servicecategory    [3,5]    60
    Should Be True    ${result}    Service (1, 1) should have servicecategory tag_id 3 and 5.
    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEUTAG7
    [Documentation]    Some services are configured with tags on two pollers. Then tags configuration is modified.
    [Tags]    broker    engine    protobuf    bbdo    tags    unstable
    Ctn Config Engine    ${2}
    Ctn Create Tags File    ${0}    ${20}
    Ctn Create Tags File    ${1}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Config Engine Add Cfg File    ${1}    tags.cfg
    Ctn Engine Config Set Value    ${0}    log_level_config    debug
    Ctn Engine Config Set Value    ${1}    log_level_config    debug
    Ctn Add Tags To Services    ${0}    group_tags    2,4    [1, 2, 3, 4]
    Ctn Add Tags To Services    ${0}    category_tags    3,5    [1, 2, 3, 4]
    Ctn Add Tags To Services    ${1}    group_tags    3,5    [501, 502, 503, 504]
    Ctn Add Tags To Services    ${1}    category_tags    2,4    [501, 502, 503, 504]
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${2}
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.0
    Ctn Broker Config Add Item    module1    bbdo_version    3.0.0
    Ctn Broker Config Add Item    central    bbdo_version    3.0.0
    Ctn Broker Config Add Item    rrd    bbdo_version    3.0.0
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    module1    neb    debug
    Ctn Broker Config Log    central    sql    trace
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # We check in the DB if the service (1,1) has well its servicegroup tags configured.
    ${result}    Ctn Check Resources Tags With Timeout    1    1    servicegroup    [2,4]    60
    Should Be True    ${result}    First step: Service (1, 1) should have servicegroup tags 2 and 4

    ${result}    Ctn Check Resources Tags With Timeout    26    502    servicecategory    [2,4]    60
    Should Be True    ${result}    First step: Service (26, 502) should have servicecategory tags 13, 9, 3 and 11.
    ${result}    Ctn Check Resources Tags With Timeout    26    502    servicegroup    [3,5]    60
    Should Be True    ${result}    First step: Service (26, 502) should have servicegroup tags 3 and 5.

    Ctn Remove Tags From Services    ${0}    group_tags
    Ctn Remove Tags From Services    ${0}    category_tags
    Ctn Remove Tags From Services    ${1}    group_tags
    Ctn Remove Tags From Services    ${1}    category_tags
    Ctn Create Tags File    ${0}    ${18}
    Ctn Create Tags File    ${1}    ${18}
    Ctn Add Tags To Services    ${1}    group_tags    3,5    [505, 506, 507, 508]
    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine
    Ctn Reload Broker
    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ${result}    Ctn Check Resources Tags With Timeout    26    507    servicegroup    [3,5]    60
    Should Be True    ${result}    Second step: Service (26, 507) should have servicegroup tags 3 and 5

    ${result}    Ctn Check Resources Tags With Timeout    26    508    servicegroup    [3,5]    60
    Should Be True    ${result}    Second step: Service (26, 508) should have servicegroup tags 3 and 5

    [Teardown]    Ctn Stop Engine Broker And Save Logs

BEUTAG8
    [Documentation]    Services have tags provided by templates.
    [Tags]    broker    engine    protobuf    bbdo    tags
    Ctn Config Engine    ${2}
    Ctn Create Tags File    ${0}    ${40}
    Ctn Create Tags File    ${1}    ${40}
    Ctn Create Template File    ${0}    service    group_tags    [1, 9]
    Ctn Create Template File    ${1}    service    group_tags    [5, 7]

    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Config Engine Add Cfg File    ${1}    tags.cfg
    Ctn Config Engine Add Cfg File    ${0}    serviceTemplates.cfg
    Ctn Config Engine Add Cfg File    ${1}    serviceTemplates.cfg
    Ctn Engine Config Set Value    ${0}    log_level_config    debug
    Ctn Engine Config Set Value    ${1}    log_level_config    debug
    Ctn Add Template To Services    0    service_template_1    [2, 4]
    Ctn Add Template To Services    0    service_template_2    [5, 7]
    Ctn Add Template To Services    1    service_template_1    [501, 502]
    Ctn Add Template To Services    1    service_template_2    [503, 504]

    Ctn Add Tags To Services    ${0}    category_tags    3,5    [1, 2]
    Ctn Add Tags To Services    ${1}    group_tags    1,4    [501, 502]

    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${2}
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.0
    Ctn Broker Config Add Item    module1    bbdo_version    3.0.0
    Ctn Broker Config Add Item    central    bbdo_version    3.0.0
    Ctn Broker Config Add Item    rrd    bbdo_version    3.0.0
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    module1    neb    debug
    Ctn Broker Config Log    central    sql    trace
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # We need to wait a little before reloading Engine
    ${result}    Ctn Check Resources Tags With Timeout    1    2    servicecategory    [3,5]    60
    Should Be True    ${result}    First step: Service (1, 2) should have servicecategory tags 3 and 5.
    ${result}    Ctn Check Resources Tags With Timeout    1    2    servicegroup    [1]    60
    Should Be True    ${result}    First step: Service (1, 2) should have servicegroup tag 1.

    ${result}    Ctn Check Resources Tags With Timeout    1    5    servicegroup    [9]    60
    Should Be True    ${result}    First step: Service (1, 5) should have servicegroup tag 9

    ${result}    Ctn Check Resources Tags With Timeout    26    502    servicegroup    [1,4,5]    60
    Should Be True    ${result}    First step: Service (26, 502) should have tags 1, 4 and 5

    ${result}    Ctn Check Resources Tags With Timeout    26    503    servicegroup    [7]    60
    Should Be True    ${result}    First step: Service (26, 503) should have servicegroup tag 7

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEUTAG9
    [Documentation]    hosts have tags provided by templates.
    [Tags]    broker    engine    protobuf    bbdo    tags
    Ctn Config Engine    ${2}
    Ctn Create Tags File    ${0}    ${40}
    Ctn Create Tags File    ${1}    ${40}
    Ctn Create Template File    ${0}    host    group_tags    [2, 6]
    Ctn Create Template File    ${1}    host    group_tags    [8, 9]

    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Config Engine Add Cfg File    ${1}    tags.cfg
    Ctn Config Engine Add Cfg File    ${0}    hostTemplates.cfg
    Ctn Config Engine Add Cfg File    ${1}    hostTemplates.cfg
    Ctn Engine Config Set Value    ${0}    log_level_config    debug
    Ctn Engine Config Set Value    ${1}    log_level_config    debug
    Ctn Add Template To Hosts    0    host_template_1    [9, 10]
    Ctn Add Template To Hosts    0    host_template_2    [11, 12]
    Ctn Add Template To Hosts    1    host_template_1    [30, 31]
    Ctn Add Template To Hosts    1    host_template_2    [32, 33]
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${2}
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.0
    Ctn Broker Config Add Item    module1    bbdo_version    3.0.0
    Ctn Broker Config Add Item    central    bbdo_version    3.0.0
    Ctn Broker Config Add Item    rrd    bbdo_version    3.0.0
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    module1    neb    debug
    Ctn Broker Config Log    central    sql    trace
    Ctn Clear Retention
    Sleep    1s
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    # We need to wait a little before reloading Engine
    ${result}    Ctn Check Resources Tags With Timeout    0    9    hostgroup    [2]    60
    Should Be True    ${result}    First step: resource 9 should have hostgroup tag with id=2

    ${result}    Ctn Check Resources Tags With Timeout    0    10    hostgroup    [2]    60
    Should Be True    ${result}    First step: resource 10 should have hostgroup tag with id=2

    ${result}    Ctn Check Resources Tags With Timeout    0    11    hostgroup    [6]    60
    Should Be True    ${result}    First step: resource 11 should have hostgroup tag with id=6

    ${result}    Ctn Check Resources Tags With Timeout    0    12    hostgroup    [6]    60
    Should Be True    ${result}    First step: resource 12 should have hostgroup tag with id=6

    ${result}    Ctn Check Resources Tags With Timeout    0    30    hostgroup    [8]    60
    Should Be True    ${result}    First step: resource 30 should have hostgroup tag with id=10

    ${result}    Ctn Check Resources Tags With Timeout    0    31    hostgroup    [8]    60
    Should Be True    ${result}    First step: resource 31 should have hostgroup tag with id=10

    ${result}    Ctn Check Resources Tags With Timeout    0    32    hostgroup    [9]    60
    Should Be True    ${result}    First step: resource 32 should have hostgroup tag with id=14

    ${result}    Ctn Check Resources Tags With Timeout    0    33    hostgroup    [9]    60
    Should Be True    ${result}    First step: host 33 should have hostgroup tag with id=14

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEUTAG10
    [Documentation]    some services are configured with tags on two pollers. Then tags are removed from some of them and in centreon_storage, we can observe resources_tags table updated.
    [Tags]    broker    engine    protobuf    bbdo    tags
    Ctn Config Engine    ${2}
    Ctn Create Tags File    ${0}    ${20}
    Ctn Create Tags File    ${1}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Config Engine Add Cfg File    ${1}    tags.cfg
    Ctn Engine Config Set Value    ${0}    log_level_config    debug
    Ctn Engine Config Set Value    ${1}    log_level_config    debug
    Ctn Add Tags To Services    ${0}    group_tags    2,4    [1, 2, 3, 4]
    Ctn Add Tags To Services    ${0}    category_tags    3,5    [1, 2, 3, 4]
    Ctn Add Tags To Services    ${1}    group_tags    3,5    [501, 502, 503, 504]
    Ctn Add Tags To Services    ${1}    category_tags    2,4    [501, 502, 503, 504]
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${2}
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.0
    Ctn Broker Config Add Item    module1    bbdo_version    3.0.0
    Ctn Broker Config Add Item    central    bbdo_version    3.0.0
    Ctn Broker Config Add Item    rrd    bbdo_version    3.0.0
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    module1    neb    debug
    Ctn Broker Config Log    central    sql    trace
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ${result}    Ctn Check Resources Tags With Timeout    1    4    servicegroup    [2,4]    60
    Should Be True    ${result}    First step: Service (1, 4) should have servicegroup tags 2 and 4
    ${result}    Ctn Check Resources Tags With Timeout    1    3    servicecategory    [3,5]    60
    Should Be True    ${result}    First step: Service (1, 3) should have servicecategory tags 3 and 5

    ${result}    Ctn Check Resources Tags With Timeout    26    504    servicegroup    [3,5]    60
    Should Be True    ${result}    First step: Service (26, 504) should have servicegroup tags 3 and 5.
    ${result}    Ctn Check Resources Tags With Timeout    26    503    servicecategory    [2,4]    60
    Should Be True    ${result}    First step: Service (26, 503) should have servicecategory tags 2 and 4.

    Ctn Remove Tags From Services    ${0}    group_tags
    Ctn Remove Tags From Services    ${0}    category_tags
    Ctn Remove Tags From Services    ${1}    group_tags
    Ctn Remove Tags From Services    ${1}    category_tags
    Ctn Create Tags File    ${0}    ${20}
    Ctn Create Tags File    ${1}    ${20}
    Ctn Add Tags To Services    ${0}    group_tags    2,4    [1, 2, 3]
    Ctn Add Tags To Services    ${0}    category_tags    3,5    [1, 2, 4]
    Ctn Add Tags To Services    ${1}    group_tags    3,5    [501, 502, 503]
    Ctn Add Tags To Services    ${1}    category_tags    2,4    [501, 502, 504]
    Ctn Reload Engine
    Ctn Reload Broker
    ${result}    Ctn Check Resources Tags With Timeout    1    4    servicegroup    [2,4]    60    False
    Should Be True    ${result}    Second step: Service (1, 4) should not have servicegroup tags 2 and 4

    ${result}    Ctn Check Resources Tags With Timeout    1    3    servicecategory    [3,5]    60    False
    Should Be True    ${result}    Second step: Service (1, 3) should not have servicecategory tags 3 and 5

    ${result}    Ctn Check Resources Tags With Timeout    26    504    servicegroup    [3,5]    60    False
    Should Be True    ${result}    Second step: Service (26, 504) should not have servicegroup tags 3 and 5

    ${result}    Ctn Check Resources Tags With Timeout    26    503    servicecategory    [3,5]    60    False
    Should Be True    ${result}    Second step: Service (26, 503) should not have servicecategory tags 3 and 5

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEUTAG11
    [Documentation]    some services are configured with tags on two pollers. Then several tags are removed, and we can observe resources_tags table updated.
    [Tags]    broker    engine    protobuf    bbdo    tags
    Ctn Config Engine    ${2}
    Ctn Create Tags File    ${0}    ${20}
    Ctn Create Tags File    ${1}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Config Engine Add Cfg File    ${1}    tags.cfg
    Ctn Engine Config Set Value    ${0}    log_level_config    debug
    Ctn Engine Config Set Value    ${1}    log_level_config    debug
    Ctn Add Tags To Services    ${0}    group_tags    2,4    [1, 2, 3, 4]
    Ctn Add Tags To Services    ${0}    category_tags    3,5    [1, 2, 3, 4]
    Ctn Add Tags To Services    ${1}    group_tags    3,5    [501, 502, 503, 504]
    Ctn Add Tags To Services    ${1}    category_tags    2,4    [501, 502, 503, 504]
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${2}
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.0
    Ctn Broker Config Add Item    module1    bbdo_version    3.0.0
    Ctn Broker Config Add Item    central    bbdo_version    3.0.0
    Ctn Broker Config Add Item    rrd    bbdo_version    3.0.0
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    module1    neb    debug
    Ctn Broker Config Log    central    sql    trace
    Ctn Clear Retention
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ${result}    Ctn Check Resources Tags With Timeout    1    4    servicegroup    [2,4]    60
    Should Be True    ${result}    First step: Service (1, 4) should have servicegroup tags 2 and 4
    ${result}    Ctn Check Resources Tags With Timeout    1    3    servicecategory    [3,5]    60
    Should Be True    ${result}    First step: Service (1, 3) should have servicecategory tags 3 and 5

    ${result}    Ctn Check Resources Tags With Timeout    26    504    servicegroup    [3,5]    60
    Should Be True    ${result}    First step: Service (26, 504) should have servicegroup tags 3 and 5.
    ${result}    Ctn Check Resources Tags With Timeout    26    503    servicecategory    [2,4]    60
    Should Be True    ${result}    First step: Service (26, 503) should have servicecategory tags 2 and 4.

    Ctn Remove Tags From Services    ${0}    group_tags
    Ctn Remove Tags From Services    ${0}    category_tags
    Ctn Remove Tags From Services    ${1}    group_tags
    Ctn Remove Tags From Services    ${1}    category_tags
    Ctn Create Tags File    ${0}    ${18}
    Ctn Create Tags File    ${1}    ${18}
    Ctn Add Tags To Services    ${0}    group_tags    2,4    [1, 2, 3, 4]
    Ctn Add Tags To Services    ${0}    category_tags    3    [1, 2, 3, 4]
    Ctn Add Tags To Services    ${1}    group_tags    3,5    [501, 502, 503]
    Ctn Add Tags To Services    ${1}    category_tags    2,4    [501, 502, 504]
    Ctn Reload Engine
    Ctn Reload Broker
    ${result}    Ctn Check Resources Tags With Timeout    1    4    servicegroup    [2,4]    60
    Should Be True    ${result}    Second step: Service (1, 4) should have servicegroup tags 2 and 4

    ${result}    Ctn Check Resources Tags With Timeout    1    3    servicecategory    [5]    60    False
    Should Be True    ${result}    Second step: Service (1, 3) should not have servicecategory tags 5

    ${result}    Ctn Check Resources Tags With Timeout    26    504    servicegroup    [3,5]    60    False
    Should Be True    ${result}    Second step: Service (26, 504) should not have servicegroup tags 3 and 5

    ${result}    Ctn Check Resources Tags With Timeout    26    503    servicecategory    [3,5]    60    False
    Should Be True    ${result}    Second step: Service (26, 503) should not have servicecategory tags 3 and 5

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEUTAG12
    [Documentation]    Engine is configured with some tags. Group tags tag2, tag6 are set to hosts 1 and 2. Category tags tag4 and tag8 are added to hosts 2, 3, 4. The resources and resources_tags tables are well filled. The tag6 and tag8 are removed and resources_tags is also well updated.
    [Tags]    broker    engine    protobuf    bbdo    tags
    # Clear Db    tags
    Ctn Config Engine    ${1}
    Ctn Create Tags File    ${0}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Add Tags To Hosts    ${0}    group_tags    2,3    [1, 2]
    Ctn Add Tags To Hosts    ${0}    category_tags    2,3    [2, 3, 4]
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Add Item    module0    bbdo_version    3.0.0
    Ctn Broker Config Add Item    central    bbdo_version    3.0.0
    Ctn Broker Config Add Item    rrd    bbdo_version    3.0.0
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    Sleep    1s
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ${result}    Ctn Check Resources Tags With Timeout    0    1    hostgroup    [2,3]    60
    Should Be True    ${result}    Host 1 should have hostgroup tags 2 and 3
    ${result}    Ctn Check Resources Tags With Timeout    0    2    hostgroup    [2,3]    60
    Should Be True    ${result}    Host 2 should have hostgroup tags 2 and 3
    ${result}    Ctn Check Resources Tags With Timeout    0    2    hostcategory    [2, 3]    60
    Should Be True    ${result}    Host 2 should have hostcategory tags 2 and 3
    ${result}    Ctn Check Resources Tags With Timeout    0    3    hostcategory    [2, 3]    60
    Should Be True    ${result}    Host 3 should have hostcategory tags 2 and 3

    Ctn Remove Tags From Hosts    ${0}    group_tags
    Ctn Remove Tags From Hosts    ${0}    category_tags
    Ctn Create Tags File    ${0}    ${5}
    Ctn Config Engine Add Cfg File    ${0}    tags.cfg

    Ctn Reload Engine
    Ctn Reload Broker

    ${result}    Ctn Check Resources Tags With Timeout    0    1    hostgroup    [2,3]    60    False
    Should Be True    ${result}    Host 1 should not have hostgroup tags 2 nor 3
    ${result}    Ctn Check Resources Tags With Timeout    0    2    hostgroup    [2,3]    60    False
    Should Be True    ${result}    Host 2 should not have hostgroup tags 2 nor 3
    ${result}    Ctn Check Resources Tags With Timeout    0    2    hostcategory    [2,3]    60    False
    Should Be True    ${result}    Host 2 should not have hostgroup tags 2 nor 3
    ${result}    Ctn Check Resources Tags With Timeout    0    3    hostcategory    [2,3]    60    False
    Should Be True    ${result}    Host 3 should not have hostgroup tags 2 nor 3
    ${result}    Ctn Check Resources Tags With Timeout    0    4    hostcategory    [2,3]    60    False
    Should Be True    ${result}    Host 4 should not have hostgroup tags 2 nor 3

    Ctn Stop Engine
    Ctn Kindly Stop Broker


BEUTAG_REMOVE_HOST_FROM_HOSTGROUP
    [Documentation]    Remove a host from hostgroup, reload, insert 2 host in the hostgroup must not make sql error
    [Tags]    broker    engine    tags
    Ctn Clear Db    tags
    Ctn Config Engine    ${1}
    Ctn Create Tags File    ${0}    ${3}    ${0}    hostgroup
    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Add Tags To Hosts    ${0}    group_tags    2    1
    Ctn Add Tags To Hosts    ${0}    group_tags    1    4
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config BBDO3    1
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    central    sql    trace
    Ctn Clear Retention
    Sleep    1s
    ${start}    Get Current Date
    Ctn Start Engine
    Ctn Start Broker

    # Let's wait for the external command check start
    ${content}    Create List    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling check_for_external_commands() should be available.

    ${result}    Ctn Check Resources Tags With Timeout    0    1    hostgroup    [2]    60    True
    Should Be True    ${result}    Host 1 should not have hostgroup tags 2

    ${content}    Create List    unified_sql:_check_queues
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    A message unified_sql:_check_queues should be available.

    Ctn Engine Config Remove Service Host    ${0}    host_1
    Ctn Engine Config Remove Host    0    host_1
    Ctn Engine Config Remove Tag    0    2
    Ctn Reload Engine

    ${result}    Ctn Check Resources Tags With Timeout    0    1    hostgroup    [2]    60    False
    Should Be True    ${result}    Host 1 should not have hostgroup tags 2

    # wait for commits
    ${start}    Get Current Date
    ${content}    Create List    unified_sql:_check_queues
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    A message unified_sql:_check_queues should be available.

    Sleep    5

    Ctn Create Tags File    ${0}    ${3}    ${0}    hostgroup
    Ctn Add Tags To Hosts    ${0}    group_tags    2    [2,3]
    Ctn Reload Engine

    ${result}    Ctn Check Resources Tags With Timeout    0    2    hostgroup    [2]    60    True
    Should Be True    ${result}    Host 2 should have hostgroup tags 2

    ${result}    Ctn Check Resources Tags With Timeout    0    3    hostgroup    [2]    60    True
    Should Be True    ${result}    Host 3 should have hostgroup tags 2

    Ctn Stop Engine
    Ctn Kindly Stop Broker

MOVE_HOST_OF_HOSTGROUP_TO_ANOTHER_POLLER
    [Documentation]    Scenario: Moving hosts between pollers without losing hostgroup tag
    ...    Given two pollers each with two hosts
    ...    And all hosts belong to the same hostgroup
    ...    When I move two hosts from one poller to the other
    ...    Then the hostgroup tag of the moved hosts is not erased
    [Tags]    broker    engine    tags    MON-169517
    Ctn Clear Db    tags
    Ctn Clear Db    resources
    Ctn Clear Db    resources_tags

    Ctn Config Engine    ${2}    ${8}    ${5}
    Ctn Create Tags File    ${0}    ${1}    ${0}    hostgroup
    Ctn Create Tags File    ${1}    ${1}    ${0}    hostgroup
    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Config Engine Add Cfg File    ${1}    tags.cfg
    Ctn Add Tags To Hosts    ${0}    group_tags    1    ['host_1', 'host_2']
    Ctn Add Tags To Hosts    ${1}    group_tags    1    ['host_5', 'host_6']
    Ctn Add Host Group    ${0}    1    ['host_1', 'host_2']
    Ctn Add Host Group    ${1}    1    ['host_5', 'host_6']
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${2}
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config BBDO3    ${2}
    Ctn Broker Config Log    module0    neb    debug
    Ctn Broker Config Log    module1    neb    debug
    Ctn Broker Config Log    central    sql    trace
    Ctn Clear Retention

    Sleep    1s
    ${start}    Get Current Date
    Ctn Start engine
    Ctn Start Broker

    ${result}    Ctn Check Resources Tags With Timeout    0    1    hostgroup    [1]    60    True
    Should Be True    ${result}    Host 1 should have hostgroup tags 1
    ${result}    Ctn Check Resources Tags With Timeout    0    2    hostgroup    [1]    60    True
    Should Be True    ${result}    Host 2 should have hostgroup tags 1
    ${result}    Ctn Check Resources Tags With Timeout    0    5    hostgroup    [1]    60    True
    Should Be True    ${result}    Host 5 should have hostgroup tags 1
    ${result}    Ctn Check Resources Tags With Timeout    0    6    hostgroup    [1]    60    True
    Should Be True    ${result}    Host 6 should have hostgroup tags 1

    #remove host_5 and host_6 from poller 1
    Log To Console    Remove host_5 and host_6 from poller 1
    Ctn Engine Config Remove Host    ${1}    host_5
    Ctn Engine Config Remove Host    ${1}    host_6
    Ctn Engine Config Remove Service Host    ${1}    host_5
    Ctn Engine Config Remove Service Host    ${1}    host_6
    Ctn Engine Config Remove Tag    ${1}    1

    ${start}    Get Current Date
    Ctn Reload Engine    poller_index=${1}
    Ctn Reload Broker

    ${result}    Ctn Check Resources Tags With Timeout    0    1    hostgroup    [1]    60    True
    Should Be True    ${result}    Host 1 should have hostgroup tags 1
    ${result}    Ctn Check Resources Tags With Timeout    0    2    hostgroup    [1]    60    True
    Should Be True    ${result}    Host 2 should have hostgroup tags 1

    ${result}    Ctn Check Resources Tags With Timeout    0    5    hostgroup    [1]    ${60}    False
    Should Be True    ${result}    tag 1 yet attached to host_5

    ${content}    Create List    pb host event .* host_6
    ${result}    Ctn Find Regex In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling pb host event host_6 should be available.

    
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}

    Check Row Count     SELECT * FROM resources r inner join resources_tags rt on r.resource_id=rt.resource_id inner join tags t WHERE r.id = 5 AND r.parent_id = 0 AND r.enabled = 1    ==    0    retry_time_out=30s    retry_pause=2s

    #host_5 and host_6 will be now on poller 0
    Log To Console    host_5 and host_6 on poller 0
    Ctn Config Engine    ${2}    ${14}    ${5}
    Ctn Create Tags File    ${0}    ${1}    ${0}    hostgroup
    Ctn Create Tags File    ${1}    ${1}    ${0}    hostgroup
    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Config Engine Add Cfg File    ${1}    tags.cfg
    Ctn Add Tags To Hosts    ${0}    group_tags    1    ['host_1', 'host_2', 'host_5', 'host_6']
    Ctn Add Host Group    ${0}    1    ['host_1', 'host_2', 'host_5', 'host_6']
    Ctn Reload Engine    ${0}
    Ctn Reload Broker

    ${result}    Ctn Check Resources Tags With Timeout    0    1    hostgroup    [1]    60    True
    Should Be True    ${result}    Host 1 should have hostgroup tags 1
    ${result}    Ctn Check Resources Tags With Timeout    0    2    hostgroup    [1]    60    True
    Should Be True    ${result}    Host 2 should have hostgroup tags 1
    ${result}    Ctn Check Resources Tags With Timeout    0    5    hostgroup    [1]    60    True
    Should Be True    ${result}    Host 5 should have hostgroup tags 1
    ${result}    Ctn Check Resources Tags With Timeout    0    6    hostgroup    [1]    60    True
    Should Be True    ${result}    Host 6 should have hostgroup tags 1

    Check Query Result    SELECT name FROM tags WHERE id = 1    equals    tag0    retry_timeout=5s    retry_pause=1s



*** Keywords ***
Ctn Init Test
    Ctn Stop Processes
    Ctn Truncate Resource Host Service
