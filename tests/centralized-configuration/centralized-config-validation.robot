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

BECFGVAL5
    [Documentation]    Scenario: PHP pushes a poller configuration with a host escalation referencing an undefined contact group
    ...    Given a centralized engine configuration with a host escalation whose contact group does not exist
    ...    And Broker is started in centralized mode
    ...    When the configuration change is notified to Broker (no CheckPollerConfig call)
    ...    Then Broker refuses to push the configuration to the poller
    ...    And it does not store the poller .prot configuration
    [Tags]    broker    engine    config    centralized    MON-187019
    Ctn Config Centralized Engine    ${1}
    Ctn Config Engine Add Cfg File    ${0}    escalations.cfg
    # The user-edited configuration is invalid: a host escalation whose contact
    # group is not defined anywhere.
    Ctn Add Host Escalation    ${0}    host_1    ghost_cg
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

BECFGVAL6
    [Documentation]    Scenario: PHP pushes a poller configuration with a service escalation referencing an undefined contact group
    ...    Given a centralized engine configuration with a service escalation whose contact group does not exist
    ...    And Broker is started in centralized mode
    ...    When the configuration change is notified to Broker (no CheckPollerConfig call)
    ...    Then Broker refuses to push the configuration to the poller
    ...    And it does not store the poller .prot configuration
    [Tags]    broker    engine    config    centralized    MON-187019
    Ctn Config Centralized Engine    ${1}
    Ctn Config Engine Add Cfg File    ${0}    escalations.cfg
    # The user-edited configuration is invalid: a service escalation whose
    # contact group is not defined anywhere.
    Ctn Add Service Escalation    ${0}    host_1    service_1    ghost_cg
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

BECFGVAL7
    [Documentation]    Scenario: PHP pushes a poller configuration with a host referencing an undefined notification period
    ...    Given a centralized engine configuration where a host has a non-existing notification period
    ...    And Broker is started in centralized mode
    ...    When the configuration change is notified to Broker (no CheckPollerConfig call)
    ...    Then Broker refuses to push the configuration to the poller
    ...    And it does not store the poller .prot configuration
    [Tags]    broker    engine    config    centralized    MON-187019
    Ctn Config Centralized Engine    ${1}
    # The user-edited configuration is invalid: a host whose notification period is
    # not defined anywhere.
    Ctn Engine Config Set Value In Hosts    ${0}    host_1    notification_period    ghost_tp
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

BECFGVAL8
    [Documentation]    Scenario: PHP pushes a poller configuration with a service referencing an undefined notification period
    ...    Given a centralized engine configuration where a service has a non-existing notification period
    ...    And Broker is started in centralized mode
    ...    When the configuration change is notified to Broker (no CheckPollerConfig call)
    ...    Then Broker refuses to push the configuration to the poller
    ...    And it does not store the poller .prot configuration
    [Tags]    broker    engine    config    centralized    MON-187019
    Ctn Config Centralized Engine    ${1}
    # The user-edited configuration is invalid: a service whose notification period
    # is not defined anywhere.
    Ctn Engine Config Set Value In Services    ${0}    service_1    notification_period    ghost_tp
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

BECFGVAL9
    [Documentation]    Scenario: PHP pushes a valid configuration for a poller that is not connected
    ...    Given a valid centralized engine configuration for poller 1
    ...    And Broker is started in centralized mode while Engine is left stopped
    ...    When the configuration change is notified to Broker
    ...    Then Broker prepares and stores the poller configuration once
    ...    And it consumes the .lck, new-1.prot being what says the delivery is pending
    ...    And nothing at all happens about that poller on the following cycles
    [Tags]    broker    engine    config    centralized    MON-187019
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}    only_central=True
    Ctn Broker Config Log    central    config    debug
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True    only_central=True
    Ctn Notify Broker Of Engine Config Change    ${0}

    # The configuration is valid, so Broker prepares it even though no poller
    # can receive it yet.
    ${content}    Create List    New Engine configuration for poller 1 stored
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    Broker did not store the valid poller configuration
    ${prot}    Set Variable
    ...    ${VarRoot}/lib/centreon-broker/central-broker-master/pollers-configuration/new-1.prot
    File Should Exist    ${prot}    a valid configuration must be stored

    # The announcement has been read and what it announced is on disk, so it
    # must be gone -- PHP waits for exactly that. What says the delivery is
    # still pending is new-1.prot, checked just above, and it says so whether
    # the poller is connected or not.
    Wait Until Removed    ${VarRoot}/lib/centreon/config/1.lck    15s

    # From here on, nothing more must happen about that poller. The 2s wait puts
    # ${middle} strictly after the preparation logged above, since the date is
    # rounded down to the second.
    Sleep    2s
    ${middle}    Ctn Get Round Current Date
    # The watcher runs every 5s, so several cycles go by here.
    Sleep    20s

    # Counting the preparations would prove nothing: the poller being absent was
    # already enough to skip the delivery. What must not happen is the whole
    # configuration store being parsed again on every cycle just because a .lck
    # is waiting for its poller.
    ${reload}    Create List    stored poller configurations for the cross-poller validation
    ${found}    Ctn Find In Log With Timeout    ${centralLog}    ${middle}    ${reload}    5
    Should Not Be True
    ...    ${found}
    ...    Broker parsed every stored poller configuration again while nothing was being deployed

    # And the cause of that reload, the requeuing of an announcement, must not
    # happen either -- there is none left to requeue.
    ${orphan}    Create List    Found orphan lock file
    ${found}    Ctn Find In Log With Timeout    ${centralLog}    ${middle}    ${orphan}    5
    Should Not Be True    ${found}    the lock file was requeued while its poller was still absent

    # Nor must the configuration be prepared a second time.
    ${stored}    Create List    New Engine configuration for poller 1 stored
    ${found}    Ctn Find In Log With Timeout    ${centralLog}    ${middle}    ${stored}    5
    Should Not Be True    ${found}    Broker prepared the configuration again for an absent poller

    # Nor must the directory be scanned at all: the scan is a safety net for what
    # inotify cannot report, and nothing here asked for it.
    ${scan}    Create List    Scanning the engine configuration directory
    ${found}    Ctn Find In Log With Timeout    ${centralLog}    ${middle}    ${scan}    5
    Should Not Be True    ${found}    Broker scanned the cache directory while nothing asked for it
    File Should Not Exist
    ...    ${VarRoot}/lib/centreon/config/1.lck
    ...    the announcement must not come back

    # An empty inotify queue is the ordinary case and must not be logged as an
    # error, otherwise every cycle leaves one error line behind.
    ${inotify}    Create List    Unable to read from inotify
    ${found}    Ctn Find In Log With Timeout    ${centralLog}    ${middle}    ${inotify}    5
    Should Not Be True    ${found}    an empty inotify queue must not be reported as an error
