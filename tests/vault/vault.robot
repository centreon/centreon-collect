*** Settings ***
Documentation       Centreon Broker and Engine start/stop tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BWVC1
    [Documentation]    Broker is tuned with a wrong vault configuration and the env file doesn't exist.
    [Tags]    broker    engine    MON-116610
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Add Item    central    vault_configuration    /tmp/wrong_file
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    core    error
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    ${content}    Create List    The env file could not be open
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No message about the env file that could not be open.
    Ctn Kindly Stop Broker

BWVC2
    [Documentation]    Broker is tuned with a wrong vault configuration and the env file exists.
    [Tags]    broker    engine    MON-116610
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Add Item    central    vault_configuration    /tmp/wrong_file
    Ctn Broker Config Add Item    central    env_file    /tmp/env_file
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    core    error

    ${env_file}    Catenate    SEPARATOR=\n
    ...    no sense

    Create File    /tmp/env_file    ${env_file}

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    ${content}    Create List    Bad value of the APP_SECRET
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No message about the bad value in APP_SECRET.
    Ctn Kindly Stop Broker

BWVC3
    [Documentation]    Broker is tuned with an env file containing a strange key APP_SECRET and a wrong vault configuration.
    [Tags]    broker    engine    MON-116610
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Add Item    central    vault_configuration    /tmp/wrong_file
    Ctn Broker Config Add Item    central    env_file    /tmp/env_file
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    core    error

    ${env_file}    Catenate    SEPARATOR=\n
    ...    APP_SECRET= turtle

    Create File    /tmp/env_file    ${env_file}

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    ${content}    Create List    Error while reading '/tmp/wrong_file'
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No message about the wrong vault file.
    Ctn Kindly Stop Broker

BWVC4
    [Documentation]    Broker is tuned with an env file containing a strange key APP_SECRET and a vault configuration with a bad json.
    [Tags]    broker    engine    MON-116610
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Add Item    central    vault_configuration    /tmp/vault_file.json
    Ctn Broker Config Add Item    central    env_file    /tmp/env_file
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    core    error

    ${vault_file}    Catenate    SEPARATOR=\n
    ...    {
    ...      "name": "vault",
    ...      "strange_key": 42
    ...    }

    Create File    /tmp/vault_file.json    ${vault_file}

    ${env_file}    Catenate    SEPARATOR=\n
    ...    APP_SECRET= turtle

    Create File    /tmp/env_file    ${env_file}

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    ${content}    Create List    The file '/tmp/vault_file.json' must contain keys 'salt', 'role_id' and 'secret_id'.
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    no message about wrong keys displayed.
    Ctn Kindly Stop Broker

BWVC5
    [Documentation]    Broker is tuned with strange keys APP_SECRET and salt.
    [Tags]    broker    engine    MON-116610
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Add Item    central    vault_configuration    /tmp/vault_file.json
    Ctn Broker Config Add Item    central    env_file    /tmp/env_file
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    core    error

    ${vault_file}    Catenate    SEPARATOR=\n
    ...    {
    ...      "name": "vault",
    ...      "strange_key": 42,
    ...      "salt": "strange",
    ...      "role_id": "strange",
    ...      "secret_id": "strange"
    ...    }

    Create File    /tmp/vault_file.json    ${vault_file}

    ${env_file}    Catenate    SEPARATOR=\n
    ...    APP_SECRET= turtle

    Create File    /tmp/env_file    ${env_file}

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    ${content}    Create List    The content is not AES256 encrypted
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    no message about the bad encryption.
    Ctn Kindly Stop Broker

BWVC6
    [Documentation]    Broker is tuned with strange keys APP_SECRET and salt that are not base64 encoded.
    [Tags]    broker    engine    MON-116610
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Add Item    central    vault_configuration    /tmp/vault_file.json
    Ctn Broker Config Add Item    central    env_file    /tmp/env_file
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    core    error

    ${vault_file}    Catenate    SEPARATOR=\n
    ...    {
    ...      "name": "vault",
    ...      "strange_key": 42,
    ...      "salt": "strange&éè",
    ...      "role_id": "strangeéé",
    ...      "secret_id": "strangeàà@"
    ...    }

    Create File    /tmp/vault_file.json    ${vault_file}

    ${env_file}    Catenate    SEPARATOR=\n
    ...    APP_SECRET= turtle

    Create File    /tmp/env_file    ${env_file}

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    ${content}    Create List    This string 'strange&éè' contains characters not legal in a base64 encoded string.
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    no message about the bad base64 encoding.
    Ctn Kindly Stop Broker

BAEOK
    [Documentation]    Broker is used to AES256 encrypt a content.
    [Tags]    broker    engine    MON-116610
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    core    error
    Ctn Start Broker

    ${encrypted}    Aes Encrypt    51001    ${AppSecret}    ${Salt}    The content to encode
    log to console    Encrypted: ${encrypted}
    ${final}    Aes Decrypt    51001    ${AppSecret}    ${Salt}    ${encrypted}
    log to console    Final: ${final}
    Should Be Equal    ${final}    The content to encode
    ...    AES Encrypting/Decrypting does not return the initial content.
    Ctn Kindly Stop Broker

BAEBS
    [Documentation]    Broker is used to AES256 encrypt a content but the salt is wrong.
    [Tags]    broker    engine    MON-116610
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    core    error
    Ctn Start Broker

    ${encrypted}    Aes Encrypt    51001    AppSecret    Sé\èalt    The content to encrypt
    Should Be Equal    ${encrypted}    This string 'Séèalt' contains characters not legal in a base64 encoded string.
    ...    We should have an RPC error during encoding.
    ${final}    Aes Decrypt    51001    AppSecret    Sé\èalt    ${encrypted}
    Should Be Equal    ${final}    This string 'Séèalt' contains characters not legal in a base64 encoded string.
    ...    We should have an RPC error during decoding.
    Ctn Kindly Stop Broker

BAEBC
    [Documentation]    Broker is used to AES256 encrypt a content but the salt is wrong.
    [Tags]    broker    engine    MON-116610
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    core    error
    Ctn Start Broker

    ${final}    Aes Decrypt    51001    AppSecret    Salt    Strange content to decrypt
    Should Be Equal    ${final}    The content is not AES256 encrypted
    ...    We should have an RPC error during decoding.
    Ctn Kindly Stop Broker

*** Variables ***
${Salt}        U2FsdA==
${AppSecret}   QXBwU2VjcmV0
