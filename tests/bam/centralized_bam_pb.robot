*** Settings ***
Documentation     Centreon Broker and BAM with centralized configuration.

Resource          ../resources/import.resource

Suite Setup       Ctn Clean Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup        Ctn BAM Setup
Test Teardown     Ctn Stop Engine Broker And Save Logs


*** Test Cases ***
CBAWORST_ACK
    [Documentation]    Scenario: Acknowledging a service acknowledges the BA, and removing it unacknowledges the BA
    ...    Given BBDO version is 3.0.1
    ...    And a Business Activity of type "worst" is configured with two services
    ...    When one of the services is acknowledged
    ...    Then the Business Activity is acknowledged
    ...    When the acknowledgement is removed from the service
    ...    Then the Business Activity is no longer acknowledged
    [Tags]    broker    downtime    engine    bam    MON-160249

    Ctn BAM Init

    @{svc}    Set Variable    ${{ [("host_16", "service_314"), ("host_16", "service_303")] }}
    ${ba__svc}    Ctn Create Ba With Services    test    worst    ${svc}
    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True

    # Let's wait for the external command check start
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA test is not OK as expected

    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is OK - All KPIs are in an OK state
    ...    60
    Should Be True    ${result}    The BA test has not the expected output

    # KPI set to critical
    Ctn Process Service Result Hard    host_16    service_303    2    output unknown for 303

    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected

    # The BA should become unknown
    ${result}    Ctn Check Ba Status With Timeout    test    2    60
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA test is not UNKNOWN as expected

    # The CRITICAL service is acknowledged.
    Ctn Acknowledge Service Problem    host_16    service_303

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Check Query Result
    ...    SELECT acknowledged FROM mod_bam_kpi WHERE host_id=16 AND service_id=303
    ...    >
    ...    ${0.5}
    ...    retry_timeout=30s
    ...    retry_pause=1s
    Disconnect From Database

    # The acknowledgement is removed.
    Ctn Remove Service Acknowledgement    host_16    service_303
    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Check Query Result
    ...    SELECT acknowledged FROM mod_bam_kpi WHERE host_id=16 AND service_id=303
    ...    <
    ...    ${0.01}
    ...    retry_timeout=30s
    ...    retry_pause=1s
    Disconnect From Database

CBAWORST
    [Documentation]    Scenario: A BA of type "worst" reacts to KPI state changes and broker stats are valid after reload
    ...    Given BBDO version is 3.0.1
    ...    And a Business Activity of type "worst" is configured with two services
    ...    When all services are OK
    ...    Then the Business Activity is OK
    ...    When one service becomes UNKNOWN
    ...    Then the Business Activity is UNKNOWN
    ...    When that service becomes WARNING
    ...    Then the Business Activity is WARNING
    ...    When another service becomes CRITICAL
    ...    Then the Business Activity is CRITICAL
    ...    And broker stats show expected endpoints state
    ...    When broker and engine are reloaded
    ...    Then broker stats still show expected endpoints state
    ...    And the GetBa gRPC command returns a valid digraph output
    [Tags]    broker    downtime    engine    bam
    Ctn BAM Init

    @{svc}    Set Variable    ${{ [("host_16", "service_314"), ("host_16", "service_303")] }}
    ${ba__svc}    Ctn Create Ba With Services    test    worst    ${svc}
    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}

    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA test is not OK as expected

    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is OK - All KPIs are in an OK state
    ...    60
    Should Be True    ${result}    The BA test has not the expected output

    # KPI set to unknown
    Ctn Process Service Result Hard    host_16    service_303    3    output unknown for 303

    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    3    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not UNKNOWN as expected

    # The BA should become unknown
    ${result}    Ctn Check Ba Status With Timeout    test    3    60
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA test is not UNKNOWN as expected

    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is UNKNOWN - At least one KPI is in an UNKNOWN state: KPI Service host_16/service_303 is in UNKNOWN state
    ...    60
    Should Be True    ${result}    The BA test has not the expected output

    # KPI set to warning
    Ctn Process Service Result Hard    host_16    service_303    1    output warning for 303

    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    1    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not WARNING as expected

    # The BA should become warning
    ${result}    Ctn Check Ba Status With Timeout    test    1    60
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA test is not WARNING as expected

    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is WARNING - At least one KPI is in a WARNING state: KPI Service host_16/service_303 is in WARNING state
    ...    60
    Should Be True    ${result}    The BA test has not the expected output

    # KPI set to critical
    Ctn Process Service Result Hard    host_16    service_314    2    output critical for 314

    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected

    # The BA should become critical
    ${result}    Ctn Check Ba Status With Timeout    test    2    60
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA test is not CRITICAL as expected

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${output}    Query
    ...    SELECT acknowledged, downtime, in_downtime, current_status FROM mod_bam WHERE name='test'
    Should Be Equal As Strings    ${output}    ((0.0, 0.0, 0, 2),)

    Disconnect From Database

    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is CRITICAL - At least one KPI is in a CRITICAL state: KPI Service host_16/service_303 is in WARNING state, KPI Service host_16/service_314 is in CRITICAL state
    ...    60
    Should Be True    ${result}    The BA test has not the expected output

    # check broker stats
    ${res}    Ctn Get Broker Stats
    ...    central
    ...    1: 127.0.0.1:[0-9]+
    ...    10
    ...    endpoint central-broker-master-input
    ...    peers
    Should Be True    ${res}    no central-broker-master-input.peers found in broker stat output

    ${res}    Ctn Get Broker Stats    central    listening    10    endpoint central-broker-master-input    state
    Should Be True    ${res}    central-broker-master-input not listening

    ${res}    Ctn Get Broker Stats    central    connected    10    endpoint centreon-bam-monitoring    state
    Should Be True    ${res}    central-bam-monitoring not connected

    ${res}    Ctn Get Broker Stats    central    connected    10    endpoint centreon-bam-reporting    state
    Should Be True    ${res}    central-bam-reporting not connected
    Disconnect From Database

    Ctn Reload Engine
    Ctn Reload Broker

    # check broker stats
    ${res}    Ctn Get Broker Stats
    ...    central
    ...    1: 127.0.0.1:[0-9]+
    ...    10
    ...    endpoint central-broker-master-input
    ...    peers
    Should Be True    ${res}    no central-broker-master-input.peers found in broker stat output

    ${res}    Ctn Get Broker Stats    central    listening    10    endpoint central-broker-master-input    state
    Should Be True    ${res}    central-broker-master-input not listening

    ${res}    Ctn Get Broker Stats    central    connected    10    endpoint centreon-bam-monitoring    state
    Should Be True    ${res}    central-bam-monitoring not connected

    ${res}    Ctn Get Broker Stats    central    connected    10    endpoint centreon-bam-reporting    state
    Should Be True    ${res}    central-bam-reporting not connected

    # Little check of the GetBa gRPC command
    ${result}    Run Keyword And Return Status    File Should Exist    /tmp/output
    IF    ${result} is True    Remove File    /tmp/output
    Ctn Broker Get Ba    51001    1    /tmp/output
    Wait Until Created    /tmp/output
    ${result}    Grep File    /tmp/output    digraph
    Should Not Be Empty    ${result}    /tmp/output does not contain the word 'digraph'

