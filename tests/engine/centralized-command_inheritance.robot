*** Settings ***
Documentation       Centreon Engine verify command inheritance with centralized configuration.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
CECMI0
    [Documentation]    Given a centralized Engine configuration with a command template having a command_line
    ...    And the command inherits from the template with its own command_line deleted
    ...    When Engine and Broker are started
    ...    Then the command's resolved command_line matches the template value
    [Tags]    engine    command    MON-152874
    Ctn Config Centralized Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module

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

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Command Info Grpc    command_1


    Should Be Equal As Strings     ${output}[commandName]    command_1
    Should Be Equal As Strings     ${output}[commandLine]    /usr/bin/true

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CECMI1
    [Documentation]    Given a centralized Engine configuration with a command template having a command_line
    ...    And the command inherits from the template but keeps its own command_line
    ...    When Engine and Broker are started
    ...    Then the command's resolved command_line is the command's own value, not the template's
    [Tags]    engine    command    MON-152874
    Ctn Config Centralized Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module

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

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Command Info Grpc    command_1


    Should Be Equal As Strings     ${output}[commandName]    command_1
    Should Be Equal As Strings     ${output}[commandLine]    /tmp/var/lib/centreon-engine/check.pl --id 1

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CECMI2
    [Documentation]    Given a centralized Engine already started with a basic configuration
    ...    And a command template with a command_line is added with the command inheriting from it and its own command_line deleted
    ...    When the new configuration is sent via Broker notification
    ...    Then the command's resolved command_line matches the template value
    [Tags]    engine    command    MON-152874
    Ctn Config Centralized Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    bbdo    info

    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Clear Retention
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
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

    # Sending the new configuration to Engine.
    ${start}    Ctn Get Round Current Date
    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    ${output}    Ctn Get Command Info Grpc    command_1


    Should Be Equal As Strings     ${output}[commandName]    command_1
    Should Be Equal As Strings     ${output}[commandLine]    /usr/bin/true

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CECMI3
    [Documentation]    Given a centralized Engine already started with a basic configuration
    ...    And a command template with a command_line is added with the command inheriting from it while keeping its own command_line
    ...    When the new configuration is sent via Broker notification
    ...    Then the command's resolved command_line is the command's own value, not the template's
    [Tags]    engine    command    MON-152874
    Ctn Config Centralized Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    bbdo    info

    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Clear Retention
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
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

    # Sending the new configuration to Engine.
    ${start}    Ctn Get Round Current Date
    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    ${output}    Ctn Get Command Info Grpc    command_1


    Should Be Equal As Strings     ${output}[commandName]    command_1
    Should Be Equal As Strings     ${output}[commandLine]    /tmp/var/lib/centreon-engine/check.pl --id 1

    Ctn Stop Engine
    Ctn Kindly Stop Broker
