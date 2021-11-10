*** Settings ***
Resource	../ressources/ressources.robot
Suite Setup	Clean Before Test
Suite Teardown    Clean After Test

Documentation	Centreon Broker and Engine communication with or without TLS
Library	Process
Library	OperatingSystem
Library	DateTime
Library	Collections
Library	../engine/Engine.py
Library	../broker/Broker.py
Library	Common.py

*** Test cases ***
BECT1: Broker/Engine communication with anonymous TLS between central and poller
	[Tags]	Broker	Engine	TLS	tcp
	Config Engine	${1}
	Config Broker	rrd
	FOR	${comp1}	IN	@{choices}
	FOR	${comp2}	IN	@{choices}
		Log To Console	TLS set to ${comp1} on central and to ${comp2} on module
		Config Broker	central
		Config Broker	module
		Broker Config Input set	central	central-broker-master-input	tls	${comp1}
		Broker Config Output set	module	central-module-master-output	tls	${comp2}
		Broker Config Log	central	bbdo	info
		Broker Config Log	module	bbdo	info
		${start}=	Get Current Date
		Start Broker
		Start Engine
		${result}=	Check Connections
		Should Be True	${result}	msg=Engine and Broker not connected
		Stop Broker
		Stop Engine
		${content1}=	Create List	we have extensions '${ext["${comp1}"]}' and peer has '${ext["${comp2}"]}'
		${content2}=	Create List	we have extensions '${ext["${comp2}"]}' and peer has '${ext["${comp1}"]}'
		IF	"${comp1}" == "yes" and "${comp2}" == "no"
			Insert Into List	${content1}	${-1}	extension 'TLS' is set to 'yes' in the configuration but cannot be activated because of peer configuration
		ELSE IF	"${comp1}" == "no" and "${comp2}" == "yes"
			Insert Into List	${content2}	${-1}	extension 'TLS' is set to 'yes' in the configuration but cannot be activated because of peer configuration
		END
		${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
		${result}=	Find In Log	${log}	${start}	${content1}
		Should Be True	${result}
		${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-module-master.log
		${result}=	Find In Log	${log}	${start}	${content2}
		Should Be True	${result}
	END
	END

BECT2: Broker/Engine communication with TLS between central and poller with key/cert
	[Tags]	Broker	Engine	TLS	tcp
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module

	${hostname}=	Get Hostname
	Create Key And Certificate	${hostname}	/etc/centreon-broker/server.key	/etc/centreon-broker/server.crt
	Create Key And Certificate	${hostname}	/etc/centreon-broker/client.key	/etc/centreon-broker/client.crt

	Broker Config Input set	central	central-broker-master-input	private_key	/etc/centreon-broker/client.key
	Broker Config Input set	central	central-broker-master-input	public_cert	/etc/centreon-broker/client.crt
	Broker Config Input set	central	central-broker-master-input	ca_certificate	/etc/centreon-broker/server.crt
	Broker Config Output set	module	central-module-master-output	private_key	/etc/centreon-broker/server.key
	Broker Config Output set	module	central-module-master-output	public_cert	/etc/centreon-broker/server.crt
	Broker Config Output set	module	central-module-master-output	ca_certificate	/etc/centreon-broker/client.crt
	Broker Config Log	central	tls	debug
	Broker Config Log	module	tls	debug
	Broker Config Log	central	bbdo	info
	Broker Config Log	module	bbdo	info
	Broker Config Input set	central	central-broker-master-input	tls	yes
	Broker Config Output set	module	central-module-master-output	tls	yes
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected
	Stop Broker
	Stop Engine
	${content1}=	Create List	we have extensions 'TLS' and peer has 'TLS'	using certificates as credentials
	${content2}=	Create List	we have extensions 'TLS' and peer has 'TLS'	using certificates as credentials
	${content1}=	Combine Lists	${content1}	${LIST_HANDSHAKE}
	${content2}=	Combine Lists	${content2}	${LIST_HANDSHAKE}
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${result}=	Find In Log	${log}	${start}	${content1}
	Should Be True	${result}
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-module-master.log
	${result}=	Find In Log	${log}	${start}	${content2}
	Should Be True	${result}

BECT3: Broker/Engine communication with anonymous TLS and ca certificate
	[Tags]	Broker	Engine	TLS	tcp
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module

	${hostname}=	Get Hostname
	Create Certificate	${hostname}	/etc/centreon-broker/server.crt
	Create Certificate	${hostname}	/etc/centreon-broker/client.crt

	Broker Config Input set	central	central-broker-master-input	ca_certificate	/etc/centreon-broker/server.crt
	Broker Config Output set	module	central-module-master-output	ca_certificate	/etc/centreon-broker/client.crt
	Broker Config Log	central	tls	debug
	Broker Config Log	module	tls	debug
	Broker Config Log	central	bbdo	info
	Broker Config Log	module	bbdo	info
	Broker Config Input set	central	central-broker-master-input	tls	yes
	Broker Config Output set	module	central-module-master-output	tls	yes
        # We get the current date just before starting broker
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected
	Stop Broker
	Stop Engine
	${content1}=	Create List	we have extensions 'TLS' and peer has 'TLS'	using anonymous server credentials
	${content2}=	Create List	we have extensions 'TLS' and peer has 'TLS'	using anonymous client credentials
	${content1}=	Combine Lists	${content1}	${LIST_HANDSHAKE}
	${content2}=	Combine Lists	${content2}	${LIST_HANDSHAKE}
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${result}=	Find In Log	${log}	${start}	${content1}
	Should Be True	${result}
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-module-master.log
	${result}=	Find In Log	${log}	${start}	${content2}
	Should Be True	${result}

BECT4: Broker/Engine communication with TLS between central and poller with key/cert and hostname forced
	[Tags]	Broker	Engine	TLS	tcp
	Config Engine	${1}
	Config Broker	rrd
	Config Broker	central
	Config Broker	module

	Set Local Variable	${hostname}	centreon
	Create Key And Certificate	${hostname}	/etc/centreon-broker/server.key	/etc/centreon-broker/server.crt
	Create Key And Certificate	${hostname}	/etc/centreon-broker/client.key	/etc/centreon-broker/client.crt

	Broker Config Input set	central	central-broker-master-input	ca_certificate	/etc/centreon-broker/server.crt
	Broker Config Output set	module	central-module-master-output	ca_certificate	/etc/centreon-broker/client.crt
	Broker Config Log	central	tls	debug
	Broker Config Log	module	tls	debug
	Broker Config Log	central	bbdo	info
	Broker Config Log	module	bbdo	info
	Broker Config Input set	central	central-broker-master-input	tls	yes
	Broker Config Input set	central	central-broker-master-input	private_key	/etc/centreon-broker/client.key
	Broker Config Input set	central	central-broker-master-input	public_cert	/etc/centreon-broker/client.crt
	Broker Config Input set	central	central-broker-master-input	ca_certificate	/etc/centreon-broker/server.crt
	Broker Config Input set	central	central-broker-master-input	tls_hostname	centreon
	Broker Config Output set	module	central-module-master-output	tls	yes
	Broker Config Output set	module	central-module-master-output	private_key	/etc/centreon-broker/server.key
	Broker Config Output set	module	central-module-master-output	public_cert	/etc/centreon-broker/server.crt
	Broker Config Output set	module	central-module-master-output	ca_certificate	/etc/centreon-broker/client.crt
        # We get the current date just before starting broker
	${start}=	Get Current Date
	Start Broker
	Start Engine
	${result}=	Check Connections
	Should Be True	${result}	msg=Engine and Broker not connected
	Stop Broker
	Stop Engine
	${content1}=	Create List	we have extensions 'TLS' and peer has 'TLS'	using certificates as credentials
	${content2}=	Create List	we have extensions 'TLS' and peer has 'TLS'	using certificates as credentials
	${content1}=	Combine Lists	${content1}	${LIST_HANDSHAKE}
	${content2}=	Combine Lists	${content2}	${LIST_HANDSHAKE}
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-broker-master.log
	${result}=	Find In Log	${log}	${start}	${content1}
	Should Be True	${result}
	${log}=	Catenate	SEPARATOR=	${BROKER_LOG}	/central-module-master.log
	${result}=	Find In Log	${log}	${start}	${content2}
	Should Be True	${result}

*** Keywords ***

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

*** Variables ***
&{ext}	yes=TLS	no=	auto=TLS
@{choices}	yes	no	auto
@{LIST_HANDSHAKE}	performing handshake	successful handshake
