*** Settings ***
Documentation       Centreon Broker and Engine anomaly detection

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
CANO_NOFILE
    [Documentation]    Given an anomaly detection service is configured for metric monitoring
    ...    and the threshold configuration file is missing from the system
    ...    when the service processes a check result with critical state
    ...    then the anomaly detection service must transition to UNKNOWN state
    ...    because it cannot determine thresholds without the configuration file
    [Tags]    broker    engine    anomaly    MON-163502    MON-153802
    Ctn Config Centralized Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}    only_central=True
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    sql    debug
    ${serv_id}    Ctn Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    Remove File    /tmp/anomaly_threshold.json
    Ctn Clear Retention
    Ctn Clear Db    services
    Ctn Clear Prot Files
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    True    True
    Ctn Start Engine    newGeneration=True    central_only=True

    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    Ctn Process Service Check Result    host_1    anomaly_${serv_id}    2    full output
    ${result}    Ctn Check Service Status With Timeout    host_1    anomaly_${serv_id}    3    30
    Should Be True    ${result}    The anomaly detection service must be in UNKNOWN state.
    Ctn Stop Engine
    Ctn Kindly Stop Broker    True

CANO_TOO_OLD_FILE
    [Documentation]    Given an anomaly detection service is configured with metric monitoring
    ...    And a threshold file exists but contains outdated prediction data
    ...    When the service processes a check result with performance data
    ...    Then the anomaly detection service must transition to UNKNOWN state
    ...    because the threshold data is too old to be reliable for current predictions
    [Tags]    broker    engine    anomaly    MON-153802
    Ctn Config Centralized Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Config BBDO3    ${1}    only_central=True
    ${serv_id}    Ctn Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,0,2],[1648812678,0,3]]
    Ctn Create Anomaly Threshold File    /tmp/anomaly_threshold.json    ${1}    ${serv_id}    metric    ${predict_data}
    Ctn Clear Retention
    Ctn Clear Db    services
    Ctn Clear Prot Files
    Ctn Start Broker    True    True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True    central_only=True

    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    Ctn Process Service Check Result    host_1    anomaly_${serv_id}    2    foobar|metric=70%;50;75
    ${result}    Ctn Check Service Status With Timeout    host_1    anomaly_${serv_id}    3    30
    Should Be True    ${result}    The anomaly detection service must be in UNKNOWN state.
    Ctn Stop Broker    True
    Ctn Stop Engine

CANO_OUT_LOWER_THAN_LIMIT
    [Documentation]    Given an anomaly detection service is configured with valid threshold data
    ...    And the threshold file contains lower and upper limits for the metric
    ...    When a service check provides performance data below the lower threshold limit
    ...    Then the anomaly detection service must transition to CRITICAL state
    ...    because the metric value indicates an anomalous condition requiring attention
    [Tags]    broker    engine    anomaly    MON-153802
    Ctn Config Centralized Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Config BBDO3    ${1}    only_central=True
    ${serv_id}    Ctn Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,50,52],[2648812678,50,63]]
    Ctn Create Anomaly Threshold File    /tmp/anomaly_threshold.json    ${1}    ${serv_id}    metric    ${predict_data}
    Ctn Clear Retention
    Ctn Clear Db    services
    Ctn Clear Prot Files
    Ctn Start Broker    True    True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True    central_only=True

    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    Ctn Process Service Check Result    host_1    anomaly_${serv_id}    2    foobar|metric=20%;50;75
    ${result}    Ctn Check Service Status With Timeout    host_1    anomaly_${serv_id}    2    30
    Should Be True    ${result}    The anomaly detection service must be in CRITICAL state.
    Ctn Stop Broker    True
    Ctn Stop Engine

CANO_OUT_UPPER_THAN_LIMIT
    [Documentation]    Given an anomaly detection service is configured with valid threshold data
    ...    And the threshold file contains lower and upper limits for the metric
    ...    When a service check provides performance data above the upper threshold limit
    ...    Then the anomaly detection service must transition to CRITICAL state
    ...    because the metric value indicates an anomalous condition requiring attention
    [Tags]    broker    engine    anomaly    MON-153802
    Ctn Config Centralized Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    config    debug
    Ctn Config BBDO3    ${1}    only_central=True
    ${serv_id}    Ctn Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,50,52],[2648812678,50,63]]
    Ctn Create Anomaly Threshold File    /tmp/anomaly_threshold.json    ${1}    ${serv_id}    metric    ${predict_data}
    Ctn Clear Retention
    Ctn Clear Db    services
    Ctn Start Broker    True    True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True    central_only=True

    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    Ctn Process Service Check Result    host_1    anomaly_${serv_id}    2    taratata|metric=80%;50;75
    ${result}    Ctn Check Service Status With Timeout    host_1    anomaly_${serv_id}    2    30
    Should Be True    ${result}    The anomaly detection service must be in CRITICAL state.
    Ctn Stop Broker    True
    Ctn Stop Engine

