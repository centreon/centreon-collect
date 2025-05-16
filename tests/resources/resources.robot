*** Settings ***
Resource    ./db_variables.robot
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
${moduleLog4}       ${BROKER_LOG}/central-module-master4.log
${rrdLog}           ${BROKER_LOG}/central-rrd-master.log

${engineLog0}       ${ENGINE_LOG}/config0/centengine.log
${engineLog1}       ${ENGINE_LOG}/config1/centengine.log
${engineLog2}       ${ENGINE_LOG}/config2/centengine.log
${engineLog3}       ${ENGINE_LOG}/config3/centengine.log
${engineLog4}       ${ENGINE_LOG}/config4/centengine.log

*** Keywords ***
Ctn Config BBDO3
	[Arguments]	${nbEngine}
	Ctn Config Broker Sql Output	central	unified_sql
        Ctn Broker Config Add Item	rrd	bbdo_version	3.0.1
        Ctn Broker Config Add Item	central	bbdo_version	3.0.1
        FOR	${i}	IN RANGE	${nbEngine}
	 ${mod}=	Catenate	SEPARATOR=	module	${i}
         Ctn Broker Config Add Item	${mod}	bbdo_version	3.0.1
	END

Ctn Clean Before Suite
	Ctn Stop Processes
	Ctn Clear Engine Logs
	Ctn Clear Broker Logs

Ctn Clean Before Suite With Rrdcached
	Ctn Clean Before Suite
	log to console	Starting RRDCached
	Run Process	/usr/bin/rrdcached	-l	unix:${BROKER_LIB}/rrdcached.sock	-V	LOG_DEBUG	-F

Ctn Clean Grpc Before Suite
	set grpc port  0
	Ctn Clean Before Suite

Ctn Clean After Suite
	# Remove Files	${ENGINE_LOG}${/}centengine.log ${ENGINE_LOG}${/}centengine.debug
	# Remove Files	${BROKER_LOG}${/}central-broker-master.log	${BROKER_LOG}${/}central-rrd-master.log	${BROKER_LOG}${/}central-module-master.log
	Terminate All Processes	kill=True

Ctn Clean After Suite With rrdcached
	Ctn Clean After Suite
	log to console	Stopping RRDCached
	Ctn Stop Rrdcached

Ctn Clear Engine Logs
	Remove Directory	${ENGINE_LOG}	Recursive=True
	Create Directory	${ENGINE_LOG}

Ctn Clear Broker Logs
	Remove Directory	${BROKER_LOG}	Recursive=True
	Create Directory	${BROKER_LOG}

Ctn Start Broker
	[Arguments]	 ${only_central}=False
	Start Process	/usr/sbin/cbd	${EtcRoot}/centreon-broker/central-broker.json	alias=b1
	IF  not ${only_central}
		Start Process	/usr/sbin/cbd	${EtcRoot}/centreon-broker/central-rrd.json	alias=b2
	END

Ctn Restart Broker
    [Arguments]    ${only_central}=False
    Ctn Kindly Stop Broker    ${only_central}
    Ctn Start Broker    ${only_central}

Ctn Reload Broker
	Send Signal To Process	SIGHUP	b1
	Send Signal To Process	SIGHUP	b2

Ctn Kindly Stop Broker
	[Arguments]	 ${only_central}=False
	Send Signal To Process	SIGTERM	b1
	IF  not ${only_central}
		Send Signal To Process	SIGTERM	b2
	END
	${result}=	Wait For Process	b1	timeout=60s
	# In case of process not stopping
	IF	"${result}" == "${None}"
	  Dump Process	b1	broker-central
	  Send Signal To Process	SIGKILL	b1
	  Fail	Central Broker not correctly stopped (coredump generated)
	ELSE
	  Should Be Equal As Integers	${result.rc}	0	msg=Central Broker not correctly stopped
	END

	IF  not ${only_central}
		${result}=	Wait For Process	b2	timeout=60s	on_timeout=kill
		# In case of process not stopping
		IF	"${result}" == "${None}"
		  Dump Process	b2	broker-rrd
		  Send Signal To Process	SIGKILL	b2
		  Fail	RRD Broker not correctly stopped (coredump generated)
		ELSE
		  Should Be Equal As Integers	${result.rc}	0	msg=RRD Broker not correctly stopped
		END
	END

