*** Settings ***
Documentation       Centreon Broker and Engine anomaly detection

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
ANO_NOFILE
    [Documentation]    an anomaly detection without threshold file must be in unknown state
    [Tags]    broker    engine    anomaly
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    ${serv_id}    Ctn Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    Remove File    /tmp/anomaly_threshold.json
    Ctn Clear Retention
    Ctn Clear Db    services
    Ctn Start Broker    True
    Ctn Start engine
    Ctn Process Service Check Result    host_1    anomaly_${serv_id}    2    taratata
    Ctn Check Service Status With Timeout    host_1    anomaly_${serv_id}    3    30
    Ctn Stop Broker    True
    Ctn Stop engine

ANO_NOFILE_VERIF_CONFIG_NO_ERROR
    [Documentation]    an anomaly detection without threshold file doesn't display error on config check
    [Tags]    broker    engine    anomaly    MON-20385
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    Remove File    /tmp/anomaly_threshold.json
    Ctn Clear Retention
    Create Directory    ${ENGINE_LOG}/config0
    Start Process
    ...    /usr/sbin/centengine
    ...    -v
    ...    ${EtcRoot}/centreon-engine/config0/centengine.cfg
    ...    alias=e0
    ...    stderr=${engineLog0}
    ...    stdout=${engineLog0}
    ${result}    Wait For Process    e0    30
    Should Be Equal As Integers    ${result.rc}    0    engine not gracefully stopped
    ${content}    Grep File    ${engineLog0}    Fail to read thresholds file
    Should Be True    len("""${content}""") < 2    anomalydetection error message must not be found

ANO_TOO_OLD_FILE
    [Documentation]    An anomaly detection with an oldest threshold file must be in unknown state
    [Tags]    broker    engine    anomaly
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    ${serv_id}    Ctn Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,0,2],[1648812678,0,3]]
    Ctn Create Anomaly Threshold File    /tmp/anomaly_threshold.json    ${1}    ${serv_id}    metric    ${predict_data}
    Ctn Clear Retention
    Ctn Clear Db    services
    Ctn Start Broker    True
    Ctn Start engine
    Ctn Process Service Check Result    host_1    anomaly_${serv_id}    2    taratata|metric=70%;50;75
    Ctn Check Service Status With Timeout    host_1    anomaly_${serv_id}    3    30
    Ctn Stop Broker    True
    Ctn Stop engine

ANO_OUT_LOWER_THAN_LIMIT
    [Documentation]    an anomaly detection with a perfdata lower than lower limit make a critical state
    [Tags]    broker    engine    anomaly
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    ${serv_id}    Ctn Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,50,52],[2648812678,50,63]]
    Ctn Create Anomaly Threshold File    /tmp/anomaly_threshold.json    ${1}    ${serv_id}    metric    ${predict_data}
    Ctn Clear Retention
    Ctn Clear Db    services
    Ctn Start Broker    True
    Ctn Start engine
    Ctn Process Service Check Result    host_1    anomaly_${serv_id}    2    taratata|metric=20%;50;75
    Ctn Check Service Status With Timeout    host_1    anomaly_${serv_id}    2    30
    Ctn Stop Broker    True
    Ctn Stop engine

ANO_OUT_UPPER_THAN_LIMIT
    [Documentation]    an anomaly detection with a perfdata upper than upper limit make a critical state
    [Tags]    broker    engine    anomaly
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    ${serv_id}    Ctn Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,50,52],[2648812678,50,63]]
    Ctn Create Anomaly Threshold File    /tmp/anomaly_threshold.json    ${1}    ${serv_id}    metric    ${predict_data}
    Ctn Clear Retention
    Ctn Clear Db    services
    Ctn Start Broker    True
    Ctn Start engine
    Ctn Process Service Check Result    host_1    anomaly_${serv_id}    2    taratata|metric=80%;50;75
    Ctn Check Service Status With Timeout    host_1    anomaly_${serv_id}    2    30
    Ctn Stop Broker    True
    Ctn Stop engine

