*** Settings ***
Resource    ./db_variables.resource
Library     Process
Library     OperatingSystem
Library     Common.py


*** Variables ***
${BROKER_LOG}       ${VarRoot}/log/centreon-broker
${BROKER_LIB}       ${VarRoot}/lib/centreon-broker
${ENGINE_LOG}       ${VarRoot}/log/centreon-engine
${SCRIPTS}          ${CURDIR}${/}scripts${/}
${centralLog}       ${BROKER_LOG}/central-broker-master.log
${moduleLog0}       ${BROKER_LOG}/central-module-master0.log
${moduleLog1}       ${BROKER_LOG}/central-module-master1.log
${moduleLog2}       ${BROKER_LOG}/central-module-master2.log
${moduleLog3}       ${BROKER_LOG}/central-module-master3.log
${rrdLog}           ${BROKER_LOG}/central-rrd-master.log

${engineLog0}       ${ENGINE_LOG}/config0/centengine.log
${engineLog1}       ${ENGINE_LOG}/config1/centengine.log
${engineLog2}       ${ENGINE_LOG}/config2/centengine.log
${engineLog3}       ${ENGINE_LOG}/config3/centengine.log
${engineLog4}       ${ENGINE_LOG}/config4/centengine.log

${agentlog}       ${VarRoot}/log/centreon-engine/centreon-agent.log

*** Keywords ***
Ctn Config BBDO3
    [Arguments]    ${nbEngine}    ${version}=3.0.1
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Broker Config Add Item    rrd    bbdo_version    ${version}
    Ctn Broker Config Add Item    central    bbdo_version    ${version}
    FOR    ${i}    IN RANGE    ${nbEngine}
        ${mod}    Catenate    SEPARATOR=    module    ${i}
        Ctn Broker Config Add Item    ${mod}    bbdo_version    ${version}
    END
    # We replace the global Bbdo2 Variable to False
    Ctn Set Bbdo2    ${False}

Ctn Clean Before Suite
    Ctn Clear Db    resources
    Ctn Clear Db    hosts
    Ctn Clear Db    services
    Ctn Clear Db    tags
    Ctn Stop Processes
    Ctn Clear Engine Logs
    Ctn Clear Broker Logs

Ctn Clean Before Suite With rrdcached
    Ctn Clean Before Suite
    Log To Console    Starting RRDCached
    Run Process    /usr/bin/rrdcached    -l    unix:${BROKER_LIB}/rrdcached.sock    -V    LOG_DEBUG    -F

Ctn Clean Grpc Before Suite
    Set Grpc Port    0
    Ctn Clean Before Suite

Ctn Clean After Suite
    # Remove Files    ${ENGINE_LOG}${/}centengine.log ${ENGINE_LOG}${/}centengine.debug
    # Remove Files    ${BROKER_LOG}${/}central-broker-master.log    ${BROKER_LOG}${/}central-rrd-master.log    ${BROKER_LOG}${/}central-module-master.log
    Terminate All Processes    kill=True

Ctn Clean After Suite With rrdcached
    Ctn Clean After Suite
    Log To Console    Stopping RRDCached
    Ctn Stop Rrdcached

Ctn Clear Engine Logs
    Remove Directory    ${ENGINE_LOG}    Recursive=True
    Create Directory    ${ENGINE_LOG}

Ctn Clear Broker Logs
    Remove Directory    ${BROKER_LOG}    Recursive=True
    Create Directory    ${BROKER_LOG}

Ctn Start Vault
    Start Process    /usr/bin/perl    vault/vault-server.pl    alias=vault

Ctn Stop Vault
    ${result}    Terminate Process    vault    kill=False
    Should Be True
    ...    ${result.rc} == -15 or ${result.rc} == 0
    ...    Vault not correctly stopped

Ctn Start Broker
    [Arguments]    ${only_central}=False
    # Currently we are in BBDO2 by default.
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-broker.json    alias=b1    stdout=${BROKER_LOG}/central-broker-master-stdout.log    stderr=${BROKER_LOG}/central-broker-master-stderr.log
    IF    not ${only_central}
        Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-rrd.json    alias=b2    stdout=${BROKER_LOG}/central-rrd-stdout.log    stderr=${BROKER_LOG}/central-rrd-stderr.log
    END

Ctn Restart Broker
    [Arguments]    ${only_central}=False
    Ctn Kindly Stop Broker    ${only_central}
    Ctn Start Broker    ${only_central}

