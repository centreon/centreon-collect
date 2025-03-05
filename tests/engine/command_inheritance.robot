*** Settings ***
Documentation       Centreon Engine verify command inheritance.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
ECMI0
    [Documentation]    Verify command inheritance : command(empty) inherit from template (full) , on Start Engine
    [Tags]    engine    command    MON-152874
    Ctn Config Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

    Ctn Clear Retention

    # Create files :
    Ctn Create Template File    ${0}    command    command_name    ["template_cmd_name"]

    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    commandTemplates.cfg

    # Delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    command_template_1    active_checks_enabled    commandTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    command_template_1    passive_checks_enabled    commandTemplates.cfg

    # Operation in commandTemplates
    Ctn Engine Config Set Key Value In Cfg     0    command_template_1    command_line    /usr/bin/true    commandTemplates.cfg
    
    Ctn Engine Config Set Key Value In Cfg     0    command_1    use    command_template_1    commands.cfg
    Ctn Engine Config Delete Key In Cfg    0    command_1    command_line    commands.cfg

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Command Info Grpc    command_1


    Should Be Equal As Strings     ${output}[commandName]    command_1
    Should Be Equal As Strings     ${output}[commandLine]    /usr/bin/true

    Ctn Stop Engine
    Ctn Kindly Stop Broker

ECMI1
    [Documentation]    Verify command inheritance : command(full) inherit from template (full) , on Start Engine
    [Tags]    engine    command    MON-152874
    Ctn Config Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

    Ctn Clear Retention

    # Create files :
    Ctn Create Template File    ${0}    command    command_name    ["template_cmd_name"]

    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    commandTemplates.cfg

    # Delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    command_template_1    active_checks_enabled    commandTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    command_template_1    passive_checks_enabled    commandTemplates.cfg

    # Operation in commandTemplates
    Ctn Engine Config Set Key Value In Cfg     0    command_template_1    command_line    /usr/bin/true    commandTemplates.cfg
    
    Ctn Engine Config Set Key Value In Cfg     0    command_1    use    command_template_1    commands.cfg

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Command Info Grpc    command_1


    Should Be Equal As Strings     ${output}[commandName]    command_1
    Should Be Equal As Strings     ${output}[commandLine]    /tmp/var/lib/centreon-engine/check.pl --id 1

    Ctn Stop Engine
    Ctn Kindly Stop Broker

ECMI2
    [Documentation]    Verify command inheritance : command(empty) inherit from template (full) , on Reload Engine
    [Tags]    engine    command    MON-152874
    Ctn Config Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

    Ctn Clear Retention
    
    ${start}    Get Current Date
    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # Create files :
    Ctn Create Template File    ${0}    command    command_name    ["template_cmd_name"]

    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    commandTemplates.cfg

    # Delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    command_template_1    active_checks_enabled    commandTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    command_template_1    passive_checks_enabled    commandTemplates.cfg

    # Operation in commandTemplates
    Ctn Engine Config Set Key Value In Cfg     0    command_template_1    command_line    /usr/bin/true    commandTemplates.cfg
    
    Ctn Engine Config Set Key Value In Cfg     0    command_1    use    command_template_1    commands.cfg
    Ctn Engine Config Delete Key In Cfg    0    command_1    command_line    commands.cfg

    ${start}    Get Current Date
    Ctn Reload Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    ${content}    Create List    Reload configuration finished
    ${result}    Ctn Find In Log With Timeout
    ...    ${ENGINE_LOG}/config0/centengine.log
    ...    ${start}
    ...    ${content}
    ...    60
    ...    verbose=False
    Should Be True    ${result}    Engine is Not Ready after 60s!!

    ${output}    Ctn Get Command Info Grpc    command_1


    Should Be Equal As Strings     ${output}[commandName]    command_1
    Should Be Equal As Strings     ${output}[commandLine]    /usr/bin/true

    Ctn Stop Engine
    Ctn Kindly Stop Broker

ECMI3
    [Documentation]    Verify command inheritance : command(full) inherit from template (full) , on reload Engine
    [Tags]    engine    command    MON-152874
    Ctn Config Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

    Ctn Clear Retention
    
    ${start}    Get Current Date
    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # Create files :
    Ctn Create Template File    ${0}    command    command_name    ["template_cmd_name"]

    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    commandTemplates.cfg

    # Delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    command_template_1    active_checks_enabled    commandTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    command_template_1    passive_checks_enabled    commandTemplates.cfg

    # Operation in commandTemplates
    Ctn Engine Config Set Key Value In Cfg     0    command_template_1    command_line    /usr/bin/true    commandTemplates.cfg
    
    Ctn Engine Config Set Key Value In Cfg     0    command_1    use    command_template_1    commands.cfg

    ${start}    Get Current Date
    Ctn Reload Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    ${content}    Create List    Reload configuration finished
    ${result}    Ctn Find In Log With Timeout
    ...    ${ENGINE_LOG}/config0/centengine.log
    ...    ${start}
    ...    ${content}
    ...    60
    ...    verbose=False
    Should Be True    ${result}    Engine is Not Ready after 60s!!

    ${output}    Ctn Get Command Info Grpc    command_1


    Should Be Equal As Strings     ${output}[commandName]    command_1
    Should Be Equal As Strings     ${output}[commandLine]    /tmp/var/lib/centreon-engine/check.pl --id 1

    Ctn Stop Engine
    Ctn Kindly Stop Broker