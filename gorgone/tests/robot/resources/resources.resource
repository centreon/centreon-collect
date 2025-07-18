*** Settings ***
Documentation       Centreon Gorgone library for Robot Framework

Library            Process
Library            RequestsLibrary
Library            OperatingSystem
Library             DatabaseLibrary

*** Variables ***
${gorgone_binary}             /usr/bin/gorgoned
${ROOT_CONFIG}                ${CURDIR}${/}..${/}config${/}
${pull_central_config}        ${ROOT_CONFIG}pull_central_config.yaml
${pull_poller_config}         ${ROOT_CONFIG}pull_poller_config.yaml
${pullwss_central_config}     ${ROOT_CONFIG}pullwss_central_config.yaml
${pullwss_poller_config}      ${ROOT_CONFIG}pullwss_poller_config.yaml
${push_central_config}        ${ROOT_CONFIG}push_central_config.yaml
${push_poller_config}         ${ROOT_CONFIG}push_poller_config.yaml
${gorgone_core_config}        ${ROOT_CONFIG}gorgone_core_central.yaml
${MOCKOON_LOG_FILE}           /tmp/mockoon.log

${DBHOST}                   127.0.0.1
${DBPORT}                   3306
${DBNAME}                   centreon_gorgone_test
${DBNAME_STORAGE}           centreon-storage
${DBUSER}                   centreon
${DBPASSWORD}               password

*** Keywords ***
Start Gorgone
    [Arguments]    ${SEVERITY}    ${ALIAS}
    ${process}    Start Process
    ...    /usr/bin/perl
    ...    ${gorgone_binary}
    ...    --config
    ...    /etc/centreon-gorgone/${ALIAS}/includer.yaml
    ...    --logfile
    ...    /var/log/centreon-gorgone/${ALIAS}/gorgoned.log
    ...    --severity
    ...    ${SEVERITY}
    ...    alias=${ALIAS}

Stop Gorgone And Remove Gorgone Config
    [Documentation]    This keyword stops the gorgone process and removes the configuration in the database. Configuration files are not modified as we want them in case something failed to analyse the problem.
    [Arguments]    @{process_alias}    ${sql_file}=
    Gorgone Execute Sql    ${sql_file}
    # remove configuration in db if needed.

    FOR    ${process}    IN    @{process_alias}
        ${result}    Terminate Process    ${process}
        BuiltIn.Run Keyword And Continue On Failure    Should Be True    ${result.rc} == -15 or ${result.rc} == 0    Gorgone ${process} badly stopped, code returned is ${result.rc}.
    END
    # this sudo is added by our robot test to emulate the sudo command when we are root in a container.
    ${output}=    Run    id -u
    IF    ${output} == 0
        Run    rm -f /usr/local/bin/sudo
    END
    
Start Mockoon
    [Arguments]    ${MOCKOON_JSON}
    Remove File    ${MOCKOON_LOG_FILE}

    ${time_start}    Get Time    str=epoch

    ${process}    Start Process
    ...    mockoon-cli
    ...    start
    ...    --data
    ...    ${MOCKOON_JSON}
    ...    --port
    ...    80
    ...    stdout=${MOCKOON_LOG_FILE}

    Wait Until Created    ${MOCKOON_LOG_FILE}
    Wait Until Keyword Succeeds
    ...     30
    ...     1
    ...     File Should Not Be Empty	${MOCKOON_LOG_FILE}

    ${time_end}    Get Time    str=epoch
    ${duration} =     Evaluate    ${time_end} - ${time_start}
    Log To Console    Mockoon finished starting after ${duration} seconds

Stop Mockoon
    Terminate All Processes
    #Remove File    ${MOCKOON_LOG_FILE}

Setup Sudo
    [Documentation]    This keyword setup a simple sudo command for when we are root in a container which don't have sudo installed.
    ${output}=    Run    id -u
    Log To Console    ${CURDIR}${/}sudo
    IF    ${output} == 0
        Run    cp ${CURDIR}${/}sudo /usr/local/bin/sudo
        Run    chmod +x /usr/local/bin/sudo
        Run    chown root:root /usr/local/bin/sudo
    END
