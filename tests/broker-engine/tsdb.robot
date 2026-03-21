*** Settings ***
Documentation       Centreon Broker tsdb tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs


*** Test Cases ***
VICT_ONE_CHECK_METRIC
    [Documentation]    victoria metrics metric output
    [Tags]    broker    engine    victoria_metrics
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Clear Retention
    Ctn Broker Config Log    central    victoria_metrics    trace
    Ctn Broker Config Log    central    perfdata    trace
    Ctn Broker Config Source Log    central    1
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config Broker Victoria Output
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Start Server    127.0.0.1    8000
    # wait all is started
    ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.

    Ctn Process Service Check Result    host_16    service_314    0    taratata|metric_taratata=80%;50;75;5;99

    ${start}    Ctn Get Round Current Date
    ${timeout}    Get Current Date    result_format=epoch    increment=00:01:00
    ${now}    Get Current Date    result_format=epoch
    WHILE    ${now} < ${timeout}
        Wait For Request    timeout=30
        ${body}    Get Request Body
        Set Test Variable    ${metric_found}    False
        IF    ${body != None}
            ${body}    Decode Bytes To String    ${body}    UTF-8
            ${metric_found}    Ctn Check Victoria Metric
            ...    ${body}
            ...    ${start}
            ...    unit=%
            ...    host_id=16
            ...    serv_id=314
            ...    host=host_16
            ...    serv=service_314
            ...    name=metric_taratata
            ...    val=80
            ...    min=5
            ...    max=99
        END
        IF    ${metric_found}    BREAK

        Reply By    200
        ${now}    Get Current Date    result_format=epoch
    END

    Should Be True    ${now} < ${timeout}


VICT_ONE_CHECK_STATUS
    [Documentation]    Given a Victoria Metrics setup with Centreon Broker
    ...                When a service check result is processed
    ...                Then the service status transitions through OK, WARNING, and CRITICAL states
    ...                And the corresponding status updates are sent to Victoria Metrics
    [Tags]    broker    engine    victoria_metrics
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Clear Retention
    Ctn Broker Config Log    central    victoria_metrics    trace
    Ctn Broker Config Log    central    perfdata    trace
    Ctn Broker Config Source Log    central    1
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config Broker Victoria Output
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Start Server    127.0.0.1    8000
    # wait all is started
    ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.

    # service ok
    ${start}    Ctn Get Round Current Date
    Ctn Process Service Check Result    host_16    service_314    0    taratata|metric_taratata=80%;50;75;5;99

    ${timeout}    Get Current Date    result_format=epoch    increment=00:01:00
    ${now}    Get Current Date    result_format=epoch
    WHILE    ${now} < ${timeout}
        Wait For Request    timeout=30
        ${body}    Get Request Body
        Set Test Variable    ${status_found}    False
        IF    ${body != None}
            ${body}    Decode Bytes To String    ${body}    UTF-8
            ${status_found}    Ctn Check Victoria Status
            ...    ${body}
            ...    ${start}
            ...    host_id=16
            ...    serv_id=314
            ...    host=host_16
            ...    serv=service_314
            ...    val=100
        END
        IF    ${status_found}    BREAK

        Reply By    200
        ${now}    Get Current Date    result_format=epoch
    END

    Should Be True    ${now} < ${timeout}

    # service warning
    ${start}    Ctn Get Round Current Date

    #we wait one second in order to avoid rrd problem with previous Ctn Process Service Result Hard
    Sleep    1s
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_314
    ...    1
    ...    taratata|metric_taratata=80%;50;75;5;99

    ${timeout}    Get Current Date    result_format=epoch    increment=00:01:00
    ${now}    Get Current Date    result_format=epoch
    WHILE    ${now} < ${timeout}
        Wait For Request    timeout=30
        ${body}    Get Request Body
        Set Test Variable    ${status_found}    False
        IF    ${body != None}
            ${body}    Decode Bytes To String    ${body}    UTF-8
            ${status_found}    Ctn Check Victoria Status
            ...    ${body}
            ...    ${start}
            ...    host_id=16
            ...    serv_id=314
            ...    host=host_16
            ...    serv=service_314
            ...    val=75
        END
        IF    ${status_found}    BREAK

        Reply By    200
        ${now}    Get Current Date    result_format=epoch
    END

    Should Be True    ${now} < ${timeout}

    # service critical

    ${start}    Ctn Get Round Current Date
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_314
    ...    2
    ...    taratata|metric_taratata=80%;50;75;5;99

    ${timeout}    Get Current Date    result_format=epoch    increment=00:01:00
    ${now}    Get Current Date    result_format=epoch
    WHILE    ${now} < ${timeout}
        Wait For Request    timeout=30
        ${body}    Get Request Body
        Set Test Variable    ${status_found}    False
        IF    ${body != None}
            ${body}    Decode Bytes To String    ${body}    UTF-8
            ${status_found}    Ctn Check Victoria Status
            ...    ${body}
            ...    ${start}
            ...    host_id=16
            ...    serv_id=314
            ...    host=host_16
            ...    serv=service_314
            ...    val=0
        END
        IF    ${status_found}    BREAK

        Reply By    200
        ${now}    Get Current Date    result_format=epoch
    END

    Should Be True    ${now} < ${timeout}


