*** Settings ***
Documentation       Centreon Broker Vault tests

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
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
    [Documentation]    Broker is used to AES256 decrypt a content not well encrypted
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

*** Variables ***
${Salt}        U2FsdA==
${AppSecret}   SGVsbG8gd29ybGQsIGRvZywgY2F0LCBwdXBwaWVzLgo=
