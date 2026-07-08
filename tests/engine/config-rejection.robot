*** Settings ***
Documentation       Centengine rejects an invalid user-edited centengine.cfg: it refuses to start,
...                 and refuses a reload (keeping the running configuration). The proto configuration
...                 pushed by Broker (centengine -p) is trusted and not re-validated here.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
ECRSTART
    [Documentation]    Scenario: centengine refuses to start with an invalid centengine.cfg
    ...    Given an engine configuration where contact U1 has no host_notification_commands
    ...    When centengine is started on that configuration
    ...    Then it refuses to start: it exits with a failure code instead of running
    [Tags]    engine    config    contact    MON-187019
    Ctn Config Engine    ${1}
    Ctn Config Broker    module
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    # A user-edited invalid configuration: a contact without notification commands.
    Ctn Engine Config Delete Value In Contact    ${0}    U1    host_notification_commands
    Ctn Start Engine
    # The gate rejects the invalid .cfg before the event loop: centengine exits.
    ${res}    Wait For Process    e0    timeout=30s
    Should Not Be Equal    ${res}    ${None}    centengine must not keep running with an invalid configuration
    Should Not Be Equal As Integers    ${res.rc}    ${0}    centengine must exit with a failure code

ECRELOAD
    [Documentation]    Scenario: centengine refuses a reload of an invalid centengine.cfg and keeps running
    ...    Given centengine is started and ready with a valid configuration
    ...    When centengine.cfg is made invalid (contact U1 without host_notification_commands) and a reload is triggered
    ...    Then the reload is rejected (it logs the error) and centengine keeps running with the previous configuration
    [Tags]    engine    config    contact    MON-187019
    Ctn Config Engine    ${1}
    Ctn Config Broker    module
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    # Make the on-disk configuration invalid, then trigger a reload (SIGHUP).
    Ctn Engine Config Delete Value In Contact    ${0}    U1    host_notification_commands
    ${reload}    Get Current Date
    Ctn Reload Engine
    ${content}    Create List    Cannot reload: the configuration has
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${reload}    ${content}    30
    Should Be True    ${result}    centengine did not reject the invalid reload
    # The reload is rejected, but the engine keeps running with its previous config.
    ${running}    Is Process Running    e0
    Should Be True    ${running}    centengine must keep running after a rejected reload
    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Save Logs If Failed

ECGRSTART
    [Documentation]    Scenario: centengine refuses to start with a contact group referencing an undefined contact
    ...    Given an engine configuration with a contact group whose member does not exist
    ...    When centengine is started on that configuration
    ...    Then it refuses to start: it exits with a failure code instead of running
    [Tags]    engine    config    contactgroup    MON-187019
    Ctn Config Engine    ${1}
    Ctn Config Broker    module
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg
    Ctn Add Contact Group    ${0}    0    ["ghost_contact"]    name=badcg
    Ctn Start Engine
    ${res}    Wait For Process    e0    timeout=30s
    Should Not Be Equal    ${res}    ${None}    centengine must not keep running with an invalid configuration
    Should Not Be Equal As Integers    ${res.rc}    ${0}    centengine must exit with a failure code

ECGRELOAD
    [Documentation]    Scenario: centengine refuses a reload adding a contact group with an undefined member
    ...    Given centengine is started and ready with a valid configuration
    ...    When a contact group referencing an undefined contact is added and a reload is triggered
    ...    Then the reload is rejected (it logs the error) and centengine keeps running with the previous configuration
    [Tags]    engine    config    contactgroup    MON-187019
    Ctn Config Engine    ${1}
    Ctn Config Broker    module
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    # Make the on-disk configuration invalid, then trigger a reload (SIGHUP).
    Ctn Add Contact Group    ${0}    0    ["ghost_contact"]    name=badcg
    ${reload}    Get Current Date
    Ctn Reload Engine
    ${content}    Create List    Cannot reload: the configuration has
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${reload}    ${content}    30
    Should Be True    ${result}    centengine did not reject the invalid reload
    ${running}    Is Process Running    e0
    Should Be True    ${running}    centengine must keep running after a rejected reload
    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Save Logs If Failed

