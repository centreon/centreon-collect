*** Settings ***
Documentation       Centreon Engine verify connector inheritance with centralized configuration.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
CECOI0
    [Documentation]    Given a centralized Engine configuration with a connector template having a connector_line
    ...    And the connector inherits from the template with its own connector_line deleted
    ...    When Engine and Broker are started
    ...    Then the connector's resolved connector_line matches the template value
    [Tags]    engine    connector    MON-152874
    Ctn Config Centralized Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module

    Ctn Clear Retention

    # Create files :
    Ctn Create Template File    ${0}    connector    connector_name    ["template_conn_name"]

    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    connectorTemplates.cfg

    # Delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    connector_template_1    active_checks_enabled    connectorTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    connector_template_1    passive_checks_enabled    connectorTemplates.cfg

    # Operation in commandTemplates
    Ctn Engine Config Set Key Value In Cfg     0    connector_template_1    connector_line    /usr/lib64/centreon-connector/centreon_connector_perl --debug --log-file=/tmp/var/log/centreon-engine/config0/connector_perl.log    connectorTemplates.cfg

    Ctn Engine Config Set Key Value In Cfg     0    Perl Connector    use    connector_template_1    connectors.cfg
    Ctn Engine Config Delete Key In Cfg    0    Perl Connector    connector_line    connectors.cfg

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Connector Info Grpc    Perl Connector


    Should Be Equal As Strings     ${output}[connectorName]    Perl Connector
    Should Be Equal As Strings     ${output}[connectorLine]    /usr/lib64/centreon-connector/centreon_connector_perl --debug --log-file=/tmp/var/log/centreon-engine/config0/connector_perl.log

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CECOI1
    [Documentation]    Given a centralized Engine configuration with a connector template having a connector_line
    ...    And the connector inherits from the template but keeps its own connector_line
    ...    When Engine and Broker are started
    ...    Then the connector's resolved connector_line is the connector's own value, not the template's
    [Tags]    engine    connector    MON-152874
    Ctn Config Centralized Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module

    Ctn Clear Retention

    # Create files :
    Ctn Create Template File    ${0}    connector    connector_name    ["template_conn_name"]

    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    connectorTemplates.cfg

    # Delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    connector_template_1    active_checks_enabled    connectorTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    connector_template_1    passive_checks_enabled    connectorTemplates.cfg

    # Operation in commandTemplates
    Ctn Engine Config Set Key Value In Cfg     0    connector_template_1    connector_line    /usr/lib64/centreon-connector/centreon_connector_ssh --debug --log-file=/tmp/var/log/centreon-engine/config0/connector_ssh.log    connectorTemplates.cfg

    Ctn Engine Config Set Key Value In Cfg     0    Perl Connector    use    connector_template_1    connectors.cfg

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Connector Info Grpc    Perl Connector


    Should Be Equal As Strings     ${output}[connectorName]    Perl Connector
    Should Be Equal As Strings     ${output}[connectorLine]    /usr/lib64/centreon-connector/centreon_connector_perl --debug --log-file=/tmp/var/log/centreon-engine/config0/connector_perl.log

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CECOI2
    [Documentation]    Given a centralized Engine already started with a basic configuration
    ...    And a connector template with a connector_line is added with the connector inheriting from it and its own connector_line deleted
    ...    When the new configuration is sent via Broker notification
    ...    Then the connector's resolved connector_line matches the template value
    [Tags]    engine    connector    MON-152874
    Ctn Config Centralized Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    central    bbdo    info

    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # Create files :
    Ctn Create Template File    ${0}    connector    connector_name    ["template_conn_name"]

    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    connectorTemplates.cfg

    # Delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    connector_template_1    active_checks_enabled    connectorTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    connector_template_1    passive_checks_enabled    connectorTemplates.cfg

    # Operation in commandTemplates
    Ctn Engine Config Set Key Value In Cfg     0    connector_template_1    connector_line    /usr/lib64/centreon-connector/centreon_connector_perl --debug --log-file=/tmp/var/log/centreon-engine/config0/connector_perl.log    connectorTemplates.cfg

    Ctn Engine Config Set Key Value In Cfg     0    Perl Connector    use    connector_template_1    connectors.cfg
    Ctn Engine Config Delete Key In Cfg    0    Perl Connector    connector_line    connectors.cfg

    # Sending the new configuration to Engine.
    ${start}    Ctn Get Round Current Date
    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    ${output}    Ctn Get Connector Info Grpc    Perl Connector


    Should Be Equal As Strings     ${output}[connectorName]    Perl Connector
    Should Be Equal As Strings     ${output}[connectorLine]    /usr/lib64/centreon-connector/centreon_connector_perl --debug --log-file=/tmp/var/log/centreon-engine/config0/connector_perl.log

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CECOI3
    [Documentation]    Given a centralized Engine already started with a basic configuration
    ...    And a connector template with a connector_line is added with the connector inheriting from it while keeping its own connector_line
    ...    When the new configuration is sent via Broker notification
    ...    Then the connector's resolved connector_line is the connector's own value, not the template's
    [Tags]    engine    connector    MON-152874
    Ctn Config Centralized Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    central    bbdo    info

    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # Create files :
    Ctn Create Template File    ${0}    connector    connector_name    ["template_conn_name"]

    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    connectorTemplates.cfg

    # Delete unnecessary fields in templates:
    Ctn Engine Config Delete Key In Cfg    0    connector_template_1    active_checks_enabled    connectorTemplates.cfg
    Ctn Engine Config Delete Key In Cfg    0    connector_template_1    passive_checks_enabled    connectorTemplates.cfg

    # Operation in commandTemplates
    Ctn Engine Config Set Key Value In Cfg     0    connector_template_1    connector_line    /usr/lib64/centreon-connector/centreon_connector_ssh --debug --log-file=/tmp/var/log/centreon-engine/config0/connector_ssh.log    connectorTemplates.cfg

    Ctn Engine Config Set Key Value In Cfg     0    Perl Connector    use    connector_template_1    connectors.cfg

    # Sending the new configuration to Engine.
    ${start}    Ctn Get Round Current Date
    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    ${output}    Ctn Get Connector Info Grpc    Perl Connector


    Should Be Equal As Strings     ${output}[connectorName]    Perl Connector
    Should Be Equal As Strings     ${output}[connectorLine]    /usr/lib64/centreon-connector/centreon_connector_perl --debug --log-file=/tmp/var/log/centreon-engine/config0/connector_perl.log

    Ctn Stop Engine
    Ctn Kindly Stop Broker
