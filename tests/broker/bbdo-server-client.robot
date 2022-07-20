*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes

Documentation	Centreon Broker start/stop tests with bbdo_server and bbdo_client input/output streams. Only these streams are used instead of grpc and tcp.
Library	DateTime
Library	Process
Library	OperatingSystem
Library	../resources/Broker.py


*** Test Cases ***
BSCSS1
	[Documentation]	Start-Stop two instances of broker and no coredump
	[Tags]	Broker	start-stop	bbdo_server	bbdo_client	tcp
	Config Broker	central
	Config Broker	rrd
	Config Broker BBDO Input	central	bbdo_server	5669  tcp
	Config Broker BBDO Output	central	bbdo_client	5670  tcp	localhost
	Config Broker BBDO Input	rrd	bbdo_server	5670  tcp
	Broker Config Log	central	config	info
	Repeat Keyword	5 times	Start Stop Service	0

BSCSS2
	[Documentation]	Start/Stop 10 times broker with 300ms interval and no coredump
	[Tags]	Broker	start-stop	bbdo_server	bbdo_client	tcp
	Config Broker	central
	Config Broker BBDO Input	central	bbdo_server	5669  tcp
	Config Broker BBDO Output	central	bbdo_client	5670  tcp	localhost
	Repeat Keyword	10 times	Start Stop Instance	300ms

BSCSS3
	[Documentation]	Start-Stop one instance of broker and no coredump
	[Tags]	Broker	start-stop	bbdo_server	bbdo_client	tcp
	Config Broker	central
	Config Broker BBDO Input	central	bbdo_server	5669  tcp
	Config Broker BBDO Output	central	bbdo_client	5670  tcp	localhost
	Repeat Keyword	5 times	Start Stop Instance	0

BSCSS4
	[Documentation]	Start/Stop 10 times broker with 1sec interval and no coredump
	[Tags]	Broker	start-stop	bbdo_server	bbdo_client	tcp
	Config Broker	central
	Config Broker BBDO Input	central	bbdo_server	5669  tcp
	Config Broker BBDO Output	central	bbdo_client	5670  tcp	localhost
	Repeat Keyword	10 times	Start Stop Instance	1s

BSCSSG1
	[Documentation]	Start-Stop two instances of broker and no coredump
	[Tags]	Broker	start-stop	bbdo_server	bbdo_client	grpc
	Config Broker	central
	Config Broker	rrd
	Config Broker BBDO Input	central	bbdo_server	5669  grpc	localhost
	Config Broker BBDO Output	central	bbdo_client	5670  grpc	localhost
	Config Broker BBDO Input	rrd	bbdo_server	5670  grpc
	Broker Config Log	central	config	info
	Repeat Keyword	5 times	Start Stop Service	0

BSCSSG2
	[Documentation]	Start/Stop 10 times broker with 300ms interval and no coredump
	[Tags]	Broker	start-stop	bbdo_server	bbdo_client	grpc
	Config Broker	central
	Config Broker BBDO Input	central	bbdo_server	5669  grpc	localhost
	Config Broker BBDO Output	central	bbdo_client	5670  grpc	localhost
	Repeat Keyword	10 times	Start Stop Instance	300ms

BSCSSG3
	[Documentation]	Start-Stop one instance of broker and no coredump
	[Tags]	Broker	start-stop	bbdo_server	bbdo_client	grpc
	Config Broker	central
	Config Broker BBDO Input	central	bbdo_server	5669  grpc
	Config Broker BBDO Output	central	bbdo_client	5670  grpc	localhost
	Repeat Keyword	5 times	Start Stop Instance	0

BSCSSG4
	[Documentation]	Start/Stop 10 times broker with 1sec interval and no coredump
	[Tags]	Broker	start-stop	bbdo_server	bbdo_client	grpc
	Config Broker	central
	Config Broker BBDO Input	central	bbdo_server	5669  grpc
	Config Broker BBDO Output	central	bbdo_client	5670  grpc	localhost
	Repeat Keyword	10 times	Start Stop Instance	1s

BSCSST1
	[Documentation]	Start-Stop two instances of broker and no coredump. Encryption is enabled on client side.
	[Tags]	Broker	start-stop	bbdo_server	bbdo_client	tcp	tls
	Config Broker	central
	Config Broker	rrd
	Config Broker BBDO Input	central	bbdo_server	5669  tcp
	Config Broker BBDO Output	central	bbdo_client	5670  tcp	localhost
	Config Broker BBDO Input	rrd	bbdo_server	5670  tcp
	Broker Config Output set	central	central-broker-master-output	encryption	yes
	Broker Config Input set	rrd	rrd-broker-master-input	encryption	yes
	Broker Config Log	central	config	off
	Broker Config Log	central	core	off
	Broker Config Log	central	tls	debug
	${start}=	Get Current Date
	Repeat Keyword	5 times	Start Stop Service	0
	${content}=	Create List	TLS: successful handshake
	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	30
	Should Be True	${result}	msg=No information about TLS activation.

