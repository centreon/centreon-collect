*** Settings ***
Documentation       Centreon Broker and Engine Creation of hosts. The goal is to compare the database content and the broker cache.

Resource    ../resources/import.resource

Suite Setup    Ctn Clean Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup    Ctn Clean Before Test
Test Teardown    Ctn Save Logs If Failed


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
        Ctn Config Engine    ${1}    ${nb_hosts}    ${20}
        Ctn Reload Engine
        Ctn Wait For Engine To Be Ready    ${start}

        TRY
            Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
            Check Query Result    SELECT COUNT(*) FROM hosts WHERE enabled = 1    ==    ${nb_hosts}    retry_timeout=30s    retry_pause=1s
            Check Query Result    SELECT COUNT(*) FROM resources WHERE enabled = 1 AND parent_id = 0    ==    ${nb_hosts}    retry_timeout=30s    retry_pause=1s
            ${host_ids1}    Query    SELECT host_id FROM hosts WHERE enabled = 1 ORDER BY host_id
            ${host_ids2}    Query    SELECT id FROM resources WHERE parent_id=0 AND enabled = 1 ORDER BY id
            ${host_ids_cache}    Ctn Get Host Ids    ${51001}    ${nb_hosts}

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
            ${host_ids1}    Query    SELECT host_id FROM hosts WHERE enabled = 1 ORDER BY host_id
            ${host_ids2}    Query    SELECT id FROM resources WHERE parent_id=0 AND enabled = 1 ORDER BY id
            ${host_ids_cache}    Ctn Get Host Ids    ${51001}    ${nb_hosts}

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
    ...    And broker notifies engines iteratively with new configurations from 50 to 70 then back to 50 total hosts by steps of 5
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
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    ${steps}    Evaluate    list(range(5)) + list(range(3, -1, -1))
    FOR    ${i}    IN    @{steps}
        ${nb_hosts}    Evaluate    ${50} + 5 * ${i}
        Log To Console    Five instances of Engine working with ${nb_hosts} hosts
        Ctn Update Engine Config    ${5}    ${nb_hosts}    ${20}

        TRY
            Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
            Check Query Result    SELECT COUNT(*) FROM hosts WHERE enabled = 1    ==    ${nb_hosts}    retry_timeout=30s    retry_pause=1s
            Check Query Result    SELECT COUNT(*) FROM resources WHERE enabled = 1 AND parent_id = 0    ==    ${nb_hosts}    retry_timeout=30s    retry_pause=1s
            ${host_ids1}    Query    SELECT host_id FROM hosts WHERE enabled = 1 ORDER BY host_id
            ${host_ids2}    Query    SELECT id FROM resources WHERE parent_id=0 AND enabled = 1 ORDER BY id
            ${host_ids_cache}    Ctn Get Host Ids    ${51001}    ${nb_hosts}

            # We check that the contents of host_ids1, host_ids2 and host_ids_cache are the same.
            ${ids1_flat}    Evaluate    [row[0] for row in $host_ids1]
            ${ids2_flat}    Evaluate    [row[0] for row in $host_ids2]
            Sort List    ${ids1_flat}
            Sort List    ${ids2_flat}
            Sort List    ${host_ids_cache}
            Lists Should Be Equal    ${ids1_flat}    ${ids2_flat}
            Lists Should Be Equal    ${ids1_flat}    ${host_ids_cache}

            # Let's check the poller ID is consistent between the database and the cache for each host.
            ${host_pollers_db}    Query    SELECT host_id, instance_id FROM hosts WHERE enabled = 1 ORDER BY host_id
            ${host_pollers_db_dict}    Evaluate    {row[0]: row[1] for row in $host_pollers_db}
            ${host_names_db}    Query    SELECT host_id, name FROM hosts WHERE enabled = 1 ORDER BY host_id
            ${host_names_db_dict}    Evaluate    {row[0]: row[1] for row in $host_names_db}
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
            ${host_ids1}    Query    SELECT host_id FROM hosts WHERE enabled = 1 ORDER BY host_id
            ${host_ids2}    Query    SELECT id FROM resources WHERE parent_id=0 AND enabled = 1 ORDER BY id
            ${host_ids_cache}    Ctn Get Host Ids    ${51001}    ${nb_hosts}

            # We check that the contents of host_ids1, host_ids2 and host_ids_cache are the same.
            ${ids1_flat}    Evaluate    [row[0] for row in $host_ids1]
            ${ids2_flat}    Evaluate    [row[0] for row in $host_ids2]
            Sort List    ${ids1_flat}
            Sort List    ${ids2_flat}
            Sort List    ${host_ids_cache}
            Lists Should Be Equal    ${ids1_flat}    ${ids2_flat}
            Lists Should Be Equal    ${ids1_flat}    ${host_ids_cache}

            # Let's check the poller ID is consistent between the database and the cache for each host.
            ${host_pollers_db}    Query    SELECT host_id, instance_id FROM hosts WHERE enabled = 1 ORDER BY host_id
            ${host_pollers_db_dict}    Evaluate    {row[0]: row[1] for row in $host_pollers_db}
            ${host_names_db}    Query    SELECT host_id, name FROM hosts WHERE enabled = 1 ORDER BY host_id
            ${host_names_db_dict}    Evaluate    {row[0]: row[1] for row in $host_names_db}
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

