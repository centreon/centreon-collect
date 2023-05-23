*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes
Test Teardown	Save logs If Failed

Documentation	Centreon Broker RRD metric deletion
Library	DatabaseLibrary
Library	Process
Library	OperatingSystem
Library	DateTime
Library	Collections
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py

*** Test Cases ***
BRRDDM1
	Start Mysql
	[Documentation]	RRD metrics deletion from metric ids.
	[Tags]	RRD	metric	deletion
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module
	Broker Config Log	central	sql	info
	Broker Config Log	rrd	rrd	debug
	Broker Config Log	rrd	core	error
	Create Metrics	3
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected

	# We choose 3 metrics to remove.
	${metrics}=	Get Metrics To Delete	3
	Log To Console	Metrics to delete ${metrics}

	${empty}=	Create List
	Remove Graphs	51001	${empty}	${metrics}
	${metrics_str}=	Catenate	SEPARATOR=,	@{metrics}
	${content}=	Create List	metrics ${metrics_str} erased from database

	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	30
	Should Be True	${result}	msg=No log message telling about metrics ${metrics_str} deletion.
	FOR	${m}	IN	@{metrics}
		Wait Until Removed	${VarRoot}/lib/centreon/metrics/${m}.rrd      20s
	END

BRRDDID1
	[Documentation]	RRD metrics deletion from index ids.
	[Tags]	RRD	metric	deletion
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module
	Broker Config Log	central	sql	info
	Broker Config Log	rrd	rrd	debug
	Broker Config Log	rrd	core	error
	Create Metrics	3

	${start}=	Get Current Date
	Sleep	1s
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected

	${indexes}=	Get Indexes To Delete	2
	${metrics}=	Get Metrics Matching Indexes	${indexes}
	Log To Console	indexes ${indexes} to delete with their metrics

	${empty}=	Create List
	Remove Graphs	51001	${indexes}	${empty}
	${indexes_str}=	Catenate	SEPARATOR=,	@{indexes}
	${content}=	Create List	indexes ${indexes_str} erased from database

	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	30
	Should Be True	${result}	msg=No log message telling about indexes ${indexes_str} deletion.
	FOR	${i}	IN	@{indexes}
		Wait Until Removed	${VarRoot}/lib/centreon/status/${i}.rrd	20s
	END
	FOR	${m}	IN	@{metrics}
		Wait Until Removed	${VarRoot}/lib/centreon/metrics/${m}.rrd	20s
	END

BRRDDMID1
	[Documentation]	RRD deletion of non existing metrics and indexes
	[Tags]	RRD	metric	deletion
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module
	Broker Config Log	central	sql	info
	Broker Config Log	rrd	rrd	debug
	Broker Config Log	rrd	core	error

	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected

	${indexes}=	Get Not Existing Indexes 	2
	${metrics}=	Get Not Existing Metrics 	2
	Log To Console	indexes ${indexes} and metrics ${metrics} to delete but they do not exist.

	Remove Graphs	51001	${indexes}	${metrics}
	${content}=	Create List	do not appear in the storage database
	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	30
	Should Be True	${result}	msg=A message telling indexes nor metrics appear in the storage database should appear.

BRRDDMU1
	[Documentation]	RRD metric deletion on table metric with unified sql output
	[Tags]	RRD	metric	deletion unified_sql
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central
	Config Broker Sql Output	central	unified_sql
	Config Broker	module
	Broker Config Log	central	sql	info
	Broker Config Log	rrd	rrd	debug
	Broker Config Log	rrd	core	error
	Create Metrics	3

	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected

	# We choose 3 metrics to remove.
	${metrics}=	Get Metrics To Delete	3
	Log To Console	metrics to delete ${metrics}

	${empty}=	Create List
	Remove Graphs	51001	${empty}	${metrics}
	${metrics_str}=	Catenate	SEPARATOR=,	@{metrics}
	${content}=	Create List	metrics ${metrics_str} erased from database

	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	50
	Should Be True	${result}	msg=No log message telling about metrics ${metrics_str} deletion.
	FOR	${m}	IN	@{metrics}
		Wait Until Removed	${VarRoot}/lib/centreon/metrics/${m}.rrd	20s
	END

BRRDDIDU1
	[Documentation]	RRD metrics deletion from index ids with unified sql output.
	[Tags]	RRD	metric	deletion	unified_sql
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central
	Config Broker Sql Output	central	unified_sql
	Config Broker	module
	Broker Config Log	central	sql	info
	Broker Config Log	rrd	rrd	debug
	Broker Config Log	rrd	core	error
	Create Metrics	3

	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected

	${indexes}=	Get Indexes To Delete	2
	${metrics}=	Get Metrics Matching Indexes	${indexes}
	Log To Console	indexes ${indexes} to delete with their metrics

	${empty}=	Create List
	Remove Graphs	51001	${indexes}	${empty}
	${indexes_str}=	Catenate	SEPARATOR=,	@{indexes}
	${content}=	Create List	indexes ${indexes_str} erased from database

	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	30
	Should Be True	${result}	msg=No log message telling about indexes ${indexes_str} deletion.
	FOR	${i}	IN	@{indexes}
		Wait Until Removed	${VarRoot}/lib/centreon/status/${i}.rrd	20s
	END
	FOR	${m}	IN	@{metrics}
		Wait Until Removed	${VarRoot}/lib/centreon/metrics/${m}.rrd	20s
	END

