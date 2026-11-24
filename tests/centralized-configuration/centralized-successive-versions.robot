*** Settings ***
Documentation     Two exports of the same poller that follow each other closely: the second one waits for the first to be answered, and is then delivered as a difference from it, or fails without holding back the first.

Resource          ../resources/import.resource

Suite Setup       Ctn Clean Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup        Ctn Clean Before Test
Test Teardown     Ctn Stop Engine Broker And Save Logs    only_central=True


*** Test Cases ***
BECSV1
    [Documentation]    Scenario: an export arrives while the previous one is in flight
    ...    Given one poller running the configuration Broker pushed to it
    ...    And Engine frozen, so that the next configuration it is sent stays in flight
    ...    When v1 is exported, and sent to Engine
    ...    And v2 is exported while v1 is in flight
    ...    Then Broker stores v2 in next-1.prot and sends nothing: v1 is in flight
    ...    When Engine is released and acknowledges v1
    ...    Then v2 takes the place of v1 and is sent as a difference from v1, not as a full configuration
    ...    And Engine acknowledges v2
    ...    And the database holds both changes.
    [Tags]    broker    engine    config    centralized
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    info
    Ctn Clear Retention
    Ctn Clear Prot Files
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Push Configuration And Wait    ${start}    ${0}

    VAR    ${dir}    ${VarRoot}/lib/centreon-broker/central-broker-master/pollers-configuration

    # Frozen, Engine receives v1 but cannot apply it: v1 stays in flight for as
    # long as the test needs.
    Run Process    pkill    -STOP    centengine
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    7
    ${start}    Ctn Get Round Current Date
    Ctn Announce Poller Configurations    batch    ${0}
    ${content}    Create List    sending DiffState to poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    v1 was not sent to the poller

    # v2 while v1 is in flight: stored aside, nothing sent.
    Ctn Engine Config Replace Value In Services    ${0}    service_1    max_check_attempts    4
    ${start}    Ctn Get Round Current Date
    Ctn Announce Poller Configurations    batch    ${0}
    Wait Until Created    ${dir}/next-1.prot    30s
    ${content}    Create List    for the poller to answer about the configuration in flight
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    10
    Should Be True    ${result}    Broker did not say that v2 waits behind v1
    ${content}    Create List    sending DiffState to poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    5
    Should Not Be True    ${result}    v2 was sent while v1 was in flight

    # Released, Engine applies v1 and acknowledges it; v2 follows as a
    # difference from v1, which Engine runs.
    Run Process    pkill    -CONT    centengine
    ${content}    Create List
    ...    received diff state ack from poller 1
    ...    is now the one prepared for poller 1
    ...    sending DiffState to poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    v1 was not acknowledged, or v2 not sent after it
    Wait Until Removed    ${dir}/next-1.prot    10s
    ${content}    Create List    sending it the full configuration
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    2
    Should Not Be True    ${result}    v2 was sent as a full configuration instead of a difference from v1

    ${content}    Create List    all engine peers have acknowledged their configuration
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    the round of v2 did not close
    Wait Until Removed    ${dir}/new-1.prot    10s

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${i}    IN RANGE    30
        ${output}    Query
        ...    SELECT s.check_interval, s.max_check_attempts FROM services s JOIN hosts h ON h.host_id = s.host_id WHERE h.name = 'host_1' AND s.description = 'service_1'
        IF    "${output}" == "((7.0, 4),)"    BREAK
        Sleep    1s
    END
    Disconnect From Database
    Should Be Equal As Strings    ${output}    ((7.0, 4),)    The database does not hold both changes

BECSV2
    [Documentation]    Scenario: the version waiting behind another one fails, and the one before still reaches the database
    ...    Given one poller running the configuration Broker pushed to it
    ...    And Engine frozen, so that the next configuration it is sent stays in flight
    ...    When v1 is exported, and sent to Engine
    ...    And v2, which gives the hosts a tag defined nowhere, is exported while v1 is in flight
    ...    Then Broker stores v2 in next-1.prot and sends nothing: v1 is in flight
    ...    When Engine is released and acknowledges v1
    ...    Then v2 takes the place of v1 and is sent, which keeps the round open
    ...    And Engine cannot apply v2 and tells Broker why
    ...    And Broker logs the failure and removes new-1.prot and new-diff-1.prot
    ...    And the round closes all the same
    ...    And the database holds v1.
    [Tags]    broker    engine    config    centralized
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    info
    Ctn Clear Retention
    Ctn Clear Prot Files
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Push Configuration And Wait    ${start}    ${0}

    VAR    ${dir}    ${VarRoot}/lib/centreon-broker/central-broker-master/pollers-configuration

    # Frozen, Engine receives v1 but cannot apply it: v1 stays in flight for as
    # long as the test needs.
    Run Process    pkill    -STOP    centengine
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_interval    7
    ${start}    Ctn Get Round Current Date
    Ctn Announce Poller Configurations    batch    ${0}
    ${content}    Create List    sending DiffState to poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    v1 was not sent to the poller

    # v2 while v1 is in flight: tag 99 is defined nowhere, Broker does not check
    # host tags, Engine does.
    Ctn Add Tags To All Hosts    ${0}    group_tags    99
    ${start}    Ctn Get Round Current Date
    Ctn Announce Poller Configurations    batch    ${0}
    Wait Until Created    ${dir}/next-1.prot    30s
    ${content}    Create List    for the poller to answer about the configuration in flight
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    10
    Should Be True    ${result}    Broker did not say that v2 waits behind v1

    # Released, Engine applies v1 and acknowledges it; v2 follows and fails.
    Run Process    pkill    -CONT    centengine
    ${content}    Create List
    ...    received diff state ack from poller 1
    ...    is now the one prepared for poller 1
    ...    sending DiffState to poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    v1 was not acknowledged, or v2 not sent after it

    ${content}    Create List    Cannot apply new configuration: Could not find tag
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    Engine did not fail to apply v2 with an undefined tag

    ${content}    Create List    Poller 1 could not apply the configuration
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Broker was not told that the poller could not apply v2
    Wait Until Removed    ${dir}/new-1.prot    10s
    Wait Until Removed    ${dir}/new-diff-1.prot    10s

    # The failure of v2 was all the round waited for: v1 goes to the database.
    ${content}    Create List    all engine peers have acknowledged their configuration
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    the round left open by v2 did not close after its failure
    Wait Until Removed    ${dir}/diff-1.prot    10s

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    FOR    ${i}    IN RANGE    30
        ${output}    Query
        ...    SELECT s.check_interval FROM services s JOIN hosts h ON h.host_id = s.host_id WHERE h.name = 'host_1' AND s.description = 'service_1'
        IF    "${output}" == "((7.0,),)"    BREAK
        Sleep    1s
    END
    Disconnect From Database
    Should Be Equal As Strings    ${output}    ((7.0,),)    The database does not hold v1
