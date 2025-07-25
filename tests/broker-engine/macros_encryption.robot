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


BROKER_LUA_ENCRYPTION
    [Documentation]    Given an engine with configured encryption, and key and salt, 
    ...    cbmod and broker use a lua script that use encrypted credentials
    ...    we give it an encrypted macro that contains path to a lua output files and 
    ...    we expect that this file will becreated by lua.
    [Tags]    broker    macros_decrypt    MON-174126

    Remove File    /tmp/test-LUA.log
    Remove File    /tmp/output-central.txt
    Remove File    /tmp/output-cbmod.txt

    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker    rrd
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    neb    trace
    Ctn Broker Config Log    central    sql    error
    Ctn Broker Config Log    central    lua    debug

    Create File    /etc/centreon-engine/engine-context.json   {"app_secret":"${AppSecret}","salt":"${Salt}"}

    ${new_content}    Catenate    SEPARATOR=\n
    ...    function init(params)
    ...        logFile = params['log-file']
    ...        broker_log:info(0, 'lua start test')
    ...    end
    ...    
    ...    function write(e)
    ...      local file,err = io.open(logFile, 'a')
    ...      if file == nil then
    ...        broker_log:info(3, "Couldn't open file: " .. err)
    ...      else
    ...        file:write("event receive")
    ...        file:close()
    ...      end
    ...    end
    ...

    # Create the initial LUA script file
    Create File    /tmp/test-LUA.lua    ${new_content}

    ${encrypted}=    Set Variable    mCfbvFEBJzdIRS8+2vo7CoQxcEqOLQLr3PlVgXclU/SV8gsbV957Sg4nBfKYmZwJu1SkaZP007N9jYPPbzDX3dt4hKdZ4W9ktPlOPS7KgbOtmQxiBN6AyYNX3gZzMYMwAgqaUVwcdjS+5BxvgZvL7A==  # /tmp/output-cbmod.txt
    Ctn Broker Config Add Lua Output    central    test-LUA    /tmp/test-LUA.lua    {"name": "log-file", "type": "password", "value":"encrypt::${encrypted}"}

    ${encrypted}=    Set Variable    PyGtc0616AdGJxB81q7RCKSGXAkGv1ETtjKCKrtnR1e7NqXl+LXEfOlwnbeX1+XDexNr5HNhOpTJ58BIApeCFpsqY54biWFfiJaeF56ErvN2JaZxraRR21aHp36xCNWcIYaoLsP97giJ6jinbUbH/g==    #/tmp/output-cbmod.txt
    Ctn Broker Config Add Lua Output    module0    test-LUA    /tmp/test-LUA.lua    {"name": "log-file", "type": "password", "value":"encrypt::${encrypted}"}

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Wait Until Created    /tmp/output-central.txt    1min
    Wait Until Created    /tmp/output-cbmod.txt    10s


*** Variables ***
${Salt}        U2FsdA==
${AppSecret}   SGVsbG8gd29ybGQsIGRvZywgY2F0LCBwdXBwaWVzLgo=