EHGRSTART
    [Documentation]    Scenario: centengine refuses to start with a host group referencing an undefined host
    ...    Given an engine configuration with a host group whose member does not exist
    ...    When centengine is started on that configuration
    ...    Then it refuses to start: it exits with a failure code instead of running
    [Tags]    engine    config    hostgroup    MON-187019
    Ctn Config Engine    ${1}
    Ctn Config Broker    module
    # hostgroups.cfg is already referenced by the default configuration. Single
    # spaces only: Robot splits arguments on runs of 2+ spaces.
    Append To File
    ...    ${EtcRoot}/centreon-engine/config0/hostgroups.cfg
    ...    \ndefine hostgroup {\nhostgroup_name badhg\nalias badhg\nmembers ghost_host\n}\n
    Ctn Start Engine
    ${res}    Wait For Process    e0    timeout=30s
    Should Not Be Equal    ${res}    ${None}    centengine must not keep running with an invalid configuration
    Should Not Be Equal As Integers    ${res.rc}    ${0}    centengine must exit with a failure code

EHGRELOAD
    [Documentation]    Scenario: centengine refuses a reload adding a host group with an undefined member
    ...    Given centengine is started and ready with a valid configuration
    ...    When a host group referencing an undefined host is added and a reload is triggered
    ...    Then the reload is rejected (it logs the error) and centengine keeps running with the previous configuration
    [Tags]    engine    config    hostgroup    MON-187019
    Ctn Config Engine    ${1}
    Ctn Config Broker    module
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    # Make the on-disk configuration invalid, then trigger a reload (SIGHUP).
    Append To File
    ...    ${EtcRoot}/centreon-engine/config0/hostgroups.cfg
    ...    \ndefine hostgroup {\nhostgroup_name badhg\nalias badhg\nmembers ghost_host\n}\n
    ${reload}    Get Current Date
    Ctn Reload Engine
    ${content}    Create List    Cannot reload: the configuration has
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${reload}    ${content}    30
    Should Be True    ${result}    centengine did not reject the invalid reload
    ${running}    Is Process Running    e0
    Should Be True    ${running}    centengine must keep running after a rejected reload
    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Save Logs If Failed

ESGRSTART
    [Documentation]    Scenario: centengine refuses to start with a service group referencing an undefined service
    ...    Given an engine configuration with a service group whose member does not exist
    ...    When centengine is started on that configuration
    ...    Then it refuses to start: it exits with a failure code instead of running
    [Tags]    engine    config    servicegroup    MON-187019
    Ctn Config Engine    ${1}
    Ctn Config Broker    module
    # host_1 exists, ghost_service does not. Single spaces only.
    Append To File
    ...    ${EtcRoot}/centreon-engine/config0/servicegroups.cfg
    ...    \ndefine servicegroup {\nservicegroup_name badsg\nalias badsg\nmembers host_1,ghost_service\n}\n
    Ctn Start Engine
    ${res}    Wait For Process    e0    timeout=30s
    Should Not Be Equal    ${res}    ${None}    centengine must not keep running with an invalid configuration
    Should Not Be Equal As Integers    ${res.rc}    ${0}    centengine must exit with a failure code

ESGRELOAD
    [Documentation]    Scenario: centengine refuses a reload adding a service group with an undefined member
    ...    Given centengine is started and ready with a valid configuration
    ...    When a service group referencing an undefined service is added and a reload is triggered
    ...    Then the reload is rejected (it logs the error) and centengine keeps running with the previous configuration
    [Tags]    engine    config    servicegroup    MON-187019
    Ctn Config Engine    ${1}
    Ctn Config Broker    module
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    # Make the on-disk configuration invalid, then trigger a reload (SIGHUP).
    Append To File
    ...    ${EtcRoot}/centreon-engine/config0/servicegroups.cfg
    ...    \ndefine servicegroup {\nservicegroup_name badsg\nalias badsg\nmembers host_1,ghost_service\n}\n
    ${reload}    Get Current Date
    Ctn Reload Engine
    ${content}    Create List    Cannot reload: the configuration has
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${reload}    ${content}    30
    Should Be True    ${result}    centengine did not reject the invalid reload
    ${running}    Is Process Running    e0
    Should Be True    ${running}    centengine must keep running after a rejected reload
    [Teardown]    Run Keywords    Ctn Stop Engine    AND    Ctn Save Logs If Failed
