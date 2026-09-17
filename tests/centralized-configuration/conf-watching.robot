*** Settings ***
Documentation       Broker watches the PHP cache directory through inotify: how it coalesces a burst of pushes, and how it recovers a watch it lost.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Clean Before Test
Test Teardown       Ctn Stop Engine Broker And Save Logs


*** Test Cases ***
BECWATCH1
    [Documentation]    Scenario: PHP notifies several poller configurations in a burst
    ...    Given a centralized platform with 3 pollers, all connected
    ...    When three changed configurations are notified one after another
    ...    Then Broker handles the whole burst as a single batch
    ...    And it reads the stored poller configurations only once for it
    ...    When the same three configurations are notified again, unchanged
    ...    Then that burst is a single batch too
    [Tags]    broker    engine    config    centralized
    Ctn Clear Prot Files
    Ctn Clear Broker Cache
    Ctn Config Centralized Engine    ${3}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True

    # The initial configurations must be through before the burst is measured,
    # otherwise their own batch would be counted with it.
    # This message carries the count, unlike the bbdo one, so it says how many
    # pollers answered rather than just that all of them did.
    ${content}    Create List    All engine peers acknowledged? 3/3 acknowledged
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    The three pollers did not acknowledge their initial configuration
    # Reading the whole configuration store is the expensive part of a cycle, so
    # counting those reads is what tells how many batches a burst became.
    #
    # Each configuration has to actually change, otherwise the pushes would be
    # answered with "already has the latest configuration" and the burst would
    # never exercise a real deployment: no diff computed, no file written, no
    # delivery -- the shortest path there is.
    FOR    ${i}    IN RANGE    ${3}
        Ctn Engine Config Set Value    ${i}    log_level_checks    trace
    END

    # The burst. This keyword touches the three .lck files one after another --
    # a few milliseconds apart, well inside the 500ms coalescing window -- then
    # waits for each poller to have answered.
    ${burst}    Ctn Get Round Current Date
    Ctn Push Configuration Per Poller And Wait    ${burst}    ${0}    ${3}
    # Counted from the burst, so no earlier phase can be mistaken for it. The
    # push keyword already waited for every poller to answer, so all the cycles
    # are behind us; settling on top of that catches a second batch that should
    # not have happened.
    ${batches}    Ctn Wait For Stable Count In Log
    ...    ${centralLog}
    ...    ${burst}
    ...    stored poller configurations for the cross-poller validation
    Should Be Equal As Integers
    ...    ${batches}
    ...    ${1}
    ...    the burst of 3 changed configurations became ${batches} batches, so it reloaded the store as many times

    # And the other real case: the user pushes again without having changed
    # anything. Broker then answers "already has the latest configuration" and
    # stops before computing any diff -- a shorter path, which has to coalesce
    # just the same.
    ${burst}    Ctn Get Round Current Date
    Ctn Push Configuration Per Poller And Wait    ${burst}    ${0}    ${3}
    ${batches}    Ctn Wait For Stable Count In Log
    ...    ${centralLog}
    ...    ${burst}
    ...    stored poller configurations for the cross-poller validation
    Should Be Equal As Integers
    ...    ${batches}
    ...    ${1}
    ...    the burst of 3 unchanged configurations became ${batches} batches instead of one

