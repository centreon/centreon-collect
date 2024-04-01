*** Settings ***
Documentation       Centreon Broker and Engine anomaly detection

Resource            ../resources/import.resource

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save Logs If Failed


*** Test Cases ***
ANO_NOFILE
    [Documentation]    an anomaly detection without threshold file must be in unknown state
    [Tags]    broker    engine    anomaly
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    ${serv_id}    Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    Remove File    /tmp/anomaly_threshold.json
    Clear Retention
    Clear Db    services
    Start Broker    True
    Start Engine
    Process Service Check Result    host_1    anomaly_${serv_id}    2    taratata
    Check Service Status With Timeout    host_1    anomaly_${serv_id}    3    30
    Stop Broker    True
    Stop Engine

ANO_NOFILE_VERIF_CONFIG_NO_ERROR
    [Documentation]    an anomaly detection without threshold file doesn't display error on config check
    [Tags]    broker    engine    anomaly    MON-20385
    Config Engine    ${1}    ${50}    ${20}
    Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    Remove File    /tmp/anomaly_threshold.json
    Clear Retention
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
    [Documentation]    an anomaly detection with an oldest threshold file must be in unknown state
    [Tags]    broker    engine    anomaly
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    ${serv_id}    Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,0,2],[1648812678,0,3]]
    Create Anomaly Threshold File    /tmp/anomaly_threshold.json    ${1}    ${serv_id}    metric    ${predict_data}
    Clear Retention
    Clear Db    services
    Start Broker    True
    Start Engine
    Process Service Check Result    host_1    anomaly_${serv_id}    2    taratata|metric=70%;50;75
    Check Service Status With Timeout    host_1    anomaly_${serv_id}    3    30
    Stop Broker    True
    Stop Engine

ANO_OUT_LOWER_THAN_LIMIT
    [Documentation]    an anomaly detection with a perfdata lower than lower limit make a critical state
    [Tags]    broker    engine    anomaly
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    ${serv_id}    Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,50,52],[2648812678,50,63]]
    Create Anomaly Threshold File    /tmp/anomaly_threshold.json    ${1}    ${serv_id}    metric    ${predict_data}
    Clear Retention
    Clear Db    services
    Start Broker    True
    Start Engine
    Process Service Check Result    host_1    anomaly_${serv_id}    2    taratata|metric=20%;50;75
    Check Service Status With Timeout    host_1    anomaly_${serv_id}    2    30
    Stop Broker    True
    Stop Engine

ANO_OUT_UPPER_THAN_LIMIT
    [Documentation]    an anomaly detection with a perfdata upper than upper limit make a critical state
    [Tags]    broker    engine    anomaly
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    ${serv_id}    Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,50,52],[2648812678,50,63]]
    Create Anomaly Threshold File    /tmp/anomaly_threshold.json    ${1}    ${serv_id}    metric    ${predict_data}
    Clear Retention
    Clear Db    services
    Start Broker    True
    Start Engine
    Process Service Check Result    host_1    anomaly_${serv_id}    2    taratata|metric=80%;50;75
    Check Service Status With Timeout    host_1    anomaly_${serv_id}    2    30
    Stop Broker    True
    Stop Engine

ANO_JSON_SENSITIVITY_NOT_SAVED
    [Documentation]    json sensitivity not saved in retention
    [Tags]    engine    anomaly    retention
    Config Engine    ${1}    ${50}    ${20}
    ${serv_id}    Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,50,2, 10],[2648812678,25,-5,6]]
    Create Anomaly Threshold File V2
    ...    /tmp/anomaly_threshold.json
    ...    ${1}
    ...    ${serv_id}
    ...    metric
    ...    55.0
    ...    ${predict_data}
    Clear Retention
    Start Engine
    Sleep    5s
    Stop Engine
    ${retention_sensitivity}    Grep Retention    ${0}    sensitivity=0.00
    Should Be Equal As Strings    ${retention_sensitivity}    sensitivity=0.00

