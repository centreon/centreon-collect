*** Settings ***
Documentation       test gorgone legacycmd module
Resource            ${CURDIR}${/}..${/}..${/}resources${/}import.resource
Test Timeout        220s


*** Test Cases ***
Legacycmd with ${communication_mode} communication
    [Documentation]    Check Legacycmd module work.
    ${tmp}=    Set Variable    ${communication_mode}
    ${central}=    Set Variable    ${communication_mode}_gorgone_central_legacycmd
    ${poller}=    Set Variable    ${communication_mode}_gorgone_poller2_legacycmd

    [Teardown]    Legacycmd Teardown    central=${central}    poller=${poller}    comm=${tmp}

    Run    mkdir /var/lib/centreon/centcore/ -p

    @{central_config}    Create List    ${ROOT_CONFIG}legacycmd.yaml    ${ROOT_CONFIG}engine.yaml    ${ROOT_CONFIG}actions.yaml
    @{poller_config}    Create List     ${ROOT_CONFIG}actions.yaml    ${ROOT_CONFIG}engine.yaml
    Setup Two Gorgone Instances
    ...    central_config=${central_config}
    ...    communication_mode=${communication_mode}
    ...    central_name=${central}
    ...    poller_name=${poller}
    ...    poller_config=${poller_config}

    Force Check Execution On Poller    comm=${communication_mode}    poller_id=${poller_id}
    Push Engine And vmware Configuration    comm=${communication_mode}    poller_id=${poller_id}
    Test SYNCTRAP Execution        poller_id=${poller_id}
    Examples:    communication_mode   poller_id    --
        ...    push_zmq       2
        ...    push_zmq       299123456
        ...    push_zmq_uid   2
        ...    push_zmq_uid   299123456
        ...    pullwss        2
        ...    pullwss        299123456
        ...    pullwss_uid    2
        ...    pullwss_uid    299123456
        ...    pull           2
        ...    pull           299123456

Legacycmd reports a final batch result over the HTTP API
    [Documentation]    Given a connected poller
    ...    When a SENDCFGFILE CENTREONCOMMAND batch is posted to the HTTP API
    ...    Then one aggregated FINISH_OK is reported once every sub-action copy is done
    ...    And an EXTERNALCMD batch (not trackable) closes immediately as untracked
    [Tags]    gorgone    legacycmd    MON-214293
    ${central}=    Set Variable    pullwss_gorgone_central_legacycmd_final
    ${poller}=    Set Variable    pullwss_gorgone_poller2_legacycmd_final
    [Teardown]    Legacycmd Final Result Teardown    central=${central}    poller=${poller}

    @{central_config}    Create List    ${ROOT_CONFIG}legacycmd.yaml    ${ROOT_CONFIG}engine.yaml    ${ROOT_CONFIG}actions.yaml
    @{poller_config}    Create List     ${ROOT_CONFIG}actions.yaml    ${ROOT_CONFIG}engine.yaml
    Setup Two Gorgone Instances
    ...    central_config=${central_config}
    ...    communication_mode=pullwss
    ...    central_name=${central}
    ...    poller_name=${poller}
    ...    poller_config=${poller_config}

    Stage Engine And vmware Configuration    poller_id=2    comm=pullwss

    ${command}    Create Dictionary    command=SENDCFGFILE    target=2
    ${body}    Create List    ${command}
    ${response}    POST    http://127.0.0.1:8085/api/centreon/legacycmd/command    json=${body}
    ${status}    ${result}    Ctn Get Legacycmd Final Result    token=${response.json()}[token]    timeout=30
    Should Be True    ${status}    No final batch result found: ${result}
    Should Be Equal As Integers    ${result}[code]    2    batch should report success, got: ${result}
    Should Be Equal As Integers    ${result}[data][tracked]    1    SENDCFGFILE must be tracked
    ${results_count}    Get Length    ${result}[data][results]
    Should Be Equal As Integers    ${results_count}    3    vmware+engine+broker copies expected

    # EXTERNALCMD isn't in %BATCH_SAFE_COMMANDS (ENGINECOMMAND logs two FINISH_OK,
    # see class.pm): the batch must close immediately as untracked, not time out.
    ${date}    Get Current Date    result_format=epoch
    ${forced_check}    Set Variable    [${date}] SCHEDULE_FORCED_SVC_CHECK;local2_pullwss;Cpu;${date}
    ${command}    Create Dictionary    command=EXTERNALCMD    target=2    param=${forced_check}
    ${body}    Create List    ${command}
    ${response}    POST    http://127.0.0.1:8085/api/centreon/legacycmd/command    json=${body}
    ${status}    ${result}    Ctn Get Legacycmd Final Result    token=${response.json()}[token]    timeout=10
    Should Be True    ${status}    No final result found for the untracked EXTERNALCMD batch: ${result}
    Should Be Equal As Integers    ${result}[code]    2    an untracked batch always reports accepted
    Should Be Equal As Integers    ${result}[data][tracked]    0    EXTERNALCMD must not be tracked

