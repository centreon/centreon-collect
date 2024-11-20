*** Settings ***
Documentation       Centreon Engine verify connector inheritance.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
ECOI0
    [Documentation]    Verify connector inheritance : connector(empty) inherit from template (full) , on Start Engine
    [Tags]    engine    connector    MON-152874
    Ctn Config Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

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

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Connector Info Grpc    Perl Connector


    Should Be Equal As Strings     ${output}[connectorName]    Perl Connector
    Should Be Equal As Strings     ${output}[connectorLine]    /usr/lib64/centreon-connector/centreon_connector_perl --debug --log-file=/tmp/var/log/centreon-engine/config0/connector_perl.log

    Ctn Stop Engine
    Ctn Kindly Stop Broker

ECOI1
    [Documentation]    Verify connector inheritance : connector(full) inherit from template (full) , on Start Engine
    [Tags]    engine    connector    MON-152874
    Ctn Config Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

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

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Connector Info Grpc    Perl Connector


    Should Be Equal As Strings     ${output}[connectorName]    Perl Connector
    Should Be Equal As Strings     ${output}[connectorLine]    /usr/lib64/centreon-connector/centreon_connector_perl --debug --log-file=/tmp/var/log/centreon-engine/config0/connector_perl.log

    Ctn Stop Engine
    Ctn Kindly Stop Broker

ECOI2
    [Documentation]    Verify connector inheritance : connector(empty) inherit from template (full) , on Reload Engine
    [Tags]    engine    connector    MON-152874
    Ctn Config Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

    Ctn Clear Retention

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
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

    ${output}    Ctn Get Connector Info Grpc    Perl Connector


    Should Be Equal As Strings     ${output}[connectorName]    Perl Connector
    Should Be Equal As Strings     ${output}[connectorLine]    /usr/lib64/centreon-connector/centreon_connector_perl --debug --log-file=/tmp/var/log/centreon-engine/config0/connector_perl.log

    Ctn Stop Engine
    Ctn Kindly Stop Broker

ECOI3
    [Documentation]    Verify connector inheritance : connector(full) inherit from template (full) , on Reload Engine
    [Tags]    engine    connector    MON-152874
    Ctn Config Engine    ${1}    ${5}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

    Ctn Clear Retention

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
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

    ${output}    Ctn Get Connector Info Grpc    Perl Connector


    Should Be Equal As Strings     ${output}[connectorName]    Perl Connector
    Should Be Equal As Strings     ${output}[connectorLine]    /usr/lib64/centreon-connector/centreon_connector_perl --debug --log-file=/tmp/var/log/centreon-engine/config0/connector_perl.log

    Ctn Stop Engine
    Ctn Kindly Stop Broker