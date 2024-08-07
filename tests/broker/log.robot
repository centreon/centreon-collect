*** Settings ***
Documentation       Centreon Broker only start/stop tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes


*** Test Cases ***
BLDIS1
    [Documentation]    Start broker with core logs 'disabled'
    [Tags]    broker    start-stop    log-v2
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Broker Config Log    central    core    disabled
    Ctn Broker Config Log    central    sql    debug
    ${start}    Get Current Date
    Ctn Start Broker
    ${content}    Create List    [sql]
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    "No sql logs produced"

    ${content}    Create List    [core]
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be Equal    ${result}    ${False}    "We should not have core logs"
    Ctn Kindly Stop Broker

BLEC1
    [Documentation]    Change live the core level log from trace to debug
    [Tags]    broker    log-v2    grpc
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Broker Config Log    central    core    trace
    Ctn Broker Config Log    central    sql    debug
    ${start}    Get Current Date
    Ctn Start Broker
    ${result}    Ctn Get Broker Log Level    51001    core
    Should Be Equal    ${result}    trace
    Ctn Set Broker Log Level    51001    core    debug
    ${result}    Ctn Get Broker Log Level    51001    core
    Should Be Equal    ${result}    debug

BLEC2
    [Documentation]    Change live the core level log from trace to foo raises an error
    [Tags]    broker    log-v2    grpc
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Broker Config Log    central    core    trace
    Ctn Broker Config Log    central    sql    debug
    ${start}    Get Current Date
    Ctn Start Broker
    ${result}    Ctn Get Broker Log Level    51001    core
    Should Be Equal    ${result}    trace
    ${result}    Ctn Set Broker Log Level    51001    core    foo
    Should Be Equal    ${result}    Enum LogLevelEnum has no value defined for name 'FOO'

BLEC3
    [Documentation]    Change live the foo level log to trace raises an error
    [Tags]    broker    log-v2    grpc
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Broker Config Log    central    core    trace
    Ctn Broker Config Log    central    sql    debug
    ${start}    Get Current Date
    Ctn Start Broker
    ${result}    Ctn Set Broker Log Level    51001    foo    trace
    Should Be Equal    ${result}    The 'foo' logger does not exist

BLBD
    [Documentation]    Start Broker with loggers levels by default
    [Tags]    broker    log-v2    MON-143565
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Broker Config Remove Item    central    log:loggers
    ${start}    Get Current Date
    Ctn Start Broker
    ${result}    Ctn Get Broker Log Info    51001    ALL
    log to console    ${result}
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
