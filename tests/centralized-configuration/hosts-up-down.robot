*** Settings ***
Documentation       Number of hosts is increased and then decreased and we check configurations are correct.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
CBEUDHOSTS
    [Documentation]    Given a Centreon platform with 3 pollers configured
    ...    And 50 hosts distributed across pollers (17+17+16)
    ...    And initially 20 services per host
    ...    And BBDO3 protocol with unified SQL output enabled
    ...
    ...    When the number of services per host is progressively increased
    ...    And the configuration is hot-reloaded 3 times (20→24→28 services/host)
    ...
    ...    Then each poller should monitor the correct number of resources
    ...    And poller 1 should monitor exactly (17 hosts × services) + 17 hosts
    ...    And poller 2 should monitor exactly (17 hosts × services) + 17 hosts
    ...    And poller 3 should monitor exactly (16 hosts × services) + 16 hosts
    ...    And the load balancing should remain stable during scaling
    [Tags]    MON-187019
    Ctn Clear Engine Configurations
    Ctn Clear Prot Files
    Ctn Config Centralized Engine    ${2}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${2}
    Ctn Config BBDO3    2
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    config    debug
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    ${content}    Create List    BBDO: all engine peers have acknowledged their configuration
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    The two pollers did not acknowledge their configuration as they should have

    ${content}    Create List
    ...    Merging diff file '/tmp/var/lib/centreon-broker/central-broker-master/pollers-configuration/diff-1.prot' into the global one
    ...    Merging diff file '/tmp/var/lib/centreon-broker/central-broker-master/pollers-configuration/diff-2.prot' into the global one
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Diff files should have been merged into the global diff file

    ${content}    Create List    Publishing global diff state
    ...    processing global diff state event
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    A global diff state event should have been built and processed

    # Check that the 50 hosts are stored in the database
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Log To Console    Check that the 50 hosts are prepared in the database (hosts and resources tables).
    Check Query Result
    ...    SELECT COUNT(*) FROM hosts WHERE enabled=1
    ...    ==
    ...    ${50}
    ...    retry_timeout=30s
    ...    retry_pause=1s
    Check Query Result
    ...    SELECT COUNT(*) FROM resources WHERE enabled=1 AND parent_id=0
    ...    ==
    ...    ${50}
    ...    retry_timeout=30s
    ...    retry_pause=1s

    Log To Console    Check that the 1000 services are prepared in the database (services and resources tables).
    Check Query Result
    ...    SELECT COUNT(*) FROM services WHERE enabled=1
    ...    ==
    ...    ${1000}
    ...    retry_timeout=30s
    ...    retry_pause=1s
    Check Query Result
    ...    SELECT COUNT(*) FROM resources WHERE enabled=1 AND parent_id<>0
    ...    ==
    ...    ${1000}
    ...    retry_timeout=30s
    ...    retry_pause=1s
    Disconnect From Database