Ctn Stop Broker
	[Arguments]	 ${only_central}=False
	${result}=	Terminate Process	b1	kill=False
	Should Be Equal As Integers	${result.rc}	0
	IF  not ${only_central}
		${result}=	Terminate Process	b2	kill=False
		Should Be Equal As Integers	${result.rc}	0
	END

Ctn Stop Processes
	Terminate All Processes	kill=True
	Ctn Kill Broker
	Ctn Kill Engine

Ctn Start Engine
	${count}=	Ctn Get Engines Count
	FOR	${idx}	IN RANGE	0	${count}
	 ${alias}=	Catenate	SEPARATOR=	e	${idx}
	 ${conf}=	Catenate	SEPARATOR=	${EtcRoot}  /centreon-engine/config	${idx}	/centengine.cfg
	 ${log}=	Catenate	SEPARATOR=	${VarRoot}  /log/centreon-engine/config	${idx}
	 ${lib}=	Catenate	SEPARATOR=	${VarRoot}  /lib/centreon-engine/config	${idx}
	 Create Directory	${log}
	 Create Directory	${lib}
	 TRY
	 	Remove File  ${lib}/rw/centengine.cmd
	 EXCEPT
	 	Log  can't remove ${lib}/rw/centengine.cmd don't worry
	 END
	 Start Process	/usr/sbin/centengine	${conf}	alias=${alias}
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
        ...    --config-file\=/tmp/centengine_extend.json
        ...    ${conf}
        ...    alias=${alias}
    END

Ctn Restart Engine
	Ctn Stop Engine
	Ctn Start Engine

Ctn Start Custom Engine
	[Arguments]	 ${conf_path}  ${process_alias}
	Start Process  /usr/sbin/centengine  ${conf_path}  alias=${process_alias}

Ctn Stop Custom Engine
	[Arguments]  ${process_alias}
	${result}=	Terminate Process	${process_alias}
	Should Be True	${result.rc} == -15 or ${result.rc} == 0	msg=Engine badly stopped alias = ${process_alias} - code returned ${result.rc}.

Ctn Stop Engine
	${count}=	Ctn Get Engines Count
	FOR	${idx}	IN RANGE	0	${count}
	 ${alias}=	Catenate	SEPARATOR=	e	${idx}
	 Send Signal To Process	SIGTERM	${alias}
	END
	FOR	${idx}	IN RANGE	0	${count}
	 ${alias}=	Catenate	SEPARATOR=	e	${idx}
	 ${result}=	Wait For Process	${alias}	timeout=60s
	 IF	"${result}" == "${None}"
	   ${name}=	Catenate	SEPARATOR=	centengine	${idx}
	   Dump Process	${alias}	${name}
	   Send Signal To Process	SIGKILL	${alias}
	   Fail	${name} not correctly stopped (coredump generated)
  	 ELSE
	  Should Be True	${result.rc} == -15 or ${result.rc} == 0	msg=Engine badly stopped with ${count} instances - code returned ${result.rc}.
	 END
	END

Ctn Stop Engine Broker And Save Logs
    [Arguments]    ${only_central}=False
    TRY
        Ctn Stop Engine
    EXCEPT
        Log    Can't kindly stop Engine
    END
    TRY
        Ctn Kindly Stop Broker    only_central=${only_central}
    EXCEPT
        Log    Can't kindly stop Broker
    END
    Ctn Save Logs If Failed

Ctn Get Engine Pid
	[Arguments]  ${process_alias}
	${pid}=  Get Process Id  ${process_alias} 
	RETURN  ${pid}
	
Ctn Reload Engine
    [Arguments]    ${poller_index}=-1
    IF    ${poller_index} == -1
        ${count}    Ctn Get Engines Count
        FOR    ${idx}    IN RANGE    0    ${count}
            ${alias}    Catenate    SEPARATOR=    e    ${idx}
            Send Signal To Process    SIGHUP    ${alias}
        END
    ELSE
        ${alias}    Catenate    SEPARATOR=    e    ${poller_index}
        Send Signal To Process    SIGHUP    ${alias}
    END