BSCSST2
	[Documentation]	Start-Stop two instances of broker and no coredump. Encryption is enabled on client side.
	[Tags]	Broker	start-stop	bbdo_server	bbdo_client	tcp	tls
	Config Broker	central
	Config Broker	rrd
	Config Broker BBDO Input	central	bbdo_server	5669  tcp
	Config Broker BBDO Output	central	bbdo_client	5670  tcp	localhost
	Config Broker BBDO Input	rrd	bbdo_server	5670  tcp
	Broker Config Output set	central	central-broker-master-output	encryption	yes
	Broker Config Input set	rrd	rrd-broker-master-input	encryption	yes
	Broker Config Log	central	config	off
	Broker Config Log	central	core	off
	Broker Config Log	central	tls	debug
	Broker Config Output set	central	central-broker-master-output	private_key	/etc/centreon-broker/server.key
	Broker Config Output set	central	central-broker-master-output	certificate	/etc/centreon-broker/server.crt
	Broker Config Output set	central	central-broker-master-output	ca_certificate	/etc/centreon-broker/client.crt
	Broker Config Input set	rrd	rrd-broker-master-input	private_key	/etc/centreon-broker/client.key
	Broker Config Input set	rrd	rrd-broker-master-input	certificate	/etc/centreon-broker/client.crt
	${start}=	Get Current Date
	Repeat Keyword	5 times	Start Stop Service	0
	${content}=	Create List	TLS: successful handshake
	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	30
	Should Be True	${result}	msg=No information about TLS activation.

BSCSSTG1
	[Documentation]	Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. This is not sufficient, then an error is raised.
	[Tags]	Broker	start-stop	bbdo_server	bbdo_client	grpc	tls
	Config Broker	central
	Config Broker	rrd
	Config Broker BBDO Input	central	bbdo_server	5669  grpc
	Config Broker BBDO Output	central	bbdo_client	5670  grpc	localhost
	Config Broker BBDO Input	rrd	bbdo_server	5670  grpc
	Broker Config Output set	central	central-broker-master-output	encryption	yes
	Broker Config Input set	rrd	rrd-broker-master-input	encryption	yes
	Broker Config Log	central	config	off
	Broker Config Log	central	core	off
	Broker Config Log	rrd	core	off
	Broker Config Log	central	tls	debug
	Broker Config Log	central	grpc	debug
        Broker Config Flush Log	central	0
	${start}=	Get Current Date
	Start Broker
	${content}=	Create List	There is no ca certificate specified for bbdo_client
	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	30
	Should Be True	${result}	msg=No information about TLS activation.

BSCSSTG2
	[Documentation]	Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with encryption enabled. It works with good certificates and keys.
	[Tags]	Broker	start-stop	bbdo_server	bbdo_client	grpc	tls
	Config Broker	central
	Config Broker	rrd
	Config Broker BBDO Input	central	bbdo_server	5669  grpc
	Config Broker BBDO Output	central	bbdo_client	5670  grpc	localhost
	Config Broker BBDO Input	rrd	bbdo_server	5670  grpc
	Broker Config Output set	central	central-broker-master-output	encryption	yes
	Broker Config Input set	rrd	rrd-broker-master-input	encryption	yes
	Broker Config Log	central	config	off
	Broker Config Log	central	core	off
	Broker Config Log	rrd	core	off
	Broker Config Log	central	tls	debug
	Broker Config Log	central	grpc	debug
	${hostname}=	Get Hostname
	Create Key And Certificate	${hostname}	/etc/centreon-broker/server.key	/etc/centreon-broker/server.crt
	Create Key And Certificate	${hostname}	/etc/centreon-broker/client.key	/etc/centreon-broker/client.crt

	Broker Config Output set	central	central-broker-master-output	private_key	/etc/centreon-broker/server.key
	Broker Config Output set	central	central-broker-master-output	certificate	/etc/centreon-broker/server.crt
	Broker Config Output set	central	central-broker-master-output	ca_certificate	/etc/centreon-broker/client.crt
	Broker Config Input set	rrd	rrd-broker-master-input	private_key	/etc/centreon-broker/client.key
	Broker Config Input set	rrd	rrd-broker-master-input	certificate	/etc/centreon-broker/client.crt
	${start}=	Get Current Date
	Start Broker
	${content}=	Create List	start_write() write:buff:	write done :buff:
	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	30
	Should Be True	${result}	msg=No information about TLS activation.
	Kindly Stop Broker

