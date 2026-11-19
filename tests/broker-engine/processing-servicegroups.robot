*** Settings ***
Documentation       Centreon Broker and Engine servicegroup management in centralized mode.
...                 The goal is to verify consistency between the database and the broker
...                 cache (name and members) after adding and removing servicegroups one by
...                 one across multiple engine instances.

Resource    ../resources/import.resource

Suite Setup    Ctn Clean Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup    Ctn Stop Processes
Test Teardown    Ctn Save Logs If Failed


*** Test Cases ***
BEPSG1
    [Documentation]
    ...    Given a central broker, a rrd broker and 5 engine instances in centralized mode
    ...    With 50 hosts each (250 hosts total, numbered 1 to 250) and 20 services per host
    ...    (5000 services total, numbered 1 to 5000)
    ...    When broker and engines are started and the initial configuration is applied
    ...    And servicegroups sg1 to sg5 are added one by one:
    ...      sg1 contains all 5000 services,
    ...      sg2 contains the 2500 services with even IDs,
    ...      sg3 contains the 1666 services with IDs divisible by 3,
    ...      sg4 contains the 1250 services with IDs divisible by 4,
    ...      sg5 contains the 1000 services with IDs divisible by 5
    ...    Then after each addition the database and the broker cache are consistent:
    ...      the services_servicegroups table has the expected number of entries,
    ...      the servicegroup name in the cache matches servicegroup_<id>,
    ...      and the member count in the cache matches the database
    ...    And when servicegroups sg1 to sg5 are removed one by one
    ...    Then after each removal the database shows zero entries for that group
    ...    And the broker cache no longer reports any members for that group
    [Tags]    broker    engine    servicegroups
    Ctn Config Centralized Engine    ${5}    ${250}    ${20}
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

    # Wait for all 5000 services to be registered in the database
    TRY
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        Check Query Result
        ...    SELECT COUNT(*) FROM services WHERE enabled = 1    ==    ${5000}
        ...    retry_timeout=60s    retry_pause=1s
    FINALLY
        Disconnect From Database
    END

    # Add servicegroups sg1..sg5 one by one and verify DB + cache consistency after each
    FOR    ${sg_id}    IN RANGE    1    6
        Log To Console    Adding servicegroup ${sg_id} to all 5 engine instances
        FOR    ${idx}    IN RANGE    5
            ${members}    Ctn Get Filtered Service Names    ${idx}    ${sg_id}
            Ctn Add Service Group    ${idx}    ${sg_id}    ${members}
            Ctn Notify Broker Of Engine Config Change    ${idx}
        END
        ${expected_count}    Evaluate    sum(1 for s in range(1, 5001) if s % ${sg_id} == 0)
        Log To Console    Expecting ${expected_count} services in servicegroup ${sg_id}

        # DB check: services_servicegroups has the expected number of entries
        ${result}    Ctn Check Number Of Relations Between Servicegroup And Services
        ...    ${sg_id}    ${expected_count}    60
        Should Be True    ${result}
        ...    Servicegroup ${sg_id} should have ${expected_count} members in DB within 60 seconds

        # Cache check: servicegroup name and member count match
        ${expected_name}    Set Variable    servicegroup_${sg_id}
        ${cache_sg}    Ctn Get Servicegroup    ${51001}    ${sg_id}
        Should Not Be Equal    ${cache_sg}    ${None}
        ...    Servicegroup ${sg_id} should be present in the broker cache
        Should Be Equal    ${cache_sg.name}    ${expected_name}
        ...    Servicegroup ${sg_id} name in cache should be '${expected_name}'

        ${cache_count}    Get Length    ${cache_sg.member_service_ids}
        Should Be Equal As Integers    ${cache_count}    ${expected_count}
        ...    Servicegroup ${sg_id} should have ${expected_count} members in the broker cache
    END

    # Remove servicegroups sg1..sg5 one by one and verify DB + cache consistency after each
    FOR    ${sg_id}    IN RANGE    1    6
        Log To Console    Removing servicegroup ${sg_id} from all 5 engine instances
        FOR    ${idx}    IN RANGE    5
            Ctn Remove Service Group    ${idx}    ${sg_id}
            Ctn Notify Broker Of Engine Config Change    ${idx}
        END

        # DB check: no remaining entries for this servicegroup
        ${result}    Ctn Check Number Of Relations Between Servicegroup And Services
        ...    ${sg_id}    ${0}    60
        Should Be True    ${result}
        ...    Servicegroup ${sg_id} should have 0 members in DB after removal within 60 seconds

        # Cache check: servicegroup no longer present (or member list is empty)
        ${cache_sg}    Ctn Get Servicegroup    ${51001}    ${sg_id}
        IF    $cache_sg is not None
            ${cache_count}    Get Length    ${cache_sg.member_service_ids}
            Should Be Equal As Integers    ${cache_count}    ${0}
            ...    Servicegroup ${sg_id} should have 0 members in the broker cache after removal
        END
    END

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEPSG2
    [Documentation]
    ...    Given a central broker, a rrd broker and 5 engine instances in centralized mode
    ...    With an initial 50 hosts (distributed across 5 instances) and 20 services per host
    ...    And 5 persistent servicegroups: sg1=all services, sg2=even IDs, sg3=div-by-3,
    ...      sg4=div-by-4, sg5=div-by-5
    ...    When the host count scales up from 50 to 250 in steps of 50
    ...    And then scales back down to 50 in the same steps
    ...    Then after each step the database and the broker cache are consistent for all 5
    ...      servicegroups: correct name, correct member count in DB, correct member count in cache
    [Tags]    broker    engine    servicegroups
    Ctn Config Centralized Engine    ${5}    ${5}    ${4}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${5}
    Ctn Config BBDO3    ${5}
    Ctn Broker Config Log    central    sql    trace
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    core    error
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    module0    core    error
    Ctn Broker Config Log    module1    core    error
    Ctn Broker Config Log    module2    core    error
    Ctn Broker Config Log    module3    core    error
    Ctn Broker Config Log    module4    core    error
    Ctn Broker Config Log    module0    config    debug
    Ctn Broker Config Log    module1    config    debug
    Ctn Broker Config Log    module2    config    debug
    Ctn Broker Config Log    module3    config    debug
    Ctn Broker Config Log    module4    config    debug
    Ctn Clear Retention
    Ctn Clear Prot Files
    Ctn Clear Broker Cache
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    # Wait for the initial 50 hosts to be registered in the database
    TRY
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        Check Query Result    SELECT COUNT(*) FROM hosts WHERE enabled = 1    ==    ${5}    retry_timeout=60s    retry_pause=1s
    FINALLY
        Disconnect From Database
    END

    # Scale up (50→100→150→200→250) then back down (200→150→100→50).
    # At each step: rebuild configs, add all 5 servicegroups, notify broker, then verify.
    FOR    ${nb_hosts}    IN    ${5}    ${6}    ${7}    ${8}    ${9}    ${8}    ${7}    ${6}    ${5}
        Log To Console    ==================================================
        Log To Console    === ${nb_hosts} hosts distributed across 5 instances ===
        Log To Console    ==================================================

        # Rebuild config files (hosts + services only, no servicegroups yet)
        Ctn Prepare Engine Config    ${5}    ${nb_hosts}    ${4}

        # Add all 5 servicegroups to every instance, then notify broker once per instance
        FOR    ${idx}    IN RANGE    5
            FOR    ${sg_id}    IN RANGE    1    6
                ${members}    Ctn Get Filtered Service Names    ${idx}    ${sg_id}
                Ctn Add Service Group    ${idx}    ${sg_id}    ${members}
            END
            Ctn Notify Broker Of Engine Config Change    ${idx}
        END

        # Wait for the service count to stabilise in DB
        ${nb_services}    Evaluate    ${nb_hosts} * ${4}
        TRY
            Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
            Check Query Result
            ...    SELECT COUNT(*) FROM services WHERE enabled = 1    ==    ${nb_services}
            ...    retry_timeout=60s    retry_pause=1s
        FINALLY
            Disconnect From Database
        END

        # Verify all 5 servicegroups: DB count, cache name, cache member count
        FOR    ${sg_id}    IN RANGE    1    6
            ${expected_count}    Evaluate
            ...    sum(1 for s in range(1, ${nb_services} + 1) if s % ${sg_id} == 0)
            Log To Console    sg${sg_id}: expecting ${expected_count} members for ${nb_hosts} hosts

            ${result}    Ctn Check Number Of Relations Between Servicegroup And Services
            ...    ${sg_id}    ${expected_count}    60
            Should Be True    ${result}
            ...    Servicegroup ${sg_id} should have ${expected_count} members in DB (${nb_hosts} hosts)

            ${expected_name}    Set Variable    servicegroup_${sg_id}
            ${cache_sg}    Ctn Get Servicegroup    ${51001}    ${sg_id}
            Should Not Be Equal    ${cache_sg}    ${None}
            ...    Servicegroup ${sg_id} should be present in broker cache
            Should Be Equal    ${cache_sg.name}    ${expected_name}
            ...    Servicegroup ${sg_id} name in cache should be '${expected_name}'
            ${cache_count}    Get Length    ${cache_sg.member_service_ids}
            Should Be Equal As Integers    ${cache_count}    ${expected_count}
            ...    Servicegroup ${sg_id} should have ${expected_count} members in cache (${nb_hosts} hosts)
        END
    END

    Ctn Stop Engine
    Ctn Kindly Stop Broker