CANO_JSON_SENSITIVITY_NOT_SAVED
    [Documentation]    Given an anomaly detection service is configured with threshold data including sensitivity
    ...    And the threshold file contains prediction data with a specific sensitivity value
    ...    When the engine and broker are started and then stopped
    ...    Then the sensitivity value should not be persisted in the retention data
    ...    because JSON sensitivity parameters are not saved during retention processing
    [Tags]    engine    anomaly    retention    MON-153802
    Ctn Config Centralized Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Config BBDO3    ${1}    only_central=True
    ${serv_id}    Ctn Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,50,2, 10],[2648812678,25,-5,6]]
    Ctn Create Anomaly Threshold File V2
    ...    /tmp/anomaly_threshold.json
    ...    ${1}
    ...    ${serv_id}
    ...    metric
    ...    55.0
    ...    ${predict_data}
    Ctn Clear Retention
    Ctn Start Broker    newGeneration=True    only_central=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True    central_only=True

    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    Sleep    5s
    Ctn Stop Engine
    Ctn Stop Broker    True
    ${retention_sensitivity}    Ctn Grep Retention    ${0}    sensitivity=0.00
    Should Be Equal As Strings    ${retention_sensitivity}    sensitivity=0.00

CANO_CFG_SENSITIVITY_SAVED
    [Documentation]    Given an anomaly detection service is configured with a specific sensitivity value in configuration
    ...    And the threshold file contains prediction data with sensitivity parameters
    ...    When the engine and broker are started and then stopped
    ...    Then the configuration-based sensitivity value should be persisted in the retention data
    ...    because CFG sensitivity parameters are properly saved during retention processing
    [Tags]    engine    anomaly    retention    MON-153802
    Ctn Config Centralized Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Config BBDO3    ${1}    only_central=True
    ${serv_id}    Ctn Create Anomaly Detection    ${0}    ${1}    ${1}    metric    4.00
    ${predict_data}    Evaluate    [[0,50,2, 10],[2648812678,25,-5,6]]
    Ctn Create Anomaly Threshold File V2
    ...    /tmp/anomaly_threshold.json
    ...    ${1}
    ...    ${serv_id}
    ...    metric
    ...    55.0
    ...    ${predict_data}
    Ctn Clear Retention
    Ctn Start Broker    newGeneration=True    only_central=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True    central_only=True
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    Sleep    5s
    Ctn Stop Engine
    Ctn Stop Broker    True
    ${retention_sensitivity}    Ctn Grep Retention    ${0}    sensitivity=4.00
    Should Be Equal As Strings    ${retention_sensitivity}    sensitivity=4.00

CANO_EXTCMD_SENSITIVITY_SAVED
    [Documentation]    Given an anomaly detection service is configured with threshold data
    ...    And the service is running with initial sensitivity parameters
    ...    When an external command updates the anomaly sensitivity value
    ...    And the engine and broker are stopped
    ...    Then the updated sensitivity value should be persisted in the retention data
    ...    because external command sensitivity changes are properly saved during retention processing
    [Tags]    engine    anomaly    retention    extcmd    MON-153802
    FOR    ${use_grpc}    IN RANGE    1    2
        Ctn Config Centralized Engine    ${1}    ${50}    ${20}
	Ctn Config Broker    module
	Ctn Config Broker    central
        Ctn Broker Config Log    central    bbdo    debug
        Ctn Config BBDO3    ${1}    only_central=True
        ${serv_id}    Ctn Create Anomaly Detection    ${0}    ${1}    ${1}    metric
        ${predict_data}    Evaluate    [[0,50,2, 10],[2648812678,25,-5,6]]
        Ctn Create Anomaly Threshold File V2
        ...    /tmp/anomaly_threshold.json
        ...    ${1}
        ...    ${serv_id}
        ...    metric
        ...    55.0
        ...    ${predict_data}
        Ctn Clear Retention
        Ctn Start Broker    newGeneration=True    only_central=True
        ${start}    Ctn Get Round Current Date
        Ctn Start Engine    newGeneration=True    central_only=True
        ${content}    Create List    received diff state ack from poller 1
        ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
        Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.
        Sleep    5s
        Ctn Update Ano Sensitivity    ${use_grpc}    host_1    anomaly_1001    4.55
        Sleep    1s
        Ctn Stop Engine
        Ctn Stop Broker	  True
        ${retention_sensitivity}    Ctn Grep Retention    ${0}    sensitivity=4.55
        Should Be Equal As Strings    ${retention_sensitivity}    sensitivity=4.55
    END