CBAWORST2
    [Documentation]    Scenario: A BA of type "worst" with a boolean KPI and a child BA KPI reacts correctly to state changes
    ...    Given BBDO version is 3.0.1
    ...    And a Business Activity of type "worst" is configured with a boolean KPI and a child BA KPI
    ...    When all KPIs are in an OK state
    ...    Then the Business Activity is OK
    ...    When the boolean rule becomes CRITICAL
    ...    Then the Business Activity is CRITICAL
    ...    When the child BA also becomes CRITICAL
    ...    Then the Business Activity is still CRITICAL with both KPIs reported
    ...    When the boolean rule recovers to OK
    ...    Then the Business Activity remains CRITICAL due to the child BA KPI
    [Tags]    broker    engine    bam
    Ctn BAM Init

    ${id_ba__sid}    Ctn Create Ba    test    worst    100    100
    Ctn Add Boolean Kpi
    ...    ${id_ba__sid[0]}
    ...    {host_16 service_302} {IS} {OK}
    ...    False
    ...    100

    # ba kpi
    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    ${id_ba__sid__child}    Ctn Create Ba With Services    test_child    worst    ${svc}
    Ctn Add Ba Kpi    ${id_ba__sid__child[0]}    ${id_ba__sid[0]}    1    2    3

    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}

    # service_302 is passive (matched by Ctn Set Services Passive pattern service_30.)
    # so it starts UNKNOWN. The boolean KPI evaluates in real-time: UNKNOWN service
    # causes the expression to return UNKNOWN, keeping the BA at UNKNOWN (3).
    # Send an initial OK result to move service_302 to OK.
    Ctn Process Service Result Hard    host_16    service_302    0    output OK
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    0    60    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not OK as expected

    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA test is not OK as expected
    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is OK - All KPIs are in an OK state
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

    # boolean critical => ba test critical
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected

    Sleep    2s
    ${result}    Ctn Check Ba Status With Timeout    test    2    60
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA test is not CRITICAL as expected
    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is CRITICAL - At least one KPI is in a CRITICAL state: KPI Boolean rule bool test is in CRITICAL state
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

    # child ba critical
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_314
    ...    2
    ...    output critical for service_314
    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected
    Sleep    2s
    ${result}    Ctn Check Ba Status With Timeout    test_child    2    60
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA test_child is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test    2    60
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA test is not CRITICAL as expected
    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is CRITICAL - At least one KPI is in a CRITICAL state: KPI Business Activity test_child is in CRITICAL state, KPI Boolean rule bool test is in CRITICAL state
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

    # boolean rule ok stay in critical
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_302
    ...    0
    ...    output OK
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    0    60    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not OK as expected
    Sleep    2s
    ${result}    Ctn Check Ba Status With Timeout    test    2    60
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA test is not CRITICAL as expected
    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is CRITICAL - At least one KPI is in a CRITICAL state: KPI Business Activity test_child is in CRITICAL state
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

CBABEST_SERVICE_CRITICAL
    [Documentation]    With bbdo version 3.0.1, a BA of type 'best' with 2 serv, ba is critical only if the 2 services are critical
    [Tags]    broker    engine    bam
    Ctn BAM Init

    @{svc}    Set Variable    ${{ [("host_16", "service_314"), ("host_16", "service_303")] }}
    ${ba__svc}    Ctn Create Ba With Services    test    best    ${svc}
    # Command of service_314 is set to critical
    ${cmd_1}    Ctn Get Service Command Id    314
    Log To Console    service_314 has command id ${cmd_1}
    Ctn Set Command Status    ${cmd_1}    2
    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}

    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA test is not OK as expected

    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is OK - At least one KPI is in an OK state
    ...    60
    Should Be True    ${result}    The BA test has not the expected output

    # KPI set to critical
    Ctn Process Service Result Hard    host_16    service_314    2    output critical for 314

    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected

    # The BA should remain OK
    Sleep    2s
    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA test is not OK as expected
    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is OK - At least one KPI is in an OK state
    ...    60
    Should Be True    ${result}    The BA test has not the expected output

    # KPI set to unknown
    Ctn Process Service Result Hard    host_16    service_303    3    output unknown for 303

    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    3    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not UNKNOWN as expected

    # The BA should become warning
    ${result}    Ctn Check Ba Status With Timeout    test    3    60
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA test is not UNKNOWN as expected
    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is UNKNOWN - All KPIs are in an UNKNOWN state or worse (WARNING or CRITICAL)
    ...    60
    Should Be True    ${result}    The BA test has not the expected output

    # KPI set to warning
    Ctn Process Service Result Hard    host_16    service_303    1    output warning for 303

    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    1    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not WARNING as expected

    # The BA should become warning
    ${result}    Ctn Check Ba Status With Timeout    test    1    60
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA test is not WARNING as expected
    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is WARNING - All KPIs are in a WARNING state or worse (CRITICAL)
    ...    60
    Should Be True    ${result}    The BA test has not the expected output

    # KPI set to critical
    Ctn Process Service Result Hard    host_16    service_303    2    output critical for 303

    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected

    # The BA should become critical
    ${result}    Ctn Check Ba Status With Timeout    test    2    60
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA test is not CRITICAL as expected
    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is CRITICAL - All KPIs are in a CRITICAL state
    ...    60
    Should Be True    ${result}    The BA test has not the expected output

    # KPI set to OK
    Ctn Process Service Check Result    host_16    service_314    0    output ok for 314

    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    0    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not OK as expected

    # The BA should become OK
    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA test is not OK as expected

CBA_IMPACT_2KPI_SERVICES
    [Documentation]    With bbdo version 3.0.1, a BA of type 'impact' with 2 serv, ba is critical only if the 2 services are critical
    [Tags]    broker    engine    bam
    Ctn BAM Init

    ${id_ba__sid}    Ctn Create Ba    test    impact    20    35
    Ctn Add Service Kpi    host_16    service_302    ${id_ba__sid[0]}    40    30    20
    Ctn Add Service Kpi    host_16    service_303    ${id_ba__sid[0]}    40    30    20

    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}

    # service_302 critical service_303 warning => ba warning 30%
    Ctn Process Service Result Hard    host_16    service_302    2    output critical for service_302
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA test is not OK as expected
    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is OK - Level = 60 (warn: 35 - crit: 20) - 1 KPI out of 2 impacts the BA: KPI Service host_16/service_302 (impact: 40)|BA_Level=60;35;20;0;100
    ...    60
    Should Be True    ${result}    The BA test has not the expected output

    Ctn Process Service Result Hard    host_16    service_303    1    output warning for service_303
    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    1    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not WARNING as expected
    ${result}    Ctn Check Ba Status With Timeout    test    1    60
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA ba_1 is not WARNING as expected
    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is WARNING - Level = 30 - 2 KPIs out of 2 impact the BA for 70 points - KPI Service host_16/service_303 (impact: 30), KPI Service host_16/service_302 (impact: 40)|BA_Level=30;35;20;0;100
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

    # service_302 critical service_303 critical => ba critical 80%
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test    2    60
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA ba_1 is not CRITICAL as expected
    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is CRITICAL - Level = 20 - 2 KPIs out of 2 impact the BA for 80 points - KPI Service host_16/service_303 (impact: 40), KPI Service host_16/service_302 (impact: 40)|BA_Level=20;35;20;0;100
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

    # service_302 ok => ba ok
    Ctn Process Service Check Result    host_16    service_302    0    output ok for service_302
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    0    60    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not OK as expected
    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA ba_1 is not OK as expected
    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is OK - Level = 60 (warn: 35 - crit: 20) - 1 KPI out of 2 impacts the BA: KPI Service host_16/service_303 (impact: 40)|BA_Level=60;35;20;0;100
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

    # both warning => ba ok
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_302
    ...    1
    ...    output warning for service_302
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_303
    ...    1
    ...    output warning for service_303
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    1    60    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not WARNING as expected
    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    1    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not WARNING as expected
    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA test is not OK as expected
    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is OK - Level = 40 (warn: 35 - crit: 20) - 2 KPIs out of 2 impact the BA: KPI Service host_16/service_303 (impact: 30), KPI Service host_16/service_302 (impact: 30)|BA_Level=40;35;20;0;100
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

