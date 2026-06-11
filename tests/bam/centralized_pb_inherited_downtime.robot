*** Settings ***
Documentation       Centreon Broker and BAM with centralized configuration.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn BAM Setup
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BECBAMIDTU1
    [Documentation]    Given BBDO version 3.0.1 is running with centralized configuration enabled
    ...    And a BA of type 'worst' with one service is configured
    ...    And The BA is in critical state due to its service
    ...    When a downtime is set on this service
    ...    Then an inherited downtime is set to the BA
    ...    When the downtime is removed from the service
    ...    Then the inherited downtime is deleted from the BA
    [Tags]    broker    downtime    engine    bam
    Ctn Clear Commands Status
    Ctn Clear Downtimes
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Broker Config Log    central    bam    trace
    Log To Console    Configuring core logger to info
    Ctn Broker Config Log    central    core    info
    Ctn Broker Config Log    rrd    core    info
    Ctn Broker Config Log    module0    core    info
    Log To Console    core logger configured

    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine
    Ctn Notify Broker Of Engine Config Change    ${0}

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    Ctn Create Ba With Services    test    worst    ${svc}
    Ctn Add Bam Config To Broker    central
    # Command of service_314 is set to critical
    ${cmd_1}    Ctn Get Service Command Id    314
    Log To Console    service_314 has command id ${cmd_1}
    Ctn Set Command Status    ${cmd_1}    2
    ${start}    Get Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    1

    # KPI set to critical
    Ctn Process Service Result Hard    host_16    service_314    2    output critical for 314
    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected

    # The BA should become critical
    ${result}    Ctn Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    The BA ba_1 is not CRITICAL as expected

    # A downtime is put on service_314
    ${dt_id}    Ctn Broker Schedule Service Downtime    host_16    service_314    3600
    ${result}    Ctn Check Service Downtime With Timeout    host_16    service_314    1    60
    Should Be True    ${result}    The service (host_16, service_314) is not in downtime as it should be
    ${result}    Ctn Check Service Downtime With Timeout    _Module_BAM_1    ba_1    1    60
    Should Be True    ${result}    The BA ba_1 is not in downtime as it should

    # Broker creates downtime comments (entry_type=2) like Engine, for the
    # service and for the inherited BA downtime.
    ${svc_cmt}    Ctn Check Comment    host_16    service_314    2    ${start}
    Should Be True    ${svc_cmt} > 0    No downtime comment was created for the service
    ${ba_cmt}    Ctn Check Comment    _Module_BAM_1    ba_1    2    ${start}
    Should Be True    ${ba_cmt} > 0    No downtime comment was created for the inherited BA downtime

    # The downtime is deleted
    Ctn Broker Delete Downtime    ${dt_id}
    ${result}    Ctn Check Service Downtime With Timeout    host_16    service_314    0    60
    Should Be True    ${result}    The service (host_16, service_314) is in downtime and should not.

    ${result}    Ctn Check Downtimes With Timeout    0    60
    Should Be True    ${result}    No downtime should still be running.
    ${result}    Ctn Check Service Downtime With Timeout    _Module_BAM_1    ba_1    0    60
    Should Be True    ${result}    The BA ba_1 is in downtime as it should not

    # The downtime comments are removed along with the downtimes.
    ${result}    Ctn Check Comment    host_16    service_314    2    ${start}    exists=False
    Should Be True    ${result}    The service downtime comment was not deleted
    ${result}    Ctn Check Comment    _Module_BAM_1    ba_1    2    ${start}    exists=False
    Should Be True    ${result}    The inherited BA downtime comment was not deleted

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BECBAMIDTU2
    [Documentation]    Given BBDO version 3.0.1 is in use
    ...    And a 'worst' type BA with one service is configured
    ...    And The BA is in critical state due to its service
    ...    When a downtime is set on this service
    ...    Then an inherited downtime is set to the BA
    ...    When Engine is restarted
    ...    And Broker is restarted
    ...    Then both downtimes are still present with no duplicates
    ...    When the downtime is removed from the service
    ...    Then the inherited downtime is deleted
    [Tags]    broker    downtime    engine    bam    start    stop
    Ctn Clear Commands Status
    Ctn Clear Downtimes
    Ctn Clear Retention
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Broker Config Log    central    bam    trace
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    module0    core    error
    Ctn Broker Config Log    rrd    core    error
    Ctn Broker Config Flush Log    central    0

    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine
    Ctn Notify Broker Of Engine Config Change    ${0}

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    Ctn Create Ba With Services    test    worst    ${svc}
    Ctn Add Bam Config To Broker    central
    # Command of service_314 is set to critical
    ${cmd_1}    Ctn Get Service Command Id    314
    Log To Console    service_314 has command id ${cmd_1}
    Ctn Set Command Status    ${cmd_1}    2
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    1

    # KPI set to critical
    Log To Console    KPI set to critical
    Ctn Process Service Result Hard    host_16    service_314    2    output critical for service_314
    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    2    60  HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected

    # The BA should become critical
    Log To Console    The BA should become critical
    ${result}    Ctn Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    The BA ba_1 is not CRITICAL as expected

    # A downtime is put on service_314
    Log To Console    A downtime is put on service_314
    # Stable reference for the comment lookups: ${start} is reassigned in the
    # restart loop below, so we cannot reuse it after the loop.
    ${cmt_ref}    Ctn Get Round Current Date
    ${dt_id}    Ctn Broker Schedule Service Downtime    host_16    service_314    3600
    ${result}    Ctn Check Service Downtime With Timeout    host_16    service_314    1    60
    Should Be True    ${result}    The service (host_16, service_314) is not in downtime as it should be

    Log To Console    An inherited downtime is propagated to the BA ba_1
    ${result}    Ctn Check Service Downtime With Timeout    _Module_BAM_1    ba_1    1    60
    Should Be True    ${result}    The BA ba_1 is not in downtime as it should

    # Broker created the downtime comments (entry_type=2), like Engine.
    ${svc_cmt}    Ctn Check Comment    host_16    service_314    2    ${cmt_ref}
    Should Be True    ${svc_cmt} > 0    No downtime comment was created for the service
    ${ba_cmt}    Ctn Check Comment    _Module_BAM_1    ba_1    2    ${cmt_ref}
    Should Be True    ${ba_cmt} > 0    No downtime comment was created for the inherited BA downtime

    # There are still two downtimes: the one on the ba and the one on the kpi.
    Log To Console    We should have two downtimes (1)
    ${result}    Ctn Number Of Downtimes Is    2    30
    Should Be True    ${result}    We should only have two downtimes

    FOR    ${i}    IN RANGE    1
        # Engine is restarted
        Ctn Stop Engine
        ${start}    Ctn Get Round Current Date
        Ctn Start Engine    newGeneration=True
        Ctn Wait For Engine To Be Ready    ${start}    1

        Log To Console    We should have two downtimes (2)
        ${result}    Ctn Number Of Downtimes Is    2    30
        Should Be True    ${result}    We should only have two downtimes

        # Broker is restarted
        Log To Console    Broker is stopped (step ${i})
        Ctn Kindly Stop Broker
        Log To Console    Broker is started
        Ctn Start Broker    newGeneration=True
        # Ctn Start Broker returns before the gRPC server is listening; wait for
        # it so the gRPC calls below this loop do not hit a refused connection.
        ${ready}    Ctn Wait For Broker To Be Ready
        Should Be True    ${ready}    Broker gRPC server should be ready after restart

        Log To Console    We should have two downtimes (3)
        ${result}    Ctn Number Of Downtimes Is    2    30
        Should Be True    ${result}    We should only have two downtimes
    END

    # There are still two downtimes: the one on the ba and the one on the kpi.
    Log To Console    We should still have two downtimes (4)
    ${result}    Ctn Number Of Downtimes Is    2    60
    Should Be True    ${result}    We should only have two downtimes

    # The downtime is deleted
    Ctn Broker Delete Downtime    ${dt_id}
    ${result}    Ctn Check Service Downtime With Timeout    host_16    service_314    0    60
    Should Be True    ${result}    The service (host_16, service_314) is in downtime and should not.
    ${result}    Ctn Check Service Downtime With Timeout    _Module_BAM_1    ba_1    0    60
    Should Be True    ${result}    The BA ba_1 is in downtime as it should not

    # The downtime comments survived the restart and are deleted with the downtimes.
    ${result}    Ctn Check Comment    host_16    service_314    2    ${cmt_ref}    exists=False
    Should Be True    ${result}    The service downtime comment was not deleted
    ${result}    Ctn Check Comment    _Module_BAM_1    ba_1    2    ${cmt_ref}    exists=False
    Should Be True    ${result}    The inherited BA downtime comment was not deleted

    # We should have no more downtime
    ${result}    Ctn Number Of Downtimes Is    0    60
    Should Be True    ${result}    We should have no more downtime

    Log To Console    Broker is stopped (end of BECBAMIDTU2)
    Ctn Stop Engine
    Ctn Kindly Stop Broker    no_rrd_test=True