Ctn Reload Broker
    [Arguments]    ${only_central}=False
    Send Signal To Process    SIGHUP    b1
    IF    not ${only_central}    Send Signal To Process    SIGHUP    b2

Ctn Kindly Stop Broker
    [Documentation]    Stop broker and check if it's correctly stopped.
    ...    If not, generate a coredump and kill the process.
    ...    If only_central is set to True, only the central broker will be stopped.
    ...    If bbdo2 is set to True, the rrd broker will not be checked for RRD logs
    ...    since there are still bugs in this context.
    [Arguments]    ${only_central}=False    ${no_rrd_test}=False

    #in some case tear down calls this function whereas broker has not been started
    ${b1_obj}    Get Process Object   b1
    IF    "${b1_obj}" == "${None}"
        Log To Console    "central broker not started"
        RETURN
    END
    Send Signal To Process    SIGTERM    b1
    IF    not ${only_central}    Send Signal To Process    SIGTERM    b2
    ${result}    Wait For Process    b1    timeout=60s
    # In case of process not stopping
    IF    "${result}" == "${None}"
        Log To Console    "fail to stop central broker"
        Ctn Save Logs
        Ctn Dump Process    b1    /usr/sbin/cbd    broker-central
        Send Signal To Process    SIGKILL    b1
        Fail    Central Broker not correctly stopped (coredump generated)
    ELSE
        IF    ${result.rc} != 0
            Ctn Save Logs
            # Copy Coredump In Failed Dir    b1    /usr/sbin/cbd    broker_central
            Ctn Coredump Info    b1    /usr/sbin/cbd    broker_central
            Should Be Equal As Integers    ${result.rc}    0    Central Broker not correctly stopped
        ELSE
            ${Bbdo2}   Ctn In Bbdo2
            IF    not ${Bbdo2} and not ${no_rrd_test}
                IF    not ${only_central}
                        Log To Console    We are in BBDO3, so we also check RRD duplicates
                        Log To Console    central broker stopped
                        # We have to check if central RRD logs contain errors about data sent in the past.
                        ${result}    Grep File    ${rrdLog}    RRD: ignored update error in file
                        IF    len("""${result}""") > 0
                            Log To Console    The RRD logs contain errors about metrics sent in the past. They are removed.
                            Ctn Save Logs
                            Remove File    ${rrdLog}
                        END
                        Should Be Empty    ${result}    Central RRD logs contain errors about metrics sent in the past.
                ELSE
                    Log To Console    Broker RRD was not started, so we don't check RRD duplicates
                END
            ELSE
                Log To Console    We don't check RRD duplicates here (bbdo2 or explicitly asked)
            END
        END
    END

    IF    not ${only_central}
        ${result}    Wait For Process    b2    timeout=60s    on_timeout=kill
        # In case of process not stopping
        IF    "${result}" == "${None}"
            Log To Console    "fail to stop rrd broker"
            Ctn Save Logs
            Ctn Dump Process    b2    /usr/sbin/cbd    broker-rrd
            Send Signal To Process    SIGKILL    b2
            Fail    RRD Broker not correctly stopped (coredump generated)
        ELSE
            IF    ${result.rc} != 0
                Ctn Save Logs
                # Copy Coredump In Failed Dir    b2    /usr/sbin/cbd    broker_rrd
                Ctn Coredump Info    b2    /usr/sbin/cbd    broker_rrd
                Should Be Equal As Integers    ${result.rc}    0    RRD Broker not correctly stopped
            END
        END
    END

Ctn Stop Broker
    [Arguments]    ${only_central}=False
    ${result}    Terminate Process    b1    kill=False
    Should Be Equal As Integers    ${result.rc}    0
    IF    not ${only_central}
        ${result}    Terminate Process    b2    kill=False
        Should Be Equal As Integers    ${result.rc}    0
    END

Ctn Stop Processes
    Terminate All Processes    kill=True
    Ctn Kill Broker
    Ctn Kill Engine

