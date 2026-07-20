*** Settings ***
Documentation       Centreon Broker and Engine hostgroup management in centralized mode.
...                 The goal is to verify consistency between the database and the broker
...                 cache (name and members) after adding and removing hostgroups one by one
...                 across multiple engine instances.

Resource    ../resources/import.resource

Suite Setup    Ctn Clean Before Suite
Suite Teardown    Ctn Clean After Suite
Test Setup    Ctn Stop Processes
Test Teardown    Ctn Save Logs If Failed


*** Test Cases ***
BEPHG1
    [Documentation]
    ...    Given a central broker, a rrd broker and 5 engine instances in centralized mode
    ...    With 50 hosts each (250 hosts total, numbered 1 to 250) and 20 services per host
    ...    When broker and engines are started and the initial configuration is applied
    ...    And hostgroups hg1 to hg5 are added one by one:
    ...      hg1 contains all 250 hosts,
    ...      hg2 contains the 125 hosts with even IDs,
    ...      hg3 contains the 83 hosts with IDs divisible by 3,
    ...      hg4 contains the 62 hosts with IDs divisible by 4,
    ...      hg5 contains the 50 hosts with IDs divisible by 5
    ...    Then after each addition the database and the broker cache are consistent:
    ...      the hosts_hostgroups table has the expected number of entries,
    ...      the hostgroup name in the cache matches hostgroup_<id>,
    ...      and the member count in the cache matches the database
    ...    And when hostgroups hg1 to hg5 are removed one by one
    ...    Then after each removal the database shows zero entries for that group
    ...    And the broker cache no longer reports any members for that group
    [Tags]    broker    engine    hostgroups
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

    # Wait for all 250 hosts to be registered in the database
    TRY
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        Check Query Result    SELECT COUNT(*) FROM hosts WHERE enabled = 1    ==    ${250}    retry_timeout=60s    retry_pause=1s
    FINALLY
        Disconnect From Database
    END

    # Add hostgroups hg1..hg5 one by one and verify DB + cache consistency after each
    FOR    ${hg_id}    IN RANGE    1    6
        Log To Console    Adding hostgroup ${hg_id} to all 5 engine instances
        FOR    ${idx}    IN RANGE    5
            ${members}    Ctn Get Filtered Host Names    ${idx}    ${hg_id}
            Ctn Add Host Group    ${idx}    ${hg_id}    ${members}
            Ctn Notify Broker Of Engine Config Change    ${idx}
        END
        ${expected_count}    Evaluate    sum(1 for h in range(1, 251) if h % ${hg_id} == 0)
        Log To Console    Expecting ${expected_count} hosts in hostgroup ${hg_id}

        # DB check: hosts_hostgroups has the expected number of entries
        ${result}    Ctn Check Number Of Relations Between Hostgroup And Hosts
        ...    ${hg_id}    ${expected_count}    60
        Should Be True    ${result}
        ...    Hostgroup ${hg_id} should have ${expected_count} members in DB within 60 seconds

        # Cache check: hostgroup name and member count match
        ${expected_name}    Set Variable    hostgroup_${hg_id}
        ${cache_hg}    Ctn Get Hostgroup    ${51001}    ${hg_id}
        Should Not Be Equal    ${cache_hg}    ${None}
        ...    Hostgroup ${hg_id} should be present in the broker cache
        Should Be Equal    ${cache_hg.name}    ${expected_name}
        ...    Hostgroup ${hg_id} name in cache should be '${expected_name}'

        ${cache_count}    Get Length    ${cache_hg.member_host_ids}
        Should Be Equal As Integers    ${cache_count}    ${expected_count}
        ...    Hostgroup ${hg_id} should have ${expected_count} members in the broker cache
    END

    # Remove hostgroups hg1..hg5 one by one and verify DB + cache consistency after each
    FOR    ${hg_id}    IN RANGE    1    6
        Log To Console    Removing hostgroup ${hg_id} from all 5 engine instances
        FOR    ${idx}    IN RANGE    5
            Ctn Remove Host Group    ${idx}    ${hg_id}
            Ctn Notify Broker Of Engine Config Change    ${idx}
        END

        # DB check: no remaining entries for this hostgroup
        ${result}    Ctn Check Number Of Relations Between Hostgroup And Hosts
        ...    ${hg_id}    ${0}    60
        Should Be True    ${result}
        ...    Hostgroup ${hg_id} should have 0 members in DB after removal within 60 seconds

        # Cache check: hostgroup no longer present (or member list is empty)
        ${cache_hg}    Ctn Get Hostgroup    ${51001}    ${hg_id}
        IF    $cache_hg is not None
            ${cache_count}    Get Length    ${cache_hg.member_host_ids}
            Should Be Equal As Integers    ${cache_count}    ${0}
            ...    Hostgroup ${hg_id} should have 0 members in the broker cache after removal
        END
    END

    Ctn Stop Engine
    Ctn Kindly Stop Broker

