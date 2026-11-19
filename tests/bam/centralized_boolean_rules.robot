*** Settings ***
Documentation       Centreon Broker and BAM with centralized configuration

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn BAM Setup
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
CBABOO
    [Documentation]    Scenario: A "worst" BA and an impact BA with an OR boolean rule built on the same 2 services behave identically when a service becomes CRITICAL
    ...    Given a BA of type "worst" with service_302 and service_303 as KPIs
    ...    And a BA of type "impact" with a boolean rule "{service_302} IS CRITICAL OR {service_303} IS CRITICAL"
    ...    When service_302 becomes CRITICAL
    ...    Then both BAs are CRITICAL
    ...    When service_302 recovers to OK
    ...    Then both BAs return to OK
    ...    And this cycle is repeated 10 times
    [Tags]    broker    engine    bam    boolean_expression
    Ctn BAM Init
    Ctn Set Services Passive    ${0}    service_302
    Ctn Set Services Passive    ${0}    service_303
    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine

    ${id_ba_worst__sid}    Ctn Create Ba    ba-worst    worst    70    80
    Ctn Add Service Kpi    host_16    service_302    ${id_ba_worst__sid[0]}    40    30    20
    Ctn Add Service Kpi    host_16    service_303    ${id_ba_worst__sid[0]}    40    30    20

    ${id_boolean_ba__sid}    Ctn Create Ba    boolean-ba    impact    70    80
    Ctn Add Boolean Kpi
    ...    ${id_boolean_ba__sid[0]}
    ...    {host_16 service_302} {IS} {CRITICAL} {OR} {host_16 service_303} {IS} {CRITICAL}
    ...    True
    ...    100

    Ctn Notify Broker Of Engine Config Change    ${0}

    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}

    # 303 is set to ok.
    Ctn Process Service Check Result    host_16    service_303    0    output ok for service_303

    FOR    ${i}    IN RANGE    10
        Log To Console    @@@@@@@@@@@@@@ Step ${i} @@@@@@@@@@@@@@
        # 302 is set to critical => the two ba become critical
        Ctn Process Service Result Hard    host_16    service_302    2    output critical for service_302

        ${result}    Ctn Check Service Resource Status With Timeout    host_16    service_302    2    30    HARD
        Should Be True    ${result}    The service (host_16:service_302) should be CRITICAL.
        ${result}    Ctn Check Ba Status With Timeout    ba-worst    2    30
        Ctn Dump Ba On Error    ${result}    ${id_ba_worst__sid[0]}
        Should Be True    ${result}    The 'ba-worst' BA is not CRITICAL as expected
        ${result}    Ctn Check Ba Status With Timeout    boolean-ba    2    30
        Ctn Dump Ba On Error    ${result}    ${id_boolean_ba__sid[0]}
        Should Be True    ${result}    The 'boolean-ba' BA is not CRITICAL as expected

        Ctn Process Service Check Result    host_16    service_302    0    output ok for service_302
        ${result}    Ctn Check Ba Status With Timeout    ba-worst    0    30
        Ctn Dump Ba On Error    ${result}    ${id_ba_worst__sid[0]}
        Should Be True    ${result}    The 'ba-worst' BA is not OK as expected
        ${result}    Ctn Check Service Resource Status With Timeout    host_16    service_302    0    30    HARD
        Should Be True    ${result}    The service (host_16:service_302) should be OK.

        ${result}    Ctn Check Ba Status With Timeout    boolean-ba    0    30
        Ctn Dump Ba On Error    ${result}    ${id_boolean_ba__sid[0]}
        Should Be True    ${result}    The 'boolean-ba' BA is not OK as expected
    END

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker

