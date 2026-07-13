*** Settings ***
Documentation       Centreon Broker database TLS/SSL connection tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs


*** Variables ***
${Salt}             U2FsdA==
${AppSecret}        SGVsbG8gd29ybGQsIGRvZywgY2F0LCBwdXBwaWVzLgo=


*** Test Cases ***
BDBTSSLC1
    [Documentation]    Verify broker establishes TLS connection to database when DB_SSL_ENABLED is true
    [Tags]    broker    database    tls    MON-191981
    Skip If    '${DBSslEnabled}' != 'true'    Test requires DB_SSL_ENABLED=true

    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Log    central    sql    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_host    127.0.0.1
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_ssl_enabled    ${DBSslEnabled}
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_ssl_verify_cert    true


    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Check for TLS configuration message in broker logs
    ${content}    Create List    mysql_connection: configuring SSL/TLS connection
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=TLS configuration message not found in broker logs

    # Verify database connection was established successfully (not just configured)
    ${content}    Create List    unified sql: stream class instanciation
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=Database connection not established

    # Verify the connection is working by checking for SQL queries in logs
    # This confirms the SSL connection was successful and broker can communicate with DB
    ${content}    Create List    run query:
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=No SQL queries executed - TLS connection may have failed

BDBTEPWD1
    [Documentation]    Given a broker configured with unified_sql and an AES256-encrypted db_password
    ...    When broker starts with a valid engine-context.json containing the matching app_secret and salt
    ...    Then broker decrypts the password transparently and establishes a working connection to the database
    [Tags]    broker    database    encryption    MON-196879

    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_host    127.0.0.1

    # Start central broker to use its gRPC endpoint for AES256 password encryption
    Ctn Start Broker    only_central=${True}
    ${encrypted_passwd}    Ctn Aes Encrypt    51001    ${AppSecret}    ${Salt}    ${DBPass}
    Ctn Kindly Stop Broker    only_central=${True}

    # Create engine-context.json so broker can initialize its AES256 decryptor on startup
    Create File    /etc/centreon-engine/engine-context.json    {"app_secret":"${AppSecret}","salt":"${Salt}"}

    # Set the encrypted password in the unified_sql output configuration
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_password    encrypt::${encrypted_passwd}

    ${start}    Get Current Date
    Ctn Start Broker
    Ctn Start Engine

    # Verify database connection was established — this confirms the encrypted password was decrypted correctly
    ${content}    Create List    unified sql: stream class instanciation
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=Database connection not established - encrypted password decryption may have failed

    # Verify SQL queries are executed, confirming the connection is fully operational
    ${content}    Create List    run query:
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=No SQL queries executed - connection with encrypted password may have failed

    [Teardown]    Run Keywords
    ...    Remove File    /etc/centreon-engine/engine-context.json
    ...    AND    Ctn Stop Engine Broker And Save Logs

BDBTEPWD2
    [Documentation]    Given a broker configured with unified_sql and a db_password encrypted with a wrong app_secret
    ...    When broker starts with an engine-context.json containing a different app_secret and salt
    ...    Then broker fails to decrypt the password, logs "No usable encrypted password" and does not connect to the database
    [Tags]    broker    database    encryption    MON-196879

    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_host    127.0.0.1

    # Encrypt the password with the correct credentials
    Ctn Start Broker    only_central=${True}
    ${encrypted_passwd}    Ctn Aes Encrypt    51001    ${AppSecret}    ${Salt}    ${DBPass}
    Ctn Kindly Stop Broker    only_central=${True}

    # Create engine-context.json with a WRONG app_secret so decryption will fail
    ${WrongAppSecret}    Set Variable    d3JvbmcgYXBwIHNlY3JldCBmb3IgdGVzdGluZwo=
    Create File    /etc/centreon-engine/engine-context.json    {"app_secret":"${WrongAppSecret}","salt":"${Salt}"}

    # Set the encrypted password (encrypted with correct key, but broker will try to decrypt with wrong key)
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_password    encrypt::${encrypted_passwd}

    ${start}    Get Current Date
    Ctn Start Broker    only_central=${True}

    # Broker should log the decryption failure
    ${content}    Create List    No usable encrypted password:
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=Expected decryption error message not found in broker logs

    # Broker should NOT establish a database connection
    ${content}    Create List    unified sql: stream class instanciation
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    10
    Should Not Be True    ${result}    msg=Database connection was established despite wrong encryption key

    [Teardown]    Run Keywords
    ...    Remove File    /etc/centreon-engine/engine-context.json
    ...    AND    Ctn Stop Engine Broker And Save Logs

BDBTEPWD3
    [Documentation]    Given a broker configured with unified_sql and a db_password prefixed with "encrypt::"
    ...    When broker starts without any engine-context.json file
    ...    Then broker logs "encrypted password but no decrypt enabled" and does not connect to the database
    [Tags]    broker    database    encryption    MON-196879

    Ctn Config Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_host    127.0.0.1

    # Encrypt the password with valid credentials
    Ctn Start Broker    only_central=${True}
    ${encrypted_passwd}    Ctn Aes Encrypt    51001    ${AppSecret}    ${Salt}    ${DBPass}
    Ctn Kindly Stop Broker    only_central=${True}

    # Make sure no engine-context.json exists so credentials_decrypt is not initialized
    Remove File    /etc/centreon-engine/engine-context.json

    # Set the encrypted password — broker has no key to decrypt it
    Ctn Broker Config Output Set    central    central-broker-unified-sql    db_password    encrypt::${encrypted_passwd}

    ${start}    Get Current Date
    Ctn Start Broker    only_central=${True}

    # Broker should log that no decryption is available
    ${content}    Create List    encrypted password but no decrypt enabled
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    msg=Expected "no decrypt enabled" error message not found in broker logs

    # Broker should NOT establish a database connection
    ${content}    Create List    unified sql: stream class instanciation
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    10
    Should Not Be True    ${result}    msg=Database connection was established despite missing engine-context.json