BSCSSC1
	[Documentation]	Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is enabled on client side.
	[Tags]	Broker	start-stop	bbdo_server	bbdo_client	compression
	Config Broker	central
	Config Broker	rrd
	Config Broker BBDO Input	central	bbdo_server	5669  tcp
	Config Broker BBDO Output	central	bbdo_client	5670  tcp	localhost
	Config Broker BBDO Input	rrd	bbdo_server	5670  tcp
	Broker Config Output set	central	central-broker-master-output	compression	yes
	Broker Config Log	central	config	off
	Broker Config Log	central	core	trace
	Broker Config Log	rrd	core	trace
        Broker Config Flush Log	central	0
	${start}=	Get Current Date
	Start Broker
	${content}=	Create List	compression: writing
	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	30
	Should Be True	${result}	msg=No compression enabled
        Kindly Stop Broker

BSCSSC2
	[Documentation]	Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with tcp transport protocol. Compression is disabled on client side.
	[Tags]	Broker	start-stop	bbdo_server	bbdo_client	compression
	Config Broker	central
	Config Broker	rrd
	Config Broker BBDO Input	central	bbdo_server	5669  tcp
	Config Broker BBDO Output	central	bbdo_client	5670  tcp	localhost
	Config Broker BBDO Input	rrd	bbdo_server	5670  tcp
	Broker Config Output set	central	central-broker-master-output	compression	no
	Broker Config Log	central	config	off
	Broker Config Log	central	core	off
	Broker Config Log	rrd	core	trace
	Broker Config Log	central	bbdo	trace
        Broker Config Flush Log	central	0
	${start}=	Get Current Date
	Start Broker
	${content}=	Create List	BBDO: we have extensions '' and peer has 'COMPRESSION'
	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	30
	Should Be True	${result}	msg=Compression enabled but should not.
        Kindly Stop Broker

BSCSSCG1
	[Documentation]	Start-Stop two instances of broker. The connection is made by bbdo_client/bbdo_server with grpc transport protocol. Compression is enabled on client side.
	[Tags]	Broker	start-stop	bbdo_server	bbdo_client	compression	tls
	Config Broker	central
	Config Broker	rrd
	Config Broker BBDO Input	central	bbdo_server	5669  grpc
	Config Broker BBDO Output	central	bbdo_client	5670  grpc	localhost
	Config Broker BBDO Input	rrd	bbdo_server	5670  grpc
	Broker Config Output set	central	central-broker-master-output	compression	yes
	Broker Config Log	central	config	off
	Broker Config Log	central	core	off
	Broker Config Log	rrd	core	off
	Broker Config Log	central	tls	debug
	Broker Config Log	central	grpc	debug
        Broker Config Flush Log	central	0
	${start}=	Get Current Date
	Start Broker
	${content}=	Create List	activate compression deflate
	${result}=	Find In Log With Timeout	${centralLog}	${start}	${content}	30
	Should Be True	${result}	msg=No compression enabled
        Kindly Stop Broker

*** Keywords ***
Start Stop Service
	[Arguments]	${interval}
	Start Process	/usr/sbin/cbd	/etc/centreon-broker/central-broker.json	alias=b1
	Start Process	/usr/sbin/cbd	/etc/centreon-broker/central-rrd.json	alias=b2
	Sleep	${interval}
	${pid1}=	Get Process Id	b1
	${pid2}=	Get Process Id	b2
	${result}=	check connection	5670	${pid1}	${pid2}
	Should Be True	${result}	msg=The connection between cbd central and rrd is not established.

	Send Signal To Process	SIGTERM	b1
	${result}=	Wait For Process	b1	timeout=60s	on_timeout=kill
	Should Be True	${result.rc} == -15 or ${result.rc} == 0	msg=Broker service badly stopped
	Send Signal To Process	SIGTERM	b2
	${result}=	Wait For Process	b2	timeout=60s	on_timeout=kill
	Should Be True	${result.rc} == -15 or ${result.rc} == 0	msg=Broker service badly stopped

Start Stop Instance
	[Arguments]	${interval}
	Start Process	/usr/sbin/cbd	/etc/centreon-broker/central-broker.json	alias=b1
	Sleep	${interval}
	Send Signal To Process	SIGTERM	b1
	${result}=	Wait For Process	b1	timeout=60s	on_timeout=kill
	Should Be True	${result.rc} == -15 or ${result.rc} == 0	msg=Broker instance badly stopped
