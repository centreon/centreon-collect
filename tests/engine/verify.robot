*** Settings ***
Documentation       Centreon Engine Tests with command line

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes


*** Test Cases ***
VERIF
    [Documentation]    When centengine is started in verification mode, it does not log in its file.
    [Tags]    engine    MON-108616
    Ctn Config Engine    ${1}
    Ctn Config Broker    module
    Remove File    ${engineLog0}
    Ctn Start Engine With Args    -v    ${EtcRoot}/centreon-engine/config0/centengine.cfg
    Sleep    5s
    File Should Not Exist    ${engineLog0}

EVOCWNV
    [Documentation]    Scenario: The new Engine checks the old configuration (concerning cbmod)
    ...    Given the Engine is configured with a valid old configuration
    ...    When the Engine is started to check the configuration
    ...    Then the Engine reads it as expected
    Ctn Config Engine    ${1}
    Ctn Engine Config Set Value    ${0}    broker_module    /usr/lib64/nagios/plugins/centreon-broker/cbmod.so ${ETC_ROOT}/centreon-broker/central-module0.json    True    True
    Ctn Engine Config Delete Key    ${0}    broker_module_cfg_file
    Ctn Start Engine With Args    -v    ${EtcRoot}/centreon-engine/config0/centengine.cfg

    Wait Until Created    /tmp/output.txt    timeout=60s
    ${result}    Grep File    /tmp/output.txt    error
    Should Be Empty    ${result}    The output /tmp/output.txt should not contain any error: ${result}

ECNHNC
    [Documentation]    Scenario: a contact without host notification commands is rejected
    ...    Given an engine configuration where contact U1 has no host_notification_commands
    ...    When centengine verifies the configuration (-v)
    ...    Then the configuration is reported invalid (non-zero return code and at least one error)
    [Tags]    engine    config    contact    MON-187019
    Ctn Config Engine    ${1}
    Ctn Config Broker    module
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Engine Config Delete Value In Contact    ${0}    U1    host_notification_commands
    ${result}    Run Process
    ...    /usr/sbin/centengine    -v    ${EtcRoot}/centreon-engine/config0/centengine.cfg
    ...    stdout=/tmp/output.txt    stderr=/tmp/error.txt
    Should Not Be Equal As Integers
    ...    ${result.rc}    ${0}
    ...    verify-config must reject a contact without host notification commands
    ${out}    Get File    /tmp/output.txt
    Should Contain    ${out}    U1    the report must name the faulty contact

ECEMPTYNAME
    [Documentation]    Scenario: a contact with no name is rejected
    ...    Given an engine configuration with a contact that has no contact_name
    ...    When centengine verifies the configuration (-v)
    ...    Then the configuration is reported invalid (non-zero return code and "Contact has no name")
    [Tags]    engine    config    contact    MON-187019
    Ctn Config Engine    ${1}
    Ctn Config Broker    module
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    # Inject a contact block with no contact_name at all (empty name).
    # Single spaces only: Robot splits arguments on runs of 2+ spaces.
    Append To File
    ...    ${EtcRoot}/centreon-engine/config0/contacts.cfg
    ...    \ndefine contact {\nalias noname\nhost_notification_period 24x7\nregister 1\n}\n
    ${result}    Run Process
    ...    /usr/sbin/centengine    -v    ${EtcRoot}/centreon-engine/config0/centengine.cfg
    ...    stdout=/tmp/output.txt    stderr=/tmp/error.txt
    Should Not Be Equal As Integers
    ...    ${result.rc}    ${0}    verify-config must reject a contact with no name
    ${out}    Get File    /tmp/output.txt
    Should Contain    ${out}    Contact has no name    the report must mention the nameless contact

*** Keywords ***
Ctn Start Engine With Args
    [Arguments]    @{options}
    Log To Console    ${options}
    Run Process    /usr/sbin/centengine    @{options}    stdout=/tmp/output.txt    stderr=/tmp/error.txt