BEPHG2
    [Documentation]
    ...    Given a central broker, a rrd broker and 5 engine instances in centralized mode
    ...    With an initial 50 hosts (distributed across 5 instances) and 20 services per host
    ...    And 5 persistent hostgroups: hg1=all hosts, hg2=even IDs, hg3=div-by-3,
    ...      hg4=div-by-4, hg5=div-by-5
    ...    When the host count scales up from 50 to 250 in steps of 50
    ...    And then scales back down to 50 in the same steps
    ...    Then after each step the database and the broker cache are consistent for all 5
    ...      hostgroups: correct name, correct member count in DB, correct member count in cache
    [Tags]    broker    engine    hostgroups
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

    # Wait for the initial 50 hosts to be registered in the database
    TRY
        Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
        Check Query Result    SELECT COUNT(*) FROM hosts WHERE enabled = 1    ==    ${50}    retry_timeout=60s    retry_pause=1s
    FINALLY
        Disconnect From Database
    END

    # Scale up (50→100→150→200→250) then back down (200→150→100→50).
    # At each step: rebuild configs, add all 5 hostgroups, notify broker, then verify.
    FOR    ${nb_hosts}    IN    ${50}    ${100}    ${150}    ${200}    ${250}    ${200}    ${150}    ${100}    ${50}
        Log To Console    === ${nb_hosts} hosts distributed across 5 instances) ===

        # Rebuild config files (hosts + services only, no hostgroups yet)
        Ctn Prepare Engine Config    ${5}    ${nb_hosts}    ${20}

        # Add all 5 hostgroups to every instance, then notify broker once per instance
        FOR    ${idx}    IN RANGE    5
            FOR    ${hg_id}    IN RANGE    1    6
                ${members}    Ctn Get Filtered Host Names    ${idx}    ${hg_id}
                Ctn Add Host Group    ${idx}    ${hg_id}    ${members}
            END
            Ctn Notify Broker Of Engine Config Change    ${idx}
        END

        # Wait for the host count to stabilise in DB
        TRY
            Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
            Check Query Result
            ...    SELECT COUNT(*) FROM hosts WHERE enabled = 1    ==    ${nb_hosts}
            ...    retry_timeout=60s    retry_pause=1s
        FINALLY
            Disconnect From Database
        END

        # Verify all 5 hostgroups: DB count, cache name, cache member count
        FOR    ${hg_id}    IN RANGE    1    6
            ${expected_count}    Evaluate    sum(1 for h in range(1, ${nb_hosts} + 1) if h % ${hg_id} == 0)
            Log To Console    hg${hg_id}: expecting ${expected_count} members for ${nb_hosts} hosts

            ${result}    Ctn Check Number Of Relations Between Hostgroup And Hosts
            ...    ${hg_id}    ${expected_count}    60
            Should Be True    ${result}
            ...    Hostgroup ${hg_id} should have ${expected_count} members in DB (${nb_hosts} hosts)

            ${expected_name}    Set Variable    hostgroup_${hg_id}
            ${cache_hg}    Ctn Get Hostgroup    ${51001}    ${hg_id}
            Should Not Be Equal    ${cache_hg}    ${None}
            ...    Hostgroup ${hg_id} should be present in broker cache
            Should Be Equal    ${cache_hg.name}    ${expected_name}
            ...    Hostgroup ${hg_id} name in cache should be '${expected_name}'
            ${cache_count}    Get Length    ${cache_hg.member_host_ids}
            Should Be Equal As Integers    ${cache_count}    ${expected_count}
            ...    Hostgroup ${hg_id} should have ${expected_count} members in cache (${nb_hosts} hosts)
        END
    END

    Ctn Stop Engine
    Ctn Kindly Stop Broker
