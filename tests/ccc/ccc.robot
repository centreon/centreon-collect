*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes

Documentation	ccc tests with engine and broker
Library	Process
Library	DateTime
Library	OperatingSystem
Library	String
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py

*** Test Cases ***
BECCC1
	[Documentation]	ccc without port fails with an error message
	[Tags]	Broker	Engine	ccc
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
	Clear Retention
	${start}=	Get Current Date
        Sleep	1s
	Start Broker
	Start Engine
	Sleep	3s
	Start Process	/usr/bin/ccc	stderr=/tmp/output.txt
	FOR	${i}	IN RANGE	10
	 Wait Until Created	/tmp/output.txt
	 ${content}=	Get File	/tmp/output.txt
	 EXIT FOR LOOP IF	len("${content.strip()}") > 0
	 Sleep	1s
	END
	should be equal as strings	${content.strip()}	You must specify a port for the connection to the gRPC server
	Stop Engine
	Kindly Stop Broker
	Remove File	/tmp/output.txt

BECCC2
	[Documentation]	ccc with -p 51001 connects to central cbd gRPC server.
	[Tags]	Broker	Engine	protobuf	bbdo	ccc
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
        Broker Config Add Item	module0	bbdo_version	3.0.0
        Broker Config Add Item	central	bbdo_version	3.0.0
        Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	sql	trace
	Config Broker Sql Output	central	unified_sql
        Broker Config Output Set	central	central-broker-unified-sql	store_in_resources	yes
        Broker Config Output Set	central	central-broker-unified-sql	store_in_hosts_services	no
	Clear Retention
	${start}=	Get Current Date
        Sleep	1s
	Start Broker
	Start Engine
	Sleep	3s
	Start Process	/usr/bin/ccc	-p 51001	stderr=/tmp/output.txt
	FOR	${i}	IN RANGE	10
	 Wait Until Created	/tmp/output.txt
	 ${content}=	Get File	/tmp/output.txt
	 EXIT FOR LOOP IF	len("${content.strip()}") > 0
	 Sleep	1s
	END

	${version}=	Get Version
	${expected}=	Catenate	Connected to a Centreon Broker	${version}	gRPC server
	Should Be Equal As Strings	${content.strip()}	${expected}
	Stop Engine
	Kindly Stop Broker
	Remove File	/tmp/output.txt

BECCC3
	[Documentation]	ccc with -p 50001 connects to centengine gRPC server.
	[Tags]	Broker	Engine	protobuf	bbdo
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
        Broker Config Add Item	module0	bbdo_version	3.0.0
        Broker Config Add Item	central	bbdo_version	3.0.0
        Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	sql	trace
	Config Broker Sql Output	central	unified_sql
        Broker Config Output Set	central	central-broker-unified-sql	store_in_resources	yes
        Broker Config Output Set	central	central-broker-unified-sql	store_in_hosts_services	no
	Clear Retention
	${start}=	Get Current Date
        Sleep	1s
	Start Broker
	Start Engine
	Sleep	3s
	Start Process	/usr/bin/ccc	-p 50001	stderr=/tmp/output.txt
	FOR	${i}	IN RANGE	10
	 Wait Until Created	/tmp/output.txt
	 ${content}=	Get File	/tmp/output.txt
	 EXIT FOR LOOP IF	len("${content.strip()}") > 0
	 Sleep	1s
	END
	${version}=	Get Version
	${expected}=	Catenate	Connected to a Centreon Engine	${version}	gRPC server
	Should Be Equal As Strings	${content.strip()}	${expected}
	Stop Engine
	Kindly Stop Broker
	Remove File	/tmp/output.txt

BECCC4
	[Documentation]	ccc with -p 51001 -l returns the available functions from Broker gRPC server
	[Tags]	Broker	Engine	protobuf	bbdo	ccc
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
        Broker Config Add Item	module0	bbdo_version	3.0.0
        Broker Config Add Item	central	bbdo_version	3.0.0
        Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	sql	trace
	Config Broker Sql Output	central	unified_sql
        Broker Config Output Set	central	central-broker-unified-sql	store_in_resources	yes
        Broker Config Output Set	central	central-broker-unified-sql	store_in_hosts_services	no
	Clear Retention
	${start}=	Get Current Date
        Sleep	1s
	Start Broker
	Start Engine
	Sleep	3s
	Start Process	/usr/bin/ccc	-p 51001	-l	stdout=/tmp/output.txt
	FOR	${i}	IN RANGE	10
	 Wait Until Created	/tmp/output.txt
	 ${content}=	Get File	/tmp/output.txt
	 EXIT FOR LOOP IF	len("""${content.strip()}""") > 0
	 Sleep	1s
	END
	${contains}=	Evaluate	"GetVersion" in """${content}"""
	Should Be True	${contains}	msg=The list of methods should contain GetVersion(Empty)
	Stop Engine
	Kindly Stop Broker
	Remove File	/tmp/output.txt

