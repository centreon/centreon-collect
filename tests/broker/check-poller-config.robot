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

BCPC5
    [Documentation]    Scenario: a contact group with a non-existing member makes the check fail
    ...    Given a centralized engine configuration with a contact group referencing an undefined contact
    ...    When CheckPollerConfig is called on the poller configuration directory
    ...    Then ok is false and an ERROR diagnostic reports the missing contact
    [Tags]    broker    grpc    config    MON-187019
    Ctn Config Centralized Engine    ${1}
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg
    Ctn Add Contact Group    ${0}    0    ["ghost_contact"]    name=badcg
    Ctn Config Broker    central
    Ctn Start Broker
    ${res}    Ctn Broker Check Poller Config    ${PollerConfigDir}
    Should Not Be Equal    ${res}    ${None}    CheckPollerConfig did not answer
    Should Not Be True    ${res}[ok]    a contact group with a non-existing member must not be ok
    ${errors}    Evaluate    [d['message'] for d in $res['diagnostics'] if d['severity'] == 'ERROR']
    Should Not Be Empty    ${errors}    the missing contact must be reported as an error
    ${joined}    Evaluate    " ".join($errors)
    Should Contain    ${joined}    ghost_contact    the error must name the missing contact

BCPC6
    [Documentation]    Scenario: a command defined twice under the same name makes the check fail
    ...    Given a centralized engine configuration with a command defined twice
    ...    When CheckPollerConfig is called on the poller configuration directory
    ...    Then ok is false and an ERROR diagnostic names the duplicated command
    [Tags]    broker    grpc    config    MON-187019
    Ctn Config Centralized Engine    ${1}
    # Append a second definition of the same command name: a named object must
    # not be defined twice. Single spaces only: Robot splits arguments on runs
    # of 2+ spaces.
    Append To File
    ...    ${PollerConfigDir}/commands.cfg
    ...    \ndefine command {\ncommand_name dup_command\ncommand_line /usr/bin/true\n}\ndefine command {\ncommand_name dup_command\ncommand_line /usr/bin/false\n}\n
    Ctn Config Broker    central
    Ctn Start Broker
    ${res}    Ctn Broker Check Poller Config    ${PollerConfigDir}
    Should Not Be Equal    ${res}    ${None}    CheckPollerConfig did not answer
    Should Not Be True    ${res}[ok]    a command defined twice must not be ok
    ${errors}    Evaluate    [d['message'] for d in $res['diagnostics'] if d['severity'] == 'ERROR']
    Should Not Be Empty    ${errors}    the duplicated command must be reported as an error
    ${joined}    Evaluate    " ".join($errors)
    Should Contain    ${joined}    dup_command    the error must name the duplicated command

BCPC7
    [Documentation]    Scenario: a host group with a non-existing member makes the check fail
    ...    Given a centralized engine configuration with a host group referencing an undefined host
    ...    When CheckPollerConfig is called on the poller configuration directory
    ...    Then ok is false and an ERROR diagnostic names the missing host
    [Tags]    broker    grpc    config    MON-187019
    Ctn Config Centralized Engine    ${1}
    # hostgroups.cfg is already part of the centralized configuration. Append a
    # host group whose member does not exist. Single spaces only: Robot splits
    # arguments on runs of 2+ spaces.
    Append To File
    ...    ${PollerConfigDir}/hostgroups.cfg
    ...    \ndefine hostgroup {\nhostgroup_name badhg\nalias badhg\nmembers ghost_host\n}\n
    Ctn Config Broker    central
    Ctn Start Broker
    ${res}    Ctn Broker Check Poller Config    ${PollerConfigDir}
    Should Not Be Equal    ${res}    ${None}    CheckPollerConfig did not answer
    Should Not Be True    ${res}[ok]    a host group with a non-existing member must not be ok
    ${errors}    Evaluate    [d['message'] for d in $res['diagnostics'] if d['severity'] == 'ERROR']
    Should Not Be Empty    ${errors}    the missing host must be reported as an error
    ${joined}    Evaluate    " ".join($errors)
    Should Contain    ${joined}    ghost_host    the error must name the missing host

BCPC8
    [Documentation]    Scenario: a service group with a non-existing member makes the check fail
    ...    Given a centralized engine configuration with a service group referencing an undefined service
    ...    When CheckPollerConfig is called on the poller configuration directory
    ...    Then ok is false and an ERROR diagnostic names the missing service
    [Tags]    broker    grpc    config    MON-187019
    Ctn Config Centralized Engine    ${1}
    # servicegroups.cfg is already part of the centralized configuration. Append
    # a service group whose (host, service) member does not exist (host_1 is
    # defined, ghost_service is not). Single spaces only.
    Append To File
    ...    ${PollerConfigDir}/servicegroups.cfg
    ...    \ndefine servicegroup {\nservicegroup_name badsg\nalias badsg\nmembers host_1,ghost_service\n}\n
    Ctn Config Broker    central
    Ctn Start Broker
    ${res}    Ctn Broker Check Poller Config    ${PollerConfigDir}
    Should Not Be Equal    ${res}    ${None}    CheckPollerConfig did not answer
    Should Not Be True    ${res}[ok]    a service group with a non-existing member must not be ok
    ${errors}    Evaluate    [d['message'] for d in $res['diagnostics'] if d['severity'] == 'ERROR']
    Should Not Be Empty    ${errors}    the missing service must be reported as an error
    ${joined}    Evaluate    " ".join($errors)
    Should Contain    ${joined}    ghost_service    the error must name the missing service