ANO_CFG_SENSITIVITY_SAVED
    [Documentation]    cfg sensitivity saved in retention
    [Tags]    engine    anomaly    retention
    Config Engine    ${1}    ${50}    ${20}
    ${serv_id}    Create Anomaly Detection    ${0}    ${1}    ${1}    metric    4.00
    ${predict_data}    Evaluate    [[0,50,2, 10],[2648812678,25,-5,6]]
    Create Anomaly Threshold File V2
    ...    /tmp/anomaly_threshold.json
    ...    ${1}
    ...    ${serv_id}
    ...    metric
    ...    55.0
    ...    ${predict_data}
    Clear Retention
    Start Engine
    Sleep    5s
    Stop Engine
    ${retention_sensitivity}    Grep Retention    ${0}    sensitivity=4.00
    Should Be Equal As Strings    ${retention_sensitivity}    sensitivity=4.00

ANO_EXTCMD_SENSITIVITY_SAVED
    [Documentation]    extcmd sensitivity saved in retention
    [Tags]    engine    anomaly    retention    extcmd
    FOR    ${use_grpc}    IN RANGE    1    2
        Config Engine    ${1}    ${50}    ${20}
        ${serv_id}    Create Anomaly Detection    ${0}    ${1}    ${1}    metric
        ${predict_data}    Evaluate    [[0,50,2, 10],[2648812678,25,-5,6]]
        Create Anomaly Threshold File V2
        ...    /tmp/anomaly_threshold.json
        ...    ${1}
        ...    ${serv_id}
        ...    metric
        ...    55.0
        ...    ${predict_data}
        Clear Retention
        Start Engine
        Sleep    5s
        Update Ano Sensitivity    ${use_grpc}    host_1    anomaly_1001    4.55
        Sleep    1s
        Stop Engine
        ${retention_sensitivity}    Grep Retention    ${0}    sensitivity=4.55
        Should Be Equal As Strings    ${retention_sensitivity}    sensitivity=4.55
    END

AOUTLU1
    [Documentation]    an anomaly detection with a perfdata upper than upper limit make a critical state with bbdo 3
    [Tags]    broker    engine    anomaly    bbdo
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    Config BBDO3    1
    ${serv_id}    Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,50,52],[2648812678,50,63]]
    Create Anomaly Threshold File    /tmp/anomaly_threshold.json    ${1}    ${serv_id}    metric    ${predict_data}
    Clear Retention
    Clear Db    services
    Start Broker
    ${start}    Get Current Date
    Start Engine
    # Let's wait for the check of external commands
    ${content}    Create List    check_for_external_commands
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    Process Service Check Result    host_1    anomaly_${serv_id}    2    taratata|metric=80%;50;75
    Check Service Status With Timeout    host_1    anomaly_${serv_id}    2    30
    Stop Engine
    Kindly Stop Broker
    ${lst}    Create List    1    0    4
    ${result}    Check Types In Resources    ${lst}
    Should Be True
    ...    ${result}
    ...    The table 'resources' should contain rows of types SERVICE, HOST and ANOMALY_DETECTION.

ANO_DT1
    [Documentation]    downtime on dependent service is inherited by ano
    [Tags]    broker    engine    anomaly
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    Config BBDO3    1
    ${serv_id}    Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,50,52],[2648812678,50,63]]
    Create Anomaly Threshold File    /tmp/anomaly_threshold.json    ${1}    ${serv_id}    metric    ${predict_data}
    Clear Retention
    Clear Db    services
    Clear Db    downtimes
    Start Broker
    ${start}    Get Current Date
    Start Engine
    # Let's wait for the check of external commands
    ${content}    Create List    check_for_external_commands
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    # create dependent service downtime
    Schedule Service Fixed Downtime    host_1    service_1    3600

    ${result}    Check Service Downtime With Timeout    host_1    service_1    1    60
    Should Be True    ${result}    dependent service must be in downtime
    ${result}    Check Service Downtime With Timeout    host_1    anomaly_${serv_id}    1    60
    Should Be True    ${result}    anomaly service must be in downtime

    Stop Engine
    Kindly Stop Broker