BEPH4
    [Documentation]
    ...    Given a central broker, a rrd broker and 1 engine instance configured in centralized mode with 50 hosts and 20 services
    ...    When broker and engine are started and the initial configuration is processed
    ...    And broker is stopped and left with neither a prot file nor a prepared one
    ...    And broker is restarted
    ...    Then broker, having nothing to send, asks engine for its configuration with the unknown flag set
    ...    And engine sends back its current configuration to broker
    ...    And broker recovers the configuration by creating a new prot file
    ...    And the database hosts and resources tables remain consistent
    ...    And the host ids in the database match the host ids in the broker cache
    [Tags]    broker    engine    hosts
    Ctn Config Centralized Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    sql    trace
    Ctn Clear Retention
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    # Wait for the initial configuration to be processed by broker
    TRY
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        Check Query Result    SELECT COUNT(*) FROM hosts WHERE enabled = 1    ==    ${50}    retry_timeout=60s    retry_pause=1s
    FINALLY
        Disconnect From Database
    END

    # Stop broker and leave it with nothing at all: no <ID>.prot to diff against,
    # and no new-<ID>.prot to hand over either. The .lck files have to go too --
    # one is enough for a cycle to rebuild new-<ID>.prot from the PHP cache, and
    # broker would then have something to send rather than being blind. That is
    # the other case, covered by BEPH4B.
    # Engine's state.prot is preserved so engine can send its configuration back.
    Ctn Kindly Stop Broker
    Ctn Clear Prot Files    broker_only=True
    Ctn Clear Lck Files

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
    TRY
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        Check Query Result    SELECT COUNT(*) FROM hosts WHERE enabled = 1    ==    ${50}    retry_timeout=30s    retry_pause=1s
        Check Query Result    SELECT COUNT(*) FROM resources WHERE enabled = 1 AND parent_id = 0    ==    ${50}    retry_timeout=30s    retry_pause=1s
        ${host_ids1}    Query    SELECT host_id FROM hosts WHERE enabled = 1 ORDER BY host_id
        ${host_ids2}    Query    SELECT id FROM resources WHERE parent_id=0 AND enabled = 1 ORDER BY id
        ${host_ids_cache}    Ctn Get Host Ids    ${51001}    ${50}

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

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEPH4B
    [Documentation]
    ...    Given a central broker and one engine instance configured in centralized mode with 50 hosts
    ...    When broker loses its <ID>.prot but a configuration has been prepared for the poller
    ...    And that prepared configuration differs from the one engine runs
    ...    Then broker hands it over as a whole state, not as a diff
    ...    And it does not ask engine for its configuration, having something to send
    ...    And the hosts remain consistent in database
    ...
    ...    The other half of BEPH4: there, broker is left with nothing and has to ask.
    ...    Here it holds new-<ID>.prot, so it is in the position of a first start --
    ...    no known reference to diff against, but a full configuration to deliver.
    [Tags]    broker    engine    hosts
    Ctn Config Centralized Engine    ${1}    ${50}    ${20}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Config BBDO3    ${1}
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Clear Retention
    # The .lck files stay: Ctn Config Centralized Engine has just posted the
    # announcement of this configuration, and without it broker would start
    # blind -- which is BEPH4, not this test.
    Ctn Clear Prot Files

    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    TRY
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        Check Query Result    SELECT COUNT(*) FROM hosts WHERE enabled = 1    ==    ${50}
        ...    retry_timeout=60s    retry_pause=1s
    FINALLY
        Disconnect From Database
    END

    # Both go down, then broker loses its reference while engine keeps running
    # nothing: what is left is the PHP cache, from which a configuration can be
    # prepared.
    Ctn Stop Engine
    Ctn Kindly Stop Broker
    Ctn Clear Prot Files    broker_only=True

    # The sources change while nobody is there to be served, so what broker
    # prepares really differs from what engine runs -- otherwise it would rightly
    # have nothing to send and the hand-over could not be observed.
    Ctn Engine Config Set Value    ${0}    log_level_checks    trace

    # Broker alone: the push prepares new-1.prot and keeps it, since the poller
    # is not there to be served.
    ${prepared}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Notify Broker Of Engine Config Change    ${0}
    ${content}    Create List    Configuration cycle: 1 ready, 0 sent
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${prepared}    ${content}    60
    Should Be True    ${result}    Broker should have prepared a configuration it could not deliver

    # And the announcement is gone even though nothing was delivered: what is
    # waiting for the poller is new-1.prot, in the directory broker owns. This is
    # what makes the hand-over below work for a batch push too, where PHP writes
    # no <ID>.lck at all.
    Wait Until Removed    ${VarRoot}/lib/centreon/config/1.lck    15s

    # Engine shows up: the prepared configuration is handed over as it stands,
    # without reading the sources again and without asking engine for anything.
    ${connect}    Ctn Get Round Current Date
    Ctn Start Engine    newGeneration=True
    ${content}    Create List    handing it over without reading its sources again
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${connect}    ${content}    60
    Should Be True    ${result}    Broker should hand over the configuration prepared while the poller was away

    ${content}    Create List    sending DiffState to poller 1 (unknown=false)
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${connect}    ${content}    60
    Should Be True    ${result}    Broker should send the whole state, not an unknown request

    # Having something to send, it must not have asked engine for anything.
    ${content}    Create List    Sending unknown diff state to peer
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${connect}    ${content}    10
    Should Not Be True    ${result}    Broker had a configuration ready and should not have asked engine for one

    TRY
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        Check Query Result    SELECT COUNT(*) FROM hosts WHERE enabled = 1    ==    ${50}
        ...    retry_timeout=60s    retry_pause=1s
    FINALLY
        Disconnect From Database
    END

    Ctn Stop Engine
    Ctn Kindly Stop Broker
