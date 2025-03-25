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

*** Keywords ***
Ctn Start Engine With Args
    [Arguments]    @{options}
    Log To Console    @{options}
    Start Process    /usr/sbin/centengine    @{options}    alias=e1    stdout=/tmp/output.txt
