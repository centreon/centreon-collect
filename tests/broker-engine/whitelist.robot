*** Settings ***
Documentation       Centreon Broker and Engine benchmark

Resource            ../resources/resources.robot
Library             DateTime
Library             Process
Library             OperatingSystem
Library             Examples
Library             ../resources/Engine.py
Library             ../resources/Broker.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Whitelist Setup
Test Teardown       Stop Engine Broker And Save Logs    only_central=${True}


*** Test Cases ***
Whitelist_No_Whitelist_Directory
    [Documentation]    log if /etc/centreon-engine-whitelist doesn't exist
    [Tags]    whitelist    engine
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    module    ${1}
    Remove Directory    /etc/centreon-engine-whitelist    recursive=${True}
    ${start}    Get Current Date
    Start Engine
    ${content}    Create List
    ...    /etc/centreon-engine-whitelist: no whitelist directory found, all commands are accepted
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    no whitelist directory found must be found in logs

Whitelist_Empty_Directory
    [Documentation]    log if /etc/centreon-engine-whitelist if empty
    [Tags]    whitelist    engine
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    module    ${1}
    Empty Directory    /etc/centreon-engine-whitelist
    ${start}    Get Current Date
    Start Engine
    ${content}    Create List
    ...    /etc/centreon-engine-whitelist: whitelist directory found, but no restrictions, all commands are accepted
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    all commands are accepted must be found in logs

Whitelist_Directory_Rights
    [Documentation]    log if /etc/centreon-engine-whitelist has not mandatory rights or owner
    [Tags]    whitelist    engine
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    module    ${1}
    Run    chown root:root /etc/centreon-engine-whitelist
    ${start}    Get Current Date
    Start Engine
    ${content}    Create List
    ...    directory /etc/centreon-engine-whitelist must be owned by root@centron_engine
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    owned by root@centron_engine must be found in logs

    ${start}    Get Current Date
    Run    chown root:centreon-engine /etc/centreon-engine-whitelist
    Run    chmod 0777 /etc/centreon-engine-whitelist
    Reload Engine
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Not Be True    ${result}    owned by root@centron_engine must not be found in logs
    ${content}    Create List    directory /etc/centreon-engine-whitelist must have 750 right access
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    must have 750 right access must be found in logs

    ${start}    Get Current Date
    Run    chmod 0750 /etc/centreon-engine-whitelist
    Reload Engine
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be False    ${result}    must have 750 right access must not be found in logs

Whitelist_Host
    [Documentation]    test allowed and forbidden commands for hosts
    [Tags]    whitelist    engine
    Config Engine    ${1}    ${50}    ${20}
    Empty Directory    /etc/centreon-engine-whitelist
    Config Broker    central
    Config Broker    module    ${1}
    Engine Config Set Value    0    log_level_checks    trace    True
    Engine Config Set Value    0    log_level_commands    trace    True
    Engine Config Change Command    0    1    /tmp/var/lib/centreon-engine/check.pl 0 $HOSTADDRESS$
    Engine Config Replace Value In Hosts    0    host_1    check_command    command_1

    ${start}    Get Current Date
    Start Broker    only_central=${True}
    Start Engine
    ${content}    Create List    check_for_external_commands
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    # no file => no restriction
    ${start}    Get Current Date
    Schedule Forced Host Check    host_1
    ${content}    Create List    raw::run: cmd='/tmp/var/lib/centreon-engine/check.pl 0 1.0.0.0'
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check result found for host_1

    # create non matching file with /tmp/var/lib/centreon-engine/check.pl 0 1.0.0.0
    ${whitelist_content}    Catenate
    ...    {"whitelist":{"wildcard":["/tmp/var/lib/centreon-engine/toto* * *"], "regex":["/tmp/var/lib/centreon-engine/check.pl [1-9] 1.0.0.0"]}}
    Create File    /etc/centreon-engine-whitelist/test    ${whitelist_content}
    Reload Engine
    ${start}    Get Current Date
    Schedule Forced Host Check    host_1
    ${content}    Create List
    ...    host_1: command not allowed by whitelist /tmp/var/lib/centreon-engine/check.pl 0 1.0.0.0
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No command not allowed found for host_1

    # matching with /tmp/var/lib/centreon-engine/check.pl [1-9] 1.0.0.0"]
    Engine Config Change Command    0    1    /tmp/var/lib/centreon-engine/check.pl 1 $HOSTADDRESS$
    Reload Engine
    ${start}    Get Current Date
    Schedule Forced Host Check    host_1
    ${content}    Create List    raw::run: cmd='/tmp/var/lib/centreon-engine/check.pl 1 1.0.0.0'
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    /tmp/var/lib/centreon-engine/check.pl 1 not run

    # matching with /tmp/var/lib/centreon-engine/toto* * *
    Engine Config Change Command    0    1    /tmp/var/lib/centreon-engine/totozea 1 $HOSTADDRESS$
    Reload Engine
    ${start}    Get Current Date
    Schedule Forced Host Check    host_1
    ${content}    Create List    raw::run: cmd='/tmp/var/lib/centreon-engine/totozea 1 1.0.0.0'
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    totozea not found

