*** Settings ***
Documentation       Centreon Broker and Engine benchmark

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean Whitelist
Test Setup          Ctn Whitelist Setup
Test Teardown       Ctn Stop Engine Broker And Save Logs    only_central=${True}


*** Test Cases ***
Whitelist_No_Whitelist_Directory
    [Documentation]    log if /etc/centreon-engine-whitelist doesn't exist
    [Tags]    whitelist    engine
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    module    ${1}
    Remove Directory    /etc/centreon-engine-whitelist    recursive=${True}
    ${start}    Get Current Date
    Ctn Start engine
    ${content}    Create List
    ...    no whitelist directory found, all commands are accepted
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    no whitelist directory found must be found in logs

Whitelist_Empty_Directory
    [Documentation]    log if /etc/centreon-engine-whitelist is empty
    [Tags]    whitelist    engine
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    module    ${1}
    Empty Directory    /etc/centreon-engine-whitelist
    ${start}    Get Current Date
    Ctn Start engine
    ${content}    Create List
    ...    whitelist directory found, but no restrictions, all commands are accepted
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    all commands are accepted must be found in logs

Whitelist_Directory_Rights
    [Documentation]    log if /etc/centreon-engine-whitelist has not mandatory rights or owner
    [Tags]    whitelist    engine
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    module    ${1}
    Run    chown root:root /etc/centreon-engine-whitelist
    ${start}    Ctn Get Round Current Date
    Ctn Start engine
    ${content}    Create List
    ...    directory /etc/centreon-engine-whitelist must be owned by root@centreon-engine
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    owned by root@centreon-engine must be found in logs

    ${start}    Get Current Date
    Run    chown root:centreon-engine /etc/centreon-engine-whitelist
    Run    chmod 0777 /etc/centreon-engine-whitelist
    Ctn Reload Engine
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Not Be True    ${result}    owned by root@centreon-engine must not be found in logs
    ${content}    Create List    directory /etc/centreon-engine-whitelist must have 750 right access
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    must have 750 right access must be found in logs

    ${start}    Get Current Date
    Run    chmod 0750 /etc/centreon-engine-whitelist
    Ctn Reload Engine
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Not Be True    ${result}    must have 750 right access must not be found in logs

Whitelist_Host
    [Documentation]    test allowed and forbidden commands for hosts
    [Tags]    whitelist    engine
    Ctn Config Engine    ${1}    ${50}    ${20}
    Empty Directory    /etc/centreon-engine-whitelist
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Engine Config Set Value    0    log_level_checks    trace    True
    Ctn Engine Config Set Value    0    log_level_commands    trace    True
    Ctn Engine Config Change Command    0    1    /tmp/var/lib/centreon-engine/check.pl 0 $HOSTADDRESS$
    Ctn Engine Config Replace Value In Hosts    0    host_1    check_command    command_1

    ${start}    Get Current Date
    Ctn Start Broker    only_central=${True}
    Ctn Start engine
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    # no file => no restriction
    ${start}    Get Current Date
    Ctn Schedule Forced Host Check    host_1
    ${content}    Create List    raw_v2::run: cmd='/tmp/var/lib/centreon-engine/check.pl 0 1.0.0.0'
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check result found for host_1

    # create non matching file with /tmp/var/lib/centreon-engine/check.pl 0 1.0.0.0
    ${whitelist_content}    Catenate
    ...    {"whitelist":{"wildcard":["/tmp/var/lib/centreon-engine/toto* * *"], "regex":["/tmp/var/lib/centreon-engine/check.pl [1-9] 1.0.0.0"]}}
    Create File    /etc/centreon-engine-whitelist/test    ${whitelist_content}
    Ctn Reload Engine
    ${start}    Get Current Date
    Ctn Schedule Forced Host Check    host_1
    ${content}    Create List
    ...    host_1: this command cannot be executed because of security restrictions on the poller. A whitelist has been defined, and it does not include this command.
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No command not allowed found for host_1

    # matching with /tmp/var/lib/centreon-engine/check.pl [1-9] 1.0.0.0"]
    Ctn Engine Config Change Command    0    1    /tmp/var/lib/centreon-engine/check.pl 1 $HOSTADDRESS$
    Ctn Reload Engine
    ${start}    Get Current Date
    Ctn Schedule Forced Host Check    host_1
    ${content}    Create List    raw_v2::run: cmd='/tmp/var/lib/centreon-engine/check.pl 1 1.0.0.0'
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    /tmp/var/lib/centreon-engine/check.pl 1 not run

    # matching with /tmp/var/lib/centreon-engine/toto* * */etc/centreon-engine-whitelist/test
    Ctn Engine Config Change Command    0    1    /tmp/var/lib/centreon-engine/totozea 1 $HOSTADDRESS$
    Ctn Reload Engine
    ${start}    Get Current Date
    Ctn Schedule Forced Host Check    host_1
    ${content}    Create List    raw_v2::run: cmd='/tmp/var/lib/centreon-engine/totozea 1 1.0.0.0'
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    totozea not found

