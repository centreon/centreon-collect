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


*** Keywords ***
Config BBDO3
    [Arguments]    ${nbEngine}    ${version}=3.0.1
    Config Broker Sql Output    central    unified_sql
    Broker Config Add Item    rrd    bbdo_version    ${version}
    Broker Config Add Item    central    bbdo_version    ${version}
    FOR    ${i}    IN RANGE    ${nbEngine}
        ${mod}    Catenate    SEPARATOR=    module    ${i}
        Broker Config Add Item    ${mod}    bbdo_version    ${version}
    END

Clean Before Suite
    Clear Db    resources
    Clear Db    hosts
    Clear Db    services
    Clear Db    tags
    Stop Processes
    Clear Engine Logs
    Clear Broker Logs

Clean Before Suite With rrdcached
    Clean Before Suite
    Log To Console    Starting RRDCached
    Run Process    /usr/bin/rrdcached    -l    unix:${BROKER_LIB}/rrdcached.sock    -V    LOG_DEBUG    -F

Clean Grpc Before Suite
    Set Grpc Port    0
    Clean Before Suite

Clean After Suite
    # Remove Files    ${ENGINE_LOG}${/}centengine.log ${ENGINE_LOG}${/}centengine.debug
    # Remove Files    ${BROKER_LOG}${/}central-broker-master.log    ${BROKER_LOG}${/}central-rrd-master.log    ${BROKER_LOG}${/}central-module-master.log
    Terminate All Processes    kill=True

Clean After Suite With rrdcached
    Clean After Suite
    Log To Console    Stopping RRDCached
    Stop rrdcached

Clear Engine Logs
    Remove Directory    ${ENGINE_LOG}    Recursive=True
    Create Directory    ${ENGINE_LOG}

Clear Broker Logs
    Remove Directory    ${BROKER_LOG}    Recursive=True
    Create Directory    ${BROKER_LOG}

Start Broker
    [Arguments]    ${only_central}=False
    Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-broker.json    alias=b1
    IF    not ${only_central}
        Start Process    /usr/sbin/cbd    ${EtcRoot}/centreon-broker/central-rrd.json    alias=b2
    END

Reload Broker
    [Arguments]    ${only_central}=False
    Send Signal To Process    SIGHUP    b1
    IF    not ${only_central}    Send Signal To Process    SIGHUP    b2

Kindly Stop Broker
    [Arguments]    ${only_central}=False
    Send Signal To Process    SIGTERM    b1
    IF    not ${only_central}    Send Signal To Process    SIGTERM    b2
    ${result}    Wait For Process    b1    timeout=60s
    # In case of process not stopping
    IF    "${result}" == "${None}"
        Save Logs
        Dump Process    b1    /usr/sbin/cbd    broker-central
        Send Signal To Process    SIGKILL    b1
        Fail    Central Broker not correctly stopped (coredump generated)
    ELSE
        IF    ${result.rc} != 0
            Save Logs
            # Copy Coredump In Failed Dir    b1    /usr/sbin/cbd    broker_central
            Coredump Info    b1    /usr/sbin/cbd    broker_central
            Should Be Equal As Integers    ${result.rc}    0    Central Broker not correctly stopped
        END
    END

    IF    not ${only_central}
        ${result}    Wait For Process    b2    timeout=60s    on_timeout=kill
        # In case of process not stopping
        IF    "${result}" == "${None}"
            Save Logs
            Dump Process    b2    /usr/sbin/cbd    broker-rrd
            Send Signal To Process    SIGKILL    b2
            Fail    RRD Broker not correctly stopped (coredump generated)
        ELSE
            IF    ${result.rc} != 0
                Save Logs
                # Copy Coredump In Failed Dir    b2    /usr/sbin/cbd    broker_rrd
                Coredump Info    b2    /usr/sbin/cbd    broker_rrd
                Should Be Equal As Integers    ${result.rc}    0    RRD Broker not correctly stopped
            END
        END
    END

Stop Broker
    [Arguments]    ${only_central}=False
    ${result}    Terminate Process    b1    kill=False
    Should Be Equal As Integers    ${result.rc}    0
    IF    not ${only_central}
        ${result}    Terminate Process    b2    kill=False
        Should Be Equal As Integers    ${result.rc}    0
    END

Stop Processes
    Terminate All Processes    kill=True
    Kill Broker
    Kill Engine

Start Engine
    ${count}    Get Engines Count
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
        Start Process    /usr/sbin/centengine    ${conf}    alias=${alias}
    END

Restart Engine
    Stop Engine
    Start Engine

Start Custom Engine
    [Arguments]    ${conf_path}    ${process_alias}
    Start Process    /usr/sbin/centengine    ${conf_path}    alias=${process_alias}

Stop Custom Engine
    [Arguments]    ${process_alias}
    ${result}    Terminate Process    ${process_alias}
    Should Be True
    ...    ${result.rc} == -15 or ${result.rc} == 0
    ...    Engine badly stopped alias = ${process_alias} - code returned ${result.rc}.

Stop Engine
    ${count}    Get Engines Count
    FOR    ${idx}    IN RANGE    0    ${count}
        ${alias}    Catenate    SEPARATOR=    e    ${idx}
        Send Signal To Process    SIGTERM    ${alias}
    END
    FOR    ${idx}    IN RANGE    0    ${count}
        ${alias}    Catenate    SEPARATOR=    e    ${idx}
        ${result}    Wait For Process    ${alias}    timeout=60s
        IF    "${result}" == "${None}"
            ${name}    Catenate    SEPARATOR=    centengine    ${idx}
            Dump Process    ${alias}    /usr/sbin/centengine    ${name}
            Send Signal To Process    SIGKILL    ${alias}
            Fail    ${name} not correctly stopped (coredump generated)
        ELSE
            IF    ${result.rc} != 0 and ${result.rc} != -15
                # Copy Coredump In Failed Dir    ${alias}    /usr/sbin/centengine    ${alias}
                Coredump Info    ${alias}    /usr/sbin/centengine    ${alias}
            END
            Should Be True
            ...    ${result.rc} == -15 or ${result.rc} == 0
            ...    Engine badly stopped with ${count} instances - code returned ${result.rc}.
        END
    END