CBABOOOR
    [Documentation]    Scenario: An OR boolean rule evaluates to CRITICAL as soon as one operand is true, even when the other service is UNKNOWN
    ...    Given a BA of type "impact" with boolean rule "{service_302} IS CRITICAL OR {service_303} IS CRITICAL"
    ...    And service_303 is passive and starts UNKNOWN
    ...    When service_302 becomes CRITICAL
    ...    Then the BA is CRITICAL (OR short-circuits on the first true operand)
    [Tags]    broker    engine    bam    boolean_expression
    Ctn BAM Init
    Ctn Set Services Passive    ${0}    service_302
    Ctn Set Services Passive    ${0}    service_303
    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine

    ${id_ba__sid}    Ctn Create Ba    boolean-ba    impact    70    80
    Ctn Add Boolean Kpi
    ...    ${id_ba__sid[0]}
    ...    {host_16 service_302} {IS} {CRITICAL} {OR} {host_16 service_303} {IS} {CRITICAL}
    ...    True
    ...    100

    Ctn Notify Broker Of Engine Config Change    ${0}

    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}
    # 303 is unknown but since the boolean operator is OR, if 302 result is true, we should have already a result.

    # 302 is set to critical => the two ba become critical
    Ctn Process Service Result Hard    host_16    service_302    2    output critical for service_302

    ${result}    Ctn Check Ba Status With Timeout    boolean-ba    2    30
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The 'boolean-ba' BA is not CRITICAL as expected

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker

CBABOOAND
    [Documentation]    Scenario: An AND boolean rule evaluates to CRITICAL as soon as one operand is false, even when the other service is UNKNOWN
    ...    Given a BA of type "impact" with boolean rule "{service_302} IS OK AND {service_303} IS OK"
    ...    And service_303 is passive and starts UNKNOWN
    ...    When service_302 becomes CRITICAL
    ...    Then the BA is CRITICAL (AND short-circuits on the first false operand)
    [Tags]    broker    engine    bam    boolean_expression
    Ctn BAM Init
    Ctn Set Services Passive    ${0}    service_302
    Ctn Set Services Passive    ${0}    service_303
    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine

    ${id_ba__sid}    Ctn Create Ba    boolean-ba    impact    70    80
    Ctn Add Boolean Kpi
    ...    ${id_ba__sid[0]}
    ...    {host_16 service_302} {IS} {OK} {AND} {host_16 service_303} {IS} {OK}
    ...    False
    ...    100

    Ctn Notify Broker Of Engine Config Change    ${0}

    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}
    # 303 is unknown but since the boolean operator is AND, if 302 result is false, we should have already a result.

    # 302 is set to critical => the two ba become critical
    Ctn Process Service Result Hard    host_16    service_302    2    output critical for service_302

    ${result}    Ctn Check Ba Status With Timeout    boolean-ba    2    30
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The 'boolean-ba' BA is not CRITICAL as expected

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker

CBABOOORREL
    [Documentation]    Scenario: Updating a boolean rule and reloading broker and engine takes effect correctly
    ...    Given a BA of type "impact" with boolean rule "{service_302} IS OK OR {service_303} IS OK"
    ...    When service_302 and service_303 are CRITICAL
    ...    Then the BA is CRITICAL
    ...    When the boolean rule is updated to "{service_302} IS OK OR {service_304} IS OK" and broker and engine are reloaded
    ...    And service_304 is OK
    ...    Then the BA is OK
    ...    When the boolean rule is restored to "{service_302} IS OK OR {service_303} IS OK" and broker and engine are reloaded
    ...    And service_302 and service_303 are CRITICAL
    ...    Then the BA is CRITICAL again
    [Tags]    broker    engine    bam    boolean_expression
    Ctn BAM Init
    Ctn Set Services Passive    ${0}    service_302
    Ctn Set Services Passive    ${0}    service_303
    Ctn Set Services Passive    ${0}    service_304
    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine

    ${id_ba__sid}    Ctn Create Ba    boolean-ba    impact    70    80
    ${id_bool}    Ctn Add Boolean Kpi
    ...    ${id_ba__sid[0]}
    ...    {host_16 service_302} {IS} {OK} {OR} {host_16 service_303} {IS} {OK}
    ...    False
    ...    100

    Ctn Notify Broker Of Engine Config Change    ${0}

    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}

    # 302 is set to critical => {host_16 service_302} {IS} {OK} is then False
    Ctn Process Service Result Hard    host_16    service_302    2    output critical for service_302
    ${result}    Ctn Check Service Status With Timeout    host_16    service_302    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_302) is not CRITICAL/HARD as expected

    # 303 is set to critical => {host_16 service_303} {IS} {OK} is then False
    Ctn Process Service Result Hard    host_16    service_303    2    output critical for service_303
    ${result}    Ctn Check Service Status With Timeout    host_16    service_303    2    30    HARD
    Should Be True    ${result}    The service (host_16,service_303) is not CRITICAL/HARD as expected

    # 304 is set to ok => {host_16 service_304} {IS} {OK} is then True
    Ctn Process Service Result Hard    host_16    service_304    0    output ok for service_304
    ${result}    Ctn Check Service Status With Timeout    host_16    service_304    0    30    HARD
    Should Be True    ${result}    The service (host_16,service_304) is not OK/HARD as expected

    ${result}    Ctn Check Ba Status With Timeout    boolean-ba    2    30
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The 'boolean-ba' BA is not CRITICAL as expected

    Ctn Update Boolean Rule
    ...    ${id_bool}
    ...    {host_16 service_302} {IS} {OK} {OR} {host_16 service_304} {IS} {OK}

    ${start}    Get Current Date
    Ctn Reload Engine
    Ctn Reload Broker
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Ctn Process Service Result Hard    host_16    service_302    2    output ok for service_302
    Ctn Process Service Result Hard    host_16    service_304    0    output ok for service_304

    ${result}    Ctn Check Ba Status With Timeout    boolean-ba    0    30
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The 'boolean-ba' BA is not OK as expected

    Ctn Update Boolean Rule
    ...    ${id_bool}
    ...    {host_16 service_302} {IS} {OK} {OR} {host_16 service_303} {IS} {OK}

    ${start}    Get Current Date
    Ctn Reload Engine
    Ctn Reload Broker
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Ctn Process Service Result Hard    host_16    service_302    2    output critical for service_302
    Ctn Process Service Result Hard    host_16    service_303    2    output critical for service_303

    ${result}    Ctn Check Ba Status With Timeout    boolean-ba    2    30
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The 'boolean-ba' BA is not CRITICAL as expected

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker

CBABOOCOMPL
    [Documentation]    Scenario: A BA with a complex AND/OR boolean rule over 20 services becomes OK only when at least one service in each AND group is OK
    ...    Given a BA of type "impact" with a rule of 10 AND groups, each requiring at least one of 2 services to be OK
    ...    When all 20 services are CRITICAL
    ...    Then the BA is CRITICAL
    ...    When odd-indexed services are set to OK one by one
    ...    Then the BA remains CRITICAL until all AND groups have at least one OK service
    ...    And the BA becomes OK once all AND groups are satisfied
    [Tags]    broker    engine    bam    boolean_expression
    Ctn BAM Init
    # Services 1 to 20 are passive now.
    FOR    ${i}    IN RANGE    ${1}    ${21}
        Ctn Set Services Passive    ${0}    service_${i}
    END
    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine

    ${id_ba__sid}    Ctn Create Ba    boolean-ba    impact    70    80
    ${id_bool}    Ctn Add Boolean Kpi
    ...    ${id_ba__sid[0]}
    ...    ({host_1 service_1} {IS} {OK} {OR} {host_1 service_2} {IS} {OK}) {AND} ({host_1 service_3} {IS} {OK} {OR} {host_1 service_4} {IS} {OK}) {AND} ({host_1 service_5} {IS} {OK} {OR} {host_1 service_6} {IS} {OK}) {AND} ({host_1 service_7} {IS} {OK} {OR} {host_1 service_8} {IS} {OK}) {AND} ({host_1 service_9} {IS} {OK} {OR} {host_1 service_10} {IS} {OK}) {AND} ({host_1 service_11} {IS} {OK} {OR} {host_1 service_12} {IS} {OK}) {AND} ({host_1 service_13} {IS} {OK} {OR} {host_1 service_14} {IS} {OK}) {AND} ({host_1 service_15} {IS} {OK} {OR} {host_1 service_16} {IS} {OK}) {AND} ({host_1 service_17} {IS} {OK} {OR} {host_1 service_18} {IS} {OK}) {AND} ({host_1 service_19} {IS} {OK} {OR} {host_1 service_20} {IS} {OK})
    ...    False
    ...    100

    Ctn Notify Broker Of Engine Config Change    ${0}

    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}

    FOR    ${i}    IN RANGE    ${1}    ${21}
        Ctn Process Service Result Hard    host_1    service_${i}    2    output critical for service_${i}
    END

    FOR    ${i}    IN RANGE    ${1}    ${21}    ${2}
        ${result}    Ctn Check Ba Status With Timeout    boolean-ba    2    30
        Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
        Should Be True    ${result}    Step${i}: The 'boolean-ba' BA is not CRITICAL as expected
        Ctn Process Service Result Hard    host_1    service_${i}    0    output ok for service_${i}
    END

    ${result}    Ctn Check Ba Status With Timeout    boolean-ba    0    30
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The 'boolean-ba' BA is not OK as expected

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker


