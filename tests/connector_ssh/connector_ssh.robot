*** Settings ***
Documentation       centreon_connector_ssh tests.

Resource            ../resources/import.resource

Suite Setup         Ctn Prepare ssh
Suite Teardown      Ctn Clean Whitelist
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save SSH Logs If Failed


*** Test Cases ***
TestBadUser
    [Documentation]    test unknown user
    [Tags]    connector    engine
    Ctn Clear Retention
    Ctn Config Broker    module    ${1}
    Ctn Config Engine    ${1}

    Ctn Engine Config Set Value    ${0}    log_level_commands    trace
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    _USER    toto
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    ssh_linux_snmp
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    address    127.0.0.10
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    _PASSWORD    titi
    Ctn Engine Config Add Command
    ...    ${0}
    ...    ssh_linux_snmp
    ...    $USER1$/check_by_ssh -H $HOSTADDRESS$ -l $_HOSTUSER$ -a $_HOSTPASSWORD$ -C "echo -n toto=$HOSTADDRESS$"
    ...    SSH Connector
    Ctn Start engine

    ${start}    Get Current Date
    ${content}    Create List    INITIAL SERVICE STATE: host_1;service_1;    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.

    ${start}    Get Current Date
    Ctn Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd

    ${content}    Create List    fail to connect to toto@127.0.0.10
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message fail to connect to toto@127.0.0.10 should be available.
    Ctn Stop engine

TestBadPwd
    [Documentation]    test bad password
    [Tags]    connector    engine
    Ctn Clear Retention
    Ctn Config Broker    module    ${1}
    Ctn Config Engine    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_commands    trace
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    _USER    testconnssh
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    ssh_linux_snmp
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    address    127.0.0.11
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    _PASSWORD    titi
    Ctn Engine Config Add Command
    ...    ${0}
    ...    ssh_linux_snmp
    ...    $USER1$/check_by_ssh -H $HOSTADDRESS$ -l $_HOSTUSER$ -a $_HOSTPASSWORD$ -C "echo -n toto=$HOSTADDRESS$"
    ...    SSH Connector
    Ctn Start engine

    ${start}    Get Current Date
    ${content}    Create List    INITIAL SERVICE STATE: host_1;service_1;    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.

    ${start}    Get Current Date
    Ctn Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd

    ${content}    Create List    fail to connect to testconnssh@127.0.0.11
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message fail to connect to testconnssh@127.0.0.11 should be available.
    Ctn Stop engine

Test6Hosts
    [Documentation]    as 127.0.0.x point to the localhost address we will simulate check on 6 hosts
    [Tags]    connector    engine
    Sleep    5 seconds    we wait sshd raz pending connexions from previous tests
    Run    cat ~testconnssh/.ssh/id_rsa.pub ~root/.ssh/id_rsa.pub > ~testconnssh/.ssh/authorized_keys
    # Run    chown testconnssh: ~testconnssh/.ssh/authorized_keys
    # Run    chmod 600 ~testconnssh/.ssh/authorized_keys
    Ctn Clear Retention
    Ctn Config Broker    module    ${1}
    Ctn Config Engine    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_commands    trace
    Ctn Engine Config Add Command
    ...    ${0}
    ...    ssh_linux_snmp
    ...    $USER1$/check_by_ssh -H $HOSTADDRESS$ -l $_HOSTUSER$ -a $_HOSTPASSWORD$ -C "echo -n toto=$HOSTADDRESS$"
    ...    SSH Connector
    ${run_env}    Ctn Run Env
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    _USER    testconnssh
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    ssh_linux_snmp
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    _IDENTITYFILE    /home/testconnssh/.ssh/id_rsa
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    _PASSWORD    passwd
    IF    "${run_env}" == "docker"
        Ctn Engine Config Replace Value In Hosts    ${0}    host_1    address    127.0.0.1
    ELSE
        Ctn Engine Config Replace Value In Hosts    ${0}    host_1    address    ::1
    END
    FOR    ${idx}    IN RANGE    2    7
        Ctn Engine Config Set Value In Hosts    ${0}    host_${idx}    _USER    testconnssh
        Ctn Engine Config Replace Value In Hosts    ${0}    host_${idx}    check_command    ssh_linux_snmp
        Ctn Engine Config Replace Value In Hosts    ${0}    host_${idx}    address    127.0.0.${idx}
        Ctn Engine Config Set Value In Hosts    ${0}    host_${idx}    _IDENTITYFILE    /home/testconnssh/.ssh/id_rsa
        Ctn Engine Config Set Value In Hosts    ${0}    host_${idx}    _PASSWORD    passwd
    END
    Ctn Start engine

    ${start}    Get Current Date
    ${content}    Create List    INITIAL SERVICE STATE: host_1;service_1;    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.

    ${start}    Get Current Date
    FOR    ${idx}    IN RANGE    1    7
	Sleep    1s    We don't want to be too brutal with sshd
        Ctn Schedule Forced Host Check    host_${idx}    /tmp/var/lib/centreon-engine/config0/rw/centengine.cmd
    END

    IF    "${run_env}" == "docker"
        ${content}    Create List    output='toto=127.0.0.1'
        ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True    ${result}    A message output='toto=127.0.0.1' should be available.
    ELSE
        ${content}    Create List    output='toto=::1'
        ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True    ${result}    A message output='toto=::1' should be available.
    END

    FOR    ${idx}    IN RANGE    2    7
        ${content}    Create List    output='toto=127.0.0.${idx}
        ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True    ${result}    A message output='toto=127.0.0.${idx}' should be available.
    END

    Ctn Stop engine