#    Log To Console    Services are progressively increased from 20 to 28 per host.
#    FOR    ${i}    IN RANGE    ${1}    ${4}
#        Sleep    10s
#        ${services_by_host}    Evaluate    20 + 4 * $i
#        Log To Console    ${services_by_host} services by host with 50 hosts among 3 pollers.
#        Ctn Update Engine Config    ${3}    ${50}    ${services_by_host}
#
#        ${services_count}    Evaluate    17 * (20 + 4 * $i)
#        ${resources_count}    Evaluate    $services_count + 17
#        ${resources_count_3}    Evaluate    16 * (20 + 4 * $i) + 16
#
#        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
#        Log To Console    Poller 1 should monitor ${resources_count} resources.
#        Check Query Result
#        ...    SELECT COUNT(*) FROM resources WHERE poller_id=1 AND enabled=1
#        ...    ==
#        ...    ${resources_count}
#        ...    retry_timeout=30s
#        ...    retry_pause=1s
#        Log To Console    Poller 2 should monitor ${resources_count} resources.
#        Check Query Result
#        ...    SELECT COUNT(*) FROM resources WHERE poller_id=2 AND enabled=1
#        ...    ==
#        ...    ${resources_count}
#        ...    retry_timeout=30s
#        ...    retry_pause=1s
#        Log To Console    Poller 3 should monitor ${resources_count_3} resources.
#        Check Query Result
#        ...    SELECT COUNT(*) FROM resources WHERE poller_id=3 AND enabled=1
#        ...    ==
#        ...    ${resources_count_3}
#        ...    retry_timeout=30s
#        ...    retry_pause=1s
#        Disconnect From Database
#
#        # Let's compare the database content and the configuration files
#
#        Log To Console    Check that the hosts configuration files are identical to the database for each poller
#        # Table hosts:
#        ${result}    Ctn Hosts Are Identical    1    ${VarRoot}/lib/centreon/config/1/hosts.cfg
#        Should Be True    ${result}    Hosts are not identical between database and configuration file for poller 1
#
#        ${result}    Ctn Hosts Are Identical    2    ${VarRoot}/lib/centreon/config/2/hosts.cfg
#        Should Be True    ${result}    Hosts are not identical between database and configuration file for poller 2
#
#        ${result}    Ctn Hosts Are Identical    3    ${VarRoot}/lib/centreon/config/3/hosts.cfg
#        Should Be True    ${result}    Hosts are not identical between database and configuration file for poller 3
#        # Table resources:
#        ${result}    Ctn Host Resources Are Identical    1    ${VarRoot}/lib/centreon/config/1/hosts.cfg
#        Should Be True    ${result}    Hosts are not identical between database and configuration file for poller 1
#
#        ${result}    Ctn Host Resources Are Identical    2    ${VarRoot}/lib/centreon/config/2/hosts.cfg
#        Should Be True    ${result}    Hosts are not identical between database and configuration file for poller 2
#
#        ${result}    Ctn Host Resources Are Identical    3    ${VarRoot}/lib/centreon/config/3/hosts.cfg
#        Should Be True    ${result}    Hosts are not identical between database and configuration file for poller 3
#
#        Log To Console    Check that the services configuration files are identical to the database for each poller
#        # Table services:
#        ${result}    Ctn Services Are Identical    1    ${VarRoot}/lib/centreon/config/1/services.cfg
#        Should Be True    ${result}    Services are not identical between database and configuration file for poller 1
#
#        ${result}    Ctn Services Are Identical    2    ${VarRoot}/lib/centreon/config/2/services.cfg
#        Should Be True    ${result}    Services are not identical between database and configuration file for poller 2
#
#        ${result}    Ctn Services Are Identical    3    ${VarRoot}/lib/centreon/config/3/services.cfg
#        Should Be True    ${result}    Services are not identical between database and configuration file for poller 3
#        # Table resources:
#        ${result}    Ctn Service Resources Are Identical    1    ${VarRoot}/lib/centreon/config/1/services.cfg
#        Should Be True
#        ...    ${result}
#        ...    Services (in resources table) are not identical between database and configuration file for poller 1
#
#        ${result}    Ctn Service Resources Are Identical    2    ${VarRoot}/lib/centreon/config/2/services.cfg
#        Should Be True
#        ...    ${result}
#        ...    Services (in resources table) are not identical between database and configuration file for poller 2
#
#        ${result}    Ctn Service Resources Are Identical    3    ${VarRoot}/lib/centreon/config/3/services.cfg
#        Should Be True
#        ...    ${result}
#        ...    Services (in resources table) are not identical between database and configuration file for poller 3
#    END
    Ctn Stop Engine
    Ctn Kindly Stop Broker