CAOUTLU1
    [Documentation]    Given an anomaly detection service is configured with valid threshold data using BBDO3 protocol
    ...    And the threshold file contains lower and upper limits for the metric
    ...    When a service check provides performance data above the upper threshold limit
    ...    Then the anomaly detection service must transition to CRITICAL state
    ...    And the resources table should contain SERVICE, HOST and ANOMALY_DETECTION type entries
    [Tags]    broker    engine    anomaly    bbdo    MON-153802
    Ctn Config Centralized Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    bbdo    debug
    ${serv_id}    Ctn Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,50,52],[2648812678,50,63]]
    Ctn Create Anomaly Threshold File    /tmp/anomaly_threshold.json    ${1}    ${serv_id}    metric    ${predict_data}
    Ctn Clear Retention
    Ctn Clear Db    services
    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    Ctn Process Service Check Result    host_1    anomaly_${serv_id}    2    taratata|metric=80%;50;75
    Ctn Check Service Status With Timeout    host_1    anomaly_${serv_id}    2    30
    Ctn Stop Engine
    Ctn Kindly Stop Broker
    ${lst}    Create List    1    0    4
    ${result}    Ctn Check Types In Resources    ${lst}
    Should Be True
    ...    ${result}
    ...    The table 'resources' should contain rows of types SERVICE, HOST and ANOMALY_DETECTION.

