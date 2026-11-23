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

    [Teardown]    Ctn Stop Engine Broker And Save Logs

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

    [Teardown]    Ctn Stop Engine Broker And Save Logs

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

    [Teardown]    Ctn Stop Engine Broker And Save Logs

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

    [Teardown]    Ctn Stop Engine Broker And Save Logs

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

    [Teardown]    Ctn Stop Engine Broker And Save Logs


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
        ${content}    Create List    virtual service states restored from cache file
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

    [Teardown]    Ctn Stop Engine Broker And Save Logs


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
        # On reload the BAM endpoint is updated in place (not destroyed), but the
        # applier may recreate individual KPI objects whose config changed (e.g. an
        # opened_event got attached). A recreated boolean-expression KPI re-syncs its
        # state from the preserved (already-known) boolean expression via
        # kpi_boolexp::link_boolexp, so it must not expose a transient UNKNOWN state.
        # Wait for BAM to finish reprocessing the reload before querying the BA again.
        ${content}    Create List    BAM: loading cache
        ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
        Should Be True    ${result}    Broker did not reprocess BAM after the reload.

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

    [Teardown]    Ctn Stop Engine Broker And Save Logs


CBABOOKPIKINDS
    [Documentation]    Scenario: A BA keeps its three kinds of KPI when the host/service ids come from the global cache
    ...    Given a centralized platform, where Broker answers the host/service questions from its global cache
    ...    And a child BA of type "worst" built on service_314
    ...    And a parent BA of type "worst" holding one KPI of each kind: service_303, a boolean rule on service_302 and the child BA
    ...    When the three services are OK
    ...    Then the parent BA is OK
    ...    When service_314 alone becomes CRITICAL
    ...    Then the parent BA is CRITICAL, which its BA KPI alone can explain
    ...    When service_302 alone becomes CRITICAL
    ...    Then the parent BA is CRITICAL, which its boolean KPI alone can explain
    ...    When service_303 alone becomes CRITICAL
    ...    Then the parent BA is CRITICAL, which its service KPI alone can explain
    [Tags]    broker    engine    bam    boolean_expression
    Ctn BAM Init
    Ctn Set Services Passive    ${0}    service_302
    Ctn Set Services Passive    ${0}    service_303
    Ctn Set Services Passive    ${0}    service_314
    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine

    # One KPI of each kind, and only the first one names a service. A boolean
    # rule and a BA carry no service at all, yet the applier asks about them
    # just the same -- with a couple of zeros. Reading that couple as a
    # deactivated service would drop those two KPIs, and the BA would then stay
    # OK below while its KPI is CRITICAL.
    @{svc}    Set Variable    ${{ [("host_16", "service_314")] }}
    ${child_ba}    Ctn Create Ba With Services    child-ba    worst    ${svc}
    ${parent_ba}    Ctn Create Ba    parent-ba    worst    100    100
    Ctn Add Service Kpi    host_16    service_303    ${parent_ba[0]}    40    30    20
    Ctn Add Boolean Kpi
    ...    ${parent_ba[0]}
    ...    {host_16 service_302} {IS} {CRITICAL}
    ...    True
    ...    100
    Ctn Add Ba Kpi    ${child_ba[0]}    ${parent_ba[0]}    1    2    3

    Ctn Notify Broker Of Engine Config Change    ${0}

    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}

    # Every service OK: both BAs are OK, and each of the three KPIs is in a
    # known state.
    Ctn Process Service Result Hard    host_16    service_302    0    output ok for service_302
    Ctn Process Service Result Hard    host_16    service_303    0    output ok for service_303
    Ctn Process Service Result Hard    host_16    service_314    0    output ok for service_314
    ${result}    Ctn Check Ba Status With Timeout    parent-ba    0    60
    Ctn Dump Ba On Error    ${result}    ${parent_ba[0]}
    Should Be True    ${result}    The 'parent-ba' BA is not OK as expected

    # The BA KPI. Nothing else is CRITICAL, so a parent that stays OK says its
    # BA KPI was dropped at load time.
    Ctn Process Service Result Hard    host_16    service_314    2    output critical for service_314
    ${result}    Ctn Check Ba Status With Timeout    child-ba    2    60
    Ctn Dump Ba On Error    ${result}    ${child_ba[0]}
    Should Be True    ${result}    The 'child-ba' BA is not CRITICAL as expected
    ${result}    Ctn Check Ba Status With Timeout    parent-ba    2    60
    Ctn Dump Ba On Error    ${result}    ${parent_ba[0]}
    Should Be True    ${result}    The 'parent-ba' BA did not follow its child: its BA KPI is missing

    Ctn Process Service Result Hard    host_16    service_314    0    output ok for service_314
    ${result}    Ctn Check Ba Status With Timeout    parent-ba    0    60
    Ctn Dump Ba On Error    ${result}    ${parent_ba[0]}
    Should Be True    ${result}    The 'parent-ba' BA is not OK again as expected

    # The boolean KPI, alone this time.
    Ctn Process Service Result Hard    host_16    service_302    2    output critical for service_302
    ${result}    Ctn Check Ba Status With Timeout    parent-ba    2    60
    Ctn Dump Ba On Error    ${result}    ${parent_ba[0]}
    Should Be True    ${result}    The 'parent-ba' BA did not follow its boolean rule: its boolean KPI is missing

    Ctn Process Service Result Hard    host_16    service_302    0    output ok for service_302
    ${result}    Ctn Check Ba Status With Timeout    parent-ba    0    60
    Ctn Dump Ba On Error    ${result}    ${parent_ba[0]}
    Should Be True    ${result}    The 'parent-ba' BA is not OK again as expected

    # The service KPI, the only one the cache has something to say about.
    Ctn Process Service Result Hard    host_16    service_303    2    output critical for service_303
    ${result}    Ctn Check Ba Status With Timeout    parent-ba    2    60
    Ctn Dump Ba On Error    ${result}    ${parent_ba[0]}
    Should Be True    ${result}    The 'parent-ba' BA did not follow service_303: its service KPI is missing

    [Teardown]    Ctn Stop Engine Broker And Save Logs