CBA_RATIO_PERCENT_BA_SERVICE
    [Documentation]    With bbdo version 3.0.1, a BA of type 'ratio percent' with 2 serv an 1 ba with one service
    [Tags]    broker    engine    bam
    Ctn BAM Init

    ${id_ba__sid}    Ctn Create Ba    test    ratio_percent    67    49
    Ctn Add Service Kpi    host_16    service_302    ${id_ba__sid[0]}    40    30    20
    Ctn Add Service Kpi    host_16    service_303    ${id_ba__sid[0]}    40    30    20

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    ${id_ba__sid__child}    Ctn Create Ba With Services    test_child    worst    ${svc}
    Ctn Add Ba Kpi    ${id_ba__sid__child[0]}    ${id_ba__sid[0]}    1    2    3

    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}

    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA test is not OK as expected
    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is OK - 0% of KPIs are in a CRITICAL state (warn: 49 - crit: 67)|BA_Level=0%;49;67;0;100
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

    # one serv critical => ba ok
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    Sleep    2s
    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA test is not OK as expected
    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is OK - 33% of KPIs are in a CRITICAL state (warn: 49 - crit: 67)|BA_Level=33%;49;67;0;100
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

    # two serv critical => ba warning
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test    1    30
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA test is not WARNING as expected
    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is WARNING - 66% of KPIs are in a CRITICAL state (warn: 49 - crit: 67)|BA_Level=66%;49;67;0;100
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

    # two serv critical and child ba critical => mother ba critical
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_314
    ...    2
    ...    output critical for service_314
    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test_child    2    30
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA test_child is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test    2    30
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA test is not CRITICAL as expected
    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is CRITICAL - 100% of KPIs are in a CRITICAL state (warn: 49 - crit: 67)|BA_Level=100%;49;67;0;100
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

CBA_RATIO_NUMBER_BA_SERVICE
    [Documentation]    With bbdo version 3.0.1, a BA of type 'ratio number' with 2 services and one ba with 1 service
    [Tags]    broker    engine    bam
    Ctn BAM Init

    ${id_ba__sid}    Ctn Create Ba    test    ratio_number    3    2
    Ctn Add Service Kpi    host_16    service_302    ${id_ba__sid[0]}    40    30    20
    Ctn Add Service Kpi    host_16    service_303    ${id_ba__sid[0]}    40    30    20

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    ${id_ba__sid__child}    Ctn Create Ba With Services    test_child    worst    ${svc}
    Ctn Add Ba Kpi    ${id_ba__sid__child[0]}    ${id_ba__sid[0]}    1    2    3

    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}

    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA test is not OK as expected
    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is OK - 0 out of 3 KPIs are in a CRITICAL state (warn: 2 - crit: 3)|BA_Level=0;2;3;0;3
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

    # One service CRITICAL => The BA is still OK
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected

    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA test is not OK as expected

    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is OK - 1 out of 3 KPIs are in a CRITICAL state (warn: 2 - crit: 3)|BA_Level=1;2;3;0;3
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

    # Two services CRITICAL => The BA passes to WARNING
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test    1    60
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The test BA is not in WARNING as expected
    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is WARNING - 2 out of 3 KPIs are in a CRITICAL state (warn: 2 - crit: 3)|BA_Level=2;2;3;0;3
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

    # Two services CRITICAL and also the child BA => The mother BA passes to CRITICAL
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_314
    ...    2
    ...    output critical for service_314
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test_child    2    30
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA test_child is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test    2    60
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA test is not CRITICAL as expected
    ${result}    Ctn Check Ba Output With Timeout
    ...    test
    ...    Status is CRITICAL - 3 out of 3 KPIs are in a CRITICAL state (warn: 2 - crit: 3)|BA_Level=3;2;3;0;3
    ...    10
    Should Be True    ${result}    The BA test has not the expected output

CBA_BOOL_KPI
    [Documentation]    With bbdo version 3.0.1, a BA of type 'worst' with 1 boolean kpi
    [Tags]    broker    engine    bam
    Ctn BAM Init

    ${id_ba__sid}    Ctn Create Ba    test    worst    100    100
    Ctn Add Boolean Kpi
    ...    ${id_ba__sid[0]}
    ...    {host_16 service_302} {IS} {OK} {OR} ( {host_16 service_303} {IS} {OK} {AND} {host_16 service_314} {NOT} {UNKNOWN} )
    ...    False
    ...    100

    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}

    # 302 warning and 303 critical    => ba critical
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_302
    ...    1
    ...    output warning for service_302
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    Ctn Process Service Check Result    host_16    service_314    0    output OK for service_314
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    1    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not WARNING as expected
    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    0    30    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not OK as expected

#    Ctn Schedule Forced Service Check    _Module_BAM_1    ba_1
    ${result}    Ctn Check Ba Status With Timeout    test    2    30
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA test is not CRITICAL as expected

BECPB_DIMENSION_BV_EVENT
    [Documentation]    bbdo_version 3 use pb_dimension_bv_event message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Ctn BAM Init

    ${id_ba__sid}    Ctn Create Ba    test    worst    100    100

    Remove File    /tmp/all_lua_event.log

    Ctn Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-log-all-event.lua

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM mod_bam_ba_groups
    Execute SQL String
    ...    INSERT INTO mod_bam_ba_groups (id_ba_group, ba_group_name, ba_group_description) VALUES (574, 'virsgtr', 'description_grtmxzo')

    Disconnect From Database
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Wait Until Created    /tmp/all_lua_event.log    30s
    FOR    ${index}    IN RANGE    10
        ${grep_res}    Grep File
        ...    /tmp/all_lua_event.log
        ...    "_type":393238, "category":6, "element":22, "bv_id":574, "bv_name":"virsgtr", "bv_description":"description_grtmxzo"
        Sleep    1s
        IF    len("""${grep_res}""") > 0    BREAK
    END

    Should Not Be Empty    ${grep_res}    event not found

    [Teardown]    Ctn Stop Engine Broker And Save Logs    ${True}

BECPB_DIMENSION_BA_EVENT
    [Documentation]    bbdo_version 3 use pb_dimension_ba_event message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Ctn BAM Init

    Remove File    /tmp/all_lua_event.log

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    ${id_ba__sid}    Ctn Create Ba With Services    test    worst    ${svc}

    Ctn Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-log-all-event.lua

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    SET FOREIGN_KEY_CHECKS=0
    Execute SQL String
    ...    UPDATE mod_bam set description='fdpgvo75', sla_month_percent_warn=1.23, sla_month_percent_crit=4.56, sla_month_duration_warn=852, sla_month_duration_crit=789, id_reporting_period=741

    Disconnect From Database
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Wait Until Created    /tmp/all_lua_event.log    30s
    FOR    ${index}    IN RANGE    10
        ${grep_res}    Grep File
        ...    /tmp/all_lua_event.log
        ...    "_type":393241, "category":6, "element":25, "ba_id":1, "ba_name":"test", "ba_description":"fdpgvo75", "sla_month_percent_crit":4.56, "sla_month_percent_warn":1.23, "sla_duration_crit":789, "sla_duration_warn":852
        Sleep    1s
        IF    len("""${grep_res}""") > 0    BREAK
    END

    Should Not Be Empty    ${grep_res}    event not found

    [Teardown]    Ctn Stop Engine Broker And Save Logs    ${True}