VICT_ONE_CHECK_METRIC_AFTER_FAILURE
    [Documentation]    victoria metrics metric output after victoria shutdown
    [Tags]    broker    engine    victoria_metrics
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Clear Retention
    Ctn Broker Config Log    central    victoria_metrics    trace
    Ctn Broker Config Log    central    perfdata    trace
    Ctn Broker Config Source Log    central    1
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config Broker Victoria Output
    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    # wait all is started
    ${content}    Create List    INITIAL SERVICE STATE: host_50;service_1000;    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.

    Ctn Process Service Check Result    host_16    service_314    0    taratata|metric_taratata=80%;50;75;5;99
    ${start}    Ctn Get Round Current Date

    ${content}    Create List    [victoria_metrics]    name: "metric_taratata"
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    victoria should add metric in a request

    Start Server    127.0.0.1    8000
    ${timeout}    Get Current Date    result_format=epoch    increment=00:01:00
    ${now}    Get Current Date    result_format=epoch
    WHILE    ${now} < ${timeout}
        Wait For Request    timeout=30
        ${body}    Get Request Body
        Set Test Variable    ${metric_found}    False
        IF    ${body != None}
            ${body}    Decode Bytes To String    ${body}    UTF-8
            ${metric_found}    Ctn Check Victoria Metric
            ...    ${body}
            ...    ${start}
            ...    unit=%
            ...    host_id=16
            ...    serv_id=314
            ...    host=host_16
            ...    serv=service_314
            ...    name=metric_taratata
            ...    val=80
            ...    min=5
            ...    max=99
        END
        IF    ${metric_found}    BREAK

        Reply By    200
        ${now}    Get Current Date    result_format=epoch
    END

    Should Be True    ${now} < ${timeout}