Ctn Start Engine
    [Arguments]    ${newGeneration}=False
    # Currently we are in BBDO2 by default.
    ${count}    Ctn Get Engines Count
    FOR    ${idx}    IN RANGE    0    ${count}
        ${alias}    Catenate    SEPARATOR=    e    ${idx}
        ${conf}    Catenate    SEPARATOR=    ${EtcRoot}    /centreon-engine/config    ${idx}    /centengine.cfg
        ${log}    Catenate    SEPARATOR=    ${VarRoot}    /log/centreon-engine/config    ${idx}
        ${lib}    Catenate    SEPARATOR=    ${VarRoot}    /lib/centreon-engine/config    ${idx}
        Create Directory    ${log}
        Create Directory    ${lib}
        TRY
            Remove File    ${lib}/rw/centengine.cmd
        EXCEPT
            Log    can't remove ${lib}/rw/centengine.cmd don't worry
        END
        IF    ${newGeneration}
            Start Process    /usr/sbin/centengine    -p    ${VarRoot}/lib/centreon-engine/config${idx}/current-conf.prot    -b    ${EtcRoot}/centreon-broker/central-module${idx}.json    ${conf}    alias=${alias}    stdout=${log}/centengine-stdout.log    stderr=${log}/centengine-stderr.log
        ELSE
            Start Process    /usr/sbin/centengine    -b    ${EtcRoot}/centreon-broker/central-module${idx}.json    ${conf}    alias=${alias}    stdout=${log}/centengine-stdout.log    stderr=${log}/centengine-stderr.log
        END
    END

Ctn Start Engine With Extend Conf
    ${count}    Ctn Get Engines Count
    FOR    ${idx}    IN RANGE    0    ${count}
        ${alias}    Catenate    SEPARATOR=    e    ${idx}
        ${conf}    Catenate    SEPARATOR=    ${EtcRoot}    /centreon-engine/config    ${idx}    /centengine.cfg
        ${log}    Catenate    SEPARATOR=    ${VarRoot}    /log/centreon-engine/config    ${idx}
        ${lib}    Catenate    SEPARATOR=    ${VarRoot}    /lib/centreon-engine/config    ${idx}
        Create Directory    ${log}
        Create Directory    ${lib}
        TRY
            Remove File    ${lib}/rw/centengine.cmd
        EXCEPT
            Log    can't remove ${lib}/rw/centengine.cmd don't worry
        END
        Start Process
        ...    /usr/sbin/centengine
        ...    -b    ${EtcRoot}/centreon-broker/central-module${idx}.json
        ...    --config-file\=/tmp/centengine_extend.json
        ...    ${conf}
        ...    alias=${alias}
    END

Ctn Restart Engine
    [Arguments]    ${newGeneration}=False
    Ctn Stop Engine
    Ctn Start Engine    ${newGeneration}

Ctn Start Custom Engine
    [Arguments]    ${broker_conf}    ${conf_path}    ${process_alias}
    Start Process    /usr/sbin/centengine
    ...    -b    ${broker_conf}
    ...    ${conf_path}
    ...    alias=${process_alias}

Ctn Stop Custom Engine
    [Arguments]    ${process_alias}
    ${result}    Terminate Process    ${process_alias}
    Should Be True
    ...    ${result.rc} == -15 or ${result.rc} == 0
    ...    Engine badly stopped alias = ${process_alias} - code returned ${result.rc}.

Ctn Stop Engine
    Log To Console    Stop centengine
    ${count}    Ctn Get Engines Count
    IF    ${count} == 0
        Log To Console    No engine to stop
        RETURN
    END
    FOR    ${idx}    IN RANGE    0    ${count}
        Send Signal To Process    SIGTERM    e${idx}
    END
    FOR    ${idx}    IN RANGE    0    ${count}
        ${alias}    Catenate    SEPARATOR=    e    ${idx}
        ${result}    Wait For Process    ${alias}    timeout=60s
        IF    "${result}" == "${None}"
            Log To Console    Failed to stop centengine
            ${name}    Catenate    SEPARATOR=    centengine    ${idx}
            Ctn Dump Process    ${alias}    /usr/sbin/centengine    ${name}
            Send Signal To Process    SIGKILL    ${alias}
            Fail    ${name} not correctly stopped (coredump generated)
        ELSE
            IF    ${result.rc} != 0 and ${result.rc} != -15
                # Copy Coredump In Failed Dir    ${alias}    /usr/sbin/centengine    ${alias}
                Ctn Coredump Info    ${alias}    /usr/sbin/centengine    ${alias}
            END
            Should Be True
            ...    ${result.rc} == -15 or ${result.rc} == 0
            ...    Engine badly stopped with ${count} instances - code returned ${result.rc}.
        END
    END

Ctn Stop Engine Broker And Save Logs
    [Arguments]    ${only_central}=False    ${no_rrd_test}=False
    TRY
        Ctn Stop Engine
    EXCEPT
        Log    can't kindly stop engine
    END
    TRY
        Ctn Kindly Stop Broker    only_central=${only_central}    no_rrd_test=${no_rrd_test}
    EXCEPT
        Log    Can't kindly stop Broker
    END
    TRY
        Ctn Kindly Stop Agent
    EXCEPT
        Log    Can't kindly stop Agent
    END
    Ctn Save Logs If Failed