BECPB_DIMENSION_BA_BV_RELATION_EVENT
    [Documentation]    bbdo_version 3 use pb_dimension_ba_bv_relation_event message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Ctn BAM Init

    Remove File    /tmp/all_lua_event.log

    Ctn Clear Db    mod_bam_reporting_relations_ba_bv
    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}

    ${id_ba__sid}    Ctn Create Ba With Services    test    worst    ${svc}

    Ctn Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-log-all-event.lua

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Delete All Rows From Table    mod_bam_bagroup_ba_relation
    Execute SQL String    INSERT INTO mod_bam_bagroup_ba_relation (id_ba, id_ba_group) VALUES (1, 456)

    Disconnect From Database
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Wait Until Created    /tmp/all_lua_event.log    30s
    FOR    ${index}    IN RANGE    10
        ${grep_res}    Grep File
        ...    /tmp/all_lua_event.log
        ...    "_type":393239, "category":6, "element":23, "ba_id":1, "bv_id":456
        Sleep    1s
        IF    len("""${grep_res}""") > 0    BREAK
    END

    Should Not Be Empty    ${grep_res}    event not found

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    @{query_results}    Query    SELECT bv_id FROM mod_bam_reporting_relations_ba_bv WHERE bv_id=456 and ba_id=1

    Should Be True    len(@{query_results}) >= 1    We should have one line in mod_bam_reporting_relations_ba_bv table
    Disconnect From Database

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker    ${True}

BECPB_DIMENSION_TIMEPERIOD
    [Documentation]    use of pb_dimension_timeperiod message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Ctn BAM Init

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    ${id_ba__sid}    Ctn Create Ba With Services    test    worst    ${svc}

    Remove File    /tmp/all_lua_event.log

    Ctn Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-log-all-event.lua

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String
    ...    INSERT INTO timeperiod (tp_id, tp_name, tp_sunday, tp_monday, tp_tuesday, tp_wednesday, tp_thursday, tp_friday, tp_saturday) VALUES (732, "ezizae", "sunday_value", "monday_value", "tuesday_value", "wednesday_value", "thursday_value", "friday_value", "saturday_value")

    Disconnect From Database
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Wait Until Created    /tmp/all_lua_event.log    30s
    FOR    ${index}    IN RANGE    10
        ${grep_res}    Grep File
        ...    /tmp/all_lua_event.log
        ...    "_type":393240, "category":6, "element":24, "id":732, "name":"ezizae", "monday":"monday_value", "tuesday":"tuesday_value", "wednesday":"wednesday_value", "thursday":"thursday_value", "friday":"friday_value", "saturday":"saturday_value", "sunday":"sunday_value"
        Sleep    1s
        IF    len("""${grep_res}""") > 0    BREAK
    END

    Should Not Be Empty    ${grep_res}    event not found

    [Teardown]    Ctn Stop Engine Broker And Save Logs    ${True}

BECPB_DIMENSION_KPI_EVENT
    [Documentation]    bbdo_version 3 use pb_dimension_kpi_event message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Ctn BAM Init

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    ${baid_svcid}    Ctn Create Ba With Services    test    worst    ${svc}

    Ctn Add Boolean Kpi    ${baid_svcid[0]}    {host_16 service_302} {IS} {OK}    False    100

    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    ${expected}    Catenate    (('bool test',    ${baid_svcid[0]}
    ${expected}    Catenate
    ...    SEPARATOR=
    ...    ${expected}
    ...    , 'test', 0, '', 0, '', 1, 'bool test'), ('host_16 service_314',
    ${expected}    Catenate    ${expected}    ${baid_svcid[0]}
    ${expected}    Catenate    SEPARATOR=    ${expected}    , 'test', 16, 'host_16', 314, 'service_314', 0, ''))
    FOR    ${index}    IN RANGE    10
        ${output}    Query
        ...    SELECT kpi_name, ba_id, ba_name, host_id, host_name, service_id, service_description, boolean_id, boolean_name FROM mod_bam_reporting_kpi order by kpi_name
        Sleep    1s
        IF    ${output} == ${expected}    BREAK
    END

    Should Be Equal As Strings    ${output}    ${expected}    mod_bam_reporting_kpi not filled
    Disconnect From Database

    [Teardown]    Ctn Stop Engine Broker And Save Logs    ${True}

BECPB_KPI_STATUS
    [Documentation]    bbdo_version 3 use kpi_status message.
    [Tags]    broker    engine    protobuf    bam    bbdo

    Ctn BAM Init

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    Ctn Create Ba With Services    test    worst    ${svc}

    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}

    # KPI set to critical
    Ctn Process Service Result Hard    host_16    service_314    2    output critical for 314
    ${result}    Ctn Check Service Status With Timeout    host_16    service_314    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${index}    IN RANGE    10
        ${output}    Query    SELECT current_status, state_type FROM mod_bam_kpi WHERE host_id=16 and service_id= 314
        Sleep    1s
        IF    ${output} == ((2, '1'),)    BREAK
    END

    Should Be Equal As Strings    ${output}    ((2, '1'),)    mod_bam_kpi not filled

    ${output}    Query    SELECT last_state_change FROM mod_bam_kpi WHERE host_id=16 and service_id= 314
    ${output}    Fetch From Right    "${output}"    (
    ${output}    Fetch From Left    ${output}    ,

    Should Be True    (${output} + 0.999) >= ${start}
    Disconnect From Database

    [Teardown]    Ctn Stop Engine Broker And Save Logs    ${True}

BECPB_BA_DURATION_EVENT
    [Documentation]    use of pb_ba_duration_event message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Ctn BAM Init

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    Ctn Create Ba With Services    test    worst    ${svc}

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM mod_bam_relations_ba_timeperiods
    Disconnect From Database

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM mod_bam_reporting_ba_events_durations

    Ctn Start Broker    newGeneration=True
    ${start_event}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start_event}

    # KPI set to critical
    Ctn Process Service Result Hard    host_16    service_314    2    output critical for 314
    ${result}    Ctn Check Service Resource Status With Timeout    host_16    service_314    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not CRITICAL as expected
    Sleep    2s
    Ctn Process Service Check Result    host_16    service_314    0    output ok for 314
    ${result}    Ctn Check Service Resource Status With Timeout    host_16    service_314    0    60    HARD
    Should Be True    ${result}    The service (host_16,service_314) is not OK as expected
    ${end_event}    Get Current Date    result_format=epoch

    FOR    ${index}    IN RANGE    10
        ${output}    Query
        ...    SELECT start_time, end_time, duration, sla_duration, timeperiod_is_default FROM mod_bam_reporting_ba_events_durations WHERE ba_event_id = 1
        Sleep    1s
        Log To Console    ${output}
        IF    "${output}" != "()"    BREAK
    END

    IF    "${output}" == "()"
        Log To Console    "Bad return for this test, the content of the table is"
        ${output}    Query
        ...    SELECT start_time, end_time, duration, sla_duration, timeperiod_is_default FROM mod_bam_reporting_ba_events_durations
        Log To Console    ${output}
    END
    Should Be True
    ...    "${output}" != "()"
    ...    No row recorded in mod_bam_reporting_ba_events_durations with ba_event_id=1
    Should Be True    ${output[0][2]} == ${output[0][1]} - ${output[0][0]}
    Should Be True    ${output[0][3]} == ${output[0][1]} - ${output[0][0]}
    Should Be True    ${output[0][4]} == 1
    Should Be True    ${output[0][1]} > ${output[0][0]}
    Should Be True    ${output[0][0]} >= ${start_event}
    Should Be True    ${output[0][1]} <= ${end_event}
    Disconnect From Database

    [Teardown]    Ctn Stop Engine Broker And Save Logs    ${True}

