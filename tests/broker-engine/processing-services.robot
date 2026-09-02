*** Settings ***
Documentation       Centreon Broker and Engine Creation of services. The goal is to compare the database content and the broker cache.

Resource    ../resources/import.resource

Suite Setup    Ctn Clean Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup    Ctn Stop Processes
Test Teardown    Ctn Stop Engine Broker And Save Logs


*** Test Cases ***
BEPS1
    [Documentation]
    ...    Given a central broker, a rrd broker and an engine configured with 20 hosts and 20 services
    ...    When engine is started and ready
    ...    And engine is reloaded iteratively with 20 to 50 hosts and 20 to 50 services per host by steps of 5
    ...    Then after each reload, the database services and resources tables are consistent
    ...    And after each reload, the (host_id, service_id) pairs in the database match the pairs in the cache
    [Tags]    broker    engine    services
    Ctn Config Engine    ${1}    ${20}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    1
    Ctn Broker Config Log    central    sql    debug
    Ctn Clear Retention
    Ctn Clear Broker Cache
    Ctn Start Broker
    ${start}    Ctn Get Round Current Date
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}

    FOR    ${i}    IN RANGE    7
        ${nb_hosts}    Evaluate    ${20} + 5 * ${i}
        ${nb_services}    Evaluate    ${20} + 5 * ${i}
        ${nb_total}    Evaluate    ${nb_hosts} * ${nb_services}
        Log To Console    Engine works with ${nb_hosts} hosts and ${nb_services} services per host (${nb_total} total)
        ${start}    Ctn Get Round Current Date
        Ctn Config Engine    ${1}    ${nb_hosts}    ${nb_services}
        Ctn Reload Engine
        Ctn Wait For Engine To Be Ready    ${start}

        TRY
            Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
            Check Query Result    SELECT COUNT(*) FROM services WHERE enabled = 1    ==    ${nb_total}    retry_timeout=30s    retry_pause=1s
            Check Query Result    SELECT COUNT(*) FROM resources WHERE enabled = 1 AND parent_id != 0    ==    ${nb_total}    retry_timeout=30s    retry_pause=1s
            ${svc_ids1}    Query    SELECT host_id, service_id FROM services WHERE enabled = 1 ORDER BY host_id, service_id
            ${svc_ids2}    Query    SELECT parent_id, id FROM resources WHERE parent_id != 0 AND enabled = 1 ORDER BY parent_id, id
            ${svc_ids_cache}    Ctn Get Service Ids    ${51001}    expected_count=${nb_total}

            # We check that the (host_id, service_id) pairs in svc_ids1, svc_ids2 and svc_ids_cache are the same.
            ${pairs1_flat}    Evaluate    sorted([(row[0], row[1]) for row in $svc_ids1])
            ${pairs2_flat}    Evaluate    sorted([(row[0], row[1]) for row in $svc_ids2])
            ${pairs_cache_sorted}    Evaluate    sorted($svc_ids_cache)
            Lists Should Be Equal    ${pairs1_flat}    ${pairs2_flat}
            Lists Should Be Equal    ${pairs1_flat}    ${pairs_cache_sorted}
        FINALLY
            Disconnect From Database
        END
    END

