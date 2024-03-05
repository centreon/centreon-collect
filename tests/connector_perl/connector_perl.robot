*** Settings ***
Documentation       centreon_connector_perl tests.

Resource            ../resources/import.resource

Suite Setup         Ctn Start engine
Suite Teardown      Ctn Stop engine


*** Test Cases ***
test use connector perl exist script
    [Documentation]    test exist script
    [Tags]    connector    engine
    Ctn Schedule Forced Host Check    local_host_test_machine    /tmp/test_connector_perl/rw/centengine.cmd
    Sleep    5 seconds    we wait engine forced checks
    ${search_result}    Ctn Check Search    /tmp/test_connector_perl/log/centengine.debug    test.pl
    Should Contain    ${search_result}    a dummy check    check not found

test use connector perl unknown script
    [Documentation]    test unknown script
    [Tags]    connector    engine
    Ctn Schedule Forced Host Check    local_host_test_machine_bad_test    /tmp/test_connector_perl/rw/centengine.cmd
    Sleep    5 seconds    we wait engine forced checks
    ${search_result}    Ctn Check Search    /tmp/test_connector_perl/log/centengine.debug    test_titi.pl
    Should Contain
    ...    ${search_result}
    ...    Embedded Perl error: failed to open Perl file '/tmp/test_connector_perl/test_titi.pl'
    ...    check not found

test use connector perl multiple script
    [Documentation]    test script multiple
    [Tags]    connector    engine
    FOR    ${idx}    IN RANGE    2    12
        ${host}    Catenate    SEPARATOR=    local_host_test_machine.    ${idx}
        Ctn Schedule Forced Host Check    ${host}    /tmp/test_connector_perl/rw/centengine.cmd
    END
    Sleep    10 seconds    we wait engine forced checks
    FOR    ${idx}    IN RANGE    2    12
        ${search_str}    Catenate    SEPARATOR=    test.pl -H 127.0.0.    ${idx}
        ${search_result}    Ctn Check Search    /tmp/test_connector_perl/log/centengine.debug    ${search_str}
        Should Contain    ${search_result}    a dummy check    check not found
    END


*** Keywords ***
Ctn Start engine
    Create Directory    /tmp/test_connector_perl/log/
    Create Directory    /tmp/test_connector_perl/rw/
    Copy Files    connector_perl/conf_engine/*    /tmp/test_connector_perl/
    Empty Directory    /tmp/test_connector_perl/log/
    Ctn Kill Engine
    Ctn Start Custom Engine    /tmp/test_connector_perl/centengine.cfg    engine_alias
    Sleep    5 seconds    we wait engine start

Ctn Stop engine
    Ctn Stop Custom Engine    engine_alias