Ctn Get Engine Pid
    [Arguments]    ${process_alias}
    ${pid}    Get Process Id    ${process_alias}
    RETURN    ${pid}

Ctn Reload Engine
    ${count}    Ctn Get Engines Count
    FOR    ${idx}    IN RANGE    0    ${count}
        ${alias}    Catenate    SEPARATOR=    e    ${idx}
        Send Signal To Process    SIGHUP    ${alias}
    END

Ctn Check Connections
    ${count}    Ctn Get Engines Count
    ${pid1}    Get Process Id    b1
    FOR    ${idx}    IN RANGE    0    ${count}
        ${alias}    Catenate    SEPARATOR=    e    ${idx}
        ${pid2}    Get Process Id    ${alias}
        Log To Console    Check Connection 5669 ${pid1} ${pid2}
        ${retval}    Ctn Check Connection    5669    ${pid1}    ${pid2}
        IF    ${retval} == ${False}    RETURN    ${False}
    END
    ${pid2}    Get Process Id    b2
    Log To Console    Check Connection 5670 ${pid1} ${pid2}
    ${retval}    Ctn Check Connection    5670    ${pid1}    ${pid2}
    RETURN    ${retval}

Ctn Disable Eth Connection On Port
    [Arguments]    ${port}
    RUN    iptables -A INPUT -p tcp --dport ${port} -j DROP
    RUN    iptables -A OUTPUT -p tcp --dport ${port} -j DROP
    RUN    iptables -A FORWARD -p tcp --dport ${port} -j DROP

Ctn Reset Eth Connection
    Run    iptables -F
    Run    iptables -X

Ctn Save Logs If Failed
    Run Keyword If Test Failed    Ctn Save Logs

