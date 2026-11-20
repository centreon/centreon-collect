*** Settings ***
Documentation       centreon_connector_perl tests with centralized configuration.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed
Test Tags           connector    engine    bbdo


*** Test Cases ***
CCONPERL
    [Documentation]    Scenario: Single host check via Perl Connector in centralized configuration
    ...    Given a centralized engine and broker configuration with the Perl Connector
    ...    When a forced host check is scheduled on host_1
    ...    Then the check execution result should appear in the engine log file.
    Ctn Config Centralized Engine    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_commands    trace
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    bbdo    info
    Ctn Clear Retention
    ${hcmd}    Ctn Get Host Command    1
    Ctn Set Command Connector    0    ${hcmd}    Perl Connector
    Ctn Set Check Command    0    ${hcmd}    /tmp/var/lib/centreon-engine/check.pl --id 0 --output "my check"

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine Configuration To Be Applied    ${start}    0
    Ctn Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
    VAR    @{content}    _recv_query_execute.*output='Host check.*: my check
    ${result}    Ctn Find Regex In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result[0]}    Impossible to find a recv_query_execute with the check result that is "my check"
    Ctn Stop Engine
    Ctn Kindly Stop Broker

CCONPERLM
    [Documentation]    Scenario: Ten host checks via Perl Connector in centralized configuration
    ...    Given a centralized engine and broker configuration with the Perl Connector on ten hosts
    ...    When a forced check is scheduled on each of the ten hosts
    ...    Then the check execution result for each host should appear in the engine log file.
    Ctn Config Centralized Engine    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_commands    trace
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    bbdo    info
    Ctn Clear Retention
    FOR    ${idx}    IN RANGE    1    11
        ${hcmd}    Ctn Get Host Command    ${idx}
        Ctn Set Command Connector    0    ${hcmd}    Perl Connector
        Ctn Set Check Command    0    ${hcmd}    /tmp/var/lib/centreon-engine/check.pl --id 0 --output "my check ${idx}"
    END

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine Configuration To Be Applied    ${start}    0

    FOR    ${idx}    IN RANGE    1    11
        Ctn Schedule Forced Host Check    host_${idx}    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
    END
    VAR    @{content}
    ...    _recv_query_execute.*output='Host check.*: my check 1
    ...    _recv_query_execute.*output='Host check.*: my check 2
    ...    _recv_query_execute.*output='Host check.*: my check 3
    ...    _recv_query_execute.*output='Host check.*: my check 4
    ...    _recv_query_execute.*output='Host check.*: my check 5
    ...    _recv_query_execute.*output='Host check.*: my check 6
    ...    _recv_query_execute.*output='Host check.*: my check 7
    ...    _recv_query_execute.*output='Host check.*: my check 8
    ...    _recv_query_execute.*output='Host check.*: my check 9
    ...    _recv_query_execute.*output='Host check.*: my check 10
    ${result}    Ctn Find Regex In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result[0]}    Impossible to find ${result[1]}
    Ctn Stop Engine
    Ctn Kindly Stop Broker