Gorgone Execute Sql
    [Arguments]    ${sql_file}
    ${length}    Get Length    ${sql_file}
    IF    ${length} > 0
        Connect To Database    pymysql    ${DBNAME}    ${DBUSER}    ${DBPASSWORD}    ${DBHOST}    ${DBPORT}
        Log To Console    Executing sql file ${sql_file}
        Execute SQL Script    ${sql_file}
    END

Setup Gorgone Config
    [Arguments]        ${file_list}    ${gorgone_name}=gorgone_process_name        ${sql_file}=
    Gorgone Execute Sql    ${sql_file}
    Create Directory    /var/log/centreon-gorgone/${gorgone_name}/
    Copy File    ${CURDIR}${/}..${/}config${/}includer.yaml    /etc/centreon-gorgone/${gorgone_name}/includer.yaml

    FOR    ${file}    IN    @{file_list}
        Copy File    ${file}    /etc/centreon-gorgone/${gorgone_name}/config.d/
    END
    ${key_thumbprint}    Run    perl /usr/local/bin/gorgone_key_thumbprint.pl --key-path=/var/lib/centreon-gorgone/.keys/rsakey.priv.pem | cut -d: -f4

    ${result}    Run    sed -i -e 's/@UNIQ_ID_FROM_ROBOT_TESTING_CONFIG_FILE@/${gorgone_name}/g' /etc/centreon-gorgone/${gorgone_name}/includer.yaml

    ${CMD}    Catenate
    ...    sed -i -e 's/@KEYTHUMBPRINT@/${key_thumbprint}/g'
    ...    -e 's/@DBNAME@/${DBNAME}/g'
    ...    -e 's/@DBNAME_STORAGE@/${DBNAME_STORAGE}/g'
    ...    -e 's/@DBHOST@/${DBHOST}/g'
    ...    -e 's/@DBPASSWORD@/${DBPASSWORD}/g'
    ...    -e 's/@DBUSER@/${DBUSER}/g'
    ...    -e 's/@UNIQ_ID_FROM_ROBOT_TESTING_CONFIG_FILE@/${gorgone_name}/g'
    ...    /etc/centreon-gorgone/${gorgone_name}/config.d/*.yaml

    ${result2}    Run    ${CMD}

Check Poller Is Connected
    [Arguments]    ${port}=    ${expected_nb}=
    Log To Console    checking TCP connection is established...
    FOR    ${i}    IN RANGE    40
        Sleep    4
        ${nb_socket_connexion}    Run    ss -tnp | grep ':${port}' | grep ESTAB | wc -l
        IF    ${expected_nb} == ${nb_socket_connexion}
          BREAK
        END
    END
    Should Be True    ${i} < 39    Gorgone did not establish tcp connection in 160 seconds.
    Log To Console    TCP connection established after ${i} attempt (4 seconds each)

Check Poller Communicate
    [Documentation]    Ask the central Gorgone rest api if it have communicated with the poller using a given ID.
    [Arguments]    ${poller_id}    ${max_failed_attempt}=0
    ${response}     Set Variable    ${EMPTY}
    Log To Console    checking Gorgone see poller in rest api response...
    FOR    ${i}    IN RANGE    25
        Sleep    5
        ${response}=    GET  http://127.0.0.1:8085/api/internal/constatus
        Log    ${response.json()}
        IF    not ${response.json()}[data]
            CONTINUE
        END
        IF    ${response.json()}[data][${poller_id}][ping_failed] > ${max_failed_attempt} or ${response.json()}[data][${poller_id}][ping_ok] > 0
            BREAK
        END
    END
    Log To Console    json response : ${response.json()}
    Should Be True    ${i} < 24    timeout after ${i} time waiting for poller status in gorgone rest api (/api/internal/constatus) : ${response.json()}
    Should Be True    ${max_failed_attempt} >= ${response.json()}[data][${poller_id}][ping_failed]    there was failed ping between the central and the poller ${poller_id}
    Should Be True    0 < ${response.json()}[data][${poller_id}][ping_ok]    there was no successful ping between the central and the poller ${poller_id}

Setup Two Gorgone Instances
    [Arguments]    ${central_config}=@{EMPTY}    ${poller_config}=@{EMPTY}    ${communication_mode}=push_zmq    ${central_name}=gorgone_central    ${poller_name}=gorgone_poller_2
    ${start_time}   Get Time    str=epoch
    ${result}    Run    perl /usr/local/bin/gorgone_key_generation.pl
    # generate key if there is none.
    # gorgone can generate it's own key, but as we need the thumbprint in the configuration we need to generate them before launching gorgone.
    # this script only create key if the files don't exists, and silently finish if the files already exists.
    IF    '${communication_mode}' == 'push_zmq'
        @{central_push_config}=    Copy List    ${central_config}
        Append To List    ${central_push_config}    ${push_central_config}    ${gorgone_core_config}

        @{poller_push_config}=    Copy List    ${poller_config}
        Append To List    ${poller_push_config}    ${push_poller_config}

        Setup Gorgone Config    ${central_push_config}    gorgone_name=${central_name}    sql_file=${ROOT_CONFIG}db_add_1_poller.sql
        Setup Gorgone Config    ${poller_push_config}     gorgone_name=${poller_name}

        Start Gorgone    debug    ${poller_name}
        Wait Until Port Is Bind    5556
        Start Gorgone    debug    ${central_name}
        
        Check Poller Is Connected    port=5556    expected_nb=2
        Check Poller Communicate     2

    ELSE IF    '${communication_mode}' == 'pullwss'

        @{central_pullwss_config}=    Copy List    ${central_config}
        Append To List    ${central_pullwss_config}    ${pullwss_central_config}    ${ROOT_CONFIG}pullwss_node_register_one_node.yaml    ${gorgone_core_config}

        @{poller_pullwss_config}=    Copy List    ${poller_config}
        Append To List    ${poller_pullwss_config}    ${gorgone_core_config}    ${pullwss_poller_config}

        Setup Gorgone Config    ${central_pullwss_config}    gorgone_name=${central_name}    sql_file=${ROOT_CONFIG}db_add_1_poller.sql
        Setup Gorgone Config    ${poller_pullwss_config}     gorgone_name=${poller_name}

        Start Gorgone    debug    ${central_name}
        Wait Until Port Is Bind    8086
        Start Gorgone    debug    ${poller_name}
        Check Poller Is Connected    port=8086    expected_nb=2
        Check Poller Communicate     2
    ELSE IF    '${communication_mode}' == 'pull'
        @{central_pull_config}=    Copy List    ${central_config}
        Append To List    ${central_pull_config}    ${pull_central_config}    ${ROOT_CONFIG}pull_node_register_one_node.yaml    ${gorgone_core_config}

        @{poller_pull_config}=    Copy List    ${poller_config}
        Append To List    ${poller_pull_config}    ${pull_poller_config}    ${gorgone_core_config}

        Setup Gorgone Config    ${central_pull_config}    gorgone_name=${central_name}    sql_file=${ROOT_CONFIG}db_add_1_poller.sql
        Setup Gorgone Config    ${poller_pull_config}     gorgone_name=${poller_name}

        Start Gorgone    debug    ${central_name}
        Wait Until Port Is Bind    5556
        Start Gorgone    debug    ${poller_name}

        Check Poller Is Connected    port=5556    expected_nb=2
        Check Poller Communicate     2    max_failed_attempt=1
    ELSE
        Fail    Unknown communication mode: ${communication_mode}. Please use one of the following: push_zmq, pullwss, pull.
    END
    ${end_time}   Get Time    str=epoch
    ${duration}=    Evaluate    ${end_time} - ${start_time}
    Log To Console    Gorgone started in ${communication_mode} after ${duration} seconds
    
Wait Until Port Is Bind
    [Arguments]    ${port}
    FOR    ${i}    IN RANGE    15
        Sleep    0.5
        ${nb_port_listening}    Run    ss -tlnp | grep ':${port}' | grep LIST | wc -l
        IF    ${nb_port_listening} == 1
          BREAK
        END
    END

    Should Be True    ${i} < 14    Gorgone did not listen on port ${port} on time.
    Log To Console    tcp port ${port} bind after ${i} attempt (0.5 seconds each)

Ctn Check No Error In Logs
    [Arguments]    ${gorgone_id}
    ${cmd}=    Set Variable     grep -vP '^\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2} ' /var/log/centreon-gorgone/${gorgone_id}/gorgoned.log
    Log To Console    \n\n${cmd}\n\n

    ${log_line_wrong}    RUN    ${cmd}
    Should Be Empty     ${log_line_wrong}    There is Log in ${gorgone_id} not mathcing the standard gorgone format : ${log_line_wrong}
