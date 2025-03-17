*** Settings ***
Documentation       centreon_connector_perl tests.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes


*** Test Cases ***
CONPERL
    [Documentation]    The test.pl script is launched using the perl connector.
    ...    Then we should find its execution in the engine log file.
    [Tags]    connector    engine
    Ctn Config Engine    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_commands    trace
    Ctn Config Broker    module    ${1}
    Ctn Clear Retention
    ${hcmd}    Ctn Get Host Command    1
    Ctn Set Command Connector    0    ${hcmd}    Perl Connector
    Ctn Set Check Command    0    ${hcmd}    /tmp/var/lib/centreon-engine/check.pl --id 0 --output "my check"

    ${start}    Ctn Get Round Current Date
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}
    Ctn Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
    ${content}    Create List    _recv_query_execute.*output='Host check.*: my check
    ${result}    Ctn Find Regex In Log With Timeout    ${engineLog0}	  ${start}    ${content}    30
    Should Be True    ${result[0]}    Impossible to find a recv_query_execute with the check result that is "my check"
    Ctn Stop Engine


CONPERLM
    [Documentation]    Ten forced checks are scheduled on ten hosts configured with the Perl Connector.
    ...    The we get the result of each of them.
    [Tags]    connector    engine
    Ctn Config Engine    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_commands    trace
    Ctn Config Broker    module    ${1}
    Ctn Clear Retention
    FOR    ${idx}    IN RANGE    1    11
	${hcmd}    Ctn Get Host Command    ${idx}
	Ctn Set Command Connector    0    ${hcmd}    Perl Connector
	Ctn Set Check Command    0    ${hcmd}    /tmp/var/lib/centreon-engine/check.pl --id 0 --output "my check ${idx}"
    END

    ${start}    Ctn Get Round Current Date
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}

    FOR    ${idx}    IN RANGE    1    11
        Ctn Schedule Forced Host Check    host_${idx}    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd
    END
    ${content}    Create List
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
    ${result}    Ctn Find Regex In Log With Timeout    ${engineLog0}	  ${start}    ${content}    30
    Should Be True    ${result[0]}    Impossible to find ${result[1]}
    Ctn Stop Engine