CANO_DT1
    [Documentation]    Given an anomaly detection service is configured with a dependent service relationship
    ...    And both services are running normally
    ...    When a downtime is scheduled on the dependent service
    ...    Then the dependent service should enter downtime state
    ...    And the anomaly detection service should automatically inherit the downtime
    ...    because anomaly detection services inherit downtime from their dependent services
    [Tags]    broker    engine    anomaly    MON-153802
    Ctn Config Centralized Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    bbdo    debug
    ${serv_id}    Ctn Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,50,52],[2648812678,50,63]]
    Ctn Create Anomaly Threshold File    /tmp/anomaly_threshold.json    ${1}    ${serv_id}    metric    ${predict_data}
    Ctn Clear Retention
    Ctn Clear Db    services
    Ctn Clear Db    downtimes
    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    # create dependent service downtime
    Ctn Schedule Service Fixed Downtime    host_1    service_1    3600

    ${result}    Ctn Check Service Downtime With Timeout    host_1    service_1    1    60
    Should Be True    ${result}    dependent service must be in downtime
    ${result}    Ctn Check Service Downtime With Timeout    host_1    anomaly_${serv_id}    1    60
    Should Be True    ${result}    anomaly service must be in downtime

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CANO_DT2
    [Documentation]    Given an anomaly detection service is configured with a dependent service relationship
    ...    And both services are running normally
    ...    When a downtime is scheduled on the dependent service
    ...    Then the anomaly detection service should automatically enter downtime
    ...    When the downtime is deleted from the dependent service
    ...    Then the anomaly detection service should automatically exit downtime
    ...    because anomaly detection downtime should follow its dependent service downtime state
    [Tags]    broker    engine    anomaly    MON-153802
    Ctn Config Centralized Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    bbdo    debug
    ${serv_id}    Ctn Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,50,52],[2648812678,50,63]]
    Ctn Create Anomaly Threshold File    /tmp/anomaly_threshold.json    ${1}    ${serv_id}    metric    ${predict_data}
    Ctn Clear Retention
    Ctn Clear Db    services
    Ctn Clear Db    downtimes
    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    # create dependent service downtime
    Ctn Schedule Service Fixed Downtime    host_1    service_1    3600

    ${result}    Ctn Check Service Downtime With Timeout    host_1    anomaly_${serv_id}    1    60
    Should Be True    ${result}    anomaly service must be in downtime

    Ctn Delete Service Downtime    host_1    service_1
    ${result}    Ctn Check Service Downtime With Timeout    host_1    service_1    0    60
    Should Be True    ${result}    dependent service must not be in downtime
    ${result}    Ctn Check Service Downtime With Timeout    host_1    anomaly_${serv_id}    0    60
    Should Be True    ${result}    anomaly service must not be in downtime

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CANO_DT3
    [Documentation]    Given an anomaly detection service is configured with a dependent service relationship
    ...    And both services are running normally
    ...    When a downtime is scheduled on the dependent service
    ...    Then the anomaly detection service should automatically enter downtime
    ...    When the downtime is deleted from the anomaly detection service
    ...    Then the dependent service should remain in its original downtime state
    ...    because deleting downtime on anomaly detection should not affect dependent service downtimes
    [Tags]    broker    engine    anomaly    MON-153802
    Ctn Config Centralized Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    bbdo    debug
    ${serv_id}    Ctn Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,50,52],[2648812678,50,63]]
    Ctn Create Anomaly Threshold File    /tmp/anomaly_threshold.json    ${1}    ${serv_id}    metric    ${predict_data}
    Ctn Clear Retention
    Ctn Clear Db    services
    Ctn Clear Db    downtimes
    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    # create dependent service downtime
    Ctn Schedule Service Fixed Downtime    host_1    service_1    3600

    ${result}    Ctn Check Service Downtime With Timeout    host_1    anomaly_${serv_id}    1    60
    Should Be True    ${result}    anomaly service must be in downtime

    Ctn Delete Service Downtime    host_1    anomaly_${serv_id}
    ${result}    Ctn Check Service Downtime With Timeout    host_1    anomaly_${serv_id}    0    60
    Should Be True    ${result}    anomaly service must be in downtime

    ${result}    Ctn Check Service Downtime With Timeout    host_1    service_1    1    60
    Should Be True    ${result}    dependent service must be in downtime

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CANO_DT4
    [Documentation]    Scenario: Removing downtime from service keeps it on anomaly detection
    ...    Given an anomaly detection is attached to a service
    ...    And a downtime is set on both the service and the anomaly detection
    ...    When the downtime is removed from the service
    ...    Then the downtime should still be present on the anomaly detection

    [Tags]    broker    engine    anomaly    MON-153802
    Ctn Config Centralized Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    bbdo    debug
    ${serv_id}    Ctn Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,50,52],[2648812678,50,63]]
    Ctn Create Anomaly Threshold File    /tmp/anomaly_threshold.json    ${1}    ${serv_id}    metric    ${predict_data}
    Ctn Clear Retention
    Ctn Clear Db    services
    Ctn Clear Db    downtimes
    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    # create dependent service downtime
    Ctn Schedule Service Fixed Downtime    host_1    service_1    3600
    Ctn Schedule Service Fixed Downtime    host_1    anomaly_${serv_id}    3600

    ${result}    Ctn Check Service Downtime With Timeout    host_1    anomaly_${serv_id}    2    60
    Should Be True    ${result}    Both anomaly detection and service must be in downtime

    Ctn Delete Service Downtime    host_1    service_1
    ${result}    Ctn Check Service Downtime With Timeout    host_1    service_1    0    60
    Should Be True    ${result}    The downtime should be removed from the service.
    ${result}    Ctn Check Service Downtime With Timeout    host_1    anomaly_${serv_id}    1    60
    Should Be True    ${result}    The anomaly detection should still be in downtime.

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CANO_INC_AD
    [Tags]    broker    engine    anomaly    MON-153802
    Ctn Config Centralized Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    config    debug
    Ctn Clear Retention
    Ctn Clear Db    services
    Ctn Clear Db    downtimes
    Ctn Clear Prot Files
    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    FOR    ${i}    IN RANGE    10
        ${start}    Ctn Get Round Current Date
        ${serv_id}    Ctn Create Anomaly Detection    ${0}    ${1}    ${1}    metric
        Ctn Notify Broker Of Engine Config Change    ${0}
        ${content}    Create List    Anomaly detection resource with id 1:${serv_id}
        ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
        Should Be True    ${result}    The broker must process the new anomaly detection service.
    END

    ${result}    Ctn Check Resource IDs    AD    ${centralLog}
    Should Be True    ${result}    The anomaly detection resources must be identical.

    FOR    ${i}    IN RANGE    10
        ${start}    Ctn Get Round Current Date
        ${serv_id}    Ctn Create Anomaly Detection    ${0}    ${1}    ${i + 1}    metric
	Ctn Delete Anomaly Detection At Index    ${0}    ${0}
	Ctn Modify Anomaly Detection    ${0}    ${i + 1010}    service_description    ad_${i + 1}
        Ctn Notify Broker Of Engine Config Change    ${0}
        ${content}    Create List    Anomaly detection resource with id 1:${serv_id}
	...    Anomaly detection resource with id 1:${i + 1010}
	...    Disabling service resource with id 1:${i + 1001}
        ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
        Should Be True    ${result}    The broker must process the new anomaly detection services configuration.
    END

    ${result}    Ctn Check Resource IDs    AD    ${centralLog}
    Should Be True    ${result}    The anomaly detection resources must be identical.

    Ctn Stop Engine
    Ctn Kindly Stop Broker

