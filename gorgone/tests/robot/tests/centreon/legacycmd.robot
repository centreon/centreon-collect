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

    Force Check Execution On Poller    comm=${communication_mode}
    Push Engine And vmware Configuration    comm=${communication_mode}
    Examples:    communication_mode   --
        ...    push_zmq
        ...    pullwss
        ...    pull
        
*** Keywords ***
Legacycmd Teardown
    [Arguments]    ${central}    ${poller}    ${comm}
    @{process_list}    Create List    ${central}    ${poller}

    Stop Gorgone And Remove Gorgone Config    @{process_list}    sql_file=${ROOT_CONFIG}db_delete_poller.sql
    Terminate Process    pipeWatcher_${comm}
    Run    rm -rf /var/cache/centreon/config
    Run    rm -rf /etc/centreon/centreon_vmware.json
    Run    rm -rf /etc/centreon-engine/randomBigFile.cfg
    Run    rm -rf /etc/centreon-engine/engine-hosts.cfg
    
Push Engine And vmware Configuration
    [Arguments]    ${comm}=    ${poller_id}=2

    Copy File    ${CURDIR}${/}legacycmd${/}centreon_vmware.json    /var/cache/centreon/config/vmware/${poller_id}/
    Copy File    ${CURDIR}${/}legacycmd${/}broker.cfg    /var/cache/centreon/config/broker/${poller_id}/
    Copy File    ${CURDIR}${/}legacycmd${/}engine-hosts.cfg    /var/cache/centreon/config/engine/${poller_id}/
    # we change all the configuration files to be sure it was copied in this run and not a rest of another test.
    Run    sed -i -e 's/@COMMUNICATION_MODE@/${comm}/g' /var/cache/centreon/config/vmware/${poller_id}/centreon_vmware.json
    Run    sed -i -e 's/@COMMUNICATION_MODE@/${comm}/g' /var/cache/centreon/config/broker/${poller_id}/broker.cfg
    Run    sed -i -e 's/@COMMUNICATION_MODE@/${comm}/g' /var/cache/centreon/config/engine/${poller_id}/engine-hosts.cfg
    Run    dd if=/dev/urandom of=/var/cache/centreon/config/engine/${poller_id}/randomBigFile.cfg bs=200MB count=1 iflag=fullblock
    ${MD5Start}=    Run    md5sum /var/cache/centreon/config/engine/${poller_id}/randomBigFile.cfg | cut -f 1 -d " "
    Run    chown www-data:www-data /var/cache/centreon/config/*/${poller_id}/*
    Run    chmod 644 /var/cache/centreon/config/*/${poller_id}/*

    # gorgone central should get these files, and send it to poller in /etc/centreon/, /etc/centreon-broker/, /etc/centreon-engine/
    # we are checking the poller have the last bit of centreon-engine before continuing.
    ${log_query}    Create List    Copy to '/etc/centreon-engine//' finished successfully
    # SENDCFGFILE say to gorgone to push conf to poller for a poller id.
    Run    echo SENDCFGFILE:${poller_id} > /var/lib/centreon/centcore/random.cmd
    ${log_status}    Ctn Find In Log With Timeout    log=/var/log/centreon-gorgone/${comm}_gorgone_poller${poller_id}_legacycmd/gorgoned.log    content=${log_query}    regex=0    timeout=40
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
    [Arguments]    ${comm}=
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
    Run    echo "EXTERNALCMD:2:[1724242926] ${forced_check_command}" > /var/lib/centreon/centcore/random.cmd
    ${log_query}    Create List    ${forced_check_command}
    ${log_status}    Ctn Find In Log With Timeout    log=/var/log/centreon-gorgone/${comm}_gorgone_central_legacycmd/legacycmd-pipe-poller.log    content=${log_query}    regex=0    timeout=20
    Should Be True    ${log_status}    Didn't found the logs : ${log_status}