ANO_JSON_SENSITIVITY_NOT_SAVED
    [Documentation]    json sensitivity not saved in retention
    [Tags]    engine    anomaly    retention
    Ctn Config Engine    ${1}    ${50}    ${20}
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
    Ctn Start engine
    Sleep    5s
    Ctn Stop engine
    ${retention_sensitivity}    Ctn Grep Retention    ${0}    sensitivity=0.00
    Should Be Equal As Strings    ${retention_sensitivity}    sensitivity=0.00

ANO_CFG_SENSITIVITY_SAVED
    [Documentation]    cfg sensitivity saved in retention
    [Tags]    engine    anomaly    retention
    Ctn Config Engine    ${1}    ${50}    ${20}
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
    Ctn Start engine
    Sleep    5s
    Ctn Stop engine
    ${retention_sensitivity}    Ctn Grep Retention    ${0}    sensitivity=4.00
    Should Be Equal As Strings    ${retention_sensitivity}    sensitivity=4.00

ANO_EXTCMD_SENSITIVITY_SAVED
    [Documentation]    extcmd sensitivity saved in retention
    [Tags]    engine    anomaly    retention    extcmd
    FOR    ${use_grpc}    IN RANGE    1    2
        Ctn Config Engine    ${1}    ${50}    ${20}
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
        Ctn Start engine
        Sleep    5s
        Ctn Update Ano Sensitivity    ${use_grpc}    host_1    anomaly_1001    4.55
        Sleep    1s
        Ctn Stop engine
        ${retention_sensitivity}    Ctn Grep Retention    ${0}    sensitivity=4.55
        Should Be Equal As Strings    ${retention_sensitivity}    sensitivity=4.55
    END

AOUTLU1
    [Documentation]    an anomaly detection with a perfdata upper than upper limit make a critical state with bbdo 3
    [Tags]    broker    engine    anomaly    bbdo
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config BBDO3    1
    ${serv_id}    Ctn Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,50,52],[2648812678,50,63]]
    Ctn Create Anomaly Threshold File    /tmp/anomaly_threshold.json    ${1}    ${serv_id}    metric    ${predict_data}
    Ctn Clear Retention
    Ctn Clear Db    services
    Ctn Start Broker
    ${start}    Get Current Date
    Ctn Start engine
    # Let's wait for the check of external commands
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    Ctn Process Service Check Result    host_1    anomaly_${serv_id}    2    taratata|metric=80%;50;75
    Ctn Check Service Status With Timeout    host_1    anomaly_${serv_id}    2    30
    Ctn Stop engine
    Ctn Kindly Stop Broker
    ${lst}    Create List    1    0    4
    ${result}    Ctn Check Types In Resources    ${lst}
    Should Be True
    ...    ${result}
    ...    The table 'resources' should contain rows of types SERVICE, HOST and ANOMALY_DETECTION.

ANO_DT1
    [Documentation]    downtime on dependent service is inherited by ano
    [Tags]    broker    engine    anomaly
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config BBDO3    1
    ${serv_id}    Ctn Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,50,52],[2648812678,50,63]]
    Ctn Create Anomaly Threshold File    /tmp/anomaly_threshold.json    ${1}    ${serv_id}    metric    ${predict_data}
    Ctn Clear Retention
    Ctn Clear Db    services
    Ctn Clear Db    downtimes
    Ctn Start Broker
    ${start}    Get Current Date
    Ctn Start engine
    # Let's wait for the check of external commands
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    # create dependent service downtime
    Ctn Schedule Service Fixed Downtime    host_1    service_1    3600

    ${result}    Ctn Check Service Downtime With Timeout    host_1    service_1    1    60
    Should Be True    ${result}    dependent service must be in downtime
    ${result}    Ctn Check Service Downtime With Timeout    host_1    anomaly_${serv_id}    1    60
    Should Be True    ${result}    anomaly service must be in downtime

    Ctn Stop engine
    Ctn Kindly Stop Broker