BECPB_DIMENSION_BA_TIMEPERIOD_RELATION
    [Documentation]    use of pb_dimension_ba_timeperiod_relation message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Ctn BAM Init

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    Ctn Create Ba With Services    test    worst    ${svc}

    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String
    ...    INSERT INTO timeperiod (tp_id, tp_name, tp_sunday, tp_monday, tp_tuesday, tp_wednesday, tp_thursday, tp_friday, tp_saturday) VALUES (732, "ezizae", "00:00-23:59", "00:00-23:59", "00:00-23:59", "00:00-23:59", "00:00-23:59", "00:00-23:59", "00:00-23:59")
    Execute SQL String    DELETE FROM mod_bam_relations_ba_timeperiods
    Execute SQL String    INSERT INTO mod_bam_relations_ba_timeperiods (ba_id, tp_id) VALUES (1,732)
    Disconnect From Database

    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${index}    IN RANGE    10
        ${output}    Query
        ...    SELECT ba_id FROM mod_bam_reporting_relations_ba_timeperiods WHERE ba_id=1 and timeperiod_id=732 and is_default=0
        Sleep    1s
        IF    "${output}" != "()"    BREAK
    END

    Should Be True
    ...    len("""${output}""") > 5
    ...    "centreon_storage.mod_bam_reporting_relations_ba_timeperiods not updated"
    Disconnect From Database

    [Teardown]    Ctn Stop Engine Broker And Save Logs    ${True}

BECPB_DIMENSION_TRUNCATE_TABLE
    [Documentation]    use of pb_dimension_timeperiod message.
    [Tags]    broker    engine    protobuf    bam    bbdo
    Ctn BAM Init

    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    Ctn Create Ba With Services    test    worst    ${svc}

    Remove File    /tmp/all_lua_event.log
    Ctn Broker Config Log    central    lua    trace

    Ctn Broker Config Add Lua Output    central    test-protobuf    ${SCRIPTS}test-log-all-event.lua

    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Wait Until Created    /tmp/all_lua_event.log    30s
    FOR    ${index}    IN RANGE    10
        ${grep_res}    Grep File
        ...    /tmp/all_lua_event.log
        ...    "_type":393246, "category":6, "element":30, "update_started":true
        Sleep    1s
        IF    len("""${grep_res}""") > 0    BREAK
    END

    Should Not Be Empty    ${grep_res}    event not found
    ${grep_res}    Grep File
    ...    /tmp/all_lua_event.log
    ...    "_type":393246, "category":6, "element":30, "update_started":false
    Should Not Be Empty    ${grep_res}    event not found

    [Teardown]    Ctn Stop Engine Broker And Save Logs    ${True}

CBA_RATIO_NUMBER_BA_4_SERVICE
    [Documentation]    With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
    [Tags]    broker    engine    bam
    Ctn BAM Init

    ${id_ba__sid}    Ctn Create Ba    test    ratio_number    2    1
    Ctn Add Service Kpi    host_16    service_302    ${id_ba__sid[0]}    40    30    20
    Ctn Add Service Kpi    host_16    service_303    ${id_ba__sid[0]}    40    30    20
    Ctn Add Service Kpi    host_16    service_304    ${id_ba__sid[0]}    40    30    20
    Ctn Add Service Kpi    host_16    service_304    ${id_ba__sid[0]}    40    30    20

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}

    # all serv ok => ba ok
    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA test is not OK as expected

    # one serv critical => ba warning
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test    1    30
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA test is not WARNING as expected

    # two services critical => ba ok
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test    2    30
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA test is not CRITICAL as expected

    # all serv ok => ba ok
    Ctn Process Service Check Result    host_16    service_302    0    output ok for service_302
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    0    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not OK as expected
    Ctn Process Service Check Result    host_16    service_303    0    output ok for service_303
    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    0    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not OK as expected
    ${result}    Ctn Check Ba Status With Timeout    test    0    30
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA test is not OK as expected

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker

CBA_RATIO_PERCENT_BA_4_SERVICE
    [Documentation]    With bbdo version 3.0.1, a BA of type 'ratio number' with 4 serv
    [Tags]    broker    engine    bam
    Ctn BAM Init

    ${id_ba__sid}    Ctn Create Ba    test    ratio_percent    50    25
    Ctn Add Service Kpi    host_16    service_302    ${id_ba__sid[0]}    40    30    20
    Ctn Add Service Kpi    host_16    service_303    ${id_ba__sid[0]}    40    30    20
    Ctn Add Service Kpi    host_16    service_304    ${id_ba__sid[0]}    40    30    20
    Ctn Add Service Kpi    host_16    service_305    ${id_ba__sid[0]}    40    30    20

    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}

    # all serv ok => ba ok
    ${result}    Ctn Check Ba Status With Timeout    test    0    60
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA test is not OK as expected

    # one serv critical => ba warning
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_302
    ...    2
    ...    output critical for service_302
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test    1    30
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA test is not WARNING as expected

    # two services critical => ba ok
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_303
    ...    2
    ...    output critical for service_303
    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    test    2    30
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA test is not CRITICAL as expected

    # all serv ok => ba ok
    Ctn Process Service Check Result    host_16    service_302    0    output ok for service_302
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    0    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not OK as expected
    Ctn Process Service Check Result    host_16    service_303    0    output ok for service_303
    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    0    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not OK as expected
    ${result}    Ctn Check Ba Status With Timeout    test    0    30
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The BA test is not OK as expected

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker

CBA_CHANGED
    [Documentation]    Scenario: Replace Service KPI with Boolean Rule KPI in Worst-type BA
    ...    Given a BA of type "worst" is configured with one service KPI
    ...    When the service KPI is replaced by a boolean rule KPI
    ...    And Broker is reloaded
    ...    Then the BA is correctly updated with the new KPI configuration
    [Tags]    MON-34895
    Ctn Bam Init

    @{svc}    Set Variable    ${{ [("host_16", "service_302")] }}
    ${ba}    Ctn Create Ba With Services    test    worst    ${svc}

    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}

    # Both services ${state} => The BA parent is ${state}
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_302
    ...    0
    ...    output OK for service 302

    ${result}    Ctn Check Ba Status With Timeout    test    0    30
    Ctn Dump Ba On Error    ${result}    ${ba[0]}
    Should Be True    ${result}    The BA test is not OK as expected

    Ctn Remove Service Kpi    ${ba[0]}    host_16    service_302
    Ctn Add Boolean Kpi
    ...    ${ba[0]}
    ...    {host_16 service_302} {IS} {OK}
    ...    False
    ...    100

    Ctn Reload Broker
    Remove File    /tmp/ba.dot
    Ctn Broker Get Ba    51001    ${ba[0]}    /tmp/ba.dot
    Wait Until Created    /tmp/ba.dot
    ${result}    Grep File    /tmp/ba.dot    Boolean exp
    Should Not Be Empty    ${result}

    Ctn Add Boolean Kpi
    ...    ${ba[0]}
    ...    {host_16 service_303} {IS} {WARNING}
    ...    False
    ...    100

    Ctn Reload Broker
    Remove File    /tmp/ba.dot
    Ctn Broker Get Ba    51001    ${ba[0]}    /tmp/ba.dot
    Wait Until Created    /tmp/ba.dot
    ${result}    Grep File    /tmp/ba.dot    BOOL Service (16, 303)
    Should Not Be Empty    ${result}
    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker

CBA_IMPACT_IMPACT
    [Documentation]    Given a Business Activity (BA) of type "impact"
    ...    And it has two child BAs of type "impact"
    ...    And the first child has an impact of 90
    ...    And the second child has an impact of 10
    ...    When both child BAs are impacting
    ...    Then the parent BA should be "critical"
    ...    When both child BAs are not impacting
    ...    Then the parent BA should be "ok"
    [Tags]    MON-34895
    Ctn Bam Init

    ${parent_ba}    Ctn Create Ba    parent    impact    20    99
    @{svc1}    Set Variable    ${{ [("host_16", "service_302")] }}
    ${child1_ba}    Ctn Create Ba    child1    impact    20    99
    Ctn Add Service Kpi    host_16    service_302    ${child1_ba[0]}    100    2    3
    ${child2_ba}    Ctn Create Ba    child2    impact    20    99
    Ctn Add Service Kpi    host_16    service_303    ${child2_ba[0]}    100    2    3

    Ctn Add Ba Kpi    ${child1_ba[0]}    ${parent_ba[0]}    90    2    3
    Ctn Add Ba Kpi    ${child2_ba[0]}    ${parent_ba[0]}    10    2    3

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}

    FOR    ${state}    ${value}    IN
    ...    OK    0
    ...    CRITICAL    2
    ...    OK    0
    ...    CRITICAL    2
        # Both services ${state} => The BA parent is ${state}
        Ctn Process Service Result Hard
        ...    host_16
        ...    service_302
        ...    ${value}
        ...    output ${state} for service 302

        # Sometimes the parent BA emits two status with less than one second between them
        # So we wait for 1s here to avoid the duplicate status in RRD.
        Sleep    1s
        Ctn Process Service Result Hard
        ...    host_16
        ...    service_303
        ...    ${value}
        ...    output ${state} for service 303

        ${result}    Ctn Check Service Status With Timeout    host_16    service_302    ${value}    60    HARD
        Should Be True    ${result}    The service (host_16,service_302) is not ${state} as expected
        ${result}    Ctn Check Service Status With Timeout    host_16    service_303    ${value}    60    HARD
        Should Be True    ${result}    The service (host_16,service_303) is not ${state} as expected

        ${result}    Ctn Check Ba Status With Timeout    child1    ${value}    30
        Ctn Dump Ba On Error    ${result}    ${child1_ba[0]}
        Should Be True    ${result}    The BA child1 is not ${state} as expected

        ${result}    Ctn Check Ba Status With Timeout    child2    ${value}    30
        Ctn Dump Ba On Error    ${result}    ${child2_ba[0]}
        Should Be True    ${result}    The BA child2 is not ${state} as expected

        ${result}    Ctn Check Ba Status With Timeout    parent    ${value}    30
        Ctn Dump Ba On Error    ${result}    ${parent_ba[0]}
        Should Be True    ${result}    The BA parent is not ${state} as expected

        Remove Files    /tmp/parent1.dot    /tmp/parent2.dot
        Ctn Broker Get Ba    51001    ${parent_ba[0]}    /tmp/parent1.dot
        Wait Until Created    /tmp/parent1.dot

        ${start}    Ctn Get Round Current Date
        Ctn Reload Broker
        # The BAM endpoint is updated in place on reload (no destroy/recreate), so
        # the BA state persists in memory and is not restored from cache. Wait for
        # BAM to reprocess the reload before querying the BA again.
        VAR    @{content}    BAM: loading cache
        ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
        Should Be True    ${result}    Broker did not reprocess BAM after the reload.

        Ctn Broker Get Ba    51001    ${parent_ba[0]}    /tmp/parent2.dot
        Wait Until Created    /tmp/parent2.dot

        ${result}    Ctn Compare Dot Files    /tmp/parent1.dot    /tmp/parent2.dot
        Should Be True    ${result}    The BA changed during Broker reload.
    END

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker

CBA_DISABLED
    [Documentation]    create a disabled BA with timeperiods and reporting filter don't create error message
    [Tags]    broker    engine    bam    MON-33778
    Ctn Bam Init
    Ctn Create Ba    test    worst    100    100    ignore    0
    Ctn Add Relations Ba Timeperiods    1    1

    ${start}    Get Current Date
    Ctn Start Broker    newGeneration=True

    VAR    @{content}    bam configuration loaded
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    A message telling 'bam configuration loaded' should be available.

    ${res}    Grep File
    ...    ${centralLog}
    ...    could not insert relation of BA to timeperiod    regexp=True
    Should Be Empty    ${res}    A mod_bam_reporting_relations_ba_timeperiods error had been found in log

    ${res}    Grep File
    ...    ${centralLog}
    ...    The configured write filters for the endpoint 'centreon-bam-reporting' are too restrictive and will be ignored
    ...    regexp=True
    Should Be Empty    ${res}    A filter error of centreon-bam-reporting had been found in log

    [Teardown]    Ctn Stop Engine Broker And Save Logs    ${True}

CBA_SERVICE_PNAME_AFTER_RELOAD
    [Documentation]    Scenario: Verify that the parent_name of a BA service is not erased after a broker reload
    ...    Given a BA "test" of type "worst" with its service "host_16:service_302"
    ...    When I start broker and engine
    ...    Then the BA service "test" should have a status of 0 within 30 seconds
    ...    When I reload the broker
    ...    Then the database should still contain a BA service with name "test" and parent_name "_Module_BAM_1"
    [Tags]    broker    engine    bam    MON-153476

    Ctn Bam Init

    @{svc}    Set Variable    ${{ [("host_16", "service_302")] }}
    ${ba}    Ctn Create Ba With Services    test    worst    ${svc}

    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}

    # Both services ${state} => The BA parent is ${state}
    Ctn Process Service Result Hard
    ...    host_16
    ...    service_302
    ...    0
    ...    output OK for service 302

    ${result}    Ctn Check Ba Status With Timeout    test    0    30
    Ctn Dump Ba On Error    ${result}    ${ba[0]}
    Should Be True    ${result}    The BA test is not OK as expected

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Check Query Result
    ...    SELECT CONCAT(name, '|', parent_name) FROM resources WHERE id=${ba[1]}
    ...    ==
    ...    test|_Module_BAM_1
    ...    retry_timeout=50s
    ...    retry_pause=1s

    Ctn Reload Broker

    Sleep    10s

    ${output}    Query
    ...    SELECT name, parent_name FROM resources WHERE id=${ba[1]}
    Should Be Equal As Strings
    ...    ${output}
    ...    (('test', '_Module_BAM_1'),)
    ...    name or parent name of ba ${ba[1]} is not as expected
    Disconnect From Database

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker

CBA_CONF_PUSHED_NO_RELOAD
    [Documentation]    Scenario: a BA created after Broker started is taken into account when its configuration is pushed, without any Broker reload
    ...    Given Broker and Engine are started in centralized mode with no BA
    ...    When a BA of type "worst" on two services is created and the Engine configuration is pushed through the .lck file
    ...    Then Broker asks the BAM endpoint to reload once the poller configuration is applied to the global cache
    ...    And the BA becomes OK without any reload of Broker
    ...    When one of its services becomes CRITICAL
    ...    Then the BA becomes CRITICAL
    [Tags]    broker    engine    bam    centralized    MON-187019
    Ctn BAM Init
    # No BA yet, so nothing wrote the BAM services file centengine.cfg refers to
    # (in centralized mode the poller configuration lives under VarRoot).
    Create File    ${VarRoot}/lib/centreon/config/1/centreon-bam-services.cfg    ${EMPTY}

    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}
    Wait Until Created
    ...    ${VarRoot}/lib/centreon-broker/central-broker-master/pollers-configuration/1.prot
    ...    timeout=60s

    # The BA is created once everything runs: PHP would push it the same way.
    ${push}    Ctn Get Round Current Date
    @{svc}    Set Variable    ${{ [("host_16", "service_314"), ("host_16", "service_303")] }}
    ${ba__svc}    Ctn Create Ba With Services    pushed    worst    ${svc}
    Ctn Notify Broker Of Engine Config Change    ${0}

    ${content}    Create List    endpoint applier: update requested for endpoint centreon-bam-monitoring
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${push}    ${content}    60
    Should Be True    ${result}    Broker should ask the BAM endpoint to reload after the configuration push

    ${result}    Ctn Check Ba Status With Timeout    pushed    0    60
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA pushed should be OK without any Broker reload

    Ctn Process Service Result Hard    host_16    service_303    2    output critical for 303
    ${result}    Ctn Check Ba Status With Timeout    pushed    2    60
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA pushed should be CRITICAL

    [Teardown]    Ctn Stop Engine Broker And Save Logs

CBA_PROT_LOST_RESTART
    [Documentation]    Scenario: a BA survives the loss of the stored poller configuration across a Broker restart
    ...    Given a BA of type "worst" on two services is OK
    ...    When Broker is stopped, its stored poller configuration (1.prot) is deleted and Broker is started again
    ...    Then BAM cannot resolve the KPI services when it opens, since the global cache is empty
    ...    And Engine sends its configuration back, Broker stores it, feeds the cache and asks the BAM endpoint to reload
    ...    And the BA reacts to a CRITICAL service without any reload of Broker
    [Tags]    broker    engine    bam    centralized    MON-187019
    Ctn BAM Init

    @{svc}    Set Variable    ${{ [("host_16", "service_314"), ("host_16", "service_303")] }}
    ${ba__svc}    Ctn Create Ba With Services    lost    worst    ${svc}
    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}

    ${result}    Ctn Check Ba Status With Timeout    lost    0    60
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA lost should be OK

    # A user deletes the stored configuration by mistake, then restarts Broker.
    Ctn Kindly Stop Broker
    Ctn Clear Prot Files    broker_only=${True}
    ${restart}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True

    ${content}    Create List    endpoint applier: update requested for endpoint centreon-bam-monitoring
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${restart}    ${content}    60
    Should Be True    ${result}    Broker should ask the BAM endpoint to reload once the poller configuration is back
    Wait Until Created
    ...    ${VarRoot}/lib/centreon-broker/central-broker-master/pollers-configuration/1.prot
    ...    timeout=60s

    Ctn Process Service Result Hard    host_16    service_303    2    output critical for 303
    ${result}    Ctn Check Ba Status With Timeout    lost    2    60
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA lost should be CRITICAL after the restart without its stored configuration

    [Teardown]    Ctn Stop Engine Broker And Save Logs

CBA_KPI_SERVICE_ADDED_LATER
    [Documentation]    Scenario: a KPI whose service does not exist yet becomes active when the service is added to the configuration
    ...    Given a BA of type "worst" with two KPIs, one on service_314 and one on service_303
    ...    And service_303 is not in the Engine configuration when Broker starts, so BAM drops that KPI
    ...    When service_303 is added to the Engine configuration and pushed
    ...    Then Broker asks the BAM endpoint to reload once the diff is applied to the global cache
    ...    And a CRITICAL result on service_303 makes the BA CRITICAL, without any reload of Broker
    [Tags]    broker    engine    bam    centralized    MON-187019
    Ctn BAM Init

    @{svc}    Set Variable    ${{ [("host_16", "service_314"), ("host_16", "service_303")] }}
    ${ba__svc}    Ctn Create Ba With Services    later    worst    ${svc}
    ${cmd_303}    Ctn Get Service Command Id    ${303}
    Ctn Engine Config Remove Service    ${0}    host_16    service_303
    Ctn Notify Broker Of Engine Config Change    ${0}
    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}

    # Only the KPI on service_314 exists: the BA is OK on it alone.
    ${result}    Ctn Check Ba Status With Timeout    later    0    60
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA later should be OK
    ${content}    Create List    endpoint applier: update requested for endpoint centreon-bam-monitoring
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    The first configuration round should ask the BAM endpoint to reload

    # service_303 is added and pushed (the keyword announces the change).
    ${push}    Ctn Get Round Current Date
    Ctn Set Command Status    ${cmd_303}    ${2}
    Ctn Engine Config Add Service    ${0}    ${16}    ${303}    service_303    command_${cmd_303}
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${push}    ${content}    60
    Should Be True    ${result}    Broker should ask the BAM endpoint to reload after the service was added

    Ctn Process Service Result Hard    host_16    service_303    2    output critical for 303
    ${result}    Ctn Check Ba Status With Timeout    later    2    90
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA later should be CRITICAL once its KPI on service_303 is active

    [Teardown]    Ctn Stop Engine Broker And Save Logs


CBA_KPI_RUNTIME_RESTORED_FROM_RESOURCES
    [Documentation]    Scenario: after a Broker restart in centralized configuration, the reference output restores the resources runtime from the resources table into the global cache, and BAM seeds its KPIs from it
    ...    Given a BA of type "worst" on two services, in centralized configuration
    ...    And one of its services is CRITICAL, so the BA is CRITICAL
    ...    When Engine is stopped, then Broker is stopped and BAM's own cache file is deleted
    ...    And Broker is started again, Engine still stopped so that no status is replayed
    ...    Then unified_sql, the reference of the global cache, restores the hosts and services runtime from the resources table into the cache
    ...    And once the cache is ready, BAM seeds its KPI services from it
    ...    And the BA is still CRITICAL
    [Tags]    broker    engine    bam    centralized    cache    MON-187019
    Ctn BAM Init
    Ctn Broker Config Log    central    sql    info

    @{svc}    Set Variable    ${{ [("host_16", "service_314"), ("host_16", "service_303")] }}
    ${ba__svc}    Ctn Create Ba With Services    overlaid    worst    ${svc}
    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}

    ${result}    Ctn Check Ba Status With Timeout    overlaid    0    60
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA overlaid should be OK

    Ctn Process Service Result Hard    host_16    service_303    2    output critical for 303
    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) should be CRITICAL
    ${result}    Ctn Check Ba Status With Timeout    overlaid    2    60
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA overlaid should be CRITICAL

    # Engine first, so that no status reaches BAM after the restart; then
    # Broker. In centralized configuration the global cache file carries no
    # resource state: the database, through the reference output, is the only
    # source left once BAM's own cache file is removed.
    Ctn Stop Engine
    Ctn Kindly Stop Broker
    Remove File    ${VarRoot}/lib/centreon-broker/central-broker-master.cache.centreon-bam-monitoring
    ${restart}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True

    ${content}    Create List    restored into the global cache from the resources table
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${restart}    ${content}    60
    Should Be True    ${result}    unified_sql should restore the resources runtime into the global cache from the resources table
    ${content}    Create List    runtime of 0 hosts and 0 services restored
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${restart}    ${content}    5
    Should Not Be True    ${result}    At least one resource should have been restored
    ${content}    Create List    service state(s) seeded from the global cache
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${restart}    ${content}    60
    Should Be True    ${result}    BAM should seed its KPI services from the global cache once it is ready
    ${content}    Create List    BAM: 0 service state(s) seeded
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${restart}    ${content}    5
    Should Not Be True    ${result}    At least one service should have been seeded

    ${result}    Ctn Check Ba Status With Timeout    overlaid    2    60
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA overlaid should still be CRITICAL after the restart

    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}
    Ctn Process Service Result Hard    host_16    service_303    0    output ok for 303
    ${result}    Ctn Check Ba Status With Timeout    overlaid    0    60
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA overlaid should be OK again

    [Teardown]    Ctn Stop Engine Broker And Save Logs