Legacycmd retries loading the pollers cache right away after a database outage
    [Documentation]    Given gorgone-central's pollers cache fails to load (e.g. cfg_nagios
    ...    unavailable while an install is still running)
    ...    When a CENTREONCOMMAND arrives once the table is available again
    ...    Then it succeeds right away instead of waiting cache_refresh_interval seconds
    [Tags]    gorgone    legacycmd    MON-214293
    ${central}=    Set Variable    pullwss_gorgone_central_legacycmd_cache
    ${poller}=    Set Variable    pullwss_gorgone_poller2_legacycmd_cache
    [Teardown]    Legacycmd Cache Refresh Teardown    central=${central}    poller=${poller}

    @{central_config}    Create List    ${ROOT_CONFIG}legacycmd.yaml    ${ROOT_CONFIG}engine.yaml    ${ROOT_CONFIG}actions.yaml
    @{poller_config}    Create List     ${ROOT_CONFIG}actions.yaml    ${ROOT_CONFIG}engine.yaml
    Setup Two Gorgone Instances
    ...    central_config=${central_config}
    ...    communication_mode=pullwss
    ...    central_name=${central}
    ...    poller_name=${poller}
    ...    poller_config=${poller_config}

    # Force legacycmd's very first cache load on this fresh process to fail, like a
    # restart racing an install that hasn't created cfg_nagios yet. The nodes module
    # (poller registration) doesn't read cfg_nagios, so the poller still reconnects.
    Execute SQL String    RENAME TABLE cfg_nagios TO cfg_nagios_robot_tmp    alias=${DBNAME}
    Stop Gorgone And Remove Gorgone Config    ${central}
    Start Gorgone    debug    ${central}
    Wait Until Port Is Bind    ${Ccom_port}
    Wait Until Port Is Bind    8085
    Check Poller Is Connected    port=${Ccom_port}    expected_nb=2

    ${command}    Create Dictionary    command=RELOAD    target=2
    ${body}    Create List    ${command}
    ${response}    POST    http://127.0.0.1:8085/api/centreon/legacycmd/command    json=${body}
    ${status}    ${result}    Ctn Get Legacycmd Final Result    token=${response.json()}[token]    timeout=15
    Should Be True    ${status}    No final batch result found: ${result}
    Should Be Equal As Integers    ${result}[code]    1    batch should fail while cfg_nagios is unavailable
    Should Be Equal As Strings    ${result}[data][message]    cannot get centreon database configuration
    ...    the KO must come from the missing table, not e.g. the poller not being reconnected yet

    Execute SQL String    RENAME TABLE cfg_nagios_robot_tmp TO cfg_nagios    alias=${DBNAME}

    ${command2}    Create Dictionary    command=RELOAD    target=2
    ${body2}    Create List    ${command2}
    ${response2}    POST    http://127.0.0.1:8085/api/centreon/legacycmd/command    json=${body2}
    ${status2}    ${result2}    Ctn Get Legacycmd Final Result    token=${response2.json()}[token]    timeout=15
    Should Be True    ${status2}    No final batch result found: ${result2}
    Should Be Equal As Integers    ${result2}[code]    2    the very next command must succeed, not wait cache_refresh_interval seconds
    Should Not Be Equal As Strings    ${result2}[data][message]    cannot get centreon database configuration
    ...    the cache must have reloaded, not failed again