Whitelist_Service_EH
    [Documentation]    test allowed and forbidden event handler for services
    [Tags]    whitelist    engine    MON-103880
    Ctn Config Engine    ${1}    ${50}    ${20}
    Empty Directory    /etc/centreon-engine-whitelist
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Engine Config Set Value    0    log_level_checks    trace    True
    Ctn Engine Config Set Value    0    log_level_commands    trace    True
    # service_1 uses command_1
    Ctn Engine Config Replace Value In Services    0    service_1    check_command    command_2
    Ctn Engine Config Change Command    0    1    /tmp/var/lib/centreon-engine/totozea 0 $HOSTADDRESS$
    Ctn Engine Config Set Value In Services    0    service_1    event_handler_enabled    1
    # command_1 poits to the command totozea that is not allowed.
    Ctn Engine Config Set Value In Services    0    service_1    event_handler    command_1

    # create non matching file
    ${whitelist_content}    Catenate
    ...    {"whitelist":{"wildcard":["/tmp/var/lib/centreon-engine/check.pl * *"], "regex":["/tmp/var/lib/centreon-engine/check.pl [1-9] .*"]}}
    Create File    /etc/centreon-engine-whitelist/test    ${whitelist_content}
    ${start}    Get Current Date
    Ctn Start Broker    only_central=${True}
    Ctn Start engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    ${cmd}    Ctn Get Service Command Id    1
    Ctn Set Command Status    ${cmd}    0
    Ctn Process Service Result Hard    host_1    service_1    0    output OK
    #Repeat Keyword    3 times    Ctn Schedule Forced Svc Check    host_1    service_1
    Ctn Set Command Status    ${cmd}    2
    Ctn Process Service Result Hard    host_1    service_1    2    output CRITICAL
    #Repeat Keyword    3 times    Ctn Schedule Forced Svc Check    host_1    service_1
    ${content}    Create List
    ...    Error: can't execute service event handler command line '/tmp/var/lib/centreon-engine/totozea 0 1.0.0.0' : it is not allowed by the whitelist
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    The event handler for service_1 should be forbidden to execute

    # Now, we allow totozea as event handler.
    ${whitelist_content}    Catenate
    ...    {"whitelist":{"wildcard":["/tmp/var/lib/centreon-engine/totozea * *"], "regex":["/tmp/var/lib/centreon-engine/check.pl [1-9] .*"]}}
    Create File    /etc/centreon-engine-whitelist/test    ${whitelist_content}
    Ctn Engine Config Change Command    0    1    /tmp/var/lib/centreon-engine/check.pl 1 $HOSTADDRESS$
    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    ${cmd}    Ctn Get Service Command Id    1
    Ctn Set Command Status    ${cmd}    0
    Ctn Process Service Result Hard    host_1    service_1    0    output OK
    #Repeat Keyword    3 times    Ctn Schedule Forced Svc Check    host_1    service_1
    Ctn Set Command Status    ${cmd}    2
    Ctn Process Service Result Hard    host_1    service_1    2    output CRITICAL
    #Repeat Keyword    3 times    Ctn Schedule Forced Svc Check    host_1    service_1
    ${content}    Create List    SERVICE EVENT HANDLER: host_1;service_1;.*;command_1    my_system_r
    ${result}    Ctn Find Regex In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result[0]}    The event handler with command totozea should be OK.