GRAPHITE_FORMAT_TEST
    [Documentation]    Given a central broker configured with a Graphite output using a custom metric and status path format
    ...    And an engine with host_16 belonging to host groups 1 and 2, service_314 belonging to service groups 4 and 5
    ...    And host_16 tagged with host group tag 2 and host category tag 2
    ...    And service_314 tagged with service group tag 4 and service category tags 4 and 5
    ...    And a TCP server listening on port 8000 acting as the Graphite endpoint
    ...    When a service check result is processed for host_16 / service_314 with perfdata metric_taratata=80%;50;75;5;99
    ...    Then the TCP server receives a metric message whose path contains all expanded macros:
    ...    instance, host id, host name, service id, service name, index id, perfdata name, max, min,
    ...    host groups, service groups, host/service tag names and ids
    ...    And the metric value is 80 with the correct timestamp
    ...    And the TCP server receives a status message whose path contains all the same context macros
    ...    And both messages include the expected Basic authentication header
    [Tags]    broker    engine    graphite    MON-195013
    Ctn Config Engine    ${1}    ${50}    ${20}    ${EMPTY}    ${False}
    Ctn Add Host Group    ${0}    ${1}    ["host_16", "host_17"]
    Ctn Add Host Group    ${0}    ${2}    ["host_16"]
    Ctn Add Service Group    ${0}    ${5}    ["host_16","service_314"]
    Ctn Add Service Group    ${0}    ${4}    ["host_16","service_314","host_16","service_315"]
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg
    Ctn Create Tags File    ${0}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Add Tags To Hosts    ${0}    group_tags    2    [16]
    Ctn Add Tags To Hosts    ${0}    category_tags    2    [16]
    Ctn Add Tags To Services    ${0}    group_tags    4    [314]
    Ctn Add Tags To Services    ${0}    category_tags    4,5    [314]


    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Clear Retention
    Ctn Broker Config Log    central    graphite    trace
    Ctn Broker Config Log    central    core    trace
    Ctn Broker Config Log    central    perfdata    trace
    Ctn Config Broker Graphite Output    
    ...    centreon.metric.instance.$INSTANCE$.$INSTANCEID$.host.$HOSTID$.$HOST$.service.$SERVICEID$.$SERVICE$.index_id.$INDEXID$.perfdata.$METRIC$.max.$MAX$.min.$MIN$.host_groups.$HOSTGROUP$.serv_groups.$SERVICE_GROUP$.host_tag_cat.$HOST_TAG_CAT_NAME$.host_tag_cat_id.$HOST_TAG_CAT_ID$.host_tag_group.$HOST_TAG_GROUP_NAME$.host_tag_group_id.$HOST_TAG_GROUP_ID$.serv_tag_cat.$SERV_TAG_CAT_NAME$.serv_tag_cat_id.$SERV_TAG_CAT_ID$.serv_tag_group.$SERV_TAG_GROUP_NAME$.serv_tag_group_id.$SERV_TAG_GROUP_ID$  
    ...    centreon.status.instance.$INSTANCE$.$INSTANCEID$.host.$HOSTID$.$HOST$.service.$SERVICEID$.$SERVICE$.index_id.$INDEXID$.host_groups.$HOSTGROUP$.serv_groups.$SERVICE_GROUP$.host_tag_cat.$HOST_TAG_CAT_NAME$.host_tag_cat_id.$HOST_TAG_CAT_ID$.host_tag_group.$HOST_TAG_GROUP_NAME$.host_tag_group_id.$HOST_TAG_GROUP_ID$.serv_tag_cat.$SERV_TAG_CAT_NAME$.serv_tag_cat_id.$SERV_TAG_CAT_ID$.serv_tag_group.$SERV_TAG_GROUP_NAME$.serv_tag_group_id.$SERV_TAG_GROUP_ID$ 
    Ctn Broker Config Source Log    central    1
    Ctn Config Broker Sql Output    central    unified_sql
    ${start}    Get Current Date

    ${tcp_server}    Ctn Create Tcp Server   8040 

    Ctn Start Broker
    Ctn Start Engine

    Ctn Wait For Engine To Be Ready    ${start}
    
    Ctn Process Service Check Result    host_16    service_314    0    taratata|metric_taratata=80%;50;75;5;99

    ${accepted}    CallMethod    ${tcp_server}    accept    ${30}
    Should Be True    ${accepted}    no incoming connection

    ${status_received}    Set Variable    ${False}
    ${metric_received}    Set Variable    ${False}

    FOR    ${i}     IN RANGE     2
        ${received}    CallMethod    ${tcp_server}    receive    ${30}

        Log To Console    received:${received}

        IF    'status' in $received
            Should Match Regexp    ${received}    Authorization: Basic dG90bzp0aXRp\ncentreon\\.status\\.instance\\.Poller0\\.1\\.host\\.16\\.host_16\\.service\\.314\\.service_314\\.index_id\\.\\d+\\.host_groups\\.1,2\\.serv_groups\\.4,5\\.host_tag_cat\\.tag8\\.host_tag_cat_id\\.2\\.host_tag_group\\.tag6\\.host_tag_group_id\\.2\\.serv_tag_cat\\.tag19,tag15\\.serv_tag_cat_id\\.4,5\\.serv_tag_group\\.tag13\\.serv_tag_group_id\\.4 0 \\d+    incorrect status received
            ${status_received}    Set Variable    ${True}
        END
        IF    'metric' in $received
            Should Match Regexp    ${received}    Authorization: Basic dG90bzp0aXRp\ncentreon\\.metric\\.instance\\.Poller0\\.1\\.host\\.16\\.host_16\\.service\\.314\\.service_314\\.index_id\\.\\d+\\.perfdata\\.metric_taratata\\.max\\.99\\.min\\.5\\.host_groups\\.1,2\\.serv_groups\\.4,5\\.host_tag_cat\\.tag8\\.host_tag_cat_id\\.2\\.host_tag_group\\.tag6\\.host_tag_group_id\\.2\\.serv_tag_cat\\.tag19,tag15\\.serv_tag_cat_id\\.4,5\\.serv_tag_group\\.tag13\\.serv_tag_group_id\\.4 80 \\d+    incorrect metric received
            ${metric_received}    Set Variable    ${True}
        END
        IF    ${metric_received} and ${status_received}
            BREAK
        END
    END


