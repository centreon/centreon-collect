*** Settings ***
Documentation       Centreon Broker and Engine progressively add services

Resource            ../resources/resources.robot
Library             DatabaseLibrary
Library             Process
Library             OperatingSystem
Library             DateTime
Library             Collections
Library             ../resources/Engine.py
Library             ../resources/Broker.py
Library             ../resources/Common.py

Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes
Test Teardown       Save logs If Failed


*** Test Cases ***
BEATOI11
    [Documentation]    external command SEND_CUSTOM_HOST_NOTIFICATION with option_number=1 should work
    [Tags]    broker    engine    host    extcmd    notification
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    ${start}=    Get Current Date
    Start Broker
    Start Engine
    ${content}=    Create List    check_for_external_commands
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=No check for external commands executed for 1mn.
    SEND CUSTOM HOST NOTIFICATION    host_1    1    admin    BEATOI11
    ${content}=    Create List    EXTERNAL COMMAND: SEND_CUSTOM_HOST_NOTIFICATION;host_1;1;admin;BEATOI11
    ${result}=    Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=command argument notification_option must be an integer between 0 and 7.
    Stop Engine
    Kindly Stop Broker

BEATOI12
    [Documentation]    external command SEND_CUSTOM_HOST_NOTIFICATION with option_number>7 should fail
    [Tags]    broker    engine    host    extcmd    notification
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    ${start}=    Get Current Date
    Sleep    1s
    Start Broker
    Start Engine
    ${content}=    Create List    check_for_external_commands
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=No check for external commands executed for 1mn.
    SEND CUSTOM HOST NOTIFICATION    host_1    8    admin    BEATOI12
    ${content}=    Create List
    ...    Error: could not send custom host notification: '8' must be an integer between 0 and 7
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=command argument notification_option must be an integer between 0 and 7.
    Stop Engine
    Kindly Stop Broker

BEATOI13
    [Documentation]    external command SCHEDULE SERVICE DOWNTIME with duration<0 should fail
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    ${s tart}=    Get Current Date
    Start Broker
    Start Engine
    ${content}=    Create List    check_for_external_commands
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=No check for external commands executed for 1mn.
    ${date}=    Get Current Date    result_format=epoch
    SCHEDULE SERVICE DOWNTIME    host_1    service_1    -1
    ${content}=    Create List    Error: could not schedule downtime : duration
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=command argument duration must be an integer >= 0.
    Stop Engine
    Kindly Stop Broker

BEATOI21
    [Documentation]    external command ADD_HOST_COMMENT and DEL_HOST_COMMENT should work
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    ${start}=    Get Current Date
    Sleep    1s
    Start Broker
    Start Engine
    ${content}=    Create List    check_for_external_commands
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=No check for external commands executed for 1mn.
    ADD HOST COMMENT    host_1    1    user    comment
    ${content}=    Create List    ADD_HOST_COMMENT, comment_id: 1, data: comment
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=the comment with id:1 was not added.
    ${com_id}=    Find Internal Id    ${start}    True    30
    DEL HOST COMMENT    ${com_id}
    ${result}=    Find Internal Id    ${start}    False    30
    Should Be True    ${result}    msg=the comment with id:${com_id} was not deleted.
    Stop Engine
    Kindly Stop Broker

BEATOI22
    [Documentation]    external command DEL_HOST_COMMENT with comment_id<0 should fail
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    debug
    ${start}=    Get Current Date
    Sleep    1s
    Start Broker
    Start Engine
    ${content}=    Create List    check_for_external_commands
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=No check for external commands executed for 1mn.
    ADD HOST COMMENT    host_1    1    user    BEATOI22
    ${content}=    Create List    ADD_HOST_COMMENT, comment_id:    , data: BEATOI22
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=the comment with BEATOI22 content was not added
    ${com_id}=    Find Internal Id    ${start}    True    30
    DEL HOST COMMENT    -1
    ${content}=    Create List    Error: could not delete comment : comment_id
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=comment_id must be an unsigned integer.
    DEL HOST COMMENT    ${com_id}
    ${result}=    Find Internal Id    ${start}    False    30
    Should Be True    ${result}    msg=the comment with id:${com_id} was not deleted.
    Stop Engine
    Kindly Stop Broker

BEATOI23
    [Documentation]    external command ADD_SVC_COMMENT with persistent=0 should work
    [Tags]    broker    engine    host    extcmd
    Config Engine    ${1}    ${50}    ${20}
    Config Broker    rrd
    Config Broker    central
    Config Broker    module    ${1}
    Broker Config Log    central    core    error
    Broker Config Log    central    sql    error
    ${start}=    Get Current Date
    Sleep    1s
    Start Broker
    Start Engine
    ${content}=    Create List    check_for_external_commands
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=No check for external commands executed for 1mn.
    ADD SVC COMMENT    host_1    service_1    0    user    comment
    ${content}=    Create List    ADD_SVC_COMMENT, comment_id: 1, data: comment
    ${result}=    Find In Log with Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    msg=command argument persistent_flag must be 0 or 1.
    Stop Engine
    Kindly Stop Broker
