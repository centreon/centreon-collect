*** Settings ***
Documentation       Gorgone connects to the Centreon databases with TLS and checks the server certificate,
...                 with the DBD::MariaDB driver (MON-211445). The tests use a database user that requires TLS,
...                 so a working connection proves it is encrypted. Skipped when the database does not offer TLS.
Resource            ${CURDIR}${/}..${/}..${/}resources${/}import.resource
Test Timeout        120s
Suite Setup         Suite Setup Database Tls
Suite Teardown      Suite Teardown Database Tls

*** Variables ***
${tls_user}         centreon_tls
${gorgone_name}     gorgone_database_tls

*** Test Cases ***
Gorgone connects to the database with TLS
    [Documentation]    the nodes module reads the database with a user that requires TLS.
    Gorgone Database Connection Should Log
    ...    ;mariadb_ssl=1;mariadb_ssl_ca_file=${DBCAFILE};mariadb_ssl_verify_server_cert=1
    ...    [nodes] retrieved

Gorgone refuses a database certificate signed by another CA
    [Documentation]    the server certificate is really checked: gorgone does not connect with another CA.
    Gorgone Database Connection Should Log
    ...    ;mariadb_ssl=1;mariadb_ssl_ca_file=${DBWRONGCAFILE};mariadb_ssl_verify_server_cert=1
    ...    cannot connect to database    certificate

Gorgone cannot connect without TLS with a user requiring TLS
    [Documentation]    check the test user really requires TLS, so the first test proves the connection is encrypted.
    Gorgone Database Connection Should Log    ${EMPTY}    cannot connect to database    Access denied

*** Keywords ***
Suite Setup Database Tls
    Ctn Init Tests
    Connect To Database    pymysql    mysql    ${DBROOTUSER}    ${DBROOTPASSWORD}    ${DBHOST}    ${DBPORT}    alias=root
    ${have_ssl}    Query    SHOW VARIABLES LIKE 'have_ssl'    alias=root
    Skip If    '${have_ssl}[0][1]' != 'YES'    The database server does not offer TLS (have_ssl=${have_ssl}[0][1]).
    Execute SQL String    DROP USER IF EXISTS '${tls_user}'@'%'    alias=root
    Execute SQL String    CREATE USER '${tls_user}'@'%' IDENTIFIED BY '${DBPASSWORD}' REQUIRE SSL    alias=root
    Execute SQL String    GRANT ALL PRIVILEGES ON `${DBNAME}`.* TO '${tls_user}'@'%'    alias=root
    Execute SQL String    GRANT ALL PRIVILEGES ON `${DBNAME_STORAGE}`.* TO '${tls_user}'@'%'    alias=root

Suite Teardown Database Tls
    Run Keyword And Ignore Error    Execute SQL String    DROP USER IF EXISTS '${tls_user}'@'%'    alias=root
    Disconnect From All Databases

Gorgone Database Connection Should Log
    [Documentation]    start gorgone with the given DSN options and the TLS user, and wait for the given log lines.
    [Arguments]    ${dsn_options}    @{expected_logs}
    ${start_date}    Get Current Date    increment=-1s
    @{config}    Create List    ${ROOT_CONFIG}database_tls.yaml
    Setup Gorgone Config    ${config}    gorgone_name=${gorgone_name}
    ...    replace_from=${{['@TESTDSNOPTIONS@', '@TESTDBUSER@']}}
    ...    replace_to=${{[$dsn_options, $tls_user]}}
    Start Gorgone    debug    ${gorgone_name}
    ${result}    Ctn Find In Log With Timeout    log=/var/log/centreon-gorgone/${gorgone_name}/gorgoned.log
    ...    content=${expected_logs}    date=${start_date}    timeout=40
    Stop Gorgone And Remove Gorgone Config    ${gorgone_name}
    Should Be True    ${result}    Log not found in /var/log/centreon-gorgone/${gorgone_name}/gorgoned.log: ${expected_logs}