INFLUXDB_FORMAT_TEST
    [Documentation]    Given a central broker configured with an InfluxDB output using the InfluxDB line protocol
    ...    And an engine with host_16 belonging to host groups 1 and 2, service_314 belonging to service groups 4 and 5
    ...    And host_16 tagged with host group tag 2 and host category tag 2
    ...    And service_314 tagged with service group tag 4 and service category tags 4 and 5
    ...    And a TCP server listening on port 8086 acting as the InfluxDB endpoint
    ...    When a service check result is processed for host_16 / service_314 with perfdata metric_taratata=80%;50;75;5;99
    ...    Then the TCP server receives an HTTP POST to /write containing a metric line in InfluxDB line protocol
    ...    with measurement centreon_metric, tags host/service/instance/metric and all group/tag fields expanded
    ...    And the metric value is 80 with the correct timestamp
    ...    And a second HTTP POST is received containing a status line with measurement centreon_status
    ...    with value 0 and the same context fields expanded
    ...    And each HTTP POST is answered with HTTP/1.0 204 No Content to acknowledge the write
    [Tags]    broker    engine    influxdb    MON-195013
    Ctn Config Engine    ${1}    ${50}    ${20}    ${EMPTY}    ${False}
    Ctn Add Host Group    ${0}    ${1}    ["host_16", "host_17"]
    Ctn Add Host Group    ${0}    ${2}    ["host_16"]
    Ctn Add Service Group    ${0}    ${5}    ["host_16","service_314"]
    Ctn Add Service Group    ${0}    ${4}    ["host_16","service_314","host_16","service_315"]
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg
    Ctn Create Tags File    ${0}    ${20}
    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Add Tags To Hosts    ${0}    group_tags    2    [16]
    Ctn Add Tags To Hosts    ${0}    category_tags    2    [16]
    Ctn Add Tags To Services    ${0}    group_tags    4    [314]
    Ctn Add Tags To Services    ${0}    category_tags    4,5    [314]

    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Clear Retention
    Ctn Broker Config Log    central    influxdb    trace
    Ctn Broker Config Log    central    core    trace
    Ctn Broker Config Log    central    perfdata    trace
    Ctn Config Broker Influxdb Output    centreon_metric    centreon_status
    Ctn Broker Config Source Log    central    1
    Ctn Config Broker Sql Output    central    unified_sql
    ${start}    Get Current Date

    Start Server    127.0.0.1    8086

    Ctn Start Broker
    Ctn Start Engine

    Ctn Wait For Engine To Be Ready    ${start}

    Ctn Process Service Check Result    host_16    service_314    0    taratata|metric_taratata=80%;50;75;5;99

    ${status_received}    Set Variable    ${False}
    ${metric_received}    Set Variable    ${False}

    FOR    ${i}    IN RANGE    2
        Wait For Request    timeout=30
        ${body}    Get Request Body

        ${body_str}    Evaluate    $body.decode("utf-8")
        Log To Console    received:${body_str}

        Reply By   204

        IF    'centreon_status' in $body_str
            Should Match Regexp    ${body_str}
            ...    centreon_status,host=host_16,service=service_314,instance=Poller0 value=0,host_id="16",service_id="314",instance_id="1",index_id="\\d+",host_groups="1,2",serv_groups="4,5",host_tag_cat="tag8",host_tag_cat_id="2",host_tag_group="tag6",host_tag_group_id="2",serv_tag_cat="tag19,tag15",serv_tag_cat_id="4,5",serv_tag_group="tag13",serv_tag_group_id="4" \\d+
            ...    incorrect influxdb status received
            ${status_received}    Set Variable    ${True}
        END
        IF    'centreon_metric' in $body_str
            Should Match Regexp    ${body_str}
            ...    centreon_metric,host=host_16,service=service_314,instance=Poller0,metric=metric_taratata value=80,min=5,max=99,host_id="16",service_id="314",instance_id="1",index_id="\\d+",host_groups="1,2",serv_groups="4,5",host_tag_cat="tag8",host_tag_cat_id="2",host_tag_group="tag6",host_tag_group_id="2",serv_tag_cat="tag19,tag15",serv_tag_cat_id="4,5",serv_tag_group="tag13",serv_tag_group_id="4" \\d+
            ...    incorrect influxdb metric received
            ${metric_received}    Set Variable    ${True}
        END
        IF    ${metric_received} and ${status_received}
            BREAK
        END
    END