BCPC9
    [Documentation]    Scenario: a dependency on a host of another poller is named as such
    ...    Given two pollers whose configurations have been ingested and acknowledged
    ...    And a host dependency of poller 1 whose dependent host belongs to poller 2
    ...    When CheckPollerConfig is called on the configuration directory of poller 1
    ...    Then ok is false, as at ingestion: a dependency does not cross a poller boundary
    ...    And the ERROR names poller 2 instead of claiming the host is defined nowhere
    [Tags]    broker    engine    grpc    config    MON-187019
    # What this test reads is the content of pollers-configuration/, so a .prot
    # left by an earlier test would make the assertions pass without any
    # ingestion having taken place -- or name a poller nobody configured here.
    Ctn Clear Prot Files
    Ctn Clear Broker Cache
    # Ten hosts over two pollers: host_1..host_5 on poller 1, host_6..host_10 on
    # poller 2.
    Ctn Config Centralized Engine    ${2}    ${10}    ${2}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${2}
    Ctn Broker Config Log    central    config    debug
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    # The index is built from the acknowledged configurations -- <N>.prot, not the
    # new-<N>.prot work files -- so both pollers have to have acknowledged before
    # anything of poller 2 can be found.
    ${content}    Create List    All engine peers acknowledged? 2/2 acknowledged
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    the two pollers did not acknowledge their configuration
    ${stored}    Set Variable
    ...    ${VarRoot}/lib/centreon-broker/central-broker-master/pollers-configuration/2.prot
    Wait Until Created    ${stored}    30s

    # Now a dependency of poller 1 on a host of poller 2. Not notified to Broker:
    # what is under test is the endpoint, not the ingestion.
    Ctn Config Engine Add Cfg File    ${0}    dependencies.cfg
    Ctn Add Host Dependency    ${0}    host_1    host_6

    ${res}    Ctn Broker Check Poller Config    ${PollerConfigDir}
    Should Not Be Equal    ${res}    ${None}    CheckPollerConfig did not answer
    Should Not Be True    ${res}[ok]    a cross-poller dependency must not be ok
    ${errors}    Evaluate    [d['message'] for d in $res['diagnostics'] if d['severity'] == 'ERROR']
    ${joined}    Evaluate    " ".join($errors)

    # The whole point of the cross-poller index: say where the host actually is.
    Should Contain    ${joined}    belongs to poller 2
    ...    the error must name the poller the host lives on
    Should Not Contain    ${joined}    host_6' is not defined anywhere
    ...    host_6 is defined, on another poller: the degraded message must not be used

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BCPC10
    [Documentation]    Scenario: an object dropped from the configuration under check reads as undefined
    ...    Given poller 1 whose configuration has been ingested and acknowledged
    ...    When host_5 is removed from its configuration and a dependency still references it
    ...    And CheckPollerConfig is called on that directory
    ...    Then host_5 reads as defined nowhere, not as living on poller 1 itself
    [Tags]    broker    engine    grpc    config    MON-187019
    Ctn Clear Prot Files
    Ctn Clear Broker Cache
    Ctn Config Centralized Engine    ${1}    ${10}    ${2}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    config    debug
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    # 1.prot has to hold host_5 before it is dropped: what is under test is the
    # index still knowing an object the configuration under check no longer has.
    ${content}    Create List    All engine peers acknowledged? 1/1 acknowledged
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    the poller did not acknowledge its configuration
    ${stored}    Set Variable
    ...    ${VarRoot}/lib/centreon-broker/central-broker-master/pollers-configuration/1.prot
    Wait Until Created    ${stored}    30s

    # host_5 leaves the configuration but stays in the stored one, and something
    # still references it. Without the self filter it would be found in 1.prot
    # and read as "living on poller 1" -- a dangling reference going unreported.
    Ctn Engine Config Remove All Services From Host    ${0}    host_5
    Ctn Engine Config Remove Host    ${0}    host_5
    Ctn Config Engine Add Cfg File    ${0}    dependencies.cfg
    Ctn Add Host Dependency    ${0}    host_1    host_5

    ${res}    Ctn Broker Check Poller Config    ${PollerConfigDir}
    Should Not Be Equal    ${res}    ${None}    CheckPollerConfig did not answer
    Should Not Be True    ${res}[ok]    a dangling reference must not be ok
    ${errors}    Evaluate    [d['message'] for d in $res['diagnostics'] if d['severity'] == 'ERROR']
    ${joined}    Evaluate    " ".join($errors)

    Should Contain    ${joined}    host_5' is not defined anywhere
    ...    a host dropped from the configuration under check must read as undefined
    Should Not Contain    ${joined}    belongs to poller 1
    ...    the poller being validated must be excluded from its own index

    Ctn Stop Engine
    Ctn Kindly Stop Broker