Ctn Save Logs
    Log To Console      Save test logs
    Create Directory    failed
    ${failDir}    Catenate    SEPARATOR=    failed/    ${Test Name}
    Create Directory    ${failDir}
    Copy Files    ${BROKER_LOG}/*    ${failDir}
    Copy Files    ${ENGINE_LOG}/config0/*.log    ${failDir}
    Copy Files    ${ENGINE_LOG}/*.log    ${failDir}
    Copy Files    ${EtcRoot}/centreon-engine/config0/*.cfg    ${failDir}/etc/centreon-engine/config0
    Copy Files    ${EtcRoot}/centreon-engine/*.json    ${failDir}/etc/centreon-engine
    Copy Files    ${EtcRoot}/centreon-broker/*.json    ${failDir}/etc/centreon-broker
    Move Files    /tmp/lua*.log    ${failDir}

Ctn Dump Process
    [Arguments]    ${process_name}    ${binary_path}    ${name}
    ${pid}    Get Process Id    ${process_name}
    IF    ${{$pid is not None}}
        ${failDir}    Catenate    SEPARATOR=    failed/    ${Test Name}
        Create Directory    ${failDir}
        ${output}    Catenate    SEPARATOR=    /tmp/core-    ${name}
        ${gdb_output}    Catenate    SEPARATOR=    ${failDir}    /core-    ${name}    .txt
        # Log To Console    Creation of core ${output}.${pid} to debug
        Run Process    gcore    -o    ${output}    ${pid}
        Log To Console    Creation of core ${gdb_output}
        Run Process
        ...    gdb
        ...    -batch
        ...    -ex
        ...    thread apply all bt 30
        ...    ${binary_path}
        ...    ${output}.${pid}
        ...    stdout=${gdb_output}
        ...    stderr=${gdb_output}
        Log To Console    Done...
    END

Ctn Coredump Info
    [Arguments]    ${process_name}    ${binary_path}    ${name}
    ${pid}    Get Process Id    ${process_name}
    ${failDir}    Catenate    SEPARATOR=    failed/    ${Test Name}
    Create Directory    ${failDir}
    ${output}    Catenate    SEPARATOR=    ${failDir}    /coreinfo-    ${name}    -${pid}    .txt
    Log To Console    info of core saved in ${output}
    Run Process
    ...    gdb
    ...    -batch
    ...    -ex
    ...    thread apply all bt 30
    ...    ${binary_path}
    ...    /tmp/core.${pid}
    ...    stdout=${output}
    ...    stderr=${output}

Ctn Copy Coredump In Failed Dir
    [Arguments]    ${process_name}    ${binary_path}    ${name}
    ${docker_env}    Get Environment Variable    RUN_ENV    ${None}
    IF    "${docker_env}" == ""
        ${pid}    Get Process Id    ${process_name}
        ${failDir}    Catenate    SEPARATOR=    failed/    ${Test Name}
        Create Directory    ${failDir}
        Copy File    /tmp/core.${pid}    ${failDir}
    END

Ctn Wait Or Dump And Kill Process
    [Arguments]    ${process_name}    ${binary_path}    ${timeout}
    ${result}    Wait For Process    ${process_name}    timeout=${timeout}    on_timeout=continu
    ${test_none}    Set Variable If    $result is None    "not killed"    "killed"
    IF    ${test_none} == "not killed"
        ${pid}    Get Process Id    ${process_name}
        Ctn Dump Process    ${process_name}    ${binary_path}    ${process_name}
        ${result}    Wait For Process    ${process_name}    timeout=1s    on_timeout=kill
    END
    RETURN    ${result}

Ctn Clear Metrics
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM metrics
    Execute SQL String    DELETE FROM index_data
    Execute SQL String    DELETE FROM data_bin

Ctn Dump Ba On Error
    [Arguments]    ${result}    ${ba_id}
    IF    not ${result}
        Ctn Save Logs
        Ctn Broker Get Ba    51001    ${ba_id}    failed/${Test Name}/ba_${ba_id}.dot
    END

Ctn Process Service Result Hard
    [Arguments]    ${host}    ${svc}    ${state}    ${output}
    FOR    ${idx}    IN RANGE    3
        Ctn Process Service Check Result
        ...    ${host}
        ...    ${svc}
        ...    ${state}
        ...    ${output}
        Sleep    1s
    END

Ctn Process Host Result Hard
    [Arguments]    ${host}    ${state}    ${output}
    FOR    ${idx}    IN RANGE    3
        Ctn Process Host Check Result
        ...    ${host}
        ...    ${state}
        ...    ${output}
        Sleep    1s
    END


Ctn Wait For Engine To Be Ready
    [Arguments]    ${start}    ${nbEngine}=1
    FOR    ${i}    IN RANGE    ${nbEngine}
        # Let's wait for the external command check start
        ${content}    Create List    check_for_external_commands()
        ${result}    Ctn Find In Log With Timeout
        ...    ${ENGINE_LOG}/config${i}/centengine.log
        ...    ${start}    ${content}    60
        ...    verbose=False
        Should Be True
        ...    ${result}
        ...    A message telling check_for_external_commands() should be available in config${i}/centengine.log.
    END


Ctn Start Agent
    ${run_env}    Ctn Run Env

    IF    "${run_env}" != "WSL"
        Start Process    /usr/bin/centagent  ${EtcRoot}/centreon-engine/centagent.json    alias=centreon_agent
    END

Ctn Workspace Win
    ${run_env}    Ctn Run Env
    IF    "${run_env}" == "WSL"
        ${workspace}    Ctn Get Workspace Win
        RETURN    ${workspace}
    END
    
    RETURN    None
    
Ctn Kindly Stop Agent
    ${run_env}    Ctn Run Env

    IF    "${run_env}" == "WSL"
        RETURN
    END

    #in most case centreon_agent is not started
    ${centreon_agent_process}    Get Process Object  centreon_agent

    IF  ${{$centreon_agent_process is None}}  RETURN

    Send Signal To Process    SIGTERM    centreon_agent
    ${result}    Wait For Process    centreon_agent    timeout=60s
    # In case of process not stopping
    IF    "${result}" == "${None}"
        Log To Console    "fail to stop centreon_agent"
        Ctn Save Logs
        Ctn Dump Process    centreon_agent    /usr/bin/centagent   centreon_agent
        Send Signal To Process    SIGKILL    centreon_agent
        Fail    centreon_agent not correctly stopped (coredump generated)
    ELSE
        IF    ${result.rc} != 0
            Ctn Save Logs
            Ctn Coredump Info    centreon_agent    /usr/bin/centagent    centreon_agent
            Fail   centreon_agent not correctly stopped, result status: ${result.rc} 
        END
    END


Ctn Wait For Otel Server To Be Ready
    [Documentation]    Wait for engine otel server to be ready
    [Arguments]    ${start}
    ${content}    Create List    unencrypted server listening on 0.0.0.0:4317
    ${result}    Ctn Find In Log With Timeout    ${engineLog0}    ${start}    ${content}    10
    Should Be True    ${result}    "unencrypted server listening on 0.0.0.0:4317" should be available.