TestWhiteList
    [Documentation]    as 127.0.0.x point to the localhost address we will simulate check on 6 hosts
    [Tags]    connector    engine
    Sleep    5 seconds    we wait sshd raz pending connexions from previous tests
    Run    cat ~testconnssh/.ssh/id_rsa.pub ~root/.ssh/id_rsa.pub > ~testconnssh/.ssh/authorized_keys
    # Run    chown testconnssh: ~testconnssh/.ssh/authorized_keys
    # Run    chmod 600 ~testconnssh/.ssh/authorized_keys
    Ctn Clear Retention
    Ctn Config Broker    module    ${1}
    Ctn Config Engine    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_commands    trace
    Ctn Engine Config Add Command
    ...    ${0}
    ...    ssh_linux_snmp
    ...    $USER1$/check_by_ssh -H $HOSTADDRESS$ -l $_HOSTUSER$ -a $_HOSTPASSWORD$ -C "echo -n toto=$HOSTADDRESS$"
    ...    SSH Connector
    ${run_env}    Ctn Run Env
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    _USER    testconnssh
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    ssh_linux_snmp
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    _IDENTITYFILE    /home/testconnssh/.ssh/id_rsa
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    _PASSWORD    passwd
    IF    "${run_env}" == "docker"
        Ctn Engine Config Replace Value In Hosts    ${0}    host_1    address    127.0.0.1
    ELSE
        Ctn Engine Config Replace Value In Hosts    ${0}    host_1    address    ::1
    END
    Create Directory    /etc/centreon-engine-whitelist
    ${whitelist_content}    Catenate
    ...    {"whitelist":{"regex":["/tmp/var/lib/centreon-engine/check.pl [1-9] 1.0.0.0"]}}
    Create File    /etc/centreon-engine-whitelist/test    ${whitelist_content}

    Ctn Start engine

    ${start}    Get Current Date
    ${content}    Create List    INITIAL SERVICE STATE: host_1;service_1;    check_for_external_commands()
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True
    ...    ${result}
    ...    An Initial host state on host_1 should be raised before we can start our external commands.

    # ssh_linux_snmp forbidden
    ${start}    Get Current Date
    Ctn Schedule Forced Host Check    host_1

    ${content}    Create List
    ...    host_1: this command cannot be executed because of security restrictions on the poller. A whitelist has been defined, and it does not include this command.
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message 'command rejected by whitelis' should be available.

    # ssh_linuw allowed
    ${whitelist_content}    Catenate
    ...    {"whitelist":{"regex":["/usr/lib64/nagios/plugins/check_by_ssh .+"]}}
    Create File    /etc/centreon-engine-whitelist/test2    ${whitelist_content}
    Ctn Reload Engine
    ${start}    Get Current Date
    Ctn Schedule Forced Host Check    host_1

    ${content}    Create List    'toto=127.0.0.1'
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60

    IF    "${run_env}" == "docker"
        ${content}    Create List    'toto=127.0.0.1'
        ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True    ${result}    A message 'toto=127.0.0.1' should be available.
    ELSE
        ${content}    Create List    'toto=::1'
        ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True    ${result}    A message 'toto=::1' should be available.
    END

    Ctn Stop engine


*** Keywords ***
Ctn Prepare ssh
    [Documentation]    in order to test ssh connector, we need to create a user, his password and his Keyword
    Run    useradd -m -d /home/testconnssh testconnssh
    Remove File    ~testconnssh/.ssh/authorized_keys
    Remove File    ~testconnssh/.ssh/id_rsa
    Remove File    ~testconnssh/.ssh/id_rsa.pub
    Remove File    ~/.ssh/id_rsa
    Remove File    ~/.ssh/id_rsa.pub
    Run    echo testconnssh:passwd | chpasswd
    Run    su testconnssh -c "ssh-keygen -q -t rsa -N '' -f ~testconnssh/.ssh/id_rsa"
    Run    ssh-keygen -q -t rsa -N '' -f ~/.ssh/id_rsa
    Ctn Clean Before Suite

Ctn Save SSH Logs If Failed
    Run Keyword If Test Failed    Ctn Save SSH Logs

Ctn Save SSH Logs
    Ctn Save Logs
    ${failDir}    Catenate    SEPARATOR=    failed/    ${Test Name}
    Copy File    ${ENGINE_LOG}/config0/connector_ssh.log    ${failDir}

Ctn Clean Whitelist
    Ctn Clean After Suite
    Remove File    /etc/centreon-engine-whitelist/test