BEPS2
    [Documentation]
    ...    Given a central broker, a rrd broker and 1 engine instance configured in centralized mode with 20 hosts and 20 services
    ...    When broker and engine are started
    ...    And broker notifies engine iteratively with new configurations from 20 to 50 then back to 20 hosts and services per host by steps of 5
    ...    Then after each notification, the database services and resources tables are consistent
    ...    And after each notification, the (host_id, service_id) pairs in the database match the pairs in the broker cache
    [Tags]    broker    engine    services
    Ctn Config Centralized Engine    ${1}    ${20}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    sql    trace
    Ctn Clear Retention
    Ctn Clear Prot Files
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    ${steps}    Evaluate    list(range(7)) + list(range(5, -1, -1))
    FOR    ${i}    IN    @{steps}
        ${nb_hosts}    Evaluate    ${20} + 5 * ${i}
        ${nb_services}    Evaluate    ${20} + 5 * ${i}
        ${nb_total}    Evaluate    ${nb_hosts} * ${nb_services}
        Log To Console    One instance of Engine working with ${nb_hosts} hosts and ${nb_services} services per host (${nb_total} total)
        Ctn Update Engine Config    ${1}    ${nb_hosts}    ${nb_services}

        TRY
            Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
            Check Query Result    SELECT COUNT(*) FROM services WHERE enabled = 1    ==    ${nb_total}    retry_timeout=30s    retry_pause=1s
            Check Query Result    SELECT COUNT(*) FROM resources WHERE enabled = 1 AND parent_id != 0    ==    ${nb_total}    retry_timeout=30s    retry_pause=1s
            ${svc_ids1}    Query    SELECT host_id, service_id FROM services WHERE enabled = 1 ORDER BY host_id, service_id
            ${svc_ids2}    Query    SELECT parent_id, id FROM resources WHERE parent_id != 0 AND enabled = 1 ORDER BY parent_id, id
            ${svc_ids_cache}    Ctn Get Service Ids    ${51001}    expected_count=${nb_total}

            # We check that the (host_id, service_id) pairs in svc_ids1, svc_ids2 and svc_ids_cache are the same.
            ${pairs1_flat}    Evaluate    sorted([(row[0], row[1]) for row in $svc_ids1])
            ${pairs2_flat}    Evaluate    sorted([(row[0], row[1]) for row in $svc_ids2])
            ${pairs_cache_sorted}    Evaluate    sorted($svc_ids_cache)
            Lists Should Be Equal    ${pairs1_flat}    ${pairs2_flat}
            Lists Should Be Equal    ${pairs1_flat}    ${pairs_cache_sorted}

            # We check that the description of each service is consistent between the database and the cache.
            ${desc_db}    Query    SELECT host_id, service_id, description FROM services WHERE enabled = 1
            ${desc_db_dict}    Evaluate    {(row[0], row[1]): row[2] for row in $desc_db}
            ${desc_cache}    Ctn Get Service Descriptions    ${51001}
            Dictionaries Should Be Equal    ${desc_db_dict}    ${desc_cache}
        FINALLY
            Disconnect From Database
        END
    END

BEPS3
    [Documentation]
    ...    Given a central broker, a rrd broker and 5 engine instances configured in centralized mode with 20 hosts and 20 services
    ...    When broker and engines are started
    ...    And broker notifies engines iteratively with new configurations from 20 to 40 then back to 20 hosts and services per host by steps of 5
    ...    Then after each notification, the database services and resources tables are consistent
    ...    And after each notification, the (host_id, service_id) pairs in the database match the pairs in the broker cache
    ...    And after each notification, the poller id for each host owning a service is consistent between the database and the broker cache
    [Tags]    broker    engine    services
    Ctn Config Centralized Engine    ${5}    ${20}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${5}
    Ctn Config BBDO3    ${5}
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    core    debug
    Ctn Clear Retention
    Ctn Clear Prot Files
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    ${steps}    Evaluate    list(range(5)) + list(range(3, -1, -1))
    FOR    ${i}    IN    @{steps}
        ${nb_hosts}    Evaluate    ${20} + 5 * ${i}
        ${nb_services}    Evaluate    ${20} + 5 * ${i}
        ${nb_total}    Evaluate    ${nb_hosts} * ${nb_services}
        Log To Console    Five instances of Engine working with ${nb_hosts} hosts and ${nb_services} services per host (${nb_total} total)
        Ctn Update Engine Config    ${5}    ${nb_hosts}    ${nb_services}

        TRY
            Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
            Check Query Result    SELECT COUNT(*) FROM services WHERE enabled = 1    ==    ${nb_total}    retry_timeout=60s    retry_pause=1s
            Check Query Result    SELECT COUNT(*) FROM resources WHERE enabled = 1 AND parent_id != 0    ==    ${nb_total}    retry_timeout=60s    retry_pause=1s
            ${svc_ids1}    Query    SELECT host_id, service_id FROM services WHERE enabled = 1 ORDER BY host_id, service_id
            ${svc_ids2}    Query    SELECT parent_id, id FROM resources WHERE parent_id != 0 AND enabled = 1 ORDER BY parent_id, id
            ${svc_ids_cache}    Ctn Get Service Ids    ${51001}    expected_count=${nb_total}

            # We check that the (host_id, service_id) pairs in svc_ids1, svc_ids2 and svc_ids_cache are the same.
            ${pairs1_flat}    Evaluate    sorted([(row[0], row[1]) for row in $svc_ids1])
            ${pairs2_flat}    Evaluate    sorted([(row[0], row[1]) for row in $svc_ids2])
            ${pairs_cache_sorted}    Evaluate    sorted($svc_ids_cache)
            Lists Should Be Equal    ${pairs1_flat}    ${pairs2_flat}
            Lists Should Be Equal    ${pairs1_flat}    ${pairs_cache_sorted}

            # Let's check the poller ID is consistent between the database and the cache for each host owning services.
            ${host_pollers_db}    Query    SELECT host_id, instance_id FROM hosts WHERE enabled = 1 ORDER BY host_id
            ${host_pollers_db_dict}    Evaluate    {row[0]: row[1] for row in $host_pollers_db}
            ${unique_host_ids}    Evaluate    sorted(set(pair[0] for pair in $svc_ids_cache))
            FOR    ${host_id}    IN    @{unique_host_ids}
                ${poller_id}    Ctn Get Host Poller Id    ${51001}    ${host_id}
                Should Be Equal    ${poller_id}    ${host_pollers_db_dict}[${host_id}]
            END

            # We check that the description of each service is consistent between the database and the cache.
            ${desc_db}    Query    SELECT host_id, service_id, description FROM services WHERE enabled = 1
            ${desc_db_dict}    Evaluate    {(row[0], row[1]): row[2] for row in $desc_db}
            ${desc_cache}    Ctn Get Service Descriptions    ${51001}
            Dictionaries Should Be Equal    ${desc_db_dict}    ${desc_cache}
        FINALLY
            Disconnect From Database
        END
    END

