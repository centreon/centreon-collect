*** Settings ***
Documentation       Centreon Broker and Engine Creation of hosts. The goal is to compare the database content and the broker cache.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
BEPH1
    [Documentation]
    ...    Given a central broker, a rrd broker and an engine configured with 50 hosts and 20 services
    ...    When engine is started and ready
    ...    And engine is reloaded iteratively with 50 to 95 hosts by steps of 5
    ...    Then after each reload, the database hosts and resources tables are consistent
    ...    And after each reload, the host ids in the database match the host ids in the cache
    [Tags]    broker    engine    hosts
    Ctn Config Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    Ctn Start Broker
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}

    FOR    ${i}    IN RANGE    10
        ${nb_hosts}    Evaluate    ${50} + 5 * ${i}
        Log To Console    Engine works with ${nb_hosts} hosts
        ${start}    Ctn Get Round Current Date
        Ctn Config Engine    ${1}    ${nb_hosts}          ${20}
        Ctn Reload Engine
        Ctn Wait For Engine To Be Ready    ${start}

        TRY
            Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
            Check Query Result    SELECT COUNT(*) FROM hosts WHERE enabled = 1    ==    ${nb_hosts}    retry_timeout=30s    retry_pause=1s
            Check Query Result    SELECT COUNT(*) FROM resources WHERE enabled = 1 AND parent_id = 0   ==    ${nb_hosts}    retry_timeout=30s    retry_pause=1s
            ${host_ids1}   Query    SELECT host_id FROM hosts WHERE enabled = 1 ORDER BY host_id
            ${host_ids2}   Query    SELECT id FROM resources WHERE parent_id=0 AND enabled = 1 ORDER BY id
            ${host_ids_cache}    Ctn Get Host Ids    ${51001}

            # We check that the contents of host_ids1, host_ids2 and host_ids_cache are the same.
            ${ids1_flat}    Evaluate    [row[0] for row in $host_ids1]
            ${ids2_flat}    Evaluate    [row[0] for row in $host_ids2]
            Sort List    ${ids1_flat}
            Sort List    ${ids2_flat}
            Sort List    ${host_ids_cache}
            Lists Should Be Equal    ${ids1_flat}    ${ids2_flat}
            Lists Should Be Equal    ${ids1_flat}    ${host_ids_cache}
        FINALLY
            Disconnect From Database
        END
    END

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEPH2
    [Documentation]
    ...    Given a central broker, a rrd broker and 1 engine instance configured in centralized mode with 50 hosts and 20 services
    ...    When broker and engine are started
    ...    And broker notifies engine iteratively with new configurations from 50 to 95 hosts by steps of 5
    ...    Then after each notification, the database hosts and resources tables are consistent
    ...    And after each notification, the host ids in the database match the host ids in the broker cache
    [Tags]    broker    engine    hosts
    Ctn Config Centralized Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    sql    trace
    Ctn Clear Retention
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    FOR    ${i}    IN RANGE    10
        ${nb_hosts}    Evaluate    ${50} + 5 * ${i}
        Log To Console    One instance of Engine working with ${nb_hosts} hosts
        Ctn Update Engine Config    ${1}    ${nb_hosts}    ${20}

        TRY
            Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
            Check Query Result    SELECT COUNT(*) FROM hosts WHERE enabled = 1    ==    ${nb_hosts}    retry_timeout=30s    retry_pause=1s
            Check Query Result    SELECT COUNT(*) FROM resources WHERE enabled = 1 AND parent_id = 0    ==    ${nb_hosts}    retry_timeout=30s    retry_pause=1s
            ${host_ids1}   Query    SELECT host_id FROM hosts WHERE enabled = 1 ORDER BY host_id
            ${host_ids2}   Query    SELECT id FROM resources WHERE parent_id=0 AND enabled = 1 ORDER BY id
            ${host_ids_cache}    Ctn Get Host Ids    ${51001}

            # We check that the contents of host_ids1, host_ids2 and host_ids_cache are the same.
            ${ids1_flat}    Evaluate    [row[0] for row in $host_ids1]
            ${ids2_flat}    Evaluate    [row[0] for row in $host_ids2]
            Sort List    ${ids1_flat}
            Sort List    ${ids2_flat}
            Sort List    ${host_ids_cache}
            Lists Should Be Equal    ${ids1_flat}    ${ids2_flat}
            Lists Should Be Equal    ${ids1_flat}    ${host_ids_cache}
        FINALLY
            Disconnect From Database
        END
    END

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEPH3
    [Documentation]
    ...    Given a central broker, a rrd broker and 5 engine instances configured in centralized mode with 50 hosts and 20 services
    ...    When broker and engines are started
    ...    And broker notifies engines iteratively with new configurations from 50 to 70 total hosts by steps of 5
    ...    Then after each notification, the database hosts and resources tables are consistent
    ...    And after each notification, the host ids in the database match the host ids in the broker cache
    ...    And after each notification, the poller id and name for each host are consistent between the database and the broker cache
    [Tags]    broker    engine    hosts
    Ctn Config Centralized Engine    ${5}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${5}
    Ctn Config BBDO3    ${5}
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Clear Retention
    Ctn Clear Prot Files
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    FOR    ${i}    IN RANGE    5
        ${nb_hosts}    Evaluate    ${50} + 5 * ${i}
        Log To Console    Five instances of Engine working with ${nb_hosts} hosts
        Ctn Update Engine Config    ${5}    ${nb_hosts}    ${20}

        TRY
            Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
            Check Query Result    SELECT COUNT(*) FROM hosts WHERE enabled = 1    ==    ${nb_hosts}    retry_timeout=30s    retry_pause=1s
            Check Query Result    SELECT COUNT(*) FROM resources WHERE enabled = 1 AND parent_id = 0    ==    ${nb_hosts}    retry_timeout=30s    retry_pause=1s
            ${host_ids1}   Query    SELECT host_id FROM hosts WHERE enabled = 1 ORDER BY host_id
            ${host_ids2}   Query    SELECT id FROM resources WHERE parent_id=0 AND enabled = 1 ORDER BY id
            ${host_ids_cache}    Ctn Get Host Ids    ${51001}

            # We check that the contents of host_ids1, host_ids2 and host_ids_cache are the same.
            ${ids1_flat}    Evaluate    [row[0] for row in $host_ids1]
            ${ids2_flat}    Evaluate    [row[0] for row in $host_ids2]
            Sort List    ${ids1_flat}
            Sort List    ${ids2_flat}
            Sort List    ${host_ids_cache}
            Lists Should Be Equal    ${ids1_flat}    ${ids2_flat}
            Lists Should Be Equal    ${ids1_flat}    ${host_ids_cache}

            # Let's check the poller ID is consistent between the database and the cache for each host.
            ${host_pollers_db}   Query    SELECT host_id, instance_id FROM hosts WHERE enabled = 1 ORDER BY host_id
            ${host_pollers_db_dict}   Evaluate    {row[0]: row[1] for row in $host_pollers_db}
            ${host_names_db}   Query    SELECT host_id, name FROM hosts WHERE enabled = 1 ORDER BY host_id
            ${host_names_db_dict}   Evaluate    {row[0]: row[1] for row in $host_names_db}
            FOR    ${host_id}    IN    @{host_ids_cache}
                ${poller_id}    Ctn Get Host Poller Id    ${51001}    ${host_id}
                Should Be Equal    ${poller_id}    ${host_pollers_db_dict}[${host_id}]
                ${host_name}    Broker.Ctn Get Host Name    ${51001}    ${host_id}
                Should Be Equal    ${host_name}    ${host_names_db_dict}[${host_id}]
            END

        FINALLY
            Disconnect From Database
        END
    END

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEPH3R
    [Documentation]
    ...    Given a central broker, a rrd broker and 5 engine instances configured in centralized mode with 50 hosts and 20 services
    ...    When broker and engines are started
    ...    And broker notifies engines iteratively with new configurations from 50 to 70 total hosts by steps of 5
    ...    And engine instances are restarted between each notification
    ...    Then after each notification, the database hosts and resources tables are consistent
    ...    And after each notification, the host ids in the database match the host ids in the broker cache
    ...    And after each notification, the poller id and name for each host are consistent between the database and the broker cache
    [Tags]    broker    engine    hosts
    Ctn Config Centralized Engine    ${5}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${5}
    Ctn Config BBDO3    ${5}
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Clear Retention
    Ctn Clear Prot Files
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    FOR    ${i}    IN RANGE    5
        ${nb_hosts}    Evaluate    ${50} + 5 * ${i}
        Log To Console    Five instances of Engine working with ${nb_hosts} hosts
        Ctn Update Engine Config    ${5}    ${nb_hosts}    ${20}

        TRY
            Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
            Check Query Result    SELECT COUNT(*) FROM hosts WHERE enabled = 1    ==    ${nb_hosts}    retry_timeout=30s    retry_pause=1s
            Check Query Result    SELECT COUNT(*) FROM resources WHERE enabled = 1 AND parent_id = 0    ==    ${nb_hosts}    retry_timeout=30s    retry_pause=1s
            ${host_ids1}   Query    SELECT host_id FROM hosts WHERE enabled = 1 ORDER BY host_id
            ${host_ids2}   Query    SELECT id FROM resources WHERE parent_id=0 AND enabled = 1 ORDER BY id
            ${host_ids_cache}    Ctn Get Host Ids    ${51001}

            # We check that the contents of host_ids1, host_ids2 and host_ids_cache are the same.
            ${ids1_flat}    Evaluate    [row[0] for row in $host_ids1]
            ${ids2_flat}    Evaluate    [row[0] for row in $host_ids2]
            Sort List    ${ids1_flat}
            Sort List    ${ids2_flat}
            Sort List    ${host_ids_cache}
            Lists Should Be Equal    ${ids1_flat}    ${ids2_flat}
            Lists Should Be Equal    ${ids1_flat}    ${host_ids_cache}

            # Let's check the poller ID is consistent between the database and the cache for each host.
            ${host_pollers_db}   Query    SELECT host_id, instance_id FROM hosts WHERE enabled = 1 ORDER BY host_id
            ${host_pollers_db_dict}   Evaluate    {row[0]: row[1] for row in $host_pollers_db}
            ${host_names_db}   Query    SELECT host_id, name FROM hosts WHERE enabled = 1 ORDER BY host_id
            ${host_names_db_dict}   Evaluate    {row[0]: row[1] for row in $host_names_db}
            FOR    ${host_id}    IN    @{host_ids_cache}
                ${poller_id}    Ctn Get Host Poller Id    ${51001}    ${host_id}
                Should Be Equal    ${poller_id}    ${host_pollers_db_dict}[${host_id}]
                ${host_name}    Broker.Ctn Get Host Name    ${51001}    ${host_id}
                Should Be Equal    ${host_name}    ${host_names_db_dict}[${host_id}]
            END

        FINALLY
            Disconnect From Database
        END

        # Here, we restart the engine instances
        Ctn Stop Engine
        Ctn Start Engine    newGeneration=True
    END

    Ctn Stop Engine
    Ctn Kindly Stop Broker