ANO_DT2
    [Documentation]    delete downtime on dependent service delete one on ano serv
    [Tags]    broker    engine    anomaly
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    Config BBDO3    1
    ${serv_id}    Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,50,52],[2648812678,50,63]]
    Create Anomaly Threshold File    /tmp/anomaly_threshold.json    ${1}    ${serv_id}    metric    ${predict_data}
    Clear Retention
    Clear Db    services
    Clear Db    downtimes
    Start Broker
    ${start}    Get Current Date
    Start Engine
    # Let's wait for the check of external commands
    ${content}    Create List    check_for_external_commands
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    # create dependent service downtime
    Schedule Service Fixed Downtime    host_1    service_1    3600

    ${result}    Check Service Downtime With Timeout    host_1    anomaly_${serv_id}    1    60
    Should Be True    ${result}    anomaly service must be in downtime

    DELETE SERVICE DOWNTIME    host_1    service_1
    ${result}    Check Service Downtime With Timeout    host_1    service_1    0    60
    Should Be True    ${result}    dependent service must be in downtime
    ${result}    Check Service Downtime With Timeout    host_1    anomaly_${serv_id}    0    60
    Should Be True    ${result}    anomaly service must be in downtime

    Stop Engine
    Kindly Stop Broker

ANO_DT3
    [Documentation]    delete downtime on anomaly don t delete dependent service one
    [Tags]    broker    engine    anomaly
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    Config BBDO3    1
    ${serv_id}    Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,50,52],[2648812678,50,63]]
    Create Anomaly Threshold File    /tmp/anomaly_threshold.json    ${1}    ${serv_id}    metric    ${predict_data}
    Clear Retention
    Clear Db    services
    Clear Db    downtimes
    Start Broker
    ${start}    Get Current Date
    Start Engine
    # Let's wait for the check of external commands
    ${content}    Create List    check_for_external_commands
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    # create dependent service downtime
    Schedule Service Fixed Downtime    host_1    service_1    3600

    ${result}    Check Service Downtime With Timeout    host_1    anomaly_${serv_id}    1    60
    Should Be True    ${result}    anomaly service must be in downtime

    DELETE SERVICE DOWNTIME    host_1    anomaly_${serv_id}
    ${result}    Check Service Downtime With Timeout    host_1    anomaly_${serv_id}    0    60
    Should Be True    ${result}    anomaly service must be in downtime

    ${result}    Check Service Downtime With Timeout    host_1    service_1    1    60
    Should Be True    ${result}    dependent service must be in downtime

    Stop Engine
    Kindly Stop Broker

ANO_DT4
    [Documentation]    set dt on anomaly and on dependent service, delete last one don t delete first one
    [Tags]    broker    engine    anomaly
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    central
    Config Broker    rrd
    Config Broker    module    ${1}
    Broker Config Log    central    sql    debug
    Config Broker Sql Output    central    unified_sql
    Config BBDO3    1
    ${serv_id}    Create Anomaly Detection    ${0}    ${1}    ${1}    metric
    ${predict_data}    Evaluate    [[0,50,52],[2648812678,50,63]]
    Create Anomaly Threshold File    /tmp/anomaly_threshold.json    ${1}    ${serv_id}    metric    ${predict_data}
    Clear Retention
    Clear Db    services
    Clear Db    downtimes
    Start Broker
    ${start}    Get Current Date
    Start Engine
    # Let's wait for the check of external commands
    ${content}    Create List    check_for_external_commands
    ${result}    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    # create dependent service downtime
    Schedule Service Fixed Downtime    host_1    service_1    3600
    Schedule Service Fixed Downtime    host_1    anomaly_${serv_id}    3600

    ${result}    Check Service Downtime With Timeout    host_1    anomaly_${serv_id}    2    60
    Should Be True    ${result}    anomaly service must be in double downtime

    DELETE SERVICE DOWNTIME    host_1    service_1
    ${result}    Check Service Downtime With Timeout    host_1    service_1    0    60
    Should Be True    ${result}    dependent service mustn t be in downtime
    ${result}    Check Service Downtime With Timeout    host_1    anomaly_${serv_id}    1    60
    Should Be True    ${result}    anomaly service must be in simple downtime

    Stop Engine
    Kindly Stop Broker
