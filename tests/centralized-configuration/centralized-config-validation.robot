*** Settings ***
Documentation       Broker validates a centralized poller configuration at ingestion and refuses to push an invalid one, even when CheckPollerConfig was not called.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Clean Before Test
Test Teardown       Ctn Stop Engine Broker And Save Logs    only_central=True


*** Test Cases ***
BECFGVAL1
    [Documentation]    Scenario: PHP pushes an invalid poller configuration without asking for a CheckPollerConfig
    ...    Given a centralized engine configuration where contact U1 has no host_notification_commands
    ...    And Broker is started in centralized mode
    ...    When the configuration change is notified to Broker (no CheckPollerConfig call)
    ...    Then Broker refuses to push the configuration to the poller
    ...    And it does not store the poller .prot configuration
    [Tags]    broker    engine    config    centralized    MON-187019
    Ctn Config Centralized Engine    ${1}
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    # The user-edited configuration is invalid: a contact without notification commands.
    Ctn Engine Config Delete Value In Contact    ${0}    U1    host_notification_commands
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}    only_central=True
    Ctn Broker Config Log    central    config    debug
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True    only_central=True
    # Notify Broker of the (invalid) configuration WITHOUT calling CheckPollerConfig.
    Ctn Notify Broker Of Engine Config Change    ${0}

    # Broker must validate at ingestion and refuse to push the invalid configuration.
    ${content}    Create List    refusing to push it to the poller
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Broker did not refuse the invalid poller configuration

    # A refused configuration must not be stored as the poller .prot file.
    ${prot}    Set Variable
    ...    ${VarRoot}/lib/centreon-broker/central-broker-master/pollers-configuration/new-1.prot
    File Should Not Exist    ${prot}    a refused configuration must not be stored

    # Broker must not have stored/propagated the configuration.
    ${stored}    Create List    New Engine configuration for poller 1 stored
    ${found}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${stored}    5
    Should Not Be True    ${found}    Broker must not store/propagate a refused configuration

    # A rejected configuration is considered processed: its .lck is consumed so
    # Broker does not retry the invalid configuration forever.
    Wait Until Removed    ${VarRoot}/lib/centreon/config/1.lck    15s

BECFGVAL2
    [Documentation]    Scenario: PHP pushes a poller configuration with a contact group referencing an undefined contact
    ...    Given a centralized engine configuration with a contact group whose member does not exist
    ...    And Broker is started in centralized mode
    ...    When the configuration change is notified to Broker (no CheckPollerConfig call)
    ...    Then Broker refuses to push the configuration to the poller
    ...    And it does not store the poller .prot configuration
    [Tags]    broker    engine    config    centralized    MON-187019
    Ctn Config Centralized Engine    ${1}
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg
    # The user-edited configuration is invalid: a contact group with a non-existing member.
    Ctn Add Contact Group    ${0}    0    ["ghost_contact"]    name=badcg
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}    only_central=True
    Ctn Broker Config Log    central    config    debug
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True    only_central=True
    # Notify Broker of the (invalid) configuration WITHOUT calling CheckPollerConfig.
    Ctn Notify Broker Of Engine Config Change    ${0}

    # Broker must validate at ingestion and refuse to push the invalid configuration.
    ${content}    Create List    refusing to push it to the poller
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Broker did not refuse the invalid poller configuration

    # A refused configuration must not be stored as the poller .prot file.
    ${prot}    Set Variable
    ...    ${VarRoot}/lib/centreon-broker/central-broker-master/pollers-configuration/new-1.prot
    File Should Not Exist    ${prot}    a refused configuration must not be stored

    # A rejected configuration is considered processed: its .lck is consumed.
    Wait Until Removed    ${VarRoot}/lib/centreon/config/1.lck    15s

BECFGVAL3
    [Documentation]    Scenario: PHP pushes a poller configuration with a host dependency referencing an undefined host
    ...    Given a centralized engine configuration with a host dependency whose dependent host does not exist
    ...    And Broker is started in centralized mode
    ...    When the configuration change is notified to Broker (no CheckPollerConfig call)
    ...    Then Broker refuses to push the configuration to the poller
    ...    And it does not store the poller .prot configuration
    [Tags]    broker    engine    config    centralized    MON-187019
    Ctn Config Centralized Engine    ${1}
    Ctn Config Engine Add Cfg File    ${0}    dependencies.cfg
    # The user-edited configuration is invalid: a host dependency whose dependent
    # host is not defined anywhere.
    Ctn Add Host Dependency    ${0}    host_1    ghost_host
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}    only_central=True
    Ctn Broker Config Log    central    config    debug
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True    only_central=True
    # Notify Broker of the (invalid) configuration WITHOUT calling CheckPollerConfig.
    Ctn Notify Broker Of Engine Config Change    ${0}

    # Broker must validate at ingestion and refuse to push the invalid configuration.
    ${content}    Create List    refusing to push it to the poller
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Broker did not refuse the invalid poller configuration

    # A refused configuration must not be stored as the poller .prot file.
    ${prot}    Set Variable
    ...    ${VarRoot}/lib/centreon-broker/central-broker-master/pollers-configuration/new-1.prot
    File Should Not Exist    ${prot}    a refused configuration must not be stored

    # A rejected configuration is considered processed: its .lck is consumed.
    Wait Until Removed    ${VarRoot}/lib/centreon/config/1.lck    15s

BECFGVAL4
    [Documentation]    Scenario: PHP pushes a poller configuration with a service dependency referencing an undefined service
    ...    Given a centralized engine configuration with a service dependency whose dependent service does not exist
    ...    And Broker is started in centralized mode
    ...    When the configuration change is notified to Broker (no CheckPollerConfig call)
    ...    Then Broker refuses to push the configuration to the poller
    ...    And it does not store the poller .prot configuration
    [Tags]    broker    engine    config    centralized    MON-187019
    Ctn Config Centralized Engine    ${1}
    Ctn Config Engine Add Cfg File    ${0}    dependencies.cfg
    # The user-edited configuration is invalid: a service dependency whose
    # dependent service is not defined anywhere.
    Ctn Add Service Dependency    ${0}    host_1    host_1    service_1    ghost_service
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}    only_central=True
    Ctn Broker Config Log    central    config    debug
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True    only_central=True
    # Notify Broker of the (invalid) configuration WITHOUT calling CheckPollerConfig.
    Ctn Notify Broker Of Engine Config Change    ${0}

    # Broker must validate at ingestion and refuse to push the invalid configuration.
    ${content}    Create List    refusing to push it to the poller
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Broker did not refuse the invalid poller configuration

    # A refused configuration must not be stored as the poller .prot file.
    ${prot}    Set Variable
    ...    ${VarRoot}/lib/centreon-broker/central-broker-master/pollers-configuration/new-1.prot
    File Should Not Exist    ${prot}    a refused configuration must not be stored

    # A rejected configuration is considered processed: its .lck is consumed.
    Wait Until Removed    ${VarRoot}/lib/centreon/config/1.lck    15s
