*** Settings ***
Documentation       Centreon Broker Vault tests in centralized configuration

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed
Test Tags           broker    MON-116610


*** Test Cases ***
CBWVC1
    [Documentation]    Scenario: Broker with missing vault env file logs an error
    ...    Given broker configured with a wrong vault configuration and no env file
    ...    When broker starts
    ...    Then broker logs an error that the env file could not be opened
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Add Item    central    vault_configuration    /tmp/wrong_file
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_password    secret::hashicorp_vault::johndoe/data/configuration/broker/08cb1f88-fc16-4d77-b27c-a97b2d5a1597::central-broker-master-unified-sql_db_password
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    VAR    @{content}    The env file could not be open
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No message about the env file that could not be open.
    Ctn Kindly Stop Broker

CBWVC2
    [Documentation]    Scenario: Broker with env file missing APP_SECRET logs an error
    ...    Given broker configured with a wrong vault configuration
    ...    And an env file with invalid content (no APP_SECRET)
    ...    When broker starts
    ...    Then broker logs an error about missing APP_SECRET
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Add Item    central    vault_configuration    /tmp/wrong_file
    Ctn Broker Config Add Item    central    env_file    /tmp/env_file
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_password    secret::hashicorp_vault::johndoe/data/configuration/broker/08cb1f88-fc16-4d77-b27c-a97b2d5a1597::central-broker-master-unified-sql_db_password
    VAR    ${env_file}    no sense
    Create File    /tmp/env_file    ${env_file}
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    VAR    @{content}    No usable Vault configuration: No APP_SECRET provided.
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No message about the bad value in APP_SECRET.
    Ctn Kindly Stop Broker

CBWVC3
    [Documentation]    Scenario: Broker with wrong vault file path logs a JSON parse error
    ...    Given broker configured with a strange APP_SECRET and a non-existent vault file
    ...    When broker starts
    ...    Then broker logs an error about the wrong vault file
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Add Item    central    vault_configuration    /tmp/wrong_file
    Ctn Broker Config Add Item    central    env_file    /tmp/env_file
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    core    error
    VAR    ${env_file}    APP_SECRET= turtle
    Create File    /tmp/env_file    ${env_file}
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    VAR    @{content}    No usable Vault configuration: .*check that your input string or stream contains the expected JSON
    ${result}    Ctn Find Regex In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No message about the wrong vault file.
    Ctn Kindly Stop Broker

CBWVC4
    [Documentation]    Scenario: Broker with malformed vault JSON file logs an error
    ...    Given broker configured with a strange APP_SECRET and a vault file missing required keys
    ...    When broker starts
    ...    Then broker logs an error about the malformed vault file
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Add Item    central    vault_configuration    /tmp/vault_file.json
    Ctn Broker Config Add Item    central    env_file    /tmp/env_file
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_password    secret::hashicorp_vault::johndoe/data/configuration/broker/08cb1f88-fc16-4d77-b27c-a97b2d5a1597::central-broker-master-unified-sql_db_password
    VAR    ${vault_file}    SEPARATOR=\n
    ...    {
    ...      "name": "vault",
    ...      "strange_key": 42
    ...    }
    Create File    /tmp/vault_file.json    ${vault_file}
    VAR    ${env_file}    APP_SECRET= turtle
    Create File    /tmp/env_file    ${env_file}
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    VAR    @{content}    No usable Vault configuration: The '/tmp/vault_file.json' file is malformed, we should have keys 'salt', 'role_id'
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    no message about wrong keys displayed.
    Ctn Kindly Stop Broker

CBWVC5
    [Documentation]    Scenario: Broker with non-string salt in vault file logs a type error
    ...    Given broker configured with a strange APP_SECRET and a vault file with numeric salt
    ...    When broker starts
    ...    Then broker logs an error about the bad encryption type
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Add Item    central    vault_configuration    /tmp/vault_file.json
    Ctn Broker Config Add Item    central    env_file    /tmp/env_file
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_password    secret::hashicorp_vault::johndoe/data/configuration/broker/08cb1f88-fc16-4d77-b27c-a97b2d5a1597::central-broker-master-unified-sql_db_password
    VAR    ${vault_file}    SEPARATOR=\n
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
    VAR    ${env_file}    APP_SECRET= ${AppSecret}
    Create File    /tmp/env_file    ${env_file}
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    VAR    @{content}    type must be string, but is number
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    no message about the bad encryption.
    Ctn Kindly Stop Broker

CBWVC6
    [Documentation]    Scenario: Broker with non-base64 salt in vault file logs an encoding error
    ...    Given broker configured with APP_SECRET and a vault file containing non-base64 salt
    ...    When broker starts
    ...    Then broker logs an error about the bad base64 encoding
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Add Item    central    vault_configuration    /tmp/vault_file.json
    Ctn Broker Config Add Item    central    env_file    /tmp/env_file
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_password    secret::hashicorp_vault::johndoe/data/configuration/broker/08cb1f88-fc16-4d77-b27c-a97b2d5a1597::central-broker-master-unified-sql_db_password
    VAR    ${vault_file}    SEPARATOR=\n
    ...    {
    ...      "name": "vault",
    ...      "port": 42,
    ...      "salt": "strange&éè",
    ...      "role_id": "strangeéé",
    ...      "secret_id": "strangeàà@",
    ...      "url": "my_url",
    ...      "root_path": "my_path"
    ...    }
    Create File    /tmp/vault_file.json    ${vault_file}
    VAR    ${env_file}    APP_SECRET= turtle
    Create File    /tmp/env_file    ${env_file}
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    VAR    @{content}    This string 'strange&éè' contains characters not legal in a base64 encoded string.
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    no message about the bad base64 encoding.
    Ctn Kindly Stop Broker

