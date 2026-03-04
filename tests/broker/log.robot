*** Settings ***
Documentation       Centreon Broker centralized configuration start/stop tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
CBLDIS1
    [Documentation]    Scenario: Broker starts with core logs disabled - sql logs still produced
    ...    Given central broker configured with core logs 'disabled' and sql logs at debug level
    ...    When broker is started in new generation mode
    ...    Then sql log entries are produced
    ...    And no core log entries are produced
    [Tags]    broker    start-stop    log-v2
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Broker Config Log    central    core    disabled
    Ctn Broker Config Log    central    sql    debug
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    ${content}    Create List    [sql]
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    "No sql logs produced"

    ${content}    Create List    [core]
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be Equal    ${result}    ${False}    "We should not have core logs"
    Ctn Kindly Stop Broker

CBLEC1
    [Documentation]    Scenario: Core log level changed live from trace to debug via gRPC API
    ...    Given central broker started with core logs at trace level in new generation mode
    ...    When the core log level is changed to debug via the gRPC API
    ...    Then the gRPC API reports the new core log level as debug
    [Tags]    broker    log-v2    grpc
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Broker Config Log    central    core    trace
    Ctn Broker Config Log    central    sql    debug
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    ${result}    Ctn Get Broker Log Level    51001    core
    Should Be Equal    ${result}    trace
    Ctn Set Broker Log Level    51001    core    debug
    ${result}    Ctn Get Broker Log Level    51001    core
    Should Be Equal    ${result}    debug

CBLEC2
    [Documentation]    Scenario: Setting an invalid log level via gRPC API raises an error
    ...    Given central broker started with core logs at trace level in new generation mode
    ...    When the core log level is set to the invalid value 'foo' via the gRPC API
    ...    Then an error message about the unknown enum value is returned
    [Tags]    broker    log-v2    grpc
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Broker Config Log    central    core    trace
    Ctn Broker Config Log    central    sql    debug
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    ${result}    Ctn Get Broker Log Level    51001    core
    Should Be Equal    ${result}    trace
    ${result}    Ctn Set Broker Log Level    51001    core    foo
    Should Be Equal    ${result}    Enum LogLevelEnum has no value defined for name 'FOO'

CBLEC3
    [Documentation]    Scenario: Setting log level for a non-existent logger via gRPC API raises an error
    ...    Given central broker started with core logs at trace level in new generation mode
    ...    When the log level of the non-existent 'foo' logger is set via the gRPC API
    ...    Then an error message about the missing logger is returned
    [Tags]    broker    log-v2    grpc
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Broker Config Log    central    core    trace
    Ctn Broker Config Log    central    sql    debug
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    ${result}    Ctn Set Broker Log Level    51001    foo    trace
    Should Be Equal    ${result}    The 'foo' logger does not exist

CBLBD
    [Documentation]    Scenario: Broker starts with default logger levels when no loggers section is configured
    ...    Given central broker configured without a loggers section
    ...    When broker is started in new generation mode
    ...    Then the gRPC API reports the expected default log levels for all loggers
    [Tags]    broker    log-v2    MON-143565
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Broker Config Remove Item    central    log:loggers
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    ${result}    Ctn Get Broker Log Info    51001    ALL
    Log To Console    ${result}
    ${LOG_RES}    Catenate    SEPARATOR=${\n}    @{LOG_RESULT}
    Should Be Equal    ${result}    ${LOG_RES}     Default loggers levels are wrong


*** Variables ***
@{LOG_RESULT}    log_name: "cbd"
...    log_file: "/tmp/var/log/centreon-broker//central-broker-master.log"
...    level {
...    ${SPACE}${SPACE}key: "victoria_metrics"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "tls"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "tcp"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "stats"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "sql"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "runtime"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "rrd"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "process"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "processing"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "perfdata"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "otl"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "notifications"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "neb"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "macros"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "lua"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "influxdb"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "grpc"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "graphite"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "functions"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "external_command"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "events"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "eventbroker"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "downtimes"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "core"
...    ${SPACE}${SPACE}value: "info"
...    }
...    level {
...    ${SPACE}${SPACE}key: "config"
...    ${SPACE}${SPACE}value: "info"
...    }
...    level {
...    ${SPACE}${SPACE}key: "comments"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "commands"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "checks"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "bbdo"
...    ${SPACE}${SPACE}value: "error"
...    }
...    level {
...    ${SPACE}${SPACE}key: "bam"
...    ${SPACE}${SPACE}value: "error"
...    }
...