BEPS3R
    [Documentation]
    ...    Given a central broker, a rrd broker and 5 engine instances configured in centralized mode with 20 hosts and 20 services
    ...    When broker and engines are started
    ...    And broker notifies engines iteratively with new configurations from 20 to 40 then back to 20 hosts and services per host by steps of 5
    ...    And engine instances are restarted between each notification
    ...    Then after each notification, the database services and resources tables are consistent
    ...    And after each notification, the (host_id, service_id) pairs in the database match the pairs in the broker cache
    ...    And after each notification, the poller id for each host owning a service is consistent between the database and the broker cache
    [Tags]    broker    engine    services
    Ctn Config Centralized Engine    ${5}    ${20}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${5}
    Ctn Config BBDO3    ${5}
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    config    trace
    Ctn Clear Retention
    Ctn Clear Prot Files
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    ${steps}    Evaluate    list(range(5)) + list(range(3, -1, -1))
    FOR    ${i}    IN    @{steps}
        ${nb_hosts}    Evaluate    ${20} + 5 * ${i}
        ${nb_services}    Evaluate    ${20} + 5 * ${i}
        ${nb_total}    Evaluate    ${nb_hosts} * ${nb_services}
        Log To Console    Five instances of Engine working with ${nb_hosts} hosts and ${nb_services} services per host (${nb_total} total)
        Ctn Update Engine Config    ${5}    ${nb_hosts}    ${nb_services}

        TRY
            Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
            Check Query Result    SELECT COUNT(*) FROM services WHERE enabled = 1    ==    ${nb_total}    retry_timeout=60s    retry_pause=1s
            Check Query Result    SELECT COUNT(*) FROM resources WHERE enabled = 1 AND parent_id != 0    ==    ${nb_total}    retry_timeout=60s    retry_pause=1s
            ${svc_ids1}    Query    SELECT host_id, service_id FROM services WHERE enabled = 1 ORDER BY host_id, service_id
            ${svc_ids2}    Query    SELECT parent_id, id FROM resources WHERE parent_id != 0 AND enabled = 1 ORDER BY parent_id, id
            ${svc_ids_cache}    Ctn Get Service Ids    ${51001}    expected_count=${nb_total}

            # We check that the (host_id, service_id) pairs in svc_ids1, svc_ids2 and svc_ids_cache are the same.
            ${pairs1_flat}    Evaluate    sorted([(row[0], row[1]) for row in $svc_ids1])
            ${pairs2_flat}    Evaluate    sorted([(row[0], row[1]) for row in $svc_ids2])
            ${pairs_cache_sorted}    Evaluate    sorted($svc_ids_cache)
            Lists Should Be Equal    ${pairs1_flat}    ${pairs2_flat}

            ${lines}    Evaluate    "\\n".join(str(row) for row in $pairs1_flat)
            Log    pairs1_flat:${lines}    level=WARN
            ${lines}    Evaluate    "\\n".join(str(row) for row in $pairs_cache_sorted)
            Log    pairs_cache_sorted:${lines}    level=WARN

            Lists Should Be Equal    ${pairs1_flat}    ${pairs_cache_sorted}

            # Let's check the poller ID is consistent between the database and the cache for each host owning services.
            ${host_pollers_db}    Query    SELECT host_id, instance_id FROM hosts WHERE enabled = 1 ORDER BY host_id
            ${host_pollers_db_dict}    Evaluate    {row[0]: row[1] for row in $host_pollers_db}
            ${unique_host_ids}    Evaluate    sorted(set(pair[0] for pair in $svc_ids_cache))
            FOR    ${host_id}    IN    @{unique_host_ids}
                ${poller_id}    Ctn Get Host Poller Id    ${51001}    ${host_id}
                Should Be Equal    ${poller_id}    ${host_pollers_db_dict}[${host_id}]
            END

            # We check that the description of each service is consistent between the database and the cache.
            ${desc_db}    Query    SELECT host_id, service_id, description FROM services WHERE enabled = 1
            ${desc_db_dict}    Evaluate    {(row[0], row[1]): row[2] for row in $desc_db}
            ${desc_cache}    Ctn Get Service Descriptions    ${51001}
            Dictionaries Should Be Equal    ${desc_db_dict}    ${desc_cache}
        FINALLY
            Disconnect From Database
        END

        # Here, we restart the engine instances
        Ctn Stop Engine
        Ctn Start Engine    newGeneration=True
    END