CBAEOK
    [Documentation]    Scenario: AES256 encrypt then decrypt returns the original content
    ...    Given broker is started in centralized mode
    ...    When AES256 encryption is applied to content
    ...    Then the decrypted result matches the original content
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    core    error
    Ctn Start Broker    newGeneration=True
    ${encrypted}    Ctn Aes Encrypt    51001    ${AppSecret}    ${Salt}    The content to encode
    Log To Console    Encrypted: ${encrypted}
    ${final}    Ctn Aes Decrypt    51001    ${AppSecret}    ${Salt}    ${encrypted}
    Log To Console    Final: ${final}
    Should Be Equal    ${final}    The content to encode
    ...    AES Encrypting/Decrypting does not return the initial content.
    Ctn Kindly Stop Broker

CBAEBS
    [Documentation]    Scenario: AES256 encryption with invalid base64 salt returns an error
    ...    Given broker is started in centralized mode
    ...    When AES256 encryption is attempted with a non-base64 salt
    ...    Then broker returns an error about illegal base64 characters
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    core    error
    Ctn Start Broker    newGeneration=True
    ${encrypted}    Ctn Aes Encrypt    51001    AppSecret    Sé\èalt    The content to encrypt
    Should Be Equal    ${encrypted}    This string 'Séèalt' contains characters not legal in a base64 encoded string.
    ...    We should have an RPC error during encoding.
    ${final}    Ctn Aes Decrypt    51001    AppSecret    Sé\èalt    ${encrypted}
    Should Be Equal    ${final}    This string 'Séèalt' contains characters not legal in a base64 encoded string.
    ...    We should have an RPC error during decoding.
    Ctn Kindly Stop Broker

CBAEBC
    [Documentation]    Scenario: AES256 decryption of non-encrypted content returns an error
    ...    Given broker is started in centralized mode
    ...    When AES256 decryption is attempted on content that is not properly encrypted
    ...    Then broker returns an error indicating the content is not AES256 encrypted
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    core    error
    Ctn Start Broker    newGeneration=True
    ${final}    Ctn Aes Decrypt    51001    ${AppSecret}    Salt    Strange content to decrypt
    Should Be Equal    ${final}    The content is not AES256 encrypted
    ...    We should have an RPC error during decoding.
    Ctn Kindly Stop Broker

CBAV
    [Documentation]    Scenario: Broker retrieves database password from a running vault
    ...    Given broker is started in centralized mode
    ...    And a vault is running with valid credentials
    ...    When broker is configured to retrieve the database password from the vault
    ...    Then broker logs that the database password was retrieved from vault
    Ctn Start Vault
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    core    error
    Ctn Start Broker    newGeneration=True
    ${encrypted_role_id}    Ctn Aes Encrypt    51001    ${AppSecret}    ${Salt}    12345678-1234-1234-1234-123456789abc
    ${encrypted_secret_id}    Ctn Aes Encrypt    51001    ${AppSecret}    ${Salt}    abcdef01-abcd-abcd-abcd-abcdef012345
    Ctn Kindly Stop Broker
    Ctn Broker Config Add Item    central    vault_configuration    /tmp/vault.json
    Ctn Broker Config Add Item    central    env_file    /tmp/env_file
    Ctn Broker Config Add Item    central    verify_vault_peer    no
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_password    secret::hashicorp_vault::johndoe/data/configuration/broker/08cb1f88-fc16-4d77-b27c-a97b2d5a1597::central-broker-master-unified-sql_db_password
    VAR    ${vault_content}    SEPARATOR=\n
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
    VAR    ${env_file}    APP_SECRET= ${AppSecret}
    Create File    /tmp/env_file    ${env_file}
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    VAR    @{content}    Database password get from Vault configuration
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No message about the password found in the vault.
    Ctn Kindly Stop Broker
    Ctn Stop Vault

CBASV
    [Documentation]    Scenario: Broker with vault configured but vault server down logs an error
    ...    Given broker is started in centralized mode
    ...    And the vault server is not running
    ...    When broker is configured to retrieve credentials from the vault
    ...    Then broker logs an error about the inactive http server
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    core    error
    Ctn Start Broker    newGeneration=True
    ${encrypted_role_id}    Ctn Aes Encrypt    51001    ${AppSecret}    ${Salt}    12345678-1234-1234-1234-123456789abc
    ${encrypted_secret_id}    Ctn Aes Encrypt    51001    ${AppSecret}    ${Salt}    abcdef01-abcd-abcd-abcd-abcdef012345
    Ctn Kindly Stop Broker
    Ctn Broker Config Add Item    central    vault_configuration    /tmp/vault.json
    Ctn Broker Config Add Item    central    env_file    /tmp/env_file
    Ctn Broker Config Add Item    central    verify_vault_peer    no
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_password    secret::hashicorp_vault::johndoe/data/configuration/broker/08cb1f88-fc16-4d77-b27c-a97b2d5a1597::central-broker-master-unified-sql_db_password
    VAR    ${vault_content}    SEPARATOR=\n
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
    VAR    ${env_file}    APP_SECRET= ${AppSecret}
    Create File    /tmp/env_file    ${env_file}
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    VAR    @{content}    No usable Vault configuration: Error from http server
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    No message about the inactivity of the http server.
    Ctn Kindly Stop Broker


*** Variables ***
${Salt}        U2FsdA==
${AppSecret}   SGVsbG8gd29ybGQsIGRvZywgY2F0LCBwdXBwaWVzLgo=