CBA_KPI_RUNTIME_RESTORED_FROM_HOSTS_SERVICES
    [Documentation]    Scenario: after a Broker restart in centralized configuration, the reference output restores the resources runtime from the hosts and services tables into the global cache (store_in_resources is false), and BAM seeds its KPIs from it
    ...    Given a BA of type "worst" on two services, in centralized configuration
    ...    And one of its services is CRITICAL, so the BA is CRITICAL
    ...    When Engine is stopped, then Broker is stopped and BAM's own cache file is deleted
    ...    And Broker is started again, Engine still stopped so that no status is replayed
    ...    Then unified_sql, the reference of the global cache, restores the hosts and services runtime from the hosts and services tables into the cache
    ...    And once the cache is ready, BAM seeds its KPI services from it
    ...    And the BA is still CRITICAL
    [Tags]    broker    engine    bam    centralized    cache    MON-187019
    Ctn BAM Init
    Ctn Broker Config Log    central    sql    info
    # What Ctn Start Broker newGeneration=True does, done here: Ctn Config BBDO3
    # rewrites the whole unified_sql output, so the key has to come after it.
    Ctn Config BBDO3    ${1}    3.1.0
    Ctn Broker Config Add Item    central    cache_config_directory    ${VarRoot}/lib/centreon/config
    Ctn Broker Config Output Set    central    central-broker-unified-sql    store_in_resources    false

    @{svc}    Set Variable    ${{ [("host_16", "service_314"), ("host_16", "service_303")] }}
    ${ba__svc}    Ctn Create Ba With Services    restored_hs    worst    ${svc}
    Ctn Start Broker
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}

    ${result}    Ctn Check Ba Status With Timeout    restored_hs    0    60
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA restored_hs should be OK

    Ctn Process Service Result Hard    host_16    service_303    2    output critical for 303
    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    2    60    HARD
    Should Be True    ${result}    The service (host_16,service_303) should be CRITICAL
    ${result}    Ctn Check Ba Status With Timeout    restored_hs    2    60
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA restored_hs should be CRITICAL

    # Engine first, so that no status reaches BAM after the restart; then
    # Broker. In centralized configuration the global cache file carries no
    # resource state: the database, through the reference output, is the only
    # source left once BAM's own cache file is removed.
    Ctn Stop Engine
    Ctn Kindly Stop Broker
    Remove File    ${VarRoot}/lib/centreon-broker/central-broker-master.cache.centreon-bam-monitoring
    # Ctn Start Engine newGeneration=True went through Ctn Config BBDO3 too, which
    # rewrote the unified_sql output: the key has to be set again before the
    # configuration is flushed by this start.
    Ctn Broker Config Output Set    central    central-broker-unified-sql    store_in_resources    false
    ${restart}    Ctn Get Round Current Date
    Ctn Start Broker

    ${content}    Create List    restored into the global cache from the hosts/services table
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${restart}    ${content}    60
    Should Be True    ${result}    unified_sql should restore the resources runtime into the global cache from the hosts/services tables
    ${content}    Create List    runtime of 0 hosts and 0 services restored
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${restart}    ${content}    5
    Should Not Be True    ${result}    At least one resource should have been restored
    ${content}    Create List    service state(s) seeded from the global cache
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${restart}    ${content}    60
    Should Be True    ${result}    BAM should seed its KPI services from the global cache once it is ready
    ${content}    Create List    BAM: 0 service state(s) seeded
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${restart}    ${content}    5
    Should Not Be True    ${result}    At least one service should have been seeded

    ${result}    Ctn Check Ba Status With Timeout    restored_hs    2    60
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA restored_hs should still be CRITICAL after the restart

    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}
    Ctn Process Service Result Hard    host_16    service_303    0    output ok for 303
    ${result}    Ctn Check Ba Status With Timeout    restored_hs    0    60
    Ctn Dump Ba On Error    ${result}    ${ba__svc[0]}
    Should Be True    ${result}    The BA restored_hs should be OK again

    [Teardown]    Ctn Stop Engine Broker And Save Logs


*** Keywords ***
Ctn BAM Setup
    [Documentation]    Test setup of the suite. It stops any Broker and Engine left
    ...    by a previous test, then empties the BAM reporting tables (kpi,
    ...    timeperiods, BA/timeperiod relations and BA events) so that every test
    ...    starts from a clean reporting history. The BA events auto-increment is
    ...    reset to 1 so that the event identifiers expected by the tests are
    ...    stable. Foreign key checks are disabled during the deletions and
    ...    restored afterwards.
    Ctn Stop Processes
    Connect To Database    pymysql    ${DBName}    ${DBUserRoot}    ${DBPassRoot}    ${DBHost}    ${DBPort}
    Execute SQL String    SET GLOBAL FOREIGN_KEY_CHECKS=0
    Execute SQL String    DELETE FROM mod_bam_reporting_kpi
    Execute SQL String    DELETE FROM mod_bam_reporting_timeperiods
    Execute SQL String    DELETE FROM mod_bam_reporting_relations_ba_timeperiods
    Execute SQL String    DELETE FROM mod_bam_reporting_ba_events
    Execute SQL String    ALTER TABLE mod_bam_reporting_ba_events AUTO_INCREMENT = 1
    Execute SQL String    SET GLOBAL FOREIGN_KEY_CHECKS=1
    Disconnect From Database

Ctn BAM Init
    [Documentation]    Prepares a fresh BAM test environment before Broker and Engine
    ...    are started, in centralized configuration mode. It clears the forced check
    ...    states, the retention files and the BAM tables, then writes a complete
    ...    configuration: one centralized Engine poller, the Broker module, central
    ...    and RRD instances, and the BAM module on the central Broker. The central
    ...    Broker logs bam and config at trace level and sql at debug level, since
    ...    the sql trace level writes very long lines that slow down the log searches
    ...    of the teardown. The services from service_300 to service_309 are made
    ...    passive so that their active checks cannot alter the BA states during the
    ...    tests. The Engine configuration is finally cloned into the database, the
    ...    BAM configuration is added to Engine and Broker is notified of the new
    ...    Engine configuration.
    Ctn Clear Commands Status
    Ctn Clear Retention
    Ctn Clear Prot Files
    # The reference output overlays the resources state kept in the database
    # onto the global cache at startup: a state left by a previous test would
    # be believed. Start from a blank platform.
    Ctn Clear Db    hosts
    Ctn Clear Db    services
    Ctn Clear Db    resources
    Ctn Clear Db Conf    mod_bam
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    bam    trace
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    config    trace
    Ctn Broker Config Source Log    central    1
    Ctn Add Bam Config To Broker    central
    # This is to avoid parasite status.
    Ctn Set Services Passive    ${0}    service_30.

    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine
    Ctn Notify Broker Of Engine Config Change    ${0}