CBEUD_POLLER_CONF_LOSS
    [Documentation]    Given a Centreon platform with 1 poller in centralized configuration mode
    ...    And BBDO3 protocol with unified SQL output enabled
    ...
    ...    When the poller connects, receives its configuration from Broker and starts sending metrics
    ...    And the poller is stopped
    ...    And it loses its whole local configuration (state.prot and the .cfg files rebuilt from it
    ...    are wiped, simulating a poller reinstalled from scratch or whose local storage was lost)
    ...    And the poller is restarted, while Broker (which was never stopped) still has the poller's
    ...    last known configuration cached in 1.prot
    ...
    ...    Then the poller should recover its host/service configuration from Broker
    ...    And metrics should resume flowing into the database
    [Tags]    MON-205630    centralized-configuration
    Ctn Clear Engine Configurations
    Ctn Clear Prot Files
    Ctn Config Centralized Engine    ${1}    ${1}    ${1}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1
    Ctn Broker Config Source Log    central    1
    Ctn Broker Config Log    central    bbdo    trace
    Ctn Broker Config Log    central    config    trace
    Ctn Broker Config Log    central    cache    trace
    Ctn Broker Config Log    central    sql    debug
    Ctn Broker Config Log    module0    bbdo    trace
    
    Ctn Config Broker Sql Output    central    unified_sql
    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    ${result}    Ctn Check Connections
    Should Be True    ${result}    Connection between Engine and Broker not established

    ${content}    Create List
    ...    Found lock file '/tmp/var/lib/centreon/config/1.lck' for poller id 1
    ...    sending DiffState to poller 1
    ...    BBDO: received diff state ack
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Poller 1 did not receive and acknowledge its initial configuration

    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Check Query Result
    ...    SELECT COUNT(*) FROM instances WHERE running=1
    ...    ==
    ...    ${1}
    ...    retry_timeout=30s
    ...    retry_pause=1s
    Check Query Result
    ...    SELECT COUNT(*) FROM hosts WHERE enabled=1
    ...    ==
    ...    ${1}
    ...    retry_timeout=30s
    ...    retry_pause=1s

    Log To Console    Check that metrics are flowing before breaking anything (control measurement).
    Check Query Result
    ...    SELECT COUNT(*) FROM data_bin db JOIN metrics m ON db.id_metric = m.metric_id JOIN index_data i ON m.index_id = i.id WHERE i.host_id=1 AND i.service_id=1
    ...    >
    ...    ${0}
    ...    retry_timeout=60s
    ...    retry_pause=2s

    Sleep    10

    Ctn Stop Engine
    Remove File    ${VarRoot}/lib/centreon-engine/config0/state.prot

    #wait all instances at 0 in bdd
    Check Query Result
    ...    SELECT COUNT(*) FROM instances WHERE running=0
    ...    ==
    ...    ${1}
    ...    retry_timeout=30s
    ...    retry_pause=1s

    ${restart}    Evaluate    int(time.time())    modules=time
    Ctn Start Engine    newGeneration=True

    ${result}    Ctn Check Connections
    Should Be True    ${result}    Poller did not reconnect after losing its local configuration

    # Broker (never stopped) still has the poller's last known configuration
    # cached in 1.prot: it must push it back to the now-empty poller so
    # host_1/service_1 get monitored again and metrics resume flowing.
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Check Query Result
    ...    SELECT COUNT(*) FROM instances WHERE running=1
    ...    ==
    ...    ${1}
    ...    retry_timeout=30s
    ...    retry_pause=1s
    Check Query Result
    ...    SELECT COUNT(*) FROM hosts WHERE enabled=1
    ...    ==
    ...    ${1}
    ...    retry_timeout=60s
    ...    retry_pause=2s
    Check Query Result
    ...    SELECT COUNT(*) FROM services WHERE enabled=1
    ...    ==
    ...    ${1}
    ...    retry_timeout=60s
    ...    retry_pause=2s
    Log To Console    Waiting for host_1/service_1 to send metrics again after the poller recovered.
    Disconnect From Database

    Ctn Stop Engine
    Ctn Kindly Stop Broker