Whitelist_Service
    [Documentation]    test allowed and forbidden commands for services
    [Tags]    whitelist    engine
    Config Engine    ${1}    ${50}    ${20}
    Empty Directory    /etc/centreon-engine-whitelist
    Config Broker    central
    Config Broker    module    ${1}
    Engine Config Set Value    0    log_level_checks    trace    True
    Engine Config Set Value    0    log_level_commands    trace    True
    # service_1 uses command_1
    Engine Config Replace Value In Services    0    service_1    check_command    command_1
    Engine Config Change Command    0    1    /tmp/var/lib/centreon-engine/check.pl 0 $HOSTADDRESS$

    ${start}    Get Current Date
    Start Broker    only_central=${True}
    Start Engine
    ${content}    Create List    check_for_external_commands
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    # no file => no restriction
    ${start}    Get Current Date
    Schedule Forced Svc Check    host_1    service_1
    ${content}    Create List    raw::run: cmd='/tmp/var/lib/centreon-engine/check.pl 0 1.0.0.0'
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check result found for service_1

    # create non matching file with /tmp/var/lib/centreon-engine/check.pl 0 1.0.0.0
    ${whitelist_content}    Catenate
    ...    {"whitelist":{"wildcard":["/tmp/var/lib/centreon-engine/toto* * *"], "regex":["/tmp/var/lib/centreon-engine/check.pl [1-9] 1.0.0.0"]}}
    Create File    /etc/centreon-engine-whitelist/test    ${whitelist_content}
    Reload Engine
    ${start}    Get Current Date
    Schedule Forced Svc Check    host_1    service_1
    ${content}    Create List
    ...    service_1: command not allowed by whitelist /tmp/var/lib/centreon-engine/check.pl 0 1.0.0.0
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No command not allowed found for service_1

    # matching with /tmp/var/lib/centreon-engine/check.pl [1-9] 1.0.0.0"]
    Engine Config Change Command    0    1    /tmp/var/lib/centreon-engine/check.pl 1 $HOSTADDRESS$
    Reload Engine
    ${start}    Get Current Date
    Schedule Forced Svc Check    host_1    service_1
    ${content}    Create List    raw::run: cmd='/tmp/var/lib/centreon-engine/check.pl 1 1.0.0.0'
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    /tmp/var/lib/centreon-engine/check.pl 1 not run

    # matching with /tmp/var/lib/centreon-engine/toto* * *
    Engine Config Change Command    0    1    /tmp/var/lib/centreon-engine/totozea 1 $HOSTADDRESS$
    Reload Engine
    ${start}    Get Current Date
    Schedule Forced Svc Check    host_1    service_1
    ${content}    Create List    raw::run: cmd='/tmp/var/lib/centreon-engine/totozea 1 1.0.0.0'
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    totozea not found

Whitelist_Perl_Connector
    [Documentation]    test allowed and forbidden commands for services
    [Tags]    whitelist    engine    connector
    Config Engine    ${1}    ${50}    ${20}
    Empty Directory    /etc/centreon-engine-whitelist
    Config Broker    central
    Config Broker    module    ${1}
    Engine Config Set Value    0    log_level_checks    trace    True
    Engine Config Set Value    0    log_level_commands    trace    True
    # service_1 uses command_14 (uses perl connector)
    Engine Config Replace Value In Services    0    service_1    check_command    command_14
    Engine Config Change Command    0    14    /tmp/var/lib/centreon-engine/check.pl 0 $HOSTADDRESS$

    ${whitelist_content}    Catenate
    ...    {"whitelist":{"wildcard":["/tmp/var/lib/centreon-engine/toto* * *"], "regex":["/tmp/var/lib/centreon-engine/check.pl [1-9] 1.0.0.0"]}}
    Create File    /etc/centreon-engine-whitelist/test    ${whitelist_content}

    ${start}    Get Current Date
    Start Broker    only_central=${True}
    Start Engine
    ${content}    Create List    check_for_external_commands
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    # command not allowed because of 0 in first argument
    ${start}    Get Current Date
    Schedule Forced Svc Check    host_1    service_1
    ${content}    Create List
    ...    service_1: command not allowed by whitelist /tmp/var/lib/centreon-engine/check.pl 0 1.0.0.0
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No command not allowed found for service_1

    # command allowed by whitelist
    Engine Config Change Command    0    14    /tmp/var/lib/centreon-engine/check.pl 1 $HOSTADDRESS$
    Reload Engine
    ${start}    Get Current Date
    Schedule Forced Svc Check    host_1    service_1
    ${content}    Create List
    ...    connector::run: connector='Perl Connector', cmd='/tmp/var/lib/centreon-engine/check.pl 1 1.0.0.0'
    ${result}    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    /tmp/var/lib/centreon-engine/check.pl 1 1.0.0.0 not found


*** Keywords ***
Whitelist Setup
    Create Directory    /etc/centreon-engine-whitelist
    Stop Processes