BECCC5
	[Documentation]	ccc with -p 51001 -l GetVersion returns an error because we can't execute a command with -l.
	[Tags]	Broker	Engine	protobuf	bbdo	ccc
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
        Broker Config Add Item	module0	bbdo_version	3.0.0
        Broker Config Add Item	central	bbdo_version	3.0.0
        Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	sql	trace
	Config Broker Sql Output	central	unified_sql
        Broker Config Output Set	central	central-broker-unified-sql	store_in_resources	yes
        Broker Config Output Set	central	central-broker-unified-sql	store_in_hosts_services	no
	Clear Retention
	${start}=	Get Current Date
        Sleep	1s
	Start Broker
	Start Engine
	Sleep	3s
	Start Process	/usr/bin/ccc	-p 51001	-l	GetVersion	stderr=/tmp/output.txt
	FOR	${i}	IN RANGE	10
	 Wait Until Created	/tmp/output.txt
	 ${content}=	Get File	/tmp/output.txt
	 EXIT FOR LOOP IF	len("""${content.strip()}""") > 0
	 Sleep	1s
	END
	${contains}=	Evaluate	"The list argument expects no command" in """${content}"""
	Should Be True	${contains}	msg=When -l option is applied, we can't call a command.
	Stop Engine
	Kindly Stop Broker
	Remove File	/tmp/output.txt

BECCC6
	[Documentation]	ccc with -p 51001 GetVersion{} calls the GetVersion command
	[Tags]	Broker	Engine	protobuf	bbdo	ccc
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
        Broker Config Add Item	module0	bbdo_version	3.0.0
        Broker Config Add Item	central	bbdo_version	3.0.0
        Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	sql	trace
	Config Broker Sql Output	central	unified_sql
        Broker Config Output Set	central	central-broker-unified-sql	store_in_resources	yes
        Broker Config Output Set	central	central-broker-unified-sql	store_in_hosts_services	no
	Clear Retention
	${start}=	Get Current Date
        Sleep	1s
	Start Broker
	Start Engine
	Sleep	3s
	Start Process	/usr/bin/ccc	-p 51001	GetVersion{}	stdout=/tmp/output.txt
	FOR	${i}	IN RANGE	10
	 Wait Until Created	/tmp/output.txt
	 ${content}=	Get File	/tmp/output.txt
	 EXIT FOR LOOP IF	len("""${content.strip().split()}""") > 50
	 Sleep	1s
	END
	${version}=     Get Version
	${vers}=        Split String    ${version}      .
	${mm}=  Evaluate        """${vers}[0]""".lstrip("0")
	${m}=   Evaluate        """${vers}[1]""".lstrip("0")
	${p}=   Evaluate        """${vers}[2]""".lstrip("0")
	Should Contain  ${content}      {\n \"major\": ${mm},\n \"minor\": ${m},\n \"patch\": ${p}\n}      msg=A version as json string should be returned
	Stop Engine
	Kindly Stop Broker
	Remove File	/tmp/output.txt

BECCC7
	[Documentation]	ccc with -p 51001 GetVersion{"idx":1} returns an error because the input message is wrong.
	[Tags]	Broker	Engine	protobuf	bbdo	ccc
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
        Broker Config Add Item	module0	bbdo_version	3.0.0
        Broker Config Add Item	central	bbdo_version	3.0.0
        Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	sql	trace
	Config Broker Sql Output	central	unified_sql
        Broker Config Output Set	central	central-broker-unified-sql	store_in_resources	yes
        Broker Config Output Set	central	central-broker-unified-sql	store_in_hosts_services	no
	Clear Retention
	${start}=	Get Current Date
        Sleep	1s
	Start Broker
	Start Engine
	Sleep	3s
	Start Process	/usr/bin/ccc	-p 51001	GetVersion{"idx":1}	stderr=/tmp/output.txt
	FOR	${i}	IN RANGE	10
	 Wait Until Created	/tmp/output.txt
	 ${content}=	Get File	/tmp/output.txt
	 EXIT FOR LOOP IF	len("""${content.strip().split()}""") > 10
	 Sleep	1s
	END
	Should Contain	${content}	Error during the execution of '/com.centreon.broker.Broker/GetVersion' method:	msg=GetVersion{"idx":1} should return an error because the input message is incompatible with the expected one.
	Stop Engine
	Kindly Stop Broker
	Remove File	/tmp/output.txt

BECCC8
	[Documentation]	ccc with -p 50001 EnableServiceNotifications{"names":{"host_name": "host_1", "service_name": "service_1"}} works and returns an empty message.
	[Tags]	Broker	Engine	protobuf	bbdo	ccc
	Config Engine	${1}
	Config Broker	central
	Config Broker	module
	Config Broker	rrd
        Broker Config Add Item	module0	bbdo_version	3.0.0
        Broker Config Add Item	central	bbdo_version	3.0.0
        Broker Config Add Item	rrd	bbdo_version	3.0.0
	Broker Config Log	central	sql	trace
	Config Broker Sql Output	central	unified_sql
        Broker Config Output Set	central	central-broker-unified-sql	store_in_resources	yes
        Broker Config Output Set	central	central-broker-unified-sql	store_in_hosts_services	no
	Clear Retention
	${start}=	Get Current Date
        Sleep	1s
	Start Broker
	Start Engine
	Sleep	3s
	Start Process	/usr/bin/ccc	-p 50001	EnableServiceNotifications{"names":{"host_name": "host_1", "service_name": "service_1"}}  stdout=/tmp/output.txt
	FOR	${i}	IN RANGE	10
	 Wait Until Created	/tmp/output.txt
	 ${content}=	Get File	/tmp/output.txt
	 EXIT FOR LOOP IF	len("""${content.strip().split()}""") > 2
	 Sleep	1s
	END
	Should Contain	${content}	{}
	Stop Engine
	Kindly Stop Broker
	Remove File	/tmp/output.txt