BECBAMIGNDTU1
    [Documentation]    With bbdo version 3.0.1, a BA of type 'worst' with two services is configured.
    ...    The downtime policy on this ba is "Ignore the indicator in the calculation". The BA is in
    ...    critical state, because of the second critical service. Then we apply two downtimes on this
    ...    last one. The BA state is ok because of the policy on indicators. A first downtime is cancelled,
    ...    the BA is still OK, but when the second downtime is cancelled, the BA should be CRITICAL.
    [Tags]    broker    downtime    engine    bam
    Ctn Clear Commands Status
    Ctn Clear Retention
    Ctn Clear Downtimes
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Add Item    central    notification_mode    broker
    Ctn Broker Config Log    central    bam    trace
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Flush Log    module0    0
    Ctn Broker Config Log    module0    neb    trace
    Ctn Broker Config Log    central    core    info
    Ctn Broker Config Log    rrd    core    info
    Ctn Broker Config Log    module0    core    info
    Ctn Config Broker Sql Output    central    unified_sql

    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine
    Ctn Notify Broker Of Engine Config Change    ${0}

    @{svc}    Set Variable    ${{ [("host_16", "service_313"), ("host_16", "service_314")] }}
    Ctn Create Ba With Services    test    worst    ${svc}    ignore
    Ctn Add Bam Config To Broker    central

    # Command of service_313 is set to ok
    ${cmd_1}    Ctn Get Service Command Id    313
    Log To Console    service_313 has command id ${cmd_1}
    Ctn Set Command Status    ${cmd_1}    0

    # Command of service_314 is set to critical
    ${cmd_2}    Ctn Get Service Command Id    314
    Log To Console    service_314 has command id ${cmd_2}
    Ctn Set Command Status    ${cmd_2}    2

    ${start}    Get Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    1

    # KPI set to ok
    Ctn Process Service Result Hard    host_16    service_313    0    output critical for 313
    ${result}    Ctn Check Service Status With Timeout    host_16    service_313    0    60  HARD
    Should Be True    ${result}    The service (host_16,service_313) is not OK as expected

    # KPI set to critical
    Ctn Process Service Result Hard    host_16    service_314    2    output critical for 314
    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    2    60  HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected

    # The BA should become critical
    ${result}    Ctn Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    The BA ba_1 is not CRITICAL as expected
    Log To Console    The BA is critical.

    # Two downtimes are applied on service_314
    ${dt_id1}    Ctn Broker Schedule Service Downtime    host_16    service_314    3600
    ${result}    Ctn Check Service Downtime With Timeout    host_16    service_314    1    60
    Should Be True    ${result}    The service (host_16, service_314) is not in downtime as it should be
    Log To Console    One downtime applied to service_314.

    ${dt_id2}    Ctn Broker Schedule Service Downtime    host_16    service_314    1800
    ${result}    Ctn Check Service Downtime With Timeout    host_16    service_314    2    60
    Should Be True    ${result}    The service (host_16, service_314) is not in downtime as it should be
    Log To Console    Two downtimes applied to service_314.

    ${result}    Ctn Check Service Downtime With Timeout    _Module_BAM_1    ba_1    0    60
    Should Be True    ${result}    The BA ba_1 is in downtime but should not
    Log To Console    The BA is configured to ignore kpis in downtime

    # The service carries downtime comments; the ignored BA has none.
    ${svc_cmt}    Ctn Check Comment    host_16    service_314    2    ${start}
    Should Be True    ${svc_cmt} > 0    No downtime comment was created for the service
    ${result}    Ctn Check Comment    _Module_BAM_1    ba_1    2    ${start}    exists=False    timeout=15
    Should Be True    ${result}    The ignored BA must not carry a downtime comment

    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The service in downtime should be ignored while computing the state of this BA.
    Log To Console    The BA is OK, since the critical service is in downtime.

    # The first downtime is deleted
    Ctn Broker Delete Downtime    ${dt_id1}

    ${result}    Ctn Check Service Downtime With Timeout    host_16    service_314    1    60
    Should Be True    ${result}    The service (host_16, service_314) does not contain 1 downtime as it should
    Log To Console    Still one downtime applied to service_314.

    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA is not OK whereas the service_314 is still in downtime.
    Log To Console    The BA is still OK

    ${result}    Ctn Check Downtimes With Timeout    1    60
    Should Be True    ${result}    We should have one running downtime

    # The second downtime is deleted
    Ctn Broker Delete Downtime    ${dt_id2}

    ${result}    Ctn Check Service Downtime With Timeout    host_16    service_314    0    60
    Should Be True    ${result}    The service (host_16, service_314) does not contain 0 downtime as it should
    Log To Console    No more downtime applied to service_314.

    # Both downtimes gone, so both downtime comments are deleted too.
    ${result}    Ctn Check Comment    host_16    service_314    2    ${start}    exists=False
    Should Be True    ${result}    The service downtime comments were not all deleted

    ${result}    Ctn Check Downtimes With Timeout    0    60
    Should Be True    ${result}    We should have no more running downtimes

    ${result}    Ctn Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    The critical service is no more in downtime, the BA should be critical.
    Log To Console    The BA is now critical (no more downtime)

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BECBAMIGNDTU2
    [Documentation]    Given BBDO version 3.0.1 is configured
    ...    And a BA of type "worst" with two services is set up
    ...    And the downtime policy on this BA is "Ignore the indicator in the calculation"
    ...    And the BA is in a critical state due to the second critical service
    ...    When two downtimes are applied to the second critical service
    ...    Then the BA state should be OK due to the policy on indicators
    ...    When the first downtime reaches its end
    ...    Then the BA state should still be OK
    ...    When the second downtime reaches its end
    ...    Then the BA should be in a critical state
    [Tags]    broker    downtime    engine    bam
    Ctn Clear Commands Status
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Broker Config Log    module0    core    error
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    bam    trace
    Ctn Config Broker    rrd
    Ctn Broker Config Add Item    central    notification_mode    broker

    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine
    Ctn Notify Broker Of Engine Config Change    ${0}

    @{svc}    Set Variable    ${{ [("host_16", "service_313"), ("host_16", "service_314")] }}
    Ctn Create Ba With Services    test    worst    ${svc}    ignore
    Ctn Add Bam Config To Broker    central
    # Command of service_313 is set to ok
    ${cmd_1}    Ctn Get Service Command Id    313
    Log To Console    service_313 has command id ${cmd_1}
    Ctn Set Command Status    ${cmd_1}    0
    # Command of service_314 is set to critical
    ${cmd_2}    Ctn Get Service Command Id    314
    Log To Console    service_314 has command id ${cmd_2}
    Ctn Set Command Status    ${cmd_2}    2
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    1

    # KPI set to ok
    Ctn Process Service Result Hard    host_16    service_313    0    output critical for 313
    ${result}    Ctn Check Service Status With Timeout    host_16    service_313    0    60  HARD
    Should Be True    ${result}    The service (host_16,service_313) is not OK as expected

    # KPI set to critical
    Ctn Process Service Result Hard    host_16    service_314    2    output critical for 314
    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    2    60  HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected

    # The BA should become critical
    ${result}    Ctn Check Ba Status With Timeout    test    2    60
    Should Be True    ${result}    The BA ba_1 is not CRITICAL as expected
    Log To Console    The BA is critical.

    # Two downtimes are applied on service_314 back-to-back: one 120s, one 60s.
    # Scheduling them together ensures both ADD events land in the same 10s flush
    # batch, so the DB shows depth=2 well before either downtime expires.
    ${dt_id1}    Ctn Broker Schedule Service Downtime    host_16    service_314    120
    ${dt_id2}    Ctn Broker Schedule Service Downtime    host_16    service_314    60
    Ctn Process Service Result Hard    host_16    service_314    2    output critical for 314

    ${result}    Ctn Check Service Downtime With Timeout    host_16    service_314    2    60
    Should Be True    ${result}    The service (host_16, service_314) is not in downtime as it should be
    Log To Console    Two downtimes applied to service_314.

    ${result}    Ctn Check Service Downtime With Timeout    _Module_BAM_1    ba_1    0    60
    Should Be True    ${result}    The BA ba_1 is in downtime but should not
    Log To Console    The BA is configured to ignore kpis in downtime

    # The service carries downtime comments; the ignored BA has none.
    ${svc_cmt}    Ctn Check Comment    host_16    service_314    2    ${start}
    Should Be True    ${svc_cmt} > 0    No downtime comment was created for the service
    ${result}    Ctn Check Comment    _Module_BAM_1    ba_1    2    ${start}    exists=False    timeout=15
    Should Be True    ${result}    The ignored BA must not carry a downtime comment

    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The service in downtime should be ignored while computing the state of this BA.
    Log To Console    The BA is OK, since the critical service is in downtime.

    # The shorter downtime (60s) expires first
    Log To Console    After 60s, the shorter downtime should be finished.
    ${result}    Ctn Check Service Downtime With Timeout    host_16    service_314    1    90
    Should Be True    ${result}    The service (host_16, service_314) does not contain 1 downtime as it should
    Log To Console    Still one downtime applied to service_314.

    Log To Console    The longer downtime (120s) is still active.
    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Should Be True    ${result}    The BA is not OK whereas the service_314 is still in downtime.
    Log To Console    The BA is still OK

    # The longer downtime (120s) finishes
    ${result}    Ctn Check Ba Status With Timeout    test    2    90
    Should Be True    ${result}    The critical service is no more in downtime, the BA should be critical.
    Log To Console    The BA is now critical (no more downtime)

    # Comments are removed when their downtimes expire, like Engine does.
    ${result}    Ctn Check Comment    host_16    service_314    2    ${start}    exists=False
    Should Be True    ${result}    The service downtime comments were not deleted when the downtimes expired

    Ctn Stop Engine
    Ctn Kindly Stop Broker


*** Keywords ***
Ctn BAM Setup
    Ctn Stop Processes
    Ctn Clear Prot Files
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${date}    Get Current Date    result_format=epoch
    Log To Console    Cleaning downtimes at date=${date}
    Execute SQL String
    ...    UPDATE downtimes SET deletion_time=${date}, actual_end_time=${date} WHERE actual_end_time is null
    Execute SQL String    UPDATE services SET scheduled_downtime_depth=0
    Execute SQL String    UPDATE hosts SET scheduled_downtime_depth=0
    Execute SQL String    UPDATE resources SET in_downtime=0
    Disconnect From Database

