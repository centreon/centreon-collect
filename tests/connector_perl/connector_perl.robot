*** Settings ***

Resource	../resources/resources.robot
Library     ../resources/Engine.py
Suite Setup	Start engine
Suite Teardown  Stop engine

Documentation	centreon_connector_perl tests.
Library  Process
Library  OperatingSystem



*** Keywords ***
Start engine
    Create Directory  /tmp/test_connector_perl/log/
    Create Directory  /tmp/test_connector_perl/rw/
    Copy Files  connector_perl/conf_engine/*  /tmp/test_connector_perl/
    Empty Directory  /tmp/test_connector_perl/log/
    Kill Engine
    Start Custom Engine  /tmp/test_connector_perl/centengine.cfg  engine_alias
    Sleep  5 seconds  we wait engine start 

Stop engine
    Stop Custom Engine  engine_alias

*** Test Cases ***
test use connector perl exist script
    [Documentation]  test exist script
    [Tags]	Connector	Engine
    schedule forced host check  local_host_test_machine  /tmp/test_connector_perl/rw/centengine.cmd
    Sleep  5 seconds  we wait engine forced checks 
    ${search_result}=  check search  /tmp/test_connector_perl/log/centengine.debug  test.pl
    Should Contain  ${search_result}  a dummy check  msg=check not found 

test use connector perl unknown script
    [Documentation]  test unknown script
    [Tags]	Connector	Engine
    schedule forced host check  local_host_test_machine_bad_test  /tmp/test_connector_perl/rw/centengine.cmd
    Sleep  5 seconds  we wait engine forced checks 
    ${search_result}=  check search  /tmp/test_connector_perl/log/centengine.debug  test_titi.pl
    Should Contain  ${search_result}  Embedded Perl error: failed to open Perl file '/tmp/test_connector_perl/test_titi.pl'  msg=check not found 

test use connector perl multiple script
    [Documentation]  test script multiple
    [Tags]	Connector	Engine
    FOR	${idx}	IN RANGE	2  12
        ${host}=	Catenate	SEPARATOR=	local_host_test_machine.	${idx}
        schedule forced host check  ${host}  /tmp/test_connector_perl/rw/centengine.cmd
    END
    Sleep  10 seconds  we wait engine forced checks 
    FOR	${idx}	IN RANGE	2	12
        ${search_str}=   Catenate	SEPARATOR=  test.pl -H 127.0.0.  ${idx} 
        ${search_result}=  check search  /tmp/test_connector_perl/log/centengine.debug  ${search_str}
        Should Contain  ${search_result}  a dummy check  msg=check not found  
    END