BRRDDMIDU1
	[Documentation]	RRD deletion of non existing metrics and indexes
	[Tags]	RRD	metric	deletion	unified_sql
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central
	Config Broker Sql Output	central	unified_sql
	Config Broker	module
	Broker Config Log	central	sql	info
	Broker Config Log	rrd	rrd	debug
	Broker Config Log	rrd	core	error

	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected

	${indexes}=	Get Not Existing Indexes 	2
	${metrics}=	Get Not Existing Metrics 	2
	Log To Console	indexes ${indexes} and metrics ${metrics} to delete but they do not exist.

	Remove Graphs	51001	${indexes}	${metrics}
	${content}=	Create List	do not appear in the storage database
	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	30
	Should Be True	${result}	msg=A message telling indexes nor metrics appear in the storage database should appear.


BRRDRM1
	[Documentation]	RRD metric rebuild with gRPC API and unified sql
	[Tags]	RRD	metric	rebuild	grpc
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module
	Broker Config Log	rrd	rrd	trace
	Broker Config Log	central	sql	trace
	Create Metrics	3

	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected

	# We get 3 indexes to rebuild
	${index}=	Get Indexes To Rebuild	3
	Rebuild Rrd Graphs	51001	${index}	1
	Log To Console	Indexes to rebuild: ${index}
	${metrics}=	Get Metrics Matching Indexes	${index}
	Log To Console	Metrics to rebuild: ${metrics}
	${content}=	Create List	Metric rebuild: metric	is sent to rebuild
	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	30
	Should Be True	${result}	msg=Central did not send metrics to rebuild

	${content1}=	Create List	RRD: Starting to rebuild metrics
	${result}=	Find In Log With Timeout	${rrdLog}	${start}	${content1}	45
	Should Be True	${result}	msg=RRD cbd did not receive metrics to rebuild START

	${content1}=	Create List	RRD: Rebuilding metric
	${result}=	Find In Log With Timeout	${rrdLog}	${start}	${content1}	45
	Should Be True	${result}	msg=RRD cbd did not receive metrics to rebuild DATA

	${content1}=	Create List	RRD: Finishing to rebuild metrics
	${result}=	Find In Log With Timeout	${rrdLog}	${start}	${content1}	500
	Should Be True	${result}	msg=RRD cbd did not receive metrics to rebuild END
	FOR	${m}	IN	@{metrics}
		${value}=	Evaluate	${m} / 2
		${result}=	Compare RRD Average Value	${m}	${value}
		Should Be True	${result}	msg=Data before RRD rebuild contain alternatively the metric ID and 0. The expected average is metric_id / 2.
	END

BRRDRMU1
	[Documentation]	RRD metric rebuild with gRPC API and unified sql
	[Tags]	RRD	metric	rebuild	unified_sql	grpc
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central
	Config Broker Sql Output	central	unified_sql
	Config Broker	module
	Broker Config Log	rrd	rrd	trace
	Broker Config Log	central	sql	trace
	Create Metrics	3

	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected

	# We get 3 indexes to rebuild
	${index}=	Get Indexes To Rebuild	3
	Rebuild Rrd Graphs	51001	${index}	1
	Log To Console	Indexes to rebuild: ${index}
	${metrics}=	Get Metrics Matching Indexes	${index}
	Log To Console	Metrics to rebuild: ${metrics}
	${content}=	Create List	Metric rebuild: metric	is sent to rebuild	Metric rebuild: Rebuild of metrics from the following indexes
	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	30
	Should Be True	${result}	msg=Central did not send metrics to rebuild

	${content1}=	Create List	RRD: Starting to rebuild metrics
	${result}=	Find In Log With Timeout	${rrdLog}	${start}	${content1}	30
	Should Be True	${result}	msg=RRD cbd did not receive metrics to rebuild START

	${content1}=	Create List	RRD: Rebuilding metric
	${result}=	Find In Log With Timeout	${rrdLog}	${start}	${content1}	30
	Should Be True	${result}	msg=RRD cbd did not receive metrics to rebuild DATA

	${content1}=	Create List	RRD: Finishing to rebuild metrics
	${result}=	Find In Log With Timeout	${rrdLog}	${start}	${content1}	500
	Should Be True	${result}	msg=RRD cbd did not receive metrics to rebuild END
        FOR	${m}	IN	@{metrics}
		${value}=	Evaluate	${m} / 2
        	${result}=	Compare RRD Average Value	${m}	${value}
                Should Be True	${result}	msg=Data before RRD rebuild contain alternatively the metric ID and 0. The expected average is metric_id / 2.
        END
