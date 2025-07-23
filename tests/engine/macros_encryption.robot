*** Settings ***
Documentation       Centreon Engine verify hostgroups inheritance.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs



*** Test Cases ***
NO_ENGINE_ENCRYPTION
    [Documentation]    Given an engine without configured encryption, we give him several macros and we expect to retrieve them in logs.
    [Tags]    engine    macros_decrypt    MON-158788
    Ctn Config Engine    ${1}    ${2}    ${10}
    Ctn Engine Config Set Value In Services    0    service_1    _CLEAR_MAC    clear_mac
    Ctn Engine Config Set Value In Services    0    service_1    _RAW_MAC    raw::raw_mac
    Ctn Engine Config Set Value In Services    0    service_1    _ENCRYPT_MAC    encrypt::encrypt_mac
    Ctn Engine Config Set Value    ${0}    log_level_commands    trace
    Ctn Engine Config Add Command    ${0}    with_mac_cmd   /bin/echo $_SERVICECLEAR_MAC$ $_SERVICERAW_MAC$ $_SERVICEENCRYPT_MAC$
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    with_mac_cmd

    Ctn Config Broker    module

    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${content}    Create List    read from stdout: clear_mac raw::raw_mac encrypt::encrypt_mac
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    "echo output not found in logs.
    
ENGINE_ENCRYPTION_BAD_CONF
    [Documentation]    Given an engine with configured encryption, but without key and salt, 
    ...    we give him several macros and we expect to retrieve them in logs without decrypt.
    [Tags]    engine    macros_decrypt    MON-158788
    Ctn Config Engine    ${1}    ${2}    ${10}
    Ctn Engine Config Add Value    0    credentials_encryption    1
    Ctn Engine Config Set Value In Services    0    service_1    _CLEAR_MAC    clear_mac
    Ctn Engine Config Set Value In Services    0    service_1    _RAW_MAC    raw::raw_mac
    Ctn Engine Config Set Value In Services    0    service_1    _ENCRYPT_MAC    encrypt::encrypt_mac
    Ctn Engine Config Set Value    ${0}    log_level_commands    trace
    Ctn Engine Config Add Command    ${0}    with_mac_cmd   /bin/echo $_SERVICECLEAR_MAC$ $_SERVICERAW_MAC$ $_SERVICEENCRYPT_MAC$
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    with_mac_cmd

    Ctn Config Broker    module

    Ctn Clear Retention
    Remove File    /etc/centreon-engine/engine-context.json

    ${start}    Ctn Get Round Current Date
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    
    ${content}    Create List     no encryption configured => can't decryp macro _SERVICEENCRYPT_MAC
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    error message not found in logs

    ${content}    Create List    read from stdout: clear_mac raw_mac encrypt::encrypt_mac
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    30
    Should Be True    ${result}    echo output not found in logs.
    

ENGINE_ENCRYPTION_GOOD_CONF
    [Documentation]    Given an engine with configured encryption, and key and salt, 
    ...    we give him several macros and we expect to retrieve them in logs without decrypt.
    [Tags]    engine    macros_decrypt    MON-158788

    #we need broker to encode values
    Ctn Config Broker    central
    Ctn Start Broker

    ${encrypted}    Ctn Aes Encrypt    51001    ${AppSecret}    ${Salt}    The content to encode
    log to console    Encrypted: ${encrypted}


    Ctn Config Engine    ${1}    ${2}    ${10}
    Ctn Engine Config Add Value    0    credentials_encryption    1
    
    Create File    /etc/centreon-engine/engine-context.json   {"app_secret":"${AppSecret}","salt":"${Salt}"}


    Ctn Engine Config Set Value In Services    0    service_1    _CLEAR_MAC    clear_mac
    Ctn Engine Config Set Value In Services    0    service_1    _RAW_MAC    raw::raw_mac
    Ctn Engine Config Set Value In Services    0    service_1    _ENCRYPT_MAC    encrypt::${encrypted}
    Ctn Engine Config Set Value    ${0}    log_level_commands    trace
    Ctn Engine Config Add Command    ${0}    with_mac_cmd   /bin/echo $_SERVICECLEAR_MAC$ $_SERVICERAW_MAC$ $_SERVICEENCRYPT_MAC$
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    with_mac_cmd

    Ctn Config Broker    module

    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Sleep    1s
    Remove File    /etc/centreon-engine/engine-context.json
    
    ${content}    Create List    read from stdout: clear_mac raw_mac The content to encode
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    60
    Should Be True    ${result}    echo output not found in logs.


*** Variables ***
${Salt}        U2FsdA==
${AppSecret}   SGVsbG8gd29ybGQsIGRvZywgY2F0LCBwdXBwaWVzLgo=
