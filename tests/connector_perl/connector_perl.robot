*** Settings ***
Documentation       centreon_connector_perl tests.

Resource            ../resources/resources.robot
Library             ../resources/Engine.py
Library             Process
Library             OperatingSystem

Suite Setup         Start Engine
Suite Teardown      Stop Engine


*** Test Cases ***
EPCWS
    [Documentation]    Engine is started to be used with the Perl connector. A host check is done and we verify it is executed by the connector.
    [Tags]    connector    engine
    Schedule Forced Host Check    local_host_test_machine    /tmp/test_connector_perl/rw/centengine.cmd
    Sleep    5 seconds    we wait for Engine forced checks
    ${search_result}    Check Search    /tmp/test_connector_perl/log/centengine.log    test.pl
    Should Contain    ${search_result}    a dummy check    check not found

EPCUS
    [Documentation]    Engine is started to be used with the Perl connector. A host check is done with an unknown script. An error should be raised
    [Tags]    connector    engine
    Schedule Forced Host Check    local_host_test_machine_bad_test    /tmp/test_connector_perl/rw/centengine.cmd
    Sleep    5 seconds    we wait for Engine forced checks
    ${search_result}    Check Search    /tmp/test_connector_perl/log/centengine.log    test_titi.pl
    Should Contain
    ...    ${search_result}
    ...    Embedded Perl error: failed to open Perl file '/tmp/test_connector_perl/test_titi.pl'
    ...    check not found

EPCMS
    [Documentation]    Engine is started to be used with the Perl connector. Several calls are made to a script. We get a result for each of them.
    [Tags]    connector    engine
    FOR    ${idx}    IN RANGE    2    12
        ${host}    Catenate    SEPARATOR=    local_host_test_machine.    ${idx}
        Schedule Forced Host Check    ${host}    /tmp/test_connector_perl/rw/centengine.cmd
    END
    Sleep    10 seconds    we wait for Engine forced checks
    FOR    ${idx}    IN RANGE    2    12
        ${search_str}    Catenate    SEPARATOR=    test.pl -H 127.0.0.    ${idx}
        ${search_result}    Check Search    /tmp/test_connector_perl/log/centengine.log    ${search_str}
        Should Contain    ${search_result}    a dummy check    check not found
    END


*** Keywords ***
Start Engine
    Create Directory    /tmp/test_connector_perl/log/
    Create Directory    /tmp/test_connector_perl/rw/
    Copy Files    connector_perl/conf_engine/*    /tmp/test_connector_perl/
    Empty Directory    /tmp/test_connector_perl/log/
    Kill Engine
    Start Custom Engine    /tmp/test_connector_perl/centengine.cfg    engine_alias
    Sleep    5 seconds    we wait engine start

Stop engine
    Stop Custom Engine    engine_alias
