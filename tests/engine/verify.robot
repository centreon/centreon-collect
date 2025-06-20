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

*** Keywords ***
Ctn Start Engine With Args
    [Arguments]    @{options}
    Log To Console    @{options}
    Run Process    /usr/sbin/centengine    @{options}    stdout=/tmp/output.txt    stderr=/tmp/error.txt