Ctn Check Connections
	${count}=	Ctn Get Engines Count
	${pid1}=	Get Process Id	b1
	FOR	${idx}	IN RANGE	0	${count}
	 ${alias}=	Catenate	SEPARATOR=	e	${idx}
	 ${pid2}=	Get Process Id	${alias}
	 Log to console	Check Connection 5669 ${pid1} ${pid2}
	 ${retval}=	Ctn Check Connection	5669	${pid1}	${pid2}
	 Return from Keyword If	${retval} == ${False}	${False}
	END
	${pid2}=	Get Process Id	b2
	Log to console	Check Connection 5670 ${pid1} ${pid2}
	${retval}=	Ctn Check Connection	5670	${pid1}	${pid2}
	RETURN	${retval}

Ctn Disable Eth Connection On Port
	[Arguments]	${port}
	RUN	iptables -A INPUT -p tcp --dport ${port} -j DROP
	RUN	iptables -A OUTPUT -p tcp --dport ${port} -j DROP
	RUN	iptables -A FORWARD -p tcp --dport ${port} -j DROP

Ctn Reset Eth Connection
	Run	iptables -F
	Run	iptables -X

Ctn Save Logs If Failed
	Run Keyword If Test Failed	Ctn Save Logs

Ctn Save Logs
	Create Directory	failed
	${failDir}=	Catenate	SEPARATOR=	failed/	${Test Name}
	Create Directory	${failDir}
	Copy files	${centralLog}	${failDir}
	Copy files	${rrdLog}	${failDir}
	Copy files	${moduleLog0}	${failDir}
	Copy files	${engineLog0}	${failDir}
	Copy files	${ENGINE_LOG}/config0/gcore_*	${failDir}
	Copy Files	${EtcRoot}/centreon-engine/config0/*.cfg	${failDir}/etc/centreon-engine/config0
	Copy Files	${EtcRoot}/centreon-broker/*.json	${failDir}/etc/centreon-broker
	Move Files	/tmp/lua*.log	${failDir}


Ctn Dump Process
	[Arguments]	${process_name}	${name}
	${pid}=	Get Process Id	${process_name}
	${failDir}=	Catenate	SEPARATOR=	failed/	${Test Name}
	Create Directory	${failDir}
	${output}=	Catenate	SEPARATOR=	${failDir}	/core-	${name}
	Log To Console	Creation of core ${output}.${pid} to debug
	Run Process	gcore	-o	${output}	${pid}
	Log To Console	Done...


Ctn Wait Or Dump And Kill Process
	[Arguments]  ${process_name}  ${timeout}
	${result}=      Wait For Process	${process_name}	timeout=${timeout}      on_timeout=continu
	${test_none}=  Set Variable If  $result is None  "not killed"  "killed"
	IF  ${test_none} == "not killed"
		${pid}=  Get Process Id  ${process_name}
		Run Process  gcore  -o  ${ENGINE_LOG}/config0/gcore_${process_name}  ${pid}
		${result}=      Wait For Process	${process_name}	timeout=1s      on_timeout=kill
	END
	RETURN	${result}

Ctn Clear Metrics
	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	Execute SQL String	DELETE FROM metrics
	Execute SQL String	DELETE FROM index_data
	Execute SQL String	DELETE FROM data_bin

Ctn Dump Ba On Error
    [Arguments]    ${result}    ${ba_id}
    IF    not ${result}
        Ctn Save Logs
        Ctn Broker Get Ba    51001    ${ba_id}    failed/${Test Name}/ba_${ba_id}.dot
    END

Ctn Process Service Result Hard
    [Arguments]    ${host}    ${svc}    ${state}    ${output}
    Repeat Keyword
    ...    3 times
    ...    Ctn Process Service Check Result
    ...    ${host}
    ...    ${svc}
    ...    ${state}
    ...    ${output}

Ctn Wait For Engine To Be Ready
    [Arguments]    ${start}    ${nbEngine}=1
    FOR    ${i}    IN RANGE    ${nbEngine}
        # Let's wait for the external command check start
        ${content}    Create List    check_for_external_commands()
        ${result}    Ctn Find In Log With Timeout
        ...    ${ENGINE_LOG}/config${i}/centengine.log
        ...    ${start}    ${content}    60
        Should Be True
        ...    ${result}
        ...    A message telling check_for_external_commands() should be available in config${i}/centengine.log.
    END