CBABOODEACTIVATEDSVC
    [Documentation]    Scenario: A KPI whose service is deactivated is dropped once the global cache knows the poller
    ...    Given a centralized platform, where Broker answers the host/service questions from its global cache
    ...    And a BA of type "worst" with two service KPIs, service_302 and service_303
    ...    When the configuration is acknowledged, so that the cache holds the poller
    ...    And service_303 is then deactivated -- its row says so and the export no longer carries it
    ...    Then the cache no longer holds service_303
    ...    And on the next start, where the cache is filled from the stored configuration, the KPI of service_303 is dropped
    ...    And the KPI of service_302 is kept, the BA still following it
    [Tags]    broker    engine    bam    boolean_expression
    Ctn BAM Init
    # Start from a cache that owes nothing to the previous test: the cache file
    # still holds the configuration, so a service dropped from the export here
    # would come back from the disk.
    Ctn Clear Broker Cache
    Ctn Clear Prot Files
    Ctn Set Services Passive    ${0}    service_302
    Ctn Set Services Passive    ${0}    service_303
    Ctn Clone Engine Config To Db
    Ctn Add Bam Config To Engine

    ${id_ba__sid}    Ctn Create Ba    deactivated-ba    worst    100    100
    Ctn Add Service Kpi    host_16    service_302    ${id_ba__sid[0]}    40    30    20
    Ctn Add Service Kpi    host_16    service_303    ${id_ba__sid[0]}    40    30    20

    Ctn Notify Broker Of Engine Config Change    ${0}

    Ctn Start Broker    newGeneration=True
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}

    ${content}    Create List    host/service ids are taken from the global cache.
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Broker did not take the host/service ids from the global cache

    # The poller has to have acknowledged its configuration before anything is
    # concluded from the cache: until then the cache knows nothing of it, and
    # nothing of its services either.
    Wait Until Created    ${VarRoot}/lib/centreon-broker/central-broker-master/pollers-configuration/1.prot    timeout=60s
    ${svc_ids}    Ctn Get Service Ids    ${51001}
    Should Contain    ${svc_ids}    ${{ (16, 303) }}    The cache does not hold service_303 yet, nothing can be told from its absence later

    # Deactivating a service is two things at once, and a test doing only one of
    # them measures the wrong regime: the row is what the configuration database
    # says, the absence from the export is what the global cache sees.
    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    UPDATE service SET service_activate='0' WHERE service_description='service_303'
    Disconnect From Database
    Ctn Engine Config Remove Service    ${0}    host_16    service_303
    Ctn Notify Broker Of Engine Config Change    ${0}

    # The cache follows the difference the poller acknowledges, so this is what
    # says the export went all the way through.
    FOR    ${i}    IN RANGE    60
        ${svc_ids}    Ctn Get Service Ids    ${51001}
        ${gone}    Evaluate    (16, 303) not in $svc_ids
        IF    ${gone}    BREAK
        Sleep    1s
    END
    Should Be True    ${gone}    The cache still holds service_303 after it was dropped from the export

    # BAM reads its configuration once, at startup. Restarting Broker is what
    # puts the two in the order production has them: the cache filled from the
    # stored configuration, then BAM reading it.
    Ctn Kindly Stop Broker
    ${restart}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True

    ${content}    Create List    linked to a deactivated service
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${restart}    ${content}    60
    Should Be True    ${result}    The KPI of the deactivated service_303 was not dropped

    # And only that one: a cache that answered for every service would have
    # taken the KPI of service_302 with it.
    ${dropped}    Grep File    ${centralLog}    linked to a deactivated service
    ${count}    Get Line Count    ${dropped}
    Should Be Equal As Integers    ${count}    ${1}    ${count} KPIs were dropped instead of the one of service_303

    Ctn Process Service Result Hard    host_16    service_302    0    output ok for service_302
    ${result}    Ctn Check Ba Status With Timeout    deactivated-ba    0    60
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The 'deactivated-ba' BA is not OK as expected

    Ctn Process Service Result Hard    host_16    service_302    2    output critical for service_302
    ${result}    Ctn Check Ba Status With Timeout    deactivated-ba    2    60
    Ctn Dump Ba On Error    ${result}    ${id_ba__sid[0]}
    Should Be True    ${result}    The 'deactivated-ba' BA did not follow service_302: its KPI was dropped too

    # This test leaves the platform amputated of a service: the row says it is
    # deactivated, the export no longer carries it, and both the cache file and
    # the stored poller configurations hold that state -- which the next suite
    # would start from, and drop the KPIs of service_303 without asking for it.
    [Teardown]    Run Keywords
    ...    Ctn Stop Engine Broker And Save Logs
    ...    AND    Ctn Reactivate Service 303

*** Keywords ***
Ctn Reactivate Service 303
    [Documentation]    Undo what CBABOODEACTIVATEDSVC did to the shared platform.
    Connect To Database    pymysql    ${DBNameConf}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    UPDATE service SET service_activate='1' WHERE service_description='service_303'
    Disconnect From Database
    Ctn Clear Broker Cache
    Ctn Clear Prot Files

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
