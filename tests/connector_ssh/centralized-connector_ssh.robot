*** Settings ***
Documentation       centreon_connector_ssh tests with centralized configuration.

Resource            ../resources/import.resource

Suite Setup         Ctn Prepare ssh
Suite Teardown      Ctn Clean Whitelist
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save SSH Logs If Failed
Test Tags           connector    engine    bbdo


*** Test Cases ***
CTestBadUser
    [Documentation]    Scenario: SSH check with unknown user fails in centralized configuration
    ...    Given a centralized engine and broker configuration with an unknown SSH user on host_1
    ...    When a forced host check is scheduled
    ...    Then a connection failure message for the unknown user should appear in the log.
    Ctn Clear Retention
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    bbdo    info
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
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine Configuration To Be Applied    ${start}    0

    Ctn Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd

    VAR    @{content}    fail to connect to toto@127.0.0.10
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message fail to connect to toto@127.0.0.10 should be available.
    Ctn Stop Engine
    Ctn Kindly Stop Broker

CTestBadPwd
    [Documentation]    Scenario: SSH check with wrong password fails in centralized configuration
    ...    Given a centralized engine and broker configuration with a wrong SSH password on host_1
    ...    When a forced host check is scheduled
    ...    Then a connection failure message for the bad password should appear in the log.
    Ctn Clear Retention
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    bbdo    info
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
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine Configuration To Be Applied    ${start}    0

    Ctn Schedule Forced Host Check    host_1    ${VarRoot}/lib/centreon-engine/config0/rw/centengine.cmd

    VAR    @{content}    fail to connect to testconnssh@127.0.0.11
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message fail to connect to testconnssh@127.0.0.11 should be available.
    Ctn Stop Engine
    Ctn Kindly Stop Broker

CTest6Hosts
    [Documentation]    Scenario: SSH checks succeed on 6 hosts in centralized configuration
    ...    Given a centralized engine and broker configuration with 6 hosts reachable via SSH
    ...    When forced checks are scheduled on all 6 hosts
    ...    Then the expected output for each host address should appear in the log.
    Run    cat ~testconnssh/.ssh/id_rsa.pub ~root/.ssh/id_rsa.pub > ~testconnssh/.ssh/authorized_keys
    Ctn Clear Logs
    Ctn Clear Retention
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    bbdo    info
    Ctn Engine Config Set Value    ${0}    log_level_commands    trace
    Ctn Engine Config Add Command
    ...    ${0}
    ...    ssh_linux_snmp
    ...    $USER1$/check_by_ssh -H $HOSTADDRESS$ -l $_HOSTUSER$ -a $_HOSTPASSWORD$ -C "echo -n foo=$HOSTADDRESS$"
    ...    SSH Connector
    ${run_env}    Ctn Run Env
    Log To Console    @@@@@@@@@@@@@@@@@@@@@ ${run_env} @@@@@@@@@@@@@@@@@@@@@
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    _USER    testconnssh
    Ctn Engine Config Replace Value In Hosts    ${0}    host_1    check_command    ssh_linux_snmp
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    _IDENTITYFILE    /home/testconnssh/.ssh/id_rsa
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    _PASSWORD    passwd
    IF    "${run_env}" == "docker" or "${run_env}" == "podman"
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
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine Configuration To Be Applied    ${start}    0

    FOR    ${idx}    IN RANGE    1    7
        Sleep    1s    We don't want to be too brutal with sshd
        Ctn Schedule Forced Host Check    host_${idx}    /tmp/var/lib/centreon-engine/config0/rw/centengine.cmd
    END

    IF    "${run_env}" == "docker" or "${run_env}" == "podman"
        VAR    @{content}    output='foo=127.0.0.1'
        ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True    ${result}    A message output='foo=127.0.0.1' should be available.
    ELSE
        VAR    @{content}    output='foo=::1'
        ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True    ${result}    A message output='foo=::1' should be available.
    END

    FOR    ${idx}    IN RANGE    2    7
        VAR    @{content}    output='foo=127.0.0.${idx}
        ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True    ${result}    A message output='foo=127.0.0.${idx}' should be available.
    END

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CTestWhiteList
    [Documentation]    Scenario: SSH check blocked then allowed by whitelist in centralized configuration
    ...    Given a centralized engine and broker configuration with a whitelist restricting SSH checks
    ...    When a forced host check is scheduled and the command is not whitelisted
    ...    Then a security restriction message should appear in the log.
    ...    When the whitelist is updated to allow the SSH command
    ...    Then the check should succeed and the expected output should appear in the log.
    Sleep    5 seconds    We wait sshd raz pending connexions from previous tests
    Run    cat ~testconnssh/.ssh/id_rsa.pub ~root/.ssh/id_rsa.pub > ~testconnssh/.ssh/authorized_keys
    Ctn Clear Retention
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    bbdo    info
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
    Remove Directory    /etc/centreon-engine-whitelist    recursive=True
    Create Directory    /etc/centreon-engine-whitelist
    VAR    ${whitelist_content}
    ...    {"whitelist":{"regex":["/tmp/var/lib/centreon-engine/check.pl [1-9] 1.0.0.0"]}}
    Create File    /etc/centreon-engine-whitelist/test    ${whitelist_content}
    Run    chown root:centreon-engine -R /etc/centreon-engine-whitelist
    Run    chmod 750 /etc/centreon-engine-whitelist

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine Configuration To Be Applied    ${start}    0

    # ssh_linux_snmp forbidden
    Ctn Schedule Forced Host Check    host_1

    VAR    @{content}
    ...    host_1: this command cannot be executed because of security restrictions on the poller. A whitelist has been defined, and it does not include this command.
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    A message 'command rejected by whitelist' should be available.

    # ssh_linux_snmp allowed
    VAR    ${whitelist_content}    {"whitelist":{"regex":["/usr/lib64/nagios/plugins/check_by_ssh .+"]}}
    Create File    /etc/centreon-engine-whitelist/test2    ${whitelist_content}
    Ctn Reload Engine
    ${start}    Ctn Get Round Current Date
    Log To Console    ${start}
    Ctn Schedule Forced Host Check    host_1

    IF    "${run_env}" == "docker"
        VAR    @{content}    toto=127.0.0.1
        ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True    ${result}    A message 'toto=127.0.0.1' should be available.
    ELSE
        VAR    @{content}    toto=::1
        ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
        Should Be True    ${result}    A message 'toto=::1' should be available.
    END

    Ctn Stop Engine
    Ctn Kindly Stop Broker


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
    Ctn Save Logs If Failed

Ctn Save SSH Logs
    Ctn Save Logs
    ${failDir}    Catenate    SEPARATOR=    failed/    ${Test Name}
    Copy File    ${ENGINE_LOG}/config0/connector_ssh.log    ${failDir}

Ctn Clean Whitelist
    Ctn Clean After Suite
    Remove File    /etc/centreon-engine-whitelist/test