Stop Engine Broker And Save Logs
    [Arguments]    ${only_central}=False
    TRY
        Stop Engine
    EXCEPT
        Log    can't kindly stop engine
    END
    TRY
        Kindly Stop Broker    only_central=${only_central}
    EXCEPT
        Log    Can't kindly stop Broker
    END
    Save Logs If Failed

Get Engine Pid
    [Arguments]    ${process_alias}
    ${pid}    Get Process Id    ${process_alias}
    RETURN    ${pid}

Reload Engine
    ${count}    Get Engines Count
    FOR    ${idx}    IN RANGE    0    ${count}
        ${alias}    Catenate    SEPARATOR=    e    ${idx}
        Send Signal To Process    SIGHUP    ${alias}
    END

Check Connections
    ${count}    Get Engines Count
    ${pid1}    Get Process Id    b1
    FOR    ${idx}    IN RANGE    0    ${count}
        ${alias}    Catenate    SEPARATOR=    e    ${idx}
        ${pid2}    Get Process Id    ${alias}
        Log To Console    Check Connection 5669 ${pid1} ${pid2}
        ${retval}    Check Connection    5669    ${pid1}    ${pid2}
        IF    ${retval} == ${False}    RETURN    ${False}
    END
    ${pid2}    Get Process Id    b2
    Log To Console    Check Connection 5670 ${pid1} ${pid2}
    ${retval}    Check Connection    5670    ${pid1}    ${pid2}
    RETURN    ${retval}

Disable Eth Connection On Port
    [Arguments]    ${port}
    RUN    iptables -A INPUT -p tcp --dport ${port} -j DROP
    RUN    iptables -A OUTPUT -p tcp --dport ${port} -j DROP
    RUN    iptables -A FORWARD -p tcp --dport ${port} -j DROP

Reset Eth Connection
    Run    iptables -F
    Run    iptables -X

Save Logs If Failed
    Run Keyword If Test Failed    Save Logs

Save Logs
    Create Directory    failed
    ${failDir}    Catenate    SEPARATOR=    failed/    ${Test Name}
    Create Directory    ${failDir}
    Copy Files    ${centralLog}    ${failDir}
    Copy Files    ${rrdLog}    ${failDir}
    Copy Files    ${moduleLog0}    ${failDir}
    Copy Files    ${engineLog0}    ${failDir}
    Copy Files    ${EtcRoot}/centreon-engine/config0/*.cfg    ${failDir}/etc/centreon-engine/config0
    Copy Files    ${EtcRoot}/centreon-broker/*.json    ${failDir}/etc/centreon-broker
    Move Files    /tmp/lua*.log    ${failDir}

Dump Process
    [Arguments]    ${process_name}    ${binary_path}    ${name}
    ${pid}    Get Process Id    ${process_name}
    IF    ${pid} is not ${None}
        ${failDir}    Catenate    SEPARATOR=    failed/    ${Test Name}
        Create Directory    ${failDir}
        ${output}    Catenate    SEPARATOR=    /tmp/core-    ${name}
        ${gdb_output}    Catenate    SEPARATOR=    ${failDir}    /core-    ${name}    .txt
        # Log To Console    Creation of core ${output}.${pid} to debug
        # Run Process    gcore    -o    ${output}    ${pid}
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

Coredump Info
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

Copy Coredump In Failed Dir
    [Arguments]    ${process_name}    ${binary_path}    ${name}
    ${docker_env}    Get Environment Variable    RUN_ENV    ${None}
    IF    "${docker_env}" == ""
        ${pid}    Get Process Id    ${process_name}
        ${failDir}    Catenate    SEPARATOR=    failed/    ${Test Name}
        Create Directory    ${failDir}
        Copy File    /tmp/core.${pid}    ${failDir}
    END

Wait Or Dump And Kill Process
    [Arguments]    ${process_name}    ${binary_path}    ${timeout}
    ${result}    Wait For Process    ${process_name}    timeout=${timeout}    on_timeout=continu
    ${test_none}    Set Variable If    $result is None    "not killed"    "killed"
    IF    ${test_none} == "not killed"
        ${pid}    Get Process Id    ${process_name}
        Dump Process    ${process_name}    ${binary_path}    ${process_name}
        ${result}    Wait For Process    ${process_name}    timeout=1s    on_timeout=kill
    END
    RETURN    ${result}

Clear Metrics
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Execute SQL String    DELETE FROM metrics
    Execute SQL String    DELETE FROM index_data
    Execute SQL String    DELETE FROM data_bin

Dump Ba On Error
    [Arguments]    ${result}    ${ba_id}
    IF    not ${result}
        Save Logs
        Dump Ba    51001    ${ba_id}    failed/${Test Name}/ba_${ba_id}.dot
    END

Process Service Result Hard
    [Arguments]    ${host}    ${svc}    ${state}    ${output}
    Repeat Keyword
    ...    3 times
    ...    Process Service Check Result
    ...    ${host}
    ...    ${svc}
    ...    ${state}
    ...    ${output}
