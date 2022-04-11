*** Settings ***
Resource	../resources/resources.robot
Library     ../resources/Engine.py
Suite Setup	Prepare ssh and start engine
Suite Teardown  Stop engine

Documentation	centreon_connector_ssh tests.
Library  Process
Library  OperatingSystem



*** Keywords ***
Prepare ssh and start engine
	[Documentation]  in order to test ssh connector, we need to create a user, his password and his Keyword
    Run  useradd testconnssh
    Remove File  ~testconnssh/.ssh/authorized_keys
    Run  echo testconnssh:passwd | chpasswd
    Run  su testconnssh -c "ssh-keygen -q -t rsa -N '' -f ~/.ssh/id_rsa <<<y"
    Run  ssh-keygen -q -t rsa -N '' -f ~/.ssh/id_rsa <<<y
    Create Directory  /tmp/test_connector_ssh/log/
    Create Directory  /tmp/test_connector_ssh/rw/
    Copy Files  connector_ssh/conf_engine/*.cfg  /tmp/test_connector_ssh/
    Copy Files  connector_ssh/conf_engine/*.json  /tmp/test_connector_ssh/
    Empty Directory  /tmp/test_connector_ssh/log/
    Kill Engine
    Start Custom Engine  /tmp/test_connector_ssh/centengine.cfg  engine_alias
    Sleep  5 seconds  we wait engine start 

Stop engine
    Stop Custom Engine  engine_alias

*** Test Cases ***
TestBadUser
    [Documentation]  test unknown user
    [Tags]	Connector	Engine
    schedule forced host check  local_host_test_machine_.bad_user  /tmp/test_connector_ssh/rw/centengine.cmd
    Sleep  5 seconds  we wait engine forced checks 
    ${search_result}=  check search  /tmp/test_connector_ssh/log/centengine.debug  /usr/lib64/nagios/plugins/check_by_ssh -H 127.0.0.10
    Should Contain  ${search_result}  fail to connect to toto@127.0.0.10  msg=check not found for fail to connect to toto@127.0.0.10  

TestBadPwd
    [Documentation]  test bad password
    [Tags]	Connector	Engine
    schedule forced host check  local_host_test_machine_.bad_pwd  /tmp/test_connector_ssh/rw/centengine.cmd
    Sleep  5 seconds  we wait engine forced checks 
    ${search_result}=  check search  /tmp/test_connector_ssh/log/centengine.debug  /usr/lib64/nagios/plugins/check_by_ssh -H 127.0.0.11
    Should Contain  ${search_result}  fail to connect to testconnssh@127.0.0.11  msg=check not found for fail to connect to testconnssh@127.0.0.11  

Test10Hosts
    [Documentation]  as 127.0.0.x point to the localhost address we will simulate check on 20 hosts
    [Tags]	Connector	Engine
    Sleep  5 seconds  we wait sshd raz pending connexions from previous tests
    Run  cat ~testconnssh/.ssh/id_rsa.pub ~root/.ssh/id_rsa.pub > ~testconnssh/.ssh/authorized_keys

    FOR	${idx}	IN RANGE	0  9
        ${host}=	Catenate	SEPARATOR=	local_host_test_machine_.	${idx}
        schedule forced host check  ${host}  /tmp/test_connector_ssh/rw/centengine.cmd
    END
    Sleep  10 seconds  we wait engine forced checks 
    ${search_result}=  check search  /tmp/test_connector_ssh/log/centengine.debug  /usr/lib64/nagios/plugins/check_by_ssh -H ::1
    Should Contain  ${search_result}  output='toto=::1'  msg=check not found for ::1  

    FOR	${idx}	IN RANGE	1	9
        ${expected_output}=  Catenate	SEPARATOR=  output='toto=127.0.0.  ${idx}
        ${search_str}=   Catenate	SEPARATOR=  /usr/lib64/nagios/plugins/check_by_ssh -H 127.0.0.  ${idx} 
        ${search_result}=  check search  /tmp/test_connector_ssh/log/centengine.debug  ${search_str}
        Should Contain  ${search_result}  ${expected_output}  msg=check not found for ${expected_output}  
    END