CBABOOCOMPL_RESTART
    [Documentation]    Scenario: A broker restart does not alter a complex boolean rule state
    ...    Given a BA of type "impact" with a complex AND/OR boolean rule over 20 services
    ...    And all 20 services are CRITICAL, then odd-indexed services 1-13 are set to OK
    ...    And the BA is still CRITICAL because even-indexed services remain CRITICAL
    ...    When broker is restarted at each remaining step (services 15, 17, 19 set to OK one by one)
    ...    Then the BA state is identical before and after each broker restart
    ...    And the BA becomes OK once all AND groups are satisfied
    [Tags]    broker    engine    bam    boolean_expression    MON-34246
    Ctn BAM Init
    # Services 1 to 20 are passive now.
    FOR    ${i}    IN RANGE    ${1}    ${21}
        Ctn Set Services Passive    ${0}    service_${i}
    END
    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine

    ${id_ba__sid}    Ctn Create Ba    boolean-ba    impact    70    80
    ${id_bool}    Ctn Add Boolean Kpi
    ...    ${id_ba__sid[0]}
    ...    ({host_1 service_1} {IS} {OK} {OR} {host_1 service_2} {IS} {OK}) {AND} ({host_1 service_3} {IS} {OK} {OR} {host_1 service_4} {IS} {OK}) {AND} ({host_1 service_5} {IS} {OK} {OR} {host_1 service_6} {IS} {OK}) {AND} ({host_1 service_7} {IS} {OK} {OR} {host_1 service_8} {IS} {OK}) {AND} ({host_1 service_9} {IS} {OK} {OR} {host_1 service_10} {IS} {OK}) {AND} ({host_1 service_11} {IS} {OK} {OR} {host_1 service_12} {IS} {OK}) {AND} ({host_1 service_13} {IS} {OK} {OR} {host_1 service_14} {IS} {OK}) {AND} ({host_1 service_15} {IS} {OK} {OR} {host_1 service_16} {IS} {OK}) {AND} ({host_1 service_17} {IS} {OK} {OR} {host_1 service_18} {IS} {OK}) {AND} ({host_1 service_19} {IS} {OK} {OR} {host_1 service_20} {IS} {OK})
    ...    False
    ...    100

    Ctn Notify Broker Of Engine Config Change    ${0}

    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}

    Log To Console    Services from 1 to 20 are set to CRITICAL.
    FOR    ${i}    IN RANGE    ${1}    ${21}
        Ctn Process Service Result Hard    host_1    service_${i}    2    output critical for service_${i}
    END
    Log To Console    Check services from 1 to 20 are CRITICAL.
    FOR    ${i}    IN RANGE    ${1}    ${21}
        ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_${i}    2    30    HARD
        Should Be True    ${result}    The service (host_1:service_${i}) should be CRITICAL.
    END

    Log To Console    Services from 1 to 14 by 2 are set to OK.
    FOR    ${i}    IN RANGE    ${1}    ${15}    ${2}
        Ctn Process Service Result Hard    host_1    service_${i}    0    output ok for service_${i}
    END
    Log To Console    Check services from 1 to 14 by 2 are OK
    FOR    ${i}    IN RANGE    ${1}    ${15}    ${2}
        ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_${i}    0    30    HARD
        Should Be True    ${result}    The service (host_1:service_${i}) should be OK.
    END
    Log To Console    Check the BA is still CRITICAL.
    ${result}    Ctn Check Ba Status With Timeout    boolean-ba    2    30
    Should Be True    ${result}    Step${i}: The 'boolean-ba' BA is not CRITICAL as expected

    Log To Console    Services from 15 to 20 by 2 are set OK. The BA must stay critical. And in each step, Broker is restarted to check that the BA states did not change during the restart.
    FOR    ${i}    IN RANGE    ${15}    ${21}    ${2}
        Remove Files    /tmp/ba${id_ba__sid[0]}_*.dot
        ${result}    Ctn Check Ba Status With Timeout    boolean-ba    2    30
        Ctn Broker Get Ba    51001    ${id_ba__sid[0]}    /tmp/ba${id_ba__sid[0]}_1.dot
        Should Be True    ${result}    Step${i}: The 'boolean-ba' BA is not CRITICAL as expected
        ${start}    Get Current Date

        # A restart of cbd should not alter the boolean rules content.
        Ctn Restart Broker
        ${content}    Create List    BA states restored
        ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
        Should Be True    ${result}    It seems that no cache has been restored into BAM.

        Ctn Broker Get Ba    51001    ${id_ba__sid[0]}    /tmp/ba${id_ba__sid[0]}_2.dot

        Wait Until Created    /tmp/ba${id_ba__sid[0]}_2.dot
        ${result}    Ctn Compare Dot Files    /tmp/ba${id_ba__sid[0]}_1.dot    /tmp/ba${id_ba__sid[0]}_2.dot
        Should Be True    ${result}    Known and values in files /tmp/ba${id_ba__sid[0]}_1.dot and /tmp/ba${id_ba__sid[0]}_2.dot should be the same.
        Ctn Process Service Result Hard    host_1    service_${i}    0    output ok for service_${i}
        ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_${i}    0    30    HARD
        Should Be True    ${result}    The service (host_16:service_${i}) should be OK.
    END

    ${result}    Ctn Check Ba Status With Timeout    boolean-ba    0    30
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The 'boolean-ba' BA is not OK as expected

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker


CBABOOCOMPL_RELOAD
    [Documentation]    Scenario: A broker reload does not alter a complex boolean rule state
    ...    Given a BA of type "impact" with a complex AND/OR boolean rule over 20 services
    ...    And all 20 services are CRITICAL, then odd-indexed services 1-13 are set to OK
    ...    And the BA is still CRITICAL because even-indexed services remain CRITICAL
    ...    When broker is reloaded at each remaining step (services 15, 17, 19 set to OK one by one)
    ...    Then the BA state is identical before and after each broker reload
    ...    And the BA becomes OK once all AND groups are satisfied
    [Tags]    broker    engine    bam    boolean_expression    MON-34246
    Ctn BAM Init
    # Services 1 to 20 are passive now.
    FOR    ${i}    IN RANGE    ${1}    ${21}
        Ctn Set Services Passive    ${0}    service_${i}
    END
    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine

    ${id_ba__sid}    Ctn Create Ba    boolean-ba    impact    70    80
    ${id_bool}    Ctn Add Boolean Kpi
    ...    ${id_ba__sid[0]}
    ...    ({host_1 service_1} {IS} {OK} {OR} {host_1 service_2} {IS} {OK}) {AND} ({host_1 service_3} {IS} {OK} {OR} {host_1 service_4} {IS} {OK}) {AND} ({host_1 service_5} {IS} {OK} {OR} {host_1 service_6} {IS} {OK}) {AND} ({host_1 service_7} {IS} {OK} {OR} {host_1 service_8} {IS} {OK}) {AND} ({host_1 service_9} {IS} {OK} {OR} {host_1 service_10} {IS} {OK}) {AND} ({host_1 service_11} {IS} {OK} {OR} {host_1 service_12} {IS} {OK}) {AND} ({host_1 service_13} {IS} {OK} {OR} {host_1 service_14} {IS} {OK}) {AND} ({host_1 service_15} {IS} {OK} {OR} {host_1 service_16} {IS} {OK}) {AND} ({host_1 service_17} {IS} {OK} {OR} {host_1 service_18} {IS} {OK}) {AND} ({host_1 service_19} {IS} {OK} {OR} {host_1 service_20} {IS} {OK})
    ...    False
    ...    100

    Ctn Notify Broker Of Engine Config Change    ${0}

    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}

    Log To Console    Services from 1 to 20 are set to CRITICAL.
    FOR    ${i}    IN RANGE    ${1}    ${21}
        Ctn Process Service Result Hard    host_1    service_${i}    2    output critical for service_${i}
    END
    Log To Console    Check services from 1 to 20 are CRITICAL.
    FOR    ${i}    IN RANGE    ${1}    ${21}
        ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_${i}    2    30    HARD
        Should Be True    ${result}    The service (host_1:service_${i}) should be CRITICAL.
    END

    Log To Console    Services from 1 to 14 by 2 are set to OK.
    FOR    ${i}    IN RANGE    ${1}    ${15}    ${2}
        Ctn Process Service Result Hard    host_1    service_${i}    0    output ok for service_${i}
    END
    Log To Console    Check services from 1 to 14 by 2 are OK
    FOR    ${i}    IN RANGE    ${1}    ${15}    ${2}
        ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_${i}    0    30    HARD
        Should Be True    ${result}    The service (host_1:service_${i}) should be OK.
    END
    Log To Console    Check the BA is still CRITICAL.
    ${result}    Ctn Check Ba Status With Timeout    boolean-ba    2    30
    Should Be True    ${result}    Step${i}: The 'boolean-ba' BA is not CRITICAL as expected

    Log To Console    Services from 15 to 20 by 2 are set OK. The BA must stay critical. And in each step, Broker is reloaded to check that the BA states did not change during the reload.
    FOR    ${i}    IN RANGE    ${15}    ${21}    ${2}
        Remove Files    /tmp/ba${id_ba__sid[0]}_*.dot
        ${result}    Ctn Check Ba Status With Timeout    boolean-ba    2    30
        Ctn Broker Get Ba    51001    ${id_ba__sid[0]}    /tmp/ba${id_ba__sid[0]}_1.dot
        Should Be True    ${result}    Step${i}: The 'boolean-ba' BA is not CRITICAL as expected
        ${start}    Get Current Date

        # A reload of cbd should not alter the boolean rules content.
        Ctn Reload Broker
        ${content}    Create List    BA states restored
        ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
        Should Be True    ${result}    It seems that no cache has been restored into BAM.

        Ctn Broker Get Ba    51001    ${id_ba__sid[0]}    /tmp/ba${id_ba__sid[0]}_2.dot

        Wait Until Created    /tmp/ba${id_ba__sid[0]}_2.dot
        ${result}    Ctn Compare Dot Files    /tmp/ba${id_ba__sid[0]}_1.dot    /tmp/ba${id_ba__sid[0]}_2.dot
        Should Be True    ${result}    Known and values in files /tmp/ba${id_ba__sid[0]}_1.dot and /tmp/ba${id_ba__sid[0]}_2.dot should be the same.
        Ctn Process Service Result Hard    host_1    service_${i}    0    output ok for service_${i}
        ${result}    Ctn Check Service Resource Status With Timeout    host_1    service_${i}    0    30    HARD
        Should Be True    ${result}    The service (host_16:service_${i}) should be OK.
    END

    ${result}    Ctn Check Ba Status With Timeout    boolean-ba    0    30
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The 'boolean-ba' BA is not OK as expected

    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Kindly Stop Broker


*** Keywords ***
Ctn BAM Init
    Ctn Clear Commands Status
    Ctn Clear Retention
    Ctn Clear Db Conf    mod_bam
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    bam    trace
    Ctn Broker Config Log    central    sql    error
    Ctn Broker Config Flush Log    central    0
    Ctn Broker Config Source Log    central    1
    Ctn Add Bam Config To Broker    central

Ctn BAM Setup
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
