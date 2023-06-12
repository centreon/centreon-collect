*** Settings ***
Resource	./db_variables.robot
Library	Process
Library	OperatingSystem
Library	Common.py

*** Keywords ***
Config BBDO3
	[Arguments]	${nbEngine}
	Config Broker Sql Output	central	unified_sql
        Broker Config Add Item	rrd	bbdo_version	3.0.0
        Broker Config Add Item	central	bbdo_version	3.0.0
        FOR	${i}	IN RANGE	${nbEngine}
	 ${mod}=	Catenate	SEPARATOR=	module	${i}
         Broker Config Add Item	${mod}	bbdo_version	3.0.0
	END

Clean Before Suite
	Stop Processes
	Clear Engine Logs
	Clear Broker Logs

Clean Before Suite With rrdcached
	Clean Before Suite
	log to console	Starting RRDCached
	Run Process	/usr/bin/rrdcached	-l	unix:${BROKER_LIB}/rrdcached.sock	-V	LOG_DEBUG	-F

Clean Grpc Before Suite
	set grpc port  0
	Clean Before Suite

Clean After Suite
	# Remove Files	${ENGINE_LOG}${/}centengine.log ${ENGINE_LOG}${/}centengine.debug
	# Remove Files	${BROKER_LOG}${/}central-broker-master.log	${BROKER_LOG}${/}central-rrd-master.log	${BROKER_LOG}${/}central-module-master.log
	Terminate All Processes	kill=True

Clean After Suite With rrdcached
	Clean after Suite
	log to console	Stopping RRDCached
	Stop rrdcached

Clear Engine Logs
	Remove Directory	${ENGINE_LOG}	Recursive=True
	Create Directory	${ENGINE_LOG}

Clear Broker Logs
	Remove Directory	${BROKER_LOG}	Recursive=True
	Create Directory	${BROKER_LOG}

Start Broker
	[Arguments]	 ${only_central}=False
	Start Process	/usr/sbin/cbd	${EtcRoot}/centreon-broker/central-broker.json	alias=b1
	IF  not ${only_central}
		Start Process	/usr/sbin/cbd	${EtcRoot}/centreon-broker/central-rrd.json	alias=b2
	END

Reload Broker
	Send Signal To Process	SIGHUP	b1
	Send Signal To Process	SIGHUP	b2

Kindly Stop Broker
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

Stop Broker
	${result}=	Terminate Process	b1	kill=False
	Should Be Equal As Integers	${result.rc}	0
	${result}=	Terminate Process	b2	kill=False
	Should Be Equal As Integers	${result.rc}	0

Stop Processes
	Terminate All Processes	kill=True
	Kill Broker
	Kill Engine

Start Engine
	${count}=	Get Engines Count
	FOR	${idx}	IN RANGE	0	${count}
	 ${alias}=	Catenate	SEPARATOR=	e	${idx}
	 ${conf}=	Catenate	SEPARATOR=	${EtcRoot}  /centreon-engine/config	${idx}	/centengine.cfg
	 ${log}=	Catenate	SEPARATOR=	${VarRoot}  /log/centreon-engine/config	${idx}
	 ${lib}=	Catenate	SEPARATOR=	${VarRoot}  /lib/centreon-engine/config	${idx}
	 Create Directory	${log}
	 Create Directory	${lib}
	 Start Process	/usr/sbin/centengine	${conf}  stdout=${VarRoot}/log/centreon-engine/engine-cout.log  stderr=${VarRoot}/log/centreon-engine/engine-cerr.log  alias=${alias}
#	 ${log_pid1}=  Get Process Id	${alias}
#	 Log To Console  \npidengine${idx}=${log_pid1}\n
	END

Start Custom Engine
	[Arguments]	 ${conf_path}  ${process_alias}
	Start Process  /usr/sbin/centengine  ${conf_path}  alias=${process_alias}

Stop Custom Engine
	[Arguments]  ${process_alias}
	${result}=	Terminate Process	${process_alias}
	Should Be True	${result.rc} == -15 or ${result.rc} == 0	msg=Engine badly stopped alias = ${process_alias} - code returned ${result.rc}.

Stop Engine
	${count}=	Get Engines Count
	FOR	${idx}	IN RANGE	0	${count}
	 ${alias}=	Catenate	SEPARATOR=	e	${idx}
	 ${result}=	Terminate Process	${alias}
	 Should Be True	${result.rc} == -15 or ${result.rc} == 0	msg=Engine badly stopped with ${count} instances - code returned ${result.rc}.
	END

Reload Engine
	${count}=	Get Engines Count
	FOR	${idx}	IN RANGE	0	${count}
	 ${alias}=	Catenate	SEPARATOR=	e	${idx}
	 Send Signal To Process	SIGHUP	${alias}
	END

Check Connections
	${count}=	Get Engines Count
	${pid1}=	Get Process Id	b1
	FOR	${idx}	IN RANGE	0	${count}
	 ${alias}=	Catenate	SEPARATOR=	e	${idx}
	 ${pid2}=	Get Process Id	${alias}
	 ${retval}=	Check Connection	5669	${pid1}	${pid2}
	 Return from Keyword If	${retval} == ${False}	${False}
	END
	${pid2}=	Get Process Id	b2
	${retval}=	Check Connection	5670	${pid1}	${pid2}
	[Return]	${retval}

Disable Eth Connection On Port
	[Arguments]	${port}
	RUN	iptables -A INPUT -p tcp --dport ${port} -j DROP
	RUN	iptables -A OUTPUT -p tcp --dport ${port} -j DROP
	RUN	iptables -A FORWARD -p tcp --dport ${port} -j DROP

Reset Eth Connection
	Run	iptables -F
	Run	iptables -X

Save Logs If failed
	Run Keyword If Test Failed	Save Logs

Save Logs
	Create Directory	failed
        ${failDir}=	Catenate	SEPARATOR=	failed/	${Test Name}
        Create Directory	${failDir}
        Copy files	${centralLog}	${failDir}
        Copy files	${moduleLog}	${failDir}
        Copy files	${logEngine0}	${failDir}

Dump Process
	[Arguments]	${process_name}	${name}
	${pid}=	Get Process Id	${process_name}
	${failDir}=	Catenate	SEPARATOR=	failed/	${Test Name}
	Create Directory	${failDir}
	${output}=	Catenate	SEPARATOR=	${failDir}	/core-	${name}
	Log To Console	Creation of core ${output}.${pid} to debug
	Run Process	gcore	-o	${output}	${pid}
	Log To Console	Done...

Clear Metrics
	Connect To Database	pymysql	${DBName}	${DBUser}	${DBPass}	${DBHost}	${DBPort}
	Execute SQL String	DELETE FROM metrics
	Execute SQL String	DELETE FROM index_data
	Execute SQL String	DELETE FROM data_bin

*** Variables ***
${BROKER_LOG}	${VarRoot}/log/centreon-broker
${BROKER_LIB}	${VarRoot}/lib/centreon-broker
${ENGINE_LOG}	${VarRoot}/log/centreon-engine
${SCRIPTS}	${CURDIR}${/}scripts${/}
${centralLog}	${BROKER_LOG}/central-broker-master.log
${moduleLog}	${BROKER_LOG}/central-module-master0.log
${rrdLog}	${BROKER_LOG}/central-rrd-master.log

${logEngine0}	${ENGINE_LOG}/config0/centengine.log
${logEngine1}	${ENGINE_LOG}/config1/centengine.log
${logEngine2}	${ENGINE_LOG}/config2/centengine.log
