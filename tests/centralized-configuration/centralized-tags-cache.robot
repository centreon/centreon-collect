*** Settings ***
Documentation       Engine/Broker tests verifying tag associations in the broker cache
...                 via gRPC, using the centralized (BBDO3) configuration mode.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs


*** Test Cases ***
BECNTAG1
    [Documentation]    Feature: Tag associations in Broker gRPC cache with centralized configuration
    ...
    ...    Background:
    ...        Given 4 pollers are configured with 5 hosts each (20 total) and 20 services per host (400 total)
    ...        And 4 tags are defined on every poller, one of each TagType:
    ...          tag1 (id=1, SERVICEGROUP=0), tag2 (id=1, HOSTGROUP=1),
    ...          tag3 (id=1, SERVICECATEGORY=2), tag4 (id=1, HOSTCATEGORY=3)
    ...        And Broker and Engine are started in centralized (BBDO3) mode
    ...
    ...    Scenario: Phase 1 - All pollers assign tags
    ...        Given all 4 pollers assign group_tags and category_tags to every host and service
    ...        When Broker and Engine are started and synchronized
    ...        Then the broker gRPC cache returns exactly the 20 expected hosts with HOSTGROUP tag 'tag2'
    ...        And the broker gRPC cache returns exactly the 20 expected hosts with HOSTCATEGORY tag 'tag4'
    ...        And the broker gRPC cache returns 400 services with SERVICEGROUP tag 'tag1', all on the 20 expected hosts
    ...        And the broker gRPC cache returns 400 services with SERVICECATEGORY tag 'tag3', all on the 20 expected hosts
    ...
    ...    Scenario: Phase 2 - Tags removed from poller 3
    ...        Given the initial state has 20 tagged hosts and 400 tagged services
    ...        When group_tags and category_tags are removed from poller 3 and broker is notified
    ...        Then the broker gRPC cache returns exactly hosts from pollers 0-2 with HOSTGROUP tag 'tag2'
    ...        And the broker gRPC cache returns exactly hosts from pollers 0-2 with HOSTCATEGORY tag 'tag4'
    ...        And the broker gRPC cache returns 300 services with SERVICEGROUP tag 'tag1', all on hosts from pollers 0-2
    ...        And the broker gRPC cache returns 300 services with SERVICECATEGORY tag 'tag3', all on hosts from pollers 0-2
    ...
    ...    Scenario: Phase 3 - Tags removed from poller 2
    ...        Given poller 3 tags have already been removed
    ...        When group_tags and category_tags are removed from poller 2 and broker is notified
    ...        Then the broker gRPC cache returns exactly hosts from pollers 0-1 with HOSTGROUP tag 'tag2'
    ...        And the broker gRPC cache returns exactly hosts from pollers 0-1 with HOSTCATEGORY tag 'tag4'
    ...        And the broker gRPC cache returns 200 services with SERVICEGROUP tag 'tag1', all on hosts from pollers 0-1
    ...        And the broker gRPC cache returns 200 services with SERVICECATEGORY tag 'tag3', all on hosts from pollers 0-1
    ...
    ...    Scenario: Phase 4 - Tags removed from all remaining pollers
    ...        Given pollers 2 and 3 tags have already been removed
    ...        When group_tags and category_tags are removed from pollers 0 and 1 and broker is notified
    ...        Then the broker gRPC cache returns 0 hosts with HOSTGROUP tag 'tag2'
    ...        And the broker gRPC cache returns 0 hosts with HOSTCATEGORY tag 'tag4'
    ...        And the broker gRPC cache returns 0 services with SERVICEGROUP tag 'tag1'
    ...        And the broker gRPC cache returns 0 services with SERVICECATEGORY tag 'tag3'
    ...
    ...    Scenario: Phase 5 - Tag cache is empty (no orphan tags)
    ...        Given all tags have been removed from all pollers
    ...        When GetTags gRPC is called
    ...        Then the broker tag cache returns an empty list (no orphan tags remain)
    [Tags]    broker    engine    cache    tags

    # 4 pollers, 20 hosts total (5 per poller), 20 services per host = 400 services
    Ctn Config Centralized Engine    ${4}    ${20}    ${20}

    # Determine expected host ID sets per poller from the generated config files
    ${host_ids_p0}    Ctn Get Host Ids For Poller    ${0}
    ${host_ids_p1}    Ctn Get Host Ids For Poller    ${1}
    ${host_ids_p2}    Ctn Get Host Ids For Poller    ${2}
    ${host_ids_p3}    Ctn Get Host Ids For Poller    ${3}
    ${all_host_ids}    Ctn Get Host Ids For Pollers    0    1    2    3
    ${host_ids_p0_p1_p2}    Ctn Get Host Ids For Pollers    0    1    2
    ${host_ids_p0_p1}    Ctn Get Host Ids For Pollers    0    1
    ${no_host_ids}    Create List

    # Create 4 tags (one per TagType) for each poller and add them to
    # hosts and services before the first start
    FOR    ${i}    IN RANGE    4
        Ctn Create Tags File    ${i}    ${4}
        Ctn Config Engine Add Cfg File    ${i}    tags.cfg
        # group_tags 1 → HOSTGROUP (tag2) on hosts, SERVICEGROUP (tag1) on services
        # category_tags 1 → HOSTCATEGORY (tag4) on hosts, SERVICECATEGORY (tag3) on services
        Ctn Add Tags To All Hosts    ${i}    group_tags    1
        Ctn Add Tags To All Hosts    ${i}    category_tags    1
        Ctn Add Tags To All Services    ${i}    group_tags    1
        Ctn Add Tags To All Services    ${i}    category_tags    1
    END

    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config Broker    module    ${4}
    Ctn Config BBDO3    ${4}
    Ctn Broker Config Log    central    cache    debug
    Ctn Broker Config Log    central    config    debug
    Ctn Broker Config Log    central    bbdo    debug
    Ctn Clear Retention
    Ctn Clear Prot Files

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine Configuration To Be Applied    ${start}    ${0}    ${4}

    # Phase 1: all 4 pollers tagged → exactly 20 hosts (IDs from all pollers) and 400 services
    Log To Console    Phase 1: verifying hosts and services identity in broker cache
    ${result}    Ctn Check Hosts By Tag With Timeout    51001    tag2    ${1}    ${all_host_ids}    60
    Should Be True    ${result}    Phase 1: wrong host set with HOSTGROUP tag 'tag2'
    ${result}    Ctn Check Hosts By Tag With Timeout    51001    tag4    ${3}    ${all_host_ids}    60
    Should Be True    ${result}    Phase 1: wrong host set with HOSTCATEGORY tag 'tag4'
    ${result}    Ctn Check Services By Tag With Timeout    51001    tag1    ${0}    ${all_host_ids}    ${400}    60
    Should Be True    ${result}    Phase 1: wrong service set with SERVICEGROUP tag 'tag1'
    ${result}    Ctn Check Services By Tag With Timeout    51001    tag3    ${2}    ${all_host_ids}    ${400}    60
    Should Be True    ${result}    Phase 1: wrong service set with SERVICECATEGORY tag 'tag3'

    # Phase 2: remove tags from poller 3 → only hosts from pollers 0-2 remain tagged
    Log To Console    Phase 2: removing tags from poller 3
    Ctn Remove Tags From Hosts    ${3}
    Ctn Remove Tags From Services    ${3}
    Ctn Notify Broker Of Engine Config Change    ${3}

    ${result}    Ctn Check Hosts By Tag With Timeout    51001    tag2    ${1}    ${host_ids_p0_p1_p2}    60
    Should Be True    ${result}    Phase 2: wrong host set with HOSTGROUP tag after removing poller 3
    ${result}    Ctn Check Hosts By Tag With Timeout    51001    tag4    ${3}    ${host_ids_p0_p1_p2}    60
    Should Be True    ${result}    Phase 2: wrong host set with HOSTCATEGORY tag after removing poller 3
    ${result}    Ctn Check Services By Tag With Timeout    51001    tag1    ${0}    ${host_ids_p0_p1_p2}    ${300}    60
    Should Be True    ${result}    Phase 2: wrong service set with SERVICEGROUP tag after removing poller 3
    ${result}    Ctn Check Services By Tag With Timeout    51001    tag3    ${2}    ${host_ids_p0_p1_p2}    ${300}    60
    Should Be True    ${result}    Phase 2: wrong service set with SERVICECATEGORY tag after removing poller 3

    # Phase 3: remove tags from poller 2 → only hosts from pollers 0-1 remain tagged
    Log To Console    Phase 3: removing tags from poller 2
    Ctn Remove Tags From Hosts    ${2}
    Ctn Remove Tags From Services    ${2}
    Ctn Notify Broker Of Engine Config Change    ${2}

    ${result}    Ctn Check Hosts By Tag With Timeout    51001    tag2    ${1}    ${host_ids_p0_p1}    60
    Should Be True    ${result}    Phase 3: wrong host set with HOSTGROUP tag after removing pollers 2+3
    ${result}    Ctn Check Hosts By Tag With Timeout    51001    tag4    ${3}    ${host_ids_p0_p1}    60
    Should Be True    ${result}    Phase 3: wrong host set with HOSTCATEGORY tag after removing pollers 2+3
    ${result}    Ctn Check Services By Tag With Timeout    51001    tag1    ${0}    ${host_ids_p0_p1}    ${200}    60
    Should Be True    ${result}    Phase 3: wrong service set with SERVICEGROUP tag after removing pollers 2+3
    ${result}    Ctn Check Services By Tag With Timeout    51001    tag3    ${2}    ${host_ids_p0_p1}    ${200}    60
    Should Be True    ${result}    Phase 3: wrong service set with SERVICECATEGORY tag after removing pollers 2+3

    # Phase 4: remove tags from pollers 0 and 1 → no tagged hosts or services remain
    Log To Console    Phase 4: removing tags from all remaining pollers
    Ctn Remove Tags From Hosts    ${0}
    Ctn Remove Tags From Services    ${0}
    Ctn Notify Broker Of Engine Config Change    ${0}
    Ctn Remove Tags From Hosts    ${1}
    Ctn Remove Tags From Services    ${1}
    Ctn Notify Broker Of Engine Config Change    ${1}

    ${result}    Ctn Check Hosts By Tag With Timeout    51001    tag2    ${1}    ${no_host_ids}    60
    Should Be True    ${result}    Phase 4: expected 0 hosts with HOSTGROUP tag after removing all pollers
    ${result}    Ctn Check Hosts By Tag With Timeout    51001    tag4    ${3}    ${no_host_ids}    60
    Should Be True    ${result}    Phase 4: expected 0 hosts with HOSTCATEGORY tag after removing all pollers
    ${result}    Ctn Check Services By Tag With Timeout    51001    tag1    ${0}    ${no_host_ids}    ${0}    60
    Should Be True    ${result}    Phase 4: expected 0 services with SERVICEGROUP tag after removing all pollers
    ${result}    Ctn Check Services By Tag With Timeout    51001    tag3    ${2}    ${no_host_ids}    ${0}    60
    Should Be True    ${result}    Phase 4: expected 0 services with SERVICECATEGORY tag after removing all pollers

    # Phase 5: verify the tag cache itself is empty (no orphan tags)
    Log To Console    Phase 5: verifying tag cache is empty via GetTags
    ${result}    Ctn Check Tags Empty With Timeout    51001    60
    Should Be True    ${result}    Phase 5: broker tag cache is not empty after all tags were removed

    Ctn Stop Engine
    Ctn Kindly Stop Broker
