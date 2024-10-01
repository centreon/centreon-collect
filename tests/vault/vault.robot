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
    ${content}    Create List    The file '/tmp/vault_file.json' must contain keys 'salt', 'role_id', 'secret_id', url, port and root_path.
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
    ...      "salt": 42,
    ...      "role_id": "strange",
    ...      "secret_id": "strange",
    ...      "url": "foo",
    ...      "port": "bar",
    ...      "root_path": "foobar"
    ...    }

    Create File    /tmp/vault_file.json    ${vault_file}

    ${env_file}    Catenate    SEPARATOR=\n
    ...    APP_SECRET= ${AppSecret}

    Create File    /tmp/env_file    ${env_file}

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    ${content}    Create List    type must be string, but is number
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

    ${final}    Aes Decrypt    51001    ${AppSecret}    Salt    Strange content to decrypt
    Should Be Equal    ${final}    The content is not AES256 encrypted
    ...    We should have an RPC error during decoding.
    Ctn Kindly Stop Broker

BAV
    [Documentation]    Broker accesses to the vault to get database credentials.
    [Tags]    broker    MON-116610

    Ctn Start Vault
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    core    error
    Ctn Config BBDO3    1
    Ctn Start Broker

    ${encrypted_role_id}    Aes Encrypt    51001    ${AppSecret}    ${Salt}    12345678-1234-1234-1234-123456789abc
    ${encrypted_secret_id}    Aes Encrypt    51001    ${AppSecret}    ${Salt}    abcdef01-abcd-abcd-abcd-abcdef012345

    Ctn Kindly Stop Broker

    Ctn Broker Config Add Item    central    vault_configuration    /tmp/vault.json
    Ctn Broker Config Add Item    central    env_file    /tmp/env_file
    Ctn Broker Config Add Item    central    verify_vault_peer    no
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_password    secret::hashicorp_vault::johndoe/data/configuration/broker/08cb1f88-fc16-4d77-b27c-a97b2d5a1597::central-broker-master-unified-sql_db_password

    ${vault_content}    Catenate    SEPARATOR=\n
    ...    {
    ...      "name": "my_vault",
    ...      "url": "localhost",
    ...      "port": 4443,
    ...      "root_path": "john-doe",
    ...      "secret_id": "${encrypted_secret_id}",
    ...      "role_id": "${encrypted_role_id}",
    ...      "salt": "${Salt}"
    ...    }

    Create File    /tmp/vault.json    ${vault_content}

    ${env_file}    Catenate    SEPARATOR=\n
    ...    APP_SECRET= ${AppSecret}

    Create File    /tmp/env_file    ${env_file}

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker

    ${content}    Create List    Database password get from Vault configuration
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No message about the password found in the vault.

    Ctn Kindly Stop Broker
    Ctn Stop Vault

*** Variables ***
${Salt}        U2FsdA==
${AppSecret}   SGVsbG8gd29ybGQsIGRvZywgY2F0LCBwdXBwaWVzLgo=