ANO_DT2
    [Documentation]    Given a service and its AD, 
    ...    when we delete downtime on dependent service, AD must not be in downtime anymore
    [Tags]    broker    engine    anomaly
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config BBDO3    1
    ${serv_id}    Ctn Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,50,52],[2648812678,50,63]]
    Ctn Create Anomaly Threshold File    /tmp/anomaly_threshold.json    ${1}    ${serv_id}    metric    ${predict_data}
    Ctn Clear Retention
    Ctn Clear Db    services
    Ctn Clear Db    downtimes
    Ctn Start Broker
    ${start}    Get Current Date
    Ctn Start engine
    # Let's wait for the check of external commands
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    # create dependent service downtime
    Ctn Schedule Service Fixed Downtime    host_1    service_1    3600

    ${result}    Ctn Check Service Downtime With Timeout    host_1    anomaly_${serv_id}    1    60
    Should Be True    ${result}    anomaly service must be in downtime

    Ctn Delete Service Downtime    host_1    service_1
    ${result}    Ctn Check Service Downtime With Timeout    host_1    service_1    0    60
    Should Be True    ${result}    dependent service must not be in downtime
    ${result}    Ctn Check Service Downtime With Timeout    host_1    anomaly_${serv_id}    0    60
    Should Be True    ${result}    anomaly service must not be in downtime

    Ctn Stop engine
    Ctn Kindly Stop Broker

ANO_DT3
    [Documentation]    delete downtime on anomaly don t delete dependent service one
    [Tags]    broker    engine    anomaly
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config BBDO3    1
    ${serv_id}    Ctn Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,50,52],[2648812678,50,63]]
    Ctn Create Anomaly Threshold File    /tmp/anomaly_threshold.json    ${1}    ${serv_id}    metric    ${predict_data}
    Ctn Clear Retention
    Ctn Clear Db    services
    Ctn Clear Db    downtimes
    Ctn Start Broker
    ${start}    Get Current Date
    Ctn Start engine
    # Let's wait for the check of external commands
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    # create dependent service downtime
    Ctn Schedule Service Fixed Downtime    host_1    service_1    3600

    ${result}    Ctn Check Service Downtime With Timeout    host_1    anomaly_${serv_id}    1    60
    Should Be True    ${result}    anomaly service must be in downtime

    Ctn Delete Service Downtime    host_1    anomaly_${serv_id}
    ${result}    Ctn Check Service Downtime With Timeout    host_1    anomaly_${serv_id}    0    60
    Should Be True    ${result}    anomaly service must be in downtime

    ${result}    Ctn Check Service Downtime With Timeout    host_1    service_1    1    60
    Should Be True    ${result}    dependent service must be in downtime

    Ctn Stop engine
    Ctn Kindly Stop Broker

ANO_DT4
    [Documentation]    set dt on anomaly and on dependent service, delete last one don t delete first one
    [Tags]    broker    engine    anomaly
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Config BBDO3    1
    ${serv_id}    Ctn Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,50,52],[2648812678,50,63]]
    Ctn Create Anomaly Threshold File    /tmp/anomaly_threshold.json    ${1}    ${serv_id}    metric    ${predict_data}
    Ctn Clear Retention
    Ctn Clear Db    services
    Ctn Clear Db    downtimes
    Ctn Start Broker
    ${start}    Get Current Date
    Ctn Start engine
    # Let's wait for the check of external commands
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    # create dependent service downtime
    Ctn Schedule Service Fixed Downtime    host_1    service_1    3600
    Ctn Schedule Service Fixed Downtime    host_1    anomaly_${serv_id}    3600

    ${result}    Ctn Check Service Downtime With Timeout    host_1    anomaly_${serv_id}    2    60
    Should Be True    ${result}    anomaly service must be in double downtime

    Ctn Delete Service Downtime    host_1    service_1
    ${result}    Ctn Check Service Downtime With Timeout    host_1    service_1    0    60
    Should Be True    ${result}    dependent service mustn t be in downtime
    ${result}    Ctn Check Service Downtime With Timeout    host_1    anomaly_${serv_id}    1    60
    Should Be True    ${result}    anomaly service must be in simple downtime

    Ctn Stop engine
    Ctn Kindly Stop Broker