BEPS4
    [Documentation]
    ...    Given a central broker, a rrd broker and 1 engine instance configured in centralized mode with 50 hosts and 20 services
    ...    When broker and engine are started and the initial configuration is processed
    ...    And broker is stopped and its prot files are deleted to simulate a lost configuration
    ...    And broker is restarted
    ...    Then broker detects that the engine configuration is unknown and sends a DiffState with the unknown flag set
    ...    And engine sends back its current configuration to broker
    ...    And broker recovers the configuration by creating a new prot file
    ...    And the database services and resources tables remain consistent
    ...    And the (host_id, service_id) pairs in the database match the pairs in the broker cache
    [Tags]    broker    engine    services
    Ctn Config Centralized Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    sql    trace
    Ctn Clear Retention
    Ctn Clear Prot Files
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    # Wait for the initial configuration to be processed by broker
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Check Query Result    SELECT COUNT(*) FROM services WHERE enabled = 1    ==    ${1000}    retry_timeout=60s    retry_pause=1s

    # Stop broker and delete its prot files to simulate a lost configuration.
    # Engine's state.prot is preserved so engine can send its configuration back.
    Ctn Kindly Stop Broker
    Ctn Clear Prot Files    broker_only=True

    # Restart broker and check that it recovers the configuration from engine
    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True

    ${content}    Create List    Sending unknown diff state to peer
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Broker should send a DiffState with unknown flag to engine

    ${content}    Create List    Created prot file
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Broker should create a prot file from the configuration received from engine

    # Verify DB consistency and cache after recovery
    Check Query Result    SELECT COUNT(*) FROM services WHERE enabled = 1    ==    ${1000}    retry_timeout=30s    retry_pause=1s
    Check Query Result    SELECT COUNT(*) FROM resources WHERE enabled = 1 AND parent_id != 0    ==    ${1000}    retry_timeout=30s    retry_pause=1s
    ${svc_ids1}    Query    SELECT host_id, service_id FROM services WHERE enabled = 1 ORDER BY host_id, service_id
    ${svc_ids2}    Query    SELECT parent_id, id FROM resources WHERE parent_id != 0 AND enabled = 1 ORDER BY parent_id, id
    ${svc_ids_cache}    Ctn Get Service Ids    ${51001}    expected_count=${1000}

    # We check that the (host_id, service_id) pairs in svc_ids1, svc_ids2 and svc_ids_cache are the same.
    ${pairs1_flat}    Evaluate    sorted([(row[0], row[1]) for row in $svc_ids1])
    ${pairs2_flat}    Evaluate    sorted([(row[0], row[1]) for row in $svc_ids2])
    ${pairs_cache_sorted}    Evaluate    sorted($svc_ids_cache)
    Lists Should Be Equal    ${pairs1_flat}    ${pairs2_flat}
    Lists Should Be Equal    ${pairs1_flat}    ${pairs_cache_sorted}

    [Teardown]    Run Keywords    Ctn Dump Services If Failed    AND    Ctn Stop Engine Broker And Save Logs    AND  Disconnect From Database  

*** Keywords ***

Ctn Dump Services If Failed
    Run Keyword If Test Failed    Ctn Dump Services

Ctn Dump Services
    ${dump}    Query    SELECT * FROM services WHERE enabled = 1
    ${lines}    Evaluate    "\\n".join(str(row) for row in $dump)
    Log    Query result dump:${lines}    level=WARN    html=True