Legacycmd reports a final KO when the target poller is not connected
    [Documentation]    Given a poller that was connected and lost its pullwss link
    ...    When a RELOAD CENTREONCOMMAND is posted while it is disconnected
    ...    Then the batch ends in a FINISH_KO, instead of an unresolved token (MON-214293)
    [Tags]    gorgone    legacycmd    MON-214293
    ${central}=    Set Variable    pullwss_gorgone_central_legacycmd_ko
    ${poller}=    Set Variable    pullwss_gorgone_poller2_legacycmd_ko
    [Teardown]    Legacycmd Final Result Teardown    central=${central}    poller=${poller}

    @{central_config}    Create List    ${ROOT_CONFIG}legacycmd.yaml    ${ROOT_CONFIG}engine.yaml    ${ROOT_CONFIG}actions.yaml
    @{poller_config}    Create List     ${ROOT_CONFIG}actions.yaml    ${ROOT_CONFIG}engine.yaml
    Setup Two Gorgone Instances
    ...    central_config=${central_config}
    ...    communication_mode=pullwss
    ...    central_name=${central}
    ...    poller_name=${poller}
    ...    poller_config=${poller_config}

    # was connected, now isn't - the ticket's own scenario (a restart, a command sent
    # during the reconnection window), not a poller that never connected at all.
    # Line-count diff, not a date search: Terminate Process can return the same
    # second the disconnect gets logged (second-precision timestamps), which a
    # date-based search can miss depending on sub-second ordering.
    ${central_log_file}    Set Variable    /var/log/centreon-gorgone/${central}/gorgoned.log
    ${lines_before}    Run    wc -l < ${central_log_file}
    Terminate Process    ${poller}

    ${disconnected}    Set Variable    ${False}
    FOR    ${i}    IN RANGE    20
        ${new_lines}    Run    tail -n +$((${lines_before} + 1)) ${central_log_file} | grep -c 'websocket client disconnected'
        IF    ${new_lines} > 0
            ${disconnected}    Set Variable    ${True}
            BREAK
        END
        Sleep    1s
    END
    Should Be True    ${disconnected}    Central never saw the poller disconnect

    ${command}    Create Dictionary    command=RELOAD    target=2
    ${body}    Create List    ${command}
    ${response}    POST    http://127.0.0.1:8085/api/centreon/legacycmd/command    json=${body}
    ${status}    ${result}    Ctn Get Legacycmd Final Result    token=${response.json()}[token]    timeout=15
    Should Be True    ${status}    No final batch result found: ${result}
    Should Be Equal As Integers    ${result}[code]    1    batch should report failure, got: ${result}
    Should Be Equal As Integers    ${result}[data][tracked]    1    RELOAD must be tracked
    Should Not Be Equal As Strings    ${result}[data][message]    timeout waiting for sub-actions to finish    KO must come from the disconnected node, not the batch timeout

*** Keywords ***
Legacycmd Final Result Teardown
    [Arguments]    ${central}    ${poller}
    @{process_list}    Create List    ${central}    ${poller}
    Stop Gorgone And Remove Gorgone Config    @{process_list}    sql_file=${ROOT_CONFIG}database/delete_pollers.sql
    Run    rm -rf /var/cache/centreon/config
    Run    rm -rf /etc/centreon/centreon_vmware.json /etc/centreon-engine/randomBigFile.cfg /etc/centreon-engine/engine-hosts.cfg /etc/centreon-broker/broker.cfg

Legacycmd Cache Refresh Teardown
    [Arguments]    ${central}    ${poller}
    # best-effort: already restored if the test's own rename-back ran.
    Run Keyword And Ignore Error    Execute SQL String    RENAME TABLE cfg_nagios_robot_tmp TO cfg_nagios    alias=${DBNAME}
    Legacycmd Final Result Teardown    central=${central}    poller=${poller}