BECWATCH2_${lck_mode}
    [Documentation]    Scenario: the watched cache directory is moved out of the way and back
    ...    Given a centralized platform with 1 poller and Broker started
    ...    When the cache directory is renamed, so the inotify watch is lost
    ...    Then Broker reports the loss and cannot establish the watch again
    ...    When the directory is put back
    ...    Then Broker establishes the watch again without waiting for the slow period
    ...    And a configuration pushed afterwards is detected, whichever shape
    ...    announces it -- pollers.lck or <ID>.lck
    [Tags]    broker    engine    config    centralized
    Ctn Clear Prot Files
    Ctn Clear Broker Cache
    Ctn Config Centralized Engine    ${1}
    Ctn Config Broker    central
    Ctn Config Broker    module    ${1}
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    core    debug

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True    only_central=True

    # The configuration shipped with the platform is handled first, so what
    # follows cannot be confused with it.
    ${content}    Create List    New Engine configuration for poller 1 stored
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    Broker did not store the initial configuration
    Sleep    2s
    ${moved}    Ctn Get Round Current Date

    # A watch is on an inode, not on a path: renaming the directory leaves the
    # watch alive on something nobody writes to any more, which is exactly the
    # case that used to go unnoticed.
    Move Directory    ${VarRoot}/lib/centreon/config    ${VarRoot}/lib/centreon/config-away
    ${content}    Create List    was lost (mask
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${moved}    ${content}    30
    Should Be True    ${result}    Broker did not notice that its watch was lost

    # Put it back. Nothing can wake Broker up while the watch is down, so this
    # only works because the safety net comes round quickly in that state.
    Move Directory    ${VarRoot}/lib/centreon/config-away    ${VarRoot}/lib/centreon/config
    ${content}    Create List    established again
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${moved}    ${content}    60
    Should Be True    ${result}    Broker did not establish the watch again

    # And the proof it is a working watch and not just a descriptor: a push made
    # now has to be seen.
    ${pushed}    Ctn Get Round Current Date
    Ctn Engine Config Set Value    ${0}    log_level_events    debug
    Ctn Announce Poller Configurations    ${lck_mode}    ${0}
    # The two shapes are reported by two different lines: a batch is announced as
    # a whole, an individual lock file names the poller it belongs to.
    IF    '${lck_mode}' == 'per_poller'
        ${announced}    Set Variable    New Engine configuration available, change in '1.lck'
    ELSE
        ${announced}    Set Variable    A poller batch was announced in 'pollers.lck'
    END
    ${content}    Create List    ${announced}
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${pushed}    ${content}    60
    Should Be True    ${result}    The re-established watch did not report a new configuration

    Examples:    lck_mode    --
    ...    batch
    ...    per_poller

    # No RRD broker here: this test only needs the central one, and the suite
    # teardown would look for a b2 that was never started. And this test leaves
    # things behind that the shared setup does not clean: its poller is never
    # connected, so its .lck is deliberately kept -- and Ctn Clear Prot Files
    # only removes .prot files -- while a failure mid-way could leave the
    # directory renamed. Both announcement files are removed: only one of them
    # exists in a given run, and Remove File is fine with a missing one.
    [Teardown]    Run Keywords
    ...    Ctn Stop Engine Broker And Save Logs    only_central=True
    ...    AND    Remove File    ${VarRoot}/lib/centreon/config/1.lck
    ...    AND    Remove File    ${VarRoot}/lib/centreon/config/pollers.lck
    ...    AND    Remove Directory    ${VarRoot}/lib/centreon/config-away    recursive=True

BECWATCH3
    [Documentation]    Scenario: PHP announces an export through the poller batch file
    ...    Given a centralized platform with 3 pollers, all connected
    ...    When the three configurations are announced by a single pollers.lck
    ...    And no individual <id>.lck is written
    ...    Then Broker handles the three of them in one pass
    ...    And it reads the stored poller configurations only once
    ...    And it consumes the batch file
    ...    And this holds whether the file was renamed into place or written directly
    [Tags]    broker    engine    config    centralized
    Ctn Clear Prot Files
    Ctn Clear Broker Cache
    Ctn Config Centralized Engine    ${3}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${3}
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    ${content}    Create List    All engine peers acknowledged? 3/3 acknowledged
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    The three pollers did not acknowledge their initial configuration
    # The contract allows two ways of writing the batch file, and Broker has to
    # act on either. First the atomic one: a temporary, then a rename, which
    # reaches Broker as IN_MOVED_TO.
    FOR    ${i}    IN RANGE    ${3}
        Ctn Engine Config Set Value    ${i}    log_level_checks    trace
    END
    ${batch}    Ctn Get Round Current Date
    Ctn Push Configuration Batch And Wait    ${batch}    ${0}    ${3}    atomic=${True}
    ${content}    Create List    announces the configuration of 3 poller(s)
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${batch}    ${content}    30
    Should Be True    ${result}    Broker did not read the renamed poller batch file

    # The whole export in one pass, which is the point of the batch: the store is
    # read once whatever the delay between the configurations would have been.
    # Counted from the push, so no earlier phase can be mistaken for it.
    ${passes}    Ctn Wait For Stable Count In Log
    ...    ${centralLog}
    ...    ${batch}
    ...    stored poller configurations for the cross-poller validation
    Should Be Equal As Integers
    ...    ${passes}
    ...    ${1}
    ...    the renamed batch of 3 pollers became ${passes} passes instead of one

    # Then the direct one: open, write, close. Broker acts on the close, so it
    # never reads a half-written batch.
    FOR    ${i}    IN RANGE    ${3}
        Ctn Engine Config Set Value    ${i}    log_level_checks    debug
    END
    ${batch}    Ctn Get Round Current Date
    Ctn Push Configuration Batch And Wait    ${batch}    ${0}    ${3}
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${batch}    ${content}    30
    Should Be True    ${result}    Broker did not read the directly written poller batch file

    ${passes}    Ctn Wait For Stable Count In Log
    ...    ${centralLog}
    ...    ${batch}
    ...    stored poller configurations for the cross-poller validation
    Should Be Equal As Integers
    ...    ${passes}
    ...    ${1}
    ...    the directly written batch of 3 pollers became ${passes} passes instead of one

BECWATCH4_${lck_mode}
    [Documentation]    Scenario: two hosts swap pollers within a single export
    ...    Given a centralized platform with 2 pollers of 5 hosts each
    ...    When host_1 moves to poller 2 and host_6 moves to poller 1
    ...    And both configurations are announced together, by one pollers.lck or
    ...    by one <ID>.lck each
    ...    Then the global diff turns each move into a modification, not a removal
    ...    And both hosts stay enabled, each attached to its new poller
    [Tags]    broker    engine    config    centralized
    Ctn Clear Prot Files
    Ctn Clear Broker Cache
    Ctn Config Centralized Engine    ${2}    ${10}    ${2}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module    ${2}
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Broker Config Log    central    sql    debug

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    ${content}    Create List    All engine peers acknowledged? 2/2 acknowledged
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    60
    Should Be True    ${result}    The two pollers did not acknowledge their initial configuration

    # host_1 belongs to poller 1, host_6 to poller 2. Check that before moving
    # them, so a change in how hosts are spread does not silently turn this test
    # into something else.
    Connect To Database    pymysql    ${DBName}    ${DBUser}    ${DBPass}    ${DBHost}    ${DBPort}
    Check Query Result
    ...    SELECT instance_id FROM hosts WHERE name = 'host_1'    ==    ${1}
    ...    retry_timeout=60s    retry_pause=2s
    Check Query Result
    ...    SELECT instance_id FROM hosts WHERE name = 'host_6'    ==    ${2}
    ...    retry_timeout=60s    retry_pause=2s

    # The swap. Both hosts keep their host_id, which is what makes this a move.
    Ctn Engine Move Host    ${0}    ${1}    host_1
    Ctn Engine Move Host    ${1}    ${0}    host_6

    ${swap}    Ctn Get Round Current Date
    Ctn Announce Poller Configurations    ${lck_mode}    ${0}    ${1}

    ${content}    Create List    All engine peers acknowledged? 2/2 acknowledged
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${swap}    ${content}    90
    Should Be True    ${result}    The two pollers did not acknowledge the swapped configuration

    # The point of the whole thing: a host that changed poller must not be
    # disabled. Both must still be enabled, each on its new instance.
    Check Query Result
    ...    SELECT instance_id FROM hosts WHERE name = 'host_1' AND enabled = 1    ==    ${2}
    ...    retry_timeout=60s    retry_pause=2s
    Check Query Result
    ...    SELECT instance_id FROM hosts WHERE name = 'host_6' AND enabled = 1    ==    ${1}
    ...    retry_timeout=60s    retry_pause=2s
    Disconnect From Database

    Examples:    lck_mode    --
    ...    batch
    ...    per_poller
