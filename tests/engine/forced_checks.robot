*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes

Documentation	Centreon Engine forced checks tests
Library	DateTime
Library	Process
Library	OperatingSystem
Library	../resources/Broker.py
Library	../resources/Engine.py

*** Test Cases ***
EFHC1
	[Documentation]	Engine is configured with hosts and we force checks on one 5 times
	[Tags]	Engine	external_cmd
	Config Engine	${1}
        Config Broker	module	${1}
	Engine Config Set Value	${0}	log_legacy_enabled	${0}
	Engine Config Set Value	${0}	log_v2_enabled	${1}
        Clear Retention
	${start}=	Get Current Date
        Start Engine
        FOR	${i}	IN RANGE	${10}
         Schedule Forced HOST CHECK	host_1	/var/lib/centreon-engine/config0/rw/centengine.cmd
         Sleep	5s
        END
        ${content}=	Create List	EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;	HOST ALERT: host_1;DOWN;SOFT;1;	EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;	HOST ALERT: host_1;DOWN;SOFT;2;	EXTERNAL COMMAND: SCHEDULE_FORCED_HOST_CHECK;host_1;    HOST ALERT: host_1;DOWN;HARD;3;

        ${result}=	Find In Log with Timeout	${logEngine0}	${start}	${content}	60
        Should Be True	${result}	msg=Message about SCHEDULE FORCED CHECK and HOST ALERT should be available in log.
        Stop Engine