Whitelist_Service
    [Documentation]    test allowed and forbidden commands for services
    [Tags]    whitelist    engine
    Ctn Config Engine    ${1}    ${50}    ${20}
    Empty Directory    /etc/centreon-engine-whitelist
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Engine Config Set Value    0    log_level_checks    trace    True
    Ctn Engine Config Set Value    0    log_level_commands    trace    True
    # service_1 uses command_1
    Ctn Engine Config Replace Value In Services    0    service_1    check_command    command_1
    Ctn Engine Config Change Command    0    1    /tmp/var/lib/centreon-engine/check.pl 0 $HOSTADDRESS$

    ${start}    Get Current Date
    Ctn Start Broker    only_central=${True}
    Ctn Start engine
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    # no file => no restriction
    ${start}    Get Current Date
    Ctn Schedule Forced Svc Check    host_1    service_1
    ${content}    Create List    raw_v2::run: cmd='/tmp/var/lib/centreon-engine/check.pl 0 1.0.0.0'
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check result found for service_1

    # create non matching file with /tmp/var/lib/centreon-engine/check.pl 0 1.0.0.0
    ${whitelist_content}    Catenate
    ...    {"whitelist":{"wildcard":["/tmp/var/lib/centreon-engine/toto* * *"], "regex":["/tmp/var/lib/centreon-engine/check.pl [1-9] 1.0.0.0"]}}
    Create File    /etc/centreon-engine-whitelist/test    ${whitelist_content}
    Ctn Reload Engine
    ${start}    Get Current Date
    Ctn Schedule Forced Svc Check    host_1    service_1
    ${content}    Create List
    ...    service_1: this command cannot be executed because of security restrictions on the poller. A whitelist has been defined, and it does not include this command.
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No command not allowed found for service_1

    # matching with /tmp/var/lib/centreon-engine/check.pl [1-9] 1.0.0.0"]
    Ctn Engine Config Change Command    0    1    /tmp/var/lib/centreon-engine/check.pl 1 $HOSTADDRESS$
    Ctn Reload Engine
    ${start}    Get Current Date
    Ctn Schedule Forced Svc Check    host_1    service_1
    ${content}    Create List    raw_v2::run: cmd='/tmp/var/lib/centreon-engine/check.pl 1 1.0.0.0'
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    /tmp/var/lib/centreon-engine/check.pl 1 not run

    # matching with /tmp/var/lib/centreon-engine/toto* * *
    Ctn Engine Config Change Command    0    1    /tmp/var/lib/centreon-engine/totozea 1 $HOSTADDRESS$
    Ctn Reload Engine
    ${start}    Get Current Date
    Ctn Schedule Forced Svc Check    host_1    service_1
    ${content}    Create List    raw_v2::run: cmd='/tmp/var/lib/centreon-engine/totozea 1 1.0.0.0'
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    totozea not found

Whitelist_Perl_Connector
    [Documentation]    test allowed and forbidden commands for services
    [Tags]    whitelist    engine    connector
    Ctn Config Engine    ${1}    ${50}    ${20}
    Empty Directory    /etc/centreon-engine-whitelist
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Engine Config Set Value    0    log_level_checks    trace    True
    Ctn Engine Config Set Value    0    log_level_commands    trace    True
    # service_1 uses command_14 (uses perl connector)
    Ctn Engine Config Replace Value In Services    0    service_1    check_command    command_14
    Ctn Engine Config Change Command    0    14    /tmp/var/lib/centreon-engine/check.pl 0 $HOSTADDRESS$

    ${whitelist_content}    Catenate
    ...    {"whitelist":{"wildcard":["/tmp/var/lib/centreon-engine/toto* * *"], "regex":["/tmp/var/lib/centreon-engine/check.pl [1-9] 1.0.0.0"]}}
    Create File    /etc/centreon-engine-whitelist/test    ${whitelist_content}

    ${start}    Get Current Date
    Ctn Start Broker    only_central=${True}
    Ctn Start engine
    ${content}    Create List    check_for_external_commands
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No check for external commands executed for 1mn.

    # command not allowed because of 0 in first argument
    ${start}    Get Current Date
    Ctn Schedule Forced Svc Check    host_1    service_1
    ${content}    Create List
    ...    service_1: this command cannot be executed because of security restrictions on the poller. A whitelist has been defined, and it does not include this command.
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    No command not allowed found for service_1

    # command allowed by whitelist
    Ctn Engine Config Change Command    0    14    /tmp/var/lib/centreon-engine/check.pl 1 $HOSTADDRESS$
    Ctn Reload Engine
    ${start}    Get Current Date
    Ctn Schedule Forced Svc Check    host_1    service_1
    ${content}    Create List
    ...    connector::run: connector='Perl Connector', cmd='/tmp/var/lib/centreon-engine/check.pl 1 1.0.0.0'
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    /tmp/var/lib/centreon-engine/check.pl 1 1.0.0.0 not found


*** Keywords ***
Ctn Whitelist Setup
    Create Directory    /etc/centreon-engine-whitelist
    Ctn Stop Processes

Ctn Clean Whitelist
    Ctn Clean After Suite
    Remove File    /etc/centreon-engine-whitelist/test