Legacycmd Teardown
    [Arguments]    ${central}    ${poller}    ${comm}
    @{process_list}    Create List    ${central}    ${poller}

    Stop Gorgone And Remove Gorgone Config    @{process_list}    sql_file=${ROOT_CONFIG}database/delete_pollers.sql
    Terminate Process    pipeWatcher_${comm}
    Run    rm -rf /var/cache/centreon/config
    Run    rm -rf /etc/centreon/centreon_vmware.json
    Run    rm -rf /etc/centreon-engine/randomBigFile.cfg
    Run    rm -rf /etc/centreon-engine/engine-hosts.cfg
    Run    rm -rf /etc/centreon-broker/broker.cfg
    Run    rm -f /etc/snmp/centreon_traps/centreontrapd.sdb
    
Stage Engine And vmware Configuration
    [Documentation]    Drops the vmware/engine/broker fixtures where gorgone expects to find
    ...    them (the part of SENDCFGFILE's job that happens before gorgone is even asked to
    ...    copy them), without triggering the copy itself - callers trigger it their own way.
    [Arguments]    ${comm}=    ${poller_id}=2

    Copy File    ${CURDIR}${/}legacycmd${/}centreon_vmware.json    /var/cache/centreon/config/vmware/${poller_id}/
    Copy File    ${CURDIR}${/}legacycmd${/}broker.cfg    /var/cache/centreon/config/broker/${poller_id}/
    Copy File    ${CURDIR}${/}legacycmd${/}engine-hosts.cfg    /var/cache/centreon/config/engine/${poller_id}/
    # we change all the configuration files to be sure it was copied in this run and not a rest of another test.
    Run    sed -i -e 's/@COMMUNICATION_MODE@/${comm}/g' /var/cache/centreon/config/vmware/${poller_id}/centreon_vmware.json
    Run    sed -i -e 's/@COMMUNICATION_MODE@/${comm}/g' /var/cache/centreon/config/broker/${poller_id}/broker.cfg
    Run    sed -i -e 's/@COMMUNICATION_MODE@/${comm}/g' /var/cache/centreon/config/engine/${poller_id}/engine-hosts.cfg
    Run    chown www-data:www-data /var/cache/centreon/config/*/${poller_id}/*
    Run    chmod 644 /var/cache/centreon/config/*/${poller_id}/*

Push Engine And vmware Configuration
    [Arguments]    ${comm}=    ${poller_id}=2

    Stage Engine And vmware Configuration    comm=${comm}    poller_id=${poller_id}
    Run    dd if=/dev/urandom of=/var/cache/centreon/config/engine/${poller_id}/randomBigFile.cfg bs=60MB count=1 iflag=fullblock
    Run    chown www-data:www-data /var/cache/centreon/config/engine/${poller_id}/randomBigFile.cfg
    Run    chmod 644 /var/cache/centreon/config/engine/${poller_id}/randomBigFile.cfg
    ${MD5Start}=    Run    md5sum /var/cache/centreon/config/engine/${poller_id}/randomBigFile.cfg | cut -f 1 -d " "

    # gorgone central should get these files, and send it to poller in /etc/centreon/, /etc/centreon-broker/, /etc/centreon-engine/
    # we are checking the poller have the last bit of centreon-engine before continuing.
    ${log_query}    Create List    Copy to '/etc/centreon-engine//' finished successfully
    # SENDCFGFILE say to gorgone to push conf to poller for a poller id.
    Run    echo SENDCFGFILE:${poller_id} > /var/lib/centreon/centcore/random.cmd
    ${log_status}    Ctn Find In Log With Timeout    log=/var/log/centreon-gorgone/${comm}_gorgone_poller2_legacycmd/gorgoned.log    content=${log_query}    regex=0    timeout=40
    Should Be True    ${log_status}    Didn't found the logs : ${log_status}
    Log To Console    File should be set in /etc/centreon/ now

    # check vmware conf file
    ${res}=    Run    cat /etc/centreon/centreon_vmware.json
    Should Be Equal As Strings    ${res}    {"communication mode": "${comm}"}    data in /etc/centreon/centreon_vmware.json is not correct.
    # check the user/group and permission are right. as gorgone run as root in the tests and as centreon-gorgone in prod, this might be different from real life.
    ${vmware_stat}=    Run    stat -c "%a %U %G" /etc/centreon/centreon_vmware.json
    Should Be Equal As Strings    ${vmware_stat}    644 centreon-gorgone centreon    for vmware file

    ${MD5Result}=    Run  md5sum /etc/centreon-engine/randomBigFile.cfg | cut -f 1 -d " "
    Should Be Equal    ${MD5Start}    ${MD5Result}    MD5 Don't match, the big file might have been corrupted.

    # check engine conf file
    # for now gorgone don't set user/group after it untar, it's only done when copying single files.
    # We can't check the user in the test as "www-data" user is "httpd" on rhel based system
    ${res}=    Run    cat /etc/centreon-engine/engine-hosts.cfg
    Should Be Equal As Strings    ${res}    Engine conf, communicationmode:${comm}    data in /etc/centreon-engine/engine-hosts.cfg is not correct.

    #check Broker conf file
    ${res}=    Run    cat /etc/centreon-broker/broker.cfg
    Should Be Equal As Strings    ${res}    Broker conf, communication mode:${comm}    data in /etc/centreon-broker/broker.cfg is not correct.

Force Check Execution On Poller
    [Arguments]    ${comm}=    ${poller_id}=2
    # @TODO: This pipe name seem hard coded somewhere in gorgone, changing it is the engine.yaml configuration don't work.
    # this should be investigated, maybe some other configuration have the same problem too ?
    ${process}    Start Process
    ...    /usr/bin/perl
    ...    ${CURDIR}${/}..${/}..${/}..${/}..${/}contrib${/}named_pipe_reader.pl
    ...    --pipename
    ...    /var/lib/centreon-engine/rw/centengine.cmd
    ...    --logfile
    ...    /var/log/centreon-gorgone/${comm}_gorgone_central_legacycmd/legacycmd-pipe-poller.log
    ...    alias=pipeWatcher_${comm}

    Sleep    0.5
    ${date}=    Get Time
    ${forced_check_command}=    Set Variable    SCHEDULE_FORCED_SVC_CHECK;local2_${comm};Cpu;${date}
    Run    echo "EXTERNALCMD:${poller_id}:[1724242926] ${forced_check_command}" > /var/lib/centreon/centcore/random.cmd
    ${log_query}    Create List    ${forced_check_command}
    ${log_status}    Ctn Find In Log With Timeout    log=/var/log/centreon-gorgone/${comm}_gorgone_central_legacycmd/legacycmd-pipe-poller.log    content=${log_query}    regex=0    timeout=20
    Should Be True    ${log_status}    Didn't found the logs : ${log_status}

Test SYNCTRAP Execution
    [Arguments]    ${poller_id}=2
    ${source}=    Set Variable    /etc/snmp/centreon_traps/${poller_id}/centreontrapd.sdb
    ${destination}=    Set Variable    /etc/snmp/centreon_traps/centreontrapd.sdb

    Log To Console    Test mode of file after SYNCTRAP
    Run    mkdir /etc/snmp/centreon_traps/${poller_id} -p
    Run    echo "DATA" > ${source}

    Run    rm -f ${destination}
    Run    chmod 644 ${source}
    Run    echo SYNCTRAP:${poller_id} > /var/lib/centreon/centcore/random.cmd
    Wait Until Keyword Succeeds    10x    2s    File Should Exist    ${destination}
    ${testmode}=    Run Process    stat    -c     %a     ${destination}    stdout=True
    ${mode}=    Strip String    ${testmode.stdout}
    Should Be Equal    ${mode}    664    Permissions of file must always be 664

    Run    rm -f ${destination}
    Run    chmod 664 ${source}
    Run    echo SYNCTRAP:${poller_id} > /var/lib/centreon/centcore/random.cmd
    Wait Until Keyword Succeeds    10x    2s    File Should Exist    ${destination}
    ${testmode}=    Run Process    stat    -c     %a     ${destination}    stdout=True
    ${mode}=    Strip String    ${testmode.stdout}
    Should Be Equal    ${mode}    664    Permissions of file must always be 664

    Run    rm -f ${source}
    Run    rm -f ${destination}
