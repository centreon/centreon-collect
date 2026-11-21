*** Settings ***
Documentation       Broker gRPC CheckPollerConfig: validate an Engine poller configuration directory.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Variables ***
# Centralized configuration of poller 0 lives in config/1 (poller_id = inst + 1).
${PollerConfigDir}      ${VarRoot}/lib/centreon/config/1


*** Test Cases ***
BCPC1
    [Documentation]    Scenario: a valid centralized poller configuration passes the check
    ...    Given a centralized engine configuration for 1 poller
    ...    And broker is started so its gRPC server answers
    ...    When CheckPollerConfig is called on the poller configuration directory
    ...    Then ok is true and there is no ERROR diagnostic
    [Tags]    broker    grpc    config    MON-187019
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Start Broker
    ${res}    Ctn Broker Check Poller Config    ${PollerConfigDir}
    Should Not Be Equal    ${res}    ${None}    CheckPollerConfig did not answer
    Should Be True    ${res}[ok]    a valid configuration must be ok
    ${errors}    Evaluate    [d for d in $res['diagnostics'] if d['severity'] == 'ERROR']
    Should Be Empty    ${errors}    a valid configuration must have no error

BCPC2
    [Documentation]    Scenario: a timeperiod excluding a non-existent one makes the check fail
    ...    Given a centralized engine configuration with an invalid timeperiod (exclude -> unknown)
    ...    When CheckPollerConfig is called on the poller configuration directory
    ...    Then ok is false and an ERROR diagnostic names the unresolved exclusion
    [Tags]    broker    grpc    config    MON-187019
    Ctn Config Centralized Engine    ${1}
    Ctn Engine Config Add Timeperiod    ${0}    badtp    exclude=does_not_exist
    Ctn Config Broker    central
    Ctn Start Broker
    ${res}    Ctn Broker Check Poller Config    ${PollerConfigDir}
    Should Not Be Equal    ${res}    ${None}    CheckPollerConfig did not answer
    Should Not Be True    ${res}[ok]    an invalid configuration must not be ok
    ${errors}    Evaluate    [d['message'] for d in $res['diagnostics'] if d['severity'] == 'ERROR']
    Should Not Be Empty    ${errors}    an invalid configuration must report at least one error
    ${joined}    Evaluate    " ".join($errors)
    Should Contain    ${joined}    does_not_exist    the error must name the missing timeperiod

BCPC3
    [Documentation]    Scenario: several invalid timeperiods are all reported (return shape)
    ...    Given a centralized engine configuration with two invalid timeperiods
    ...    When CheckPollerConfig is called on the poller configuration directory
    ...    Then ok is false, the response holds a list of {severity, message} diagnostics,
    ...    and both missing exclusions are reported as errors
    [Tags]    broker    grpc    config    MON-187019
    Ctn Config Centralized Engine    ${1}
    Ctn Engine Config Add Timeperiod    ${0}    badtp1    exclude=ghost1
    Ctn Engine Config Add Timeperiod    ${0}    badtp2    exclude=ghost2
    Ctn Config Broker    central
    Ctn Start Broker
    ${res}    Ctn Broker Check Poller Config    ${PollerConfigDir}
    Should Not Be Equal    ${res}    ${None}    CheckPollerConfig did not answer
    Should Not Be True    ${res}[ok]    an invalid configuration must not be ok
    # The response carries a list of diagnostics, each shaped {severity, message}.
    ${diags}    Set Variable    ${res}[diagnostics]
    ${count}    Get Length    ${diags}
    Should Be True    ${count} >= 2    both invalid timeperiods must be reported
    FOR    ${d}    IN    @{diags}
        Should Contain    ${d}    severity
        Should Contain    ${d}    message
    END
    ${errors}    Evaluate    [d['message'] for d in $res['diagnostics'] if d['severity'] == 'ERROR']
    ${joined}    Evaluate    " ".join($errors)
    Should Contain    ${joined}    ghost1
    Should Contain    ${joined}    ghost2

BCPC4
    [Documentation]    Scenario: a contact with no host notification commands makes the check fail
    ...    Given a centralized engine configuration where contact U1 has no host_notification_commands
    ...    When CheckPollerConfig is called on the poller configuration directory
    ...    Then ok is false and an ERROR diagnostic reports the missing host notification commands
    [Tags]    broker    grpc    config    MON-187019
    Ctn Config Centralized Engine    ${1}
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Engine Config Delete Value In Contact    ${0}    U1    host_notification_commands
    Ctn Config Broker    central
    Ctn Start Broker
    ${res}    Ctn Broker Check Poller Config    ${PollerConfigDir}
    Should Not Be Equal    ${res}    ${None}    CheckPollerConfig did not answer
    Should Not Be True    ${res}[ok]    a contact without host notification commands must not be ok
    ${errors}    Evaluate    [d['message'] for d in $res['diagnostics'] if d['severity'] == 'ERROR']
    Should Not Be Empty    ${errors}    the missing host notification commands must be reported as an error
    ${joined}    Evaluate    " ".join($errors)
    Should Contain    ${joined}    U1    the error must name the contact
    Should Contain    ${joined}    host notification commands    the error must mention the missing commands
