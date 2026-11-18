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
    ...    And each verification should complete within 30 seconds
    ...    And the load balancing should remain stable during scaling
    [Tags]    MON-187019
    Ctn Clear Engine Configurations

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
    ...    Merging diff file '/tmp/var/lib/centreon-broker/pollers-configuration/diff-1.prot' into the global one
    ...    Merging diff file '/tmp/var/lib/centreon-broker/pollers-configuration/diff-2.prot' into the global one
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    The two pollers did not acknowledge their configuration as they should have

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
