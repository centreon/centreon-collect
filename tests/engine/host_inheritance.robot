*** Settings ***
Documentation       Centreon Engine verify hosts inheritance.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
EHI0
    [Documentation]    Verify inheritance host : host(empty) inherit from template (full) , on Start Engine
    [Tags]    broker    engine    hosts    MON-148837
    Ctn Config Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

    Ctn Clear Retention

    Ctn Create Tags File    ${0}    ${40}
    Ctn Create Template File    ${0}    host    group_tags    [1, 6]
    Ctn Create Severities File    ${0}    ${20}

    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Config Engine Add Cfg File    ${0}    hostTemplates.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg
    Ctn Config Engine Add Cfg File    ${0}    severities.cfg
    Ctn Engine Config Add Command
    ...    0
    ...    command_notif
    ...    /usr/bin/true

    # Operation in contact group
    Ctn Add Contact Group    ${0}    ${1}    ["John_Doe"]
    Ctn Add Contact Group    ${0}    ${2}    ["U1","U2"]
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    # Operation in host group
    Ctn Add Host Group    ${0}    ${1}    ["host_2","host_3"]

    # Operation in host
    Ctn Add Template To Hosts    0    host_template_1    [1,3]
    Ctn Add Template To Hosts    0    host_template_2    [2]
    Ctn Engine Config Delete Value In Hosts    0    host_1    alias
    Ctn Engine Config Delete Value In Hosts    0    host_1    check_period
    Ctn Engine Config Delete Value In Hosts    0    host_1    address
    Ctn Engine Config Delete Value In Hosts    0    host_1    _KEY1
    Ctn Engine Config Delete Value In Hosts    0    host_1    _SNMPCOMMUNITY
    Ctn Engine Config Delete Value In Hosts    0    host_1    _SNMPVERSION

    # Operation in template host
    Ctn Engine Config Set Value In Hosts    0    host_template_1    alias    alias_Template_1    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    acknowledgement_timeout
    ...    10
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    address    127.0.0.2    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    parents    host_2    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0
    ...    host_template_1
    ...    hostgroups
    ...    hostgroup_1
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0
    ...    host_template_1
    ...    contact_groups
    ...    contactgroup_1
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    contacts    John_Doe    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    notification_period
    ...    workhours
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    check_command    checkh2    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    check_period    workhours    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    event_handler    command_notif    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    notes    template_note    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    notes_url    template_note_url    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    action_url
    ...    template_action_url
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    icon_image
    ...    template_icon_image
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    icon_image_alt
    ...    template_icon_image_alt
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    vrml_image
    ...    template_vrml_image
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    gd2_image
    ...    template_gd2_image
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    statusmap_image
    ...    template_statusmap_image
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    check_interval    2    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    retry_interval    1    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    recovery_notification_delay
    ...    1
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    max_check_attempts    4    hostTemplates.cfg
    Ctn Engine Config Replace Value In Hosts
    ...    0
    ...    host_template_1
    ...    active_checks_enabled
    ...    0
    ...    hostTemplates.cfg
    Ctn Engine Config Replace Value In Hosts
    ...    0
    ...    host_template_1
    ...    passive_checks_enabled
    ...    0
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    event_handler_enabled
    ...    0
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    check_freshness    1    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    freshness_threshold    123    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    low_flap_threshold    83    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    high_flap_threshold    126    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    flap_detection_enabled    0    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    flap_detection_options    all    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    notification_options    all    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    notifications_enabled    1    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    notification_interval    6    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    first_notification_delay    3    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    stalking_options    all    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    process_perf_data    1    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    2d_coords    250,390    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    3d_coords    4.57,3.98,152    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    obsess_over_host    0    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    retain_status_information    0    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    retain_nonstatus_information
    ...    0
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    timezone    GMT+02    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    severity_id    10    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    icon_id    1    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    category_tags    2    hostTemplates.cfg

    Ctn Engine Config Set Value In Hosts    0    host_template_1    _KEY1    VALtemp    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    _SNMPCOMMUNITY    public    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    _SNMPVERSION    2c    hostTemplates.cfg

    ${start}    Ctn Get Round Current Date
    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Host Info Grpc    ${1}

    Should Be Equal As Strings    ${output}[name]    host_1    name
    Should Be Equal As Strings    ${output}[alias]    host_1    alias
    Should Be Equal As Numbers    ${output}[acknowledgementTimeout]    600    acknowledgementTimeout
    Should Be Equal As Strings    ${output}[address]    127.0.0.2    address
    Should Contain    ${output}[parentHosts]    host_2    parentHosts
    Should Contain    ${output}[groupName]    hostgroup_1    groupName
    Should Contain    ${output}[contactgroups]    contactgroup_1    contactgroups
    Should Contain    ${output}[contacts]    John_Doe    contacts
    Should Be Equal As Strings    ${output}[notificationPeriod]    workhours    notificationPeriod
    Should Be Equal As Strings    ${output}[checkCommand]    checkh1    checkCommand
    Should Be Equal As Strings    ${output}[checkPeriod]    workhours    checkPeriod
    Should Be Equal As Strings    ${output}[eventHandler]    command_notif    eventHandler
    Should Be Equal As Strings    ${output}[notes]    template_note    notes
    Should Be Equal As Strings    ${output}[notesUrl]    template_note_url    notesUrl
    Should Be Equal As Strings    ${output}[actionUrl]    template_action_url    actionUrl
    Should Be Equal As Strings    ${output}[iconImage]    template_icon_image    iconImage
    Should Be Equal As Strings    ${output}[iconImageAlt]    template_icon_image_alt    iconImageAlt
    Should Be Equal As Strings    ${output}[vrmlImage]    template_vrml_image    vrmlImage
    Should Be Equal As Strings    ${output}[statusmapImage]    template_gd2_image    statusmapImage
    Should Be Equal As Strings    ${output}[initialState]    UP    initialState should take default value "UP"
    Should Be Equal As Numbers    ${output}[checkInterval]    2    checkInterval
    Should Be Equal As Numbers    ${output}[retryInterval]    1.0    retryInterval
    Should Be Equal As Numbers    ${output}[recoveryNotificationDelay]    1    recoveryNotificationDelay
    Should Be Equal As Numbers    ${output}[maxAttempts]    4    maxAttempts
    Should Not Be True    ${output}[checksEnabled]    checksEnabled:Should Not Be True
    Should Not Be True    ${output}[acceptPassiveChecks]    acceptPassiveChecks:Should Not Be True
    Should Not Be True    ${output}[eventHandlerEnabled]    eventHandlerEnabled:Should Not Be True
    Should Be True    ${output}[checkFreshness]    checkFreshness:Should Be True
    Should Be Equal As Numbers    ${output}[freshnessThreshold]    123    freshnessThreshold
    Should Be Equal As Numbers    ${output}[lowFlapThreshold]    83.0    lowFlapThreshold
    Should Be Equal As Numbers    ${output}[highFlapThreshold]    126.0    highFlapThreshold
    Should Not Be True    ${output}[flapDetectionEnabled]    flapDetectionEnabled:Should Not Be True
    Should Be True    ${output}[flapDetectionOnUp]    flapDetectionOnUp:Should Be True
    Should Be True    ${output}[flapDetectionOnDown]    flapDetectionOnDown:Should Be True
    Should Be True    ${output}[flapDetectionOnUnreachable]    flapDetectionOnUnreachable:Should Be True
    Should Be True    ${output}[notifyUp]    notifyUp:Should Be True
    Should Be True    ${output}[notifyDown]    notifyDown:Should Be True
    Should Be True    ${output}[notifyUnreachable]    notifyUnreachable:Should Be True
    Should Be True    ${output}[notifyOnFlappingstart]    notifyOnFlappingstart:Should Be True
    Should Be True    ${output}[notifyOnFlappingstop]    notifyOnFlappingstop:Should Be True
    Should Be True    ${output}[notifyOnFlappingdisabled]    notifyOnFlappingdisabled:Should Be True
    Should Be True    ${output}[notifyDowntime]    notifyDowntime:Should Be True
    Should Be True    ${output}[notificationsEnabled]    notificationsEnabled:Should Be True
    Should Be Equal As Numbers    ${output}[notificationInterval]    6    notificationInterval
    Should Be Equal As Numbers    ${output}[firstNotificationDelay]    3    firstNotificationDelay
    Should Be True    ${output}[stalkOnUp]    stalkOnUp:Should Be True
    Should Be True    ${output}[stalkOnDown]    stalkOnDown:Should Be True
    Should Be True    ${output}[stalkOnUnreachable]    stalkOnUnreachable:Should Be True
    Should Be True    ${output}[processPerformanceData]    processPerformanceData:Should Be True
    Should Be True    ${output}[have2dCoords]    have2dCoords:Should Be True
    Should Be Equal As Numbers    ${output}[x2d]    250    x2d
    Should Be Equal As Numbers    ${output}[y2d]    390    y2d
    Should Be True    ${output}[have3dCoords]    have3dCoords:Should Be True
    Should Be Equal As Numbers    ${output}[x3d]    4.57    x3d
    Should Be Equal As Numbers    ${output}[y3d]    3.98    y3d
    Should Be Equal As Numbers    ${output}[z3d]    152    z3d
    Should Not Be True    ${output}[obsessOverHost]    obsessOverHost:Should Not Be True
    Should Be Equal As Strings    ${output}[retainStatusInformation]    0    retainStatusInformation
    Should Not Be True    ${output}[retainNonstatusInformation]    retainNonstatusInformation:Should Not Be True
    Should Be Equal As Strings    ${output}[timezone]    GMT+02    timezone
    Should Be Equal As Strings    ${output}[severityLevel]    5    severityLevel
    Should Be Equal As Strings    ${output}[severityId]    10    severityId
    Should Contain    ${output}[tag]    id:1,name:tag2,type:1
    Should Contain    ${output}[tag]    id:2,name:tag8,type:3
    Should Be Equal As Strings    ${output}[iconId]    1    iconId
    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    KEY1    VALtemp
    Should Be True    ${ret}    customVariables_KEY1:Should Be True
    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    SNMPCOMMUNITY    public
    Should Be True    ${ret}    customVariables_SNMPCOMMUNITY:Should Be True
    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    SNMPVERSION    2c
    Should Be True    ${ret}    customVariables_SNMPVERSION:Should Be True

    Ctn Stop Engine
    Ctn Kindly Stop Broker

EHI1
    [Documentation]    Verify inheritance host : host(full) inherit from template (full) , on Start engine
    [Tags]    broker    engine    hosts    MON-148837
    Ctn Config Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

    Ctn Create Tags File    ${0}    ${40}
    Ctn Create Template File    ${0}    host    group_tags    [1, 6]
    Ctn Create Severities File    ${0}    ${20}

    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Config Engine Add Cfg File    ${0}    hostTemplates.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg
    Ctn Config Engine Add Cfg File    ${0}    severities.cfg
    Ctn Engine Config Add Command
    ...    0
    ...    command_notif
    ...    /usr/bin/true

    # Operation in contact group
    Ctn Add Contact Group    ${0}    ${1}    ["John_Doe"]
    Ctn Add Contact Group    ${0}    ${2}    ["U1","U2"]
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    # Operation in host group
    Ctn Add Host Group    ${0}    ${1}    ["host_2","host_3"]
    Ctn Add Host Group    ${0}    ${2}    ["host_4"]

    # Operation in host
    Ctn Add Template To Hosts    0    host_template_1    [1,3]
    Ctn Add Template To Hosts    0    host_template_2    [2]
    Ctn Engine Config Delete Value In Hosts    0    host_1    alias
    Ctn Engine Config Delete Value In Hosts    0    host_1    check_period
    Ctn Engine Config Delete Value In Hosts    0    host_1    address
    Ctn Engine Config Delete Value In Hosts    0    host_1    _KEY1
    Ctn Engine Config Delete Value In Hosts    0    host_1    _SNMPCOMMUNITY
    Ctn Engine Config Delete Value In Hosts    0    host_1    _SNMPVERSION

    # Add in host
    Ctn Engine Config Set Value In Hosts    0    host_1    alias    host_1_alias
    Ctn Engine Config Set Value In Hosts    0    host_1    acknowledgement_timeout    5
    Ctn Engine Config Set Value In Hosts    0    host_1    address    127.0.0.1
    Ctn Engine Config Set Value In Hosts    0    host_1    parents    host_3
    Ctn Engine Config Set Value In Hosts    0    host_1    hostgroups    hostgroup_2
    Ctn Engine Config Set Value In Hosts    0    host_1    contact_groups    contactgroup_2
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    U1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_period    none
    Ctn Engine Config Set Value In Hosts    0    host_1    check_period    none
    Ctn Engine Config Set Value In Hosts    0    host_1    event_handler    command_1
    Ctn Engine Config Set Value In Hosts    0    host_1    notes    notes
    Ctn Engine Config Set Value In Hosts    0    host_1    notes_url    notes_url
    Ctn Engine Config Set Value In Hosts    0    host_1    action_url    action_url
    Ctn Engine Config Set Value In Hosts    0    host_1    icon_image    icon_image
    Ctn Engine Config Set Value In Hosts    0    host_1    icon_image_alt    icon_image_alt
    Ctn Engine Config Set Value In Hosts    0    host_1    vrml_image    vrml_image
    Ctn Engine Config Set Value In Hosts    0    host_1    gd2_image    gd2_image
    Ctn Engine Config Set Value In Hosts    0    host_1    statusmap_image    statusmap_image
    Ctn Engine Config Set Value In Hosts    0    host_1    check_interval    3
    Ctn Engine Config Set Value In Hosts    0    host_1    retry_interval    2
    Ctn Engine Config Set Value In Hosts    0    host_1    recovery_notification_delay    2
    Ctn Engine Config Set Value In Hosts    0    host_1    max_check_attempts    8
    Ctn Engine Config Set Value In Hosts    0    host_1    active_checks_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    passive_checks_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    event_handler_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    check_freshness    0
    Ctn Engine Config Set Value In Hosts    0    host_1    freshness_threshold    14
    Ctn Engine Config Set Value In Hosts    0    host_1    low_flap_threshold    53
    Ctn Engine Config Set Value In Hosts    0    host_1    high_flap_threshold    15
    Ctn Engine Config Set Value In Hosts    0    host_1    flap_detection_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    flap_detection_options    none
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    none
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    0
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_interval    8
    Ctn Engine Config Set Value In Hosts    0    host_1    first_notification_delay    6
    Ctn Engine Config Set Value In Hosts    0    host_1    stalking_options    none
    Ctn Engine Config Set Value In Hosts    0    host_1    process_perf_data    0
    Ctn Engine Config Set Value In Hosts    0    host_1    2d_coords    50,50
    Ctn Engine Config Set Value In Hosts    0    host_1    3d_coords    3.5,400,500
    Ctn Engine Config Set Value In Hosts    0    host_1    obsess_over_host    1
    Ctn Engine Config Set Value In Hosts    0    host_1    retain_status_information    1
    Ctn Engine Config Set Value In Hosts    0    host_1    retain_nonstatus_information    1
    Ctn Engine Config Set Value In Hosts    0    host_1    timezone    GMT+01
    Ctn Engine Config Set Value In Hosts    0    host_1    severity_id    2
    Ctn Engine Config Set Value In Hosts    0    host_1    icon_id    15
    Ctn Engine Config Set Value In Hosts    0    host_1    _KEY1    VAL1
    Ctn Engine Config Set Value In Hosts    0    host_1    _KEY2    VAL2
    Ctn Engine Config Set Value In Hosts    0    host_1    _KEY3    VAL3
    Ctn Engine Config Set Value In Hosts    0    host_1    group_tags    6
    Ctn Engine Config Set Value In Hosts    0    host_1    category_tags    7

    # Operation in template host
    Ctn Engine Config Set Value In Hosts    0    host_template_1    alias    alias_Template_1    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    acknowledgement_timeout
    ...    10
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    address    127.0.0.2    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    parents    host_2    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0
    ...    host_template_1
    ...    hostgroups
    ...    hostgroup_1
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0
    ...    host_template_1
    ...    contact_groups
    ...    contactgroup_1
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    contacts    John_Doe    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    notification_period
    ...    workhours
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    check_command    checkh2    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    check_period    workhours    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    event_handler    command_notif    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    notes    template_note    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    notes_url    template_note_url    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    action_url
    ...    template_action_url
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    icon_image
    ...    template_icon_image
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    icon_image_alt
    ...    template_icon_image_alt
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    vrml_image
    ...    template_vrml_image
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    gd2_image
    ...    template_gd2_image
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    statusmap_image
    ...    template_statusmap_image
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    check_interval    2    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    retry_interval    1    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    recovery_notification_delay
    ...    1
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    max_check_attempts    4    hostTemplates.cfg
    Ctn Engine Config Replace Value In Hosts
    ...    0
    ...    host_template_1
    ...    active_checks_enabled
    ...    0
    ...    hostTemplates.cfg
    Ctn Engine Config Replace Value In Hosts
    ...    0
    ...    host_template_1
    ...    passive_checks_enabled
    ...    0
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    event_handler_enabled
    ...    0
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    check_freshness    1    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    freshness_threshold    123    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    low_flap_threshold    83    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    high_flap_threshold    126    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    flap_detection_enabled    0    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    flap_detection_options    all    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    notification_options    all    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    notifications_enabled    1    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    notification_interval    6    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    first_notification_delay    3    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    stalking_options    all    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    process_perf_data    1    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    2d_coords    250,390    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    3d_coords    4.57,3.98,152    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    obsess_over_host    0    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    retain_status_information    0    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    retain_nonstatus_information
    ...    0
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    timezone    GMT+05    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    severity_id    10    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    icon_id    1    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    category_tags    2    hostTemplates.cfg

    Ctn Engine Config Set Value In Hosts    0    host_template_1    _KEY1    VALtemp    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    _SNMPCOMMUNITY    public    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    _SNMPVERSION    2c    hostTemplates.cfg

    ${start}    Ctn Get Round Current Date
    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Host Info Grpc    ${1}

    Should Be Equal As Strings    ${output}[name]    host_1    name
    Should Be Equal As Strings    ${output}[alias]    host_1_alias    alias
    Should Be Equal As Numbers    ${output}[acknowledgementTimeout]    300    acknowledgementTimeout
    Should Be Equal As Strings    ${output}[address]    127.0.0.1    address
    Should Contain    ${output}[parentHosts]    host_3    parentHosts
    Should Contain    ${output}[groupName]    hostgroup_2    groupName
    Should Contain    ${output}[contactgroups]    contactgroup_2    contactgroups
    Should Contain    ${output}[contacts]    U1    contacts
    Should Be Equal As Strings    ${output}[notificationPeriod]    none    notificationPeriod
    Should Be Equal As Strings    ${output}[checkCommand]    checkh1    checkCommand
    Should Be Equal As Strings    ${output}[checkPeriod]    none    checkPeriod
    Should Be Equal As Strings    ${output}[eventHandler]    command_1    eventHandler
    Should Be Equal As Strings    ${output}[notes]    notes    notes
    Should Be Equal As Strings    ${output}[notesUrl]    notes_url    notesUrl
    Should Be Equal As Strings    ${output}[actionUrl]    action_url    actionUrl
    Should Be Equal As Strings    ${output}[iconImage]    icon_image    iconImage
    Should Be Equal As Strings    ${output}[iconImageAlt]    icon_image_alt    iconImageAlt
    Should Be Equal As Strings    ${output}[vrmlImage]    vrml_image    vrmlImage
    Should Be Equal As Strings    ${output}[statusmapImage]    gd2_image    statusmapImage
    Should Be Equal As Strings    ${output}[initialState]    UP    initialState should take default value "UP"
    Should Be Equal As Numbers    ${output}[checkInterval]    3    checkInterval
    Should Be Equal As Numbers    ${output}[retryInterval]    2.0    retryInterval
    Should Be Equal As Numbers    ${output}[recoveryNotificationDelay]    2    recoveryNotificationDelay
    Should Be Equal As Numbers    ${output}[maxAttempts]    8    maxAttempts
    Should Be True    ${output}[checksEnabled]    checksEnabled:Should Be True
    Should Be True    ${output}[acceptPassiveChecks]    acceptPassiveChecks:Should Be True
    Should Be True    ${output}[eventHandlerEnabled]    eventHandlerEnabled:Should Be True
    Should Not Be True    ${output}[checkFreshness]    checkFreshness:Should Not Be True
    Should Be Equal As Numbers    ${output}[freshnessThreshold]    14    freshnessThreshold
    Should Be Equal As Numbers    ${output}[lowFlapThreshold]    53.0    lowFlapThreshold
    Should Be Equal As Numbers    ${output}[highFlapThreshold]    15.0    highFlapThreshold
    Should Be True    ${output}[flapDetectionEnabled]    flapDetectionEnabled:Should Be True
    Should Not Be True    ${output}[flapDetectionOnUp]    flapDetectionOnUp:Should Not Be True
    Should Not Be True    ${output}[flapDetectionOnDown]    flapDetectionOnDown:Should Not Be True
    Should Not Be True    ${output}[flapDetectionOnUnreachable]    flapDetectionOnUnreachable:Should Not Be True
    Should Not Be True    ${output}[notifyUp]    notifyUp:Should Not Be True
    Should Not Be True    ${output}[notifyDown]    notifyDown:Should Not Be True
    Should Not Be True    ${output}[notifyUnreachable]    notifyUnreachable:Should Not Be True
    Should Not Be True    ${output}[notifyOnFlappingstart]    notifyOnFlappingstart:Should Not Be True
    Should Not Be True    ${output}[notifyOnFlappingstop]    notifyOnFlappingstop:Should Not Be True
    Should Not Be True    ${output}[notifyOnFlappingdisabled]    notifyOnFlappingdisabled:Should Not Be True
    Should Not Be True    ${output}[notifyDowntime]    notifyDowntime:Should Not Be True
    Should Not Be True    ${output}[notificationsEnabled]    notificationsEnabled::Should Not Be True
    Should Be Equal As Numbers    ${output}[notificationInterval]    8    notificationInterval
    Should Be Equal As Numbers    ${output}[firstNotificationDelay]    6    firstNotificationDelay
    Should Not Be True    ${output}[stalkOnUp]    stalkOnUp::Should Not Be True
    Should Not Be True    ${output}[stalkOnDown]    stalkOnDown::Should Not Be True
    Should Not Be True    ${output}[stalkOnUnreachable]    stalkOnUnreachable::Should Not Be True
    Should Not Be True    ${output}[processPerformanceData]    processPerformanceData
    Should Be True    ${output}[have2dCoords]    have2dCoords:Should Be True
    Should Be Equal As Numbers    ${output}[x2d]    50    x2d
    Should Be Equal As Numbers    ${output}[y2d]    50    y2d
    Should Be True    ${output}[have3dCoords]    have3dCoords:Should Be True
    Should Be Equal As Numbers    ${output}[x3d]    3.5    x3d
    Should Be Equal As Numbers    ${output}[y3d]    400    y3d
    Should Be Equal As Numbers    ${output}[z3d]    500    z3d
    Should Be True    ${output}[obsessOverHost]    obsessOverHost:Should Be True
    Should Be True    ${output}[retainStatusInformation]    retainStatusInformation:Should Be True
    Should Be True    ${output}[retainNonstatusInformation]    retainNonstatusInformation:Should Be True
    Should Be Equal As Strings    ${output}[timezone]    GMT+01    timezone
    Should Be Equal As Strings    ${output}[severityLevel]    2    severityLevel
    Should Be Equal As Strings    ${output}[severityId]    2    severityId
    Should Contain    ${output}[tag]    id:6,name:tag22,type:1
    Should Contain    ${output}[tag]    id:7,name:tag28,type:3
    Should Contain    ${output}[tag]    id:1,name:tag2,type:1
    Should Contain    ${output}[tag]    id:2,name:tag8,type:3
    Should Be Equal As Strings    ${output}[iconId]    15    iconId
    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    KEY1    VAL1
    Should Be True    ${ret}    customVariables_KEY1:Should Be True
    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    KEY2    VAL2
    Should Be True    ${ret}    customVariables_KEY1:Should Be True
    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    KEY3    VAL3
    Should Be True    ${ret}    customVariables_KEY1:Should Be True
    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    SNMPCOMMUNITY    public
    Should Be True    ${ret}    customVariables_SNMPCOMMUNITY:Should Be True
    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    SNMPVERSION    2c
    Should Be True    ${ret}    customVariables_SNMPVERSION:Should Be True

    Ctn Stop Engine
    Ctn Kindly Stop Broker

EHI2
    [Documentation]    Verify inheritance host : host(empty) inherit from template (full) , on Reload engine
    [Tags]    broker    engine    hosts    MON-148837
    Ctn Config Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

    ${start}    Ctn Get Round Current Date
    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Ctn Create Tags File    ${0}    ${40}
    Ctn Create Template File    ${0}    host    group_tags    [1, 6]
    Ctn Create Severities File    ${0}    ${20}

    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Config Engine Add Cfg File    ${0}    hostTemplates.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg
    Ctn Config Engine Add Cfg File    ${0}    severities.cfg
    Ctn Engine Config Add Command
    ...    0
    ...    command_notif
    ...    /usr/bin/true

    # Operation in contact group
    Ctn Add Contact Group    ${0}    ${1}    ["John_Doe"]
    Ctn Add Contact Group    ${0}    ${2}    ["U1","U2"]
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    # Operation in host group
    Ctn Add Host Group    ${0}    ${1}    ["host_2","host_3"]

    # Operation in host
    Ctn Add Template To Hosts    0    host_template_1    [1,3]
    Ctn Add Template To Hosts    0    host_template_2    [2]
    Ctn Engine Config Delete Value In Hosts    0    host_1    alias
    Ctn Engine Config Delete Value In Hosts    0    host_1    check_period
    Ctn Engine Config Delete Value In Hosts    0    host_1    check_command
    Ctn Engine Config Delete Value In Hosts    0    host_1    address
    Ctn Engine Config Delete Value In Hosts    0    host_1    _KEY1
    Ctn Engine Config Delete Value In Hosts    0    host_1    _SNMPCOMMUNITY
    Ctn Engine Config Delete Value In Hosts    0    host_1    _SNMPVERSION

    # Operation in template host
    Ctn Engine Config Set Value In Hosts    0    host_template_1    alias    alias_Template_1    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    acknowledgement_timeout
    ...    10
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    address    127.0.0.2    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    parents    host_2    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0
    ...    host_template_1
    ...    hostgroups
    ...    hostgroup_1
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0
    ...    host_template_1
    ...    contact_groups
    ...    contactgroup_1
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    contacts    John_Doe    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    notification_period
    ...    workhours
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    check_command    checkh2    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    check_period    workhours    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    event_handler    command_notif    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    notes    template_note    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    notes_url    template_note_url    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    action_url
    ...    template_action_url
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    icon_image
    ...    template_icon_image
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    icon_image_alt
    ...    template_icon_image_alt
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    vrml_image
    ...    template_vrml_image
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    gd2_image
    ...    template_gd2_image
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    statusmap_image
    ...    template_statusmap_image
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    check_interval    2    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    retry_interval    1    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    recovery_notification_delay
    ...    1
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    max_check_attempts    4    hostTemplates.cfg
    Ctn Engine Config Replace Value In Hosts
    ...    0
    ...    host_template_1
    ...    active_checks_enabled
    ...    0
    ...    hostTemplates.cfg
    Ctn Engine Config Replace Value In Hosts
    ...    0
    ...    host_template_1
    ...    passive_checks_enabled
    ...    0
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    event_handler_enabled
    ...    0
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    check_freshness    1    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    freshness_threshold    123    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    low_flap_threshold    83    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    high_flap_threshold    126    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    flap_detection_enabled    0    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    flap_detection_options    all    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    notification_options    all    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    notifications_enabled    1    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    notification_interval    6    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    first_notification_delay    3    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    stalking_options    all    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    process_perf_data    1    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    2d_coords    250,390    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    3d_coords    4.57,3.98,152    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    obsess_over_host    0    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    retain_status_information    0    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    retain_nonstatus_information
    ...    0
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    timezone    GMT+02    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    severity_id    10    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    icon_id    1    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    category_tags    2    hostTemplates.cfg

    Ctn Engine Config Set Value In Hosts    0    host_template_1    _KEY1    VALtemp    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    _SNMPCOMMUNITY    public    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    _SNMPVERSION    2c    hostTemplates.cfg

    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}
    ${content}    Create List    Reload configuration finished
    ${result}    Ctn Find In Log With Timeout
    ...    ${ENGINE_LOG}/config0/centengine.log
    ...    ${start}
    ...    ${content}
    ...    60
    ...    verbose=False
    Should Be True    ${result}    Engine is Not Ready after 60s!!

    ${output}    Ctn Get Host Info Grpc    ${1}

    Should Be Equal As Strings    ${output}[name]    host_1    name
    Should Be Equal As Strings    ${output}[alias]    host_1    alias
    Should Be Equal As Numbers    ${output}[acknowledgementTimeout]    600    acknowledgementTimeout
    Should Be Equal As Strings    ${output}[address]    127.0.0.2    address
    Should Contain    ${output}[parentHosts]    host_2    parentHosts
    Should Contain    ${output}[groupName]    hostgroup_1    groupName
    Should Contain    ${output}[contactgroups]    contactgroup_1    contactgroups
    Should Contain    ${output}[contacts]    John_Doe    contacts
    Should Be Equal As Strings    ${output}[notificationPeriod]    workhours    notificationPeriod
    Should Be Equal As Strings    ${output}[checkCommand]    checkh2    checkCommand
    Should Be Equal As Strings    ${output}[checkPeriod]    workhours    checkPeriod
    Should Be Equal As Strings    ${output}[eventHandler]    command_notif    eventHandler
    Should Be Equal As Strings    ${output}[notes]    template_note    notes
    Should Be Equal As Strings    ${output}[notesUrl]    template_note_url    notesUrl
    Should Be Equal As Strings    ${output}[actionUrl]    template_action_url    actionUrl
    Should Be Equal As Strings    ${output}[iconImage]    template_icon_image    iconImage
    Should Be Equal As Strings    ${output}[iconImageAlt]    template_icon_image_alt    iconImageAlt
    Should Be Equal As Strings    ${output}[vrmlImage]    template_vrml_image    vrmlImage
    Should Be Equal As Strings    ${output}[statusmapImage]    template_gd2_image    statusmapImage
    Should Be Equal As Strings    ${output}[initialState]    UP    initialState should take default value "UP"
    Should Be Equal As Numbers    ${output}[checkInterval]    2    checkInterval
    Should Be Equal As Numbers    ${output}[retryInterval]    1.0    retryInterval
    Should Be Equal As Numbers    ${output}[recoveryNotificationDelay]    1    recoveryNotificationDelay
    Should Be Equal As Numbers    ${output}[maxAttempts]    4    maxAttempts
    Should Not Be True    ${output}[checksEnabled]    checksEnabled:Should Not Be True
    Should Not Be True    ${output}[acceptPassiveChecks]    acceptPassiveChecks:Should Not Be True
    Should Not Be True    ${output}[eventHandlerEnabled]    eventHandlerEnabled:Should Not Be True
    Should Be True    ${output}[checkFreshness]    checkFreshness:Should Be True
    Should Be Equal As Numbers    ${output}[freshnessThreshold]    123    freshnessThreshold
    Should Be Equal As Numbers    ${output}[lowFlapThreshold]    83.0    lowFlapThreshold
    Should Be Equal As Numbers    ${output}[highFlapThreshold]    126.0    highFlapThreshold
    Should Not Be True    ${output}[flapDetectionEnabled]    flapDetectionEnabled:Should Not Be True
    Should Be True    ${output}[flapDetectionOnUp]    flapDetectionOnUp:Should Be True
    Should Be True    ${output}[flapDetectionOnDown]    flapDetectionOnDown:Should Be True
    Should Be True    ${output}[flapDetectionOnUnreachable]    flapDetectionOnUnreachable:Should Be True
    Should Be True    ${output}[notifyUp]    notifyUp:Should Be True
    Should Be True    ${output}[notifyDown]    notifyDown:Should Be True
    Should Be True    ${output}[notifyUnreachable]    notifyUnreachable:Should Be True
    Should Be True    ${output}[notifyOnFlappingstart]    notifyOnFlappingstart:Should Be True
    Should Be True    ${output}[notifyOnFlappingstop]    notifyOnFlappingstop:Should Be True
    Should Be True    ${output}[notifyOnFlappingdisabled]    notifyOnFlappingdisabled:Should Be True
    Should Be True    ${output}[notifyDowntime]    notifyDowntime:Should Be True
    Should Be True    ${output}[notificationsEnabled]    notificationsEnabled:Should Be True
    Should Be Equal As Numbers    ${output}[notificationInterval]    6    notificationInterval
    Should Be Equal As Numbers    ${output}[firstNotificationDelay]    3    firstNotificationDelay
    Should Be True    ${output}[stalkOnUp]    stalkOnUp:Should Be True
    Should Be True    ${output}[stalkOnDown]    stalkOnDown:Should Be True
    Should Be True    ${output}[stalkOnUnreachable]    stalkOnUnreachable:Should Be True
    Should Be True    ${output}[processPerformanceData]    processPerformanceData:Should Be True
    Should Be True    ${output}[have2dCoords]    have2dCoords:Should Be True
    Should Be Equal As Numbers    ${output}[x2d]    250    x2d
    Should Be Equal As Numbers    ${output}[y2d]    390    y2d
    Should Be True    ${output}[have3dCoords]    have3dCoords:Should Be True
    Should Be Equal As Numbers    ${output}[x3d]    4.57    x3d
    Should Be Equal As Numbers    ${output}[y3d]    3.98    y3d
    Should Be Equal As Numbers    ${output}[z3d]    152    z3d
    Should Not Be True    ${output}[obsessOverHost]    obsessOverHost:Should Not Be True
    Should Be Equal As Strings    ${output}[retainStatusInformation]    0    retainStatusInformation
    Should Not Be True    ${output}[retainNonstatusInformation]    retainNonstatusInformation:Should Not Be True
    Should Be Equal As Strings    ${output}[timezone]    GMT+02    timezone
    Should Be Equal As Strings    ${output}[severityLevel]    5    severityLevel
    Should Be Equal As Strings    ${output}[severityId]    10    severityId
    Should Contain    ${output}[tag]    id:1,name:tag2,type:1
    Should Contain    ${output}[tag]    id:2,name:tag8,type:3
    Should Be Equal As Strings    ${output}[iconId]    1    iconId
    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    KEY1    VALtemp
    Should Be True    ${ret}    customVariables_KEY1:Should Be True
    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    SNMPCOMMUNITY    public
    Should Be True    ${ret}    customVariables_SNMPCOMMUNITY:Should Be True
    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    SNMPVERSION    2c
    Should Be True    ${ret}    customVariables_SNMPVERSION:Should Be True

    Ctn Stop Engine
    Ctn Kindly Stop Broker

EHI3
    [Documentation]    Verify inheritance host : host(full) inherit from template (full) , on engine Reload
    [Tags]    broker    engine    hosts    MON-148837
    Ctn Config Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

    Ctn Create Tags File    ${0}    ${40}
    Ctn Create Template File    ${0}    host    group_tags    [1, 6]
    Ctn Create Severities File    ${0}    ${20}

    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Config Engine Add Cfg File    ${0}    hostTemplates.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg
    Ctn Config Engine Add Cfg File    ${0}    severities.cfg
    Ctn Engine Config Add Command
    ...    0
    ...    command_notif
    ...    /usr/bin/true

    # Operation in contact group
    Ctn Add Contact Group    ${0}    ${1}    ["John_Doe"]
    Ctn Add Contact Group    ${0}    ${2}    ["U1","U2"]
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    # Operation in host group
    Ctn Add Host Group    ${0}    ${1}    ["host_2","host_3"]
    Ctn Add Host Group    ${0}    ${2}    ["host_4"]

    # Operation in host
    Ctn Add Template To Hosts    0    host_template_1    [1,3]
    Ctn Add Template To Hosts    0    host_template_2    [2]
    Ctn Engine Config Delete Value In Hosts    0    host_1    alias
    Ctn Engine Config Delete Value In Hosts    0    host_1    check_period
    Ctn Engine Config Delete Value In Hosts    0    host_1    address
    Ctn Engine Config Delete Value In Hosts    0    host_1    _KEY1
    Ctn Engine Config Delete Value In Hosts    0    host_1    _SNMPCOMMUNITY
    Ctn Engine Config Delete Value In Hosts    0    host_1    _SNMPVERSION

    # Operation in template host
    Ctn Engine Config Set Value In Hosts    0    host_template_1    alias    alias_Template_1    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    acknowledgement_timeout
    ...    10
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    address    127.0.0.2    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    parents    host_2    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0
    ...    host_template_1
    ...    hostgroups
    ...    hostgroup_1
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0
    ...    host_template_1
    ...    contact_groups
    ...    contactgroup_1
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    contacts    John_Doe    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    notification_period
    ...    workhours
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    check_command    checkh2    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    check_period    workhours    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    event_handler    command_notif    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    notes    template_note    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    notes_url    template_note_url    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    action_url
    ...    template_action_url
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    icon_image
    ...    template_icon_image
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    icon_image_alt
    ...    template_icon_image_alt
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    vrml_image
    ...    template_vrml_image
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    gd2_image
    ...    template_gd2_image
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    statusmap_image
    ...    template_statusmap_image
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    check_interval    2    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    retry_interval    1    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    recovery_notification_delay
    ...    1
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    max_check_attempts    4    hostTemplates.cfg
    Ctn Engine Config Replace Value In Hosts
    ...    0
    ...    host_template_1
    ...    active_checks_enabled
    ...    0
    ...    hostTemplates.cfg
    Ctn Engine Config Replace Value In Hosts
    ...    0
    ...    host_template_1
    ...    passive_checks_enabled
    ...    0
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    event_handler_enabled
    ...    0
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    check_freshness    1    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    freshness_threshold    123    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    low_flap_threshold    83    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    high_flap_threshold    126    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    flap_detection_enabled    0    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    flap_detection_options    d    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    notification_options    d    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    notifications_enabled    1    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    notification_interval    6    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    first_notification_delay    3    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    stalking_options    all    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    process_perf_data    1    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    2d_coords    250,390    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    3d_coords    4.57,3.98,152    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    obsess_over_host    0    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    retain_status_information    0    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts
    ...    0
    ...    host_template_1
    ...    retain_nonstatus_information
    ...    0
    ...    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    timezone    GMT+02    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    severity_id    10    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    icon_id    1    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    category_tags    2    hostTemplates.cfg

    Ctn Engine Config Set Value In Hosts    0    host_template_1    _KEY1    VALtemp    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    _SNMPCOMMUNITY    public    hostTemplates.cfg
    Ctn Engine Config Set Value In Hosts    0    host_template_1    _SNMPVERSION    2c    hostTemplates.cfg

    ${start}    Ctn Get Round Current Date
    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    Ctn Engine Config Set Value In Hosts    0    host_1    alias    alias_1
    Ctn Engine Config Set Value In Hosts    0    host_1    acknowledgement_timeout    5
    Ctn Engine Config Set Value In Hosts    0    host_1    address    127.0.0.1
    Ctn Engine Config Set Value In Hosts    0    host_1    parents    host_3
    Ctn Engine Config Set Value In Hosts    0    host_1    hostgroups    hostgroup_2
    Ctn Engine Config Set Value In Hosts    0    host_1    contact_groups    contactgroup_2
    Ctn Engine Config Set Value In Hosts    0    host_1    contacts    U1
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_period    none
    Ctn Engine Config Set Value In Hosts    0    host_1    check_period    none
    Ctn Engine Config Set Value In Hosts    0    host_1    event_handler    command_1
    Ctn Engine Config Set Value In Hosts    0    host_1    notes    notes
    Ctn Engine Config Set Value In Hosts    0    host_1    notes_url    notes_url
    Ctn Engine Config Set Value In Hosts    0    host_1    action_url    action_url
    Ctn Engine Config Set Value In Hosts    0    host_1    icon_image    icon_image
    Ctn Engine Config Set Value In Hosts    0    host_1    icon_image_alt    icon_image_alt
    Ctn Engine Config Set Value In Hosts    0    host_1    vrml_image    vrml_image
    Ctn Engine Config Set Value In Hosts    0    host_1    gd2_image    gd2_image
    Ctn Engine Config Set Value In Hosts    0    host_1    statusmap_image    statusmap_image
    Ctn Engine Config Set Value In Hosts    0    host_1    check_interval    3
    Ctn Engine Config Set Value In Hosts    0    host_1    retry_interval    2
    Ctn Engine Config Set Value In Hosts    0    host_1    recovery_notification_delay    2
    Ctn Engine Config Set Value In Hosts    0    host_1    max_check_attempts    8
    Ctn Engine Config Set Value In Hosts    0    host_1    active_checks_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    passive_checks_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    event_handler_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    check_freshness    0
    Ctn Engine Config Set Value In Hosts    0    host_1    freshness_threshold    14
    Ctn Engine Config Set Value In Hosts    0    host_1    low_flap_threshold    53
    Ctn Engine Config Set Value In Hosts    0    host_1    high_flap_threshold    15
    Ctn Engine Config Set Value In Hosts    0    host_1    flap_detection_enabled    1
    Ctn Engine Config Set Value In Hosts    0    host_1    flap_detection_options    up
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_options    r
    Ctn Engine Config Set Value In Hosts    0    host_1    notifications_enabled    0
    Ctn Engine Config Set Value In Hosts    0    host_1    notification_interval    8
    Ctn Engine Config Set Value In Hosts    0    host_1    first_notification_delay    6
    Ctn Engine Config Set Value In Hosts    0    host_1    stalking_options    up
    Ctn Engine Config Set Value In Hosts    0    host_1    process_perf_data    0
    Ctn Engine Config Set Value In Hosts    0    host_1    2d_coords    50,50
    Ctn Engine Config Set Value In Hosts    0    host_1    3d_coords    3.5,400,500
    Ctn Engine Config Set Value In Hosts    0    host_1    obsess_over_host    1
    Ctn Engine Config Set Value In Hosts    0    host_1    retain_status_information    1
    Ctn Engine Config Set Value In Hosts    0    host_1    retain_nonstatus_information    1
    Ctn Engine Config Set Value In Hosts    0    host_1    timezone    GMT+01
    Ctn Engine Config Set Value In Hosts    0    host_1    severity_id    2
    Ctn Engine Config Set Value In Hosts    0    host_1    icon_id    15
    Ctn Engine Config Set Value In Hosts    0    host_1    _KEY1    VAL1
    Ctn Engine Config Set Value In Hosts    0    host_1    _SNMPCOMMUNITY    pu
    Ctn Engine Config Set Value In Hosts    0    host_1    _SNMPVERSION    2v
    Ctn Engine Config Set Value In Hosts    0    host_1    group_tags    6
    Ctn Engine Config Set Value In Hosts    0    host_1    category_tags    7

    ${start}    Ctn Get Round Current Date
    Ctn Reload Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Host Info Grpc    ${1}

    Should Be Equal As Strings    ${output}[name]    host_1    name
    Should Be Equal As Strings    ${output}[alias]    alias_1    alias
    Should Be Equal As Numbers    ${output}[acknowledgementTimeout]    300    acknowledgementTimeout
    Should Be Equal As Strings    ${output}[address]    127.0.0.1    address
    Should Contain    ${output}[parentHosts]    host_3    host_3 is not in [parentHosts]
    Should Contain    ${output}[groupName]    hostgroup_2    hostgroup_2 is not in [groupName]
    Should Contain    ${output}[contactgroups]    contactgroup_2    contactgroup_2 is not in [contactgroups]
    Should Contain    ${output}[contacts]    U1    U1 is not in [contacts]
    Should Be Equal As Strings    ${output}[notificationPeriod]    none    notificationPeriod
    Should Be Equal As Strings    ${output}[checkCommand]    checkh1    checkCommand
    Should Be Equal As Strings    ${output}[checkPeriod]    none    checkPeriod
    Should Be Equal As Strings    ${output}[eventHandler]    command_1    eventHandler
    Should Be Equal As Strings    ${output}[notes]    notes    notes
    Should Be Equal As Strings    ${output}[notesUrl]    notes_url    notesUrl
    Should Be Equal As Strings    ${output}[actionUrl]    action_url    actionUrl
    Should Be Equal As Strings    ${output}[iconImage]    icon_image    iconImage
    Should Be Equal As Strings    ${output}[iconImageAlt]    icon_image_alt    iconImageAlt
    Should Be Equal As Strings    ${output}[vrmlImage]    vrml_image    vrmlImage
    Should Be Equal As Strings    ${output}[statusmapImage]    gd2_image    statusmapImage
    Should Be Equal As Strings    ${output}[initialState]    UP    initialState should take default value "UP"
    Should Be Equal As Numbers    ${output}[checkInterval]    3    checkInterval
    Should Be Equal As Numbers    ${output}[retryInterval]    2.0    retryInterval
    Should Be Equal As Numbers    ${output}[recoveryNotificationDelay]    2    recoveryNotificationDelay
    Should Be Equal As Numbers    ${output}[maxAttempts]    8    maxAttempts
    Should Be True    ${output}[checksEnabled]    checksEnabled:Should Be True
    Should Be True    ${output}[acceptPassiveChecks]    acceptPassiveChecks:Should Be True
    Should Be True    ${output}[eventHandlerEnabled]    eventHandlerEnabled:Should Be True
    Should Not Be True    ${output}[checkFreshness]    checkFreshness:Should Not Be True
    Should Be Equal As Numbers    ${output}[freshnessThreshold]    14    freshnessThreshold
    Should Be Equal As Numbers    ${output}[lowFlapThreshold]    53.0    lowFlapThreshold
    Should Be Equal As Numbers    ${output}[highFlapThreshold]    15.0    highFlapThreshold
    Should Be True    ${output}[flapDetectionEnabled]    flapDetectionEnabled:Should Be True
    Should Be True    ${output}[flapDetectionOnUp]    flapDetectionOnUp:Should Be True
    Should Not Be True    ${output}[flapDetectionOnDown]    flapDetectionOnDown:Should Not Be True
    Should Not Be True    ${output}[flapDetectionOnUnreachable]    flapDetectionOnUnreachable:Should Not Be True
    Should Be True    ${output}[notifyUp]    notifyUp:Should Be True
    Should Not Be True    ${output}[notifyDown]    notifyDown:Should Not Be True
    Should Not Be True    ${output}[notifyUnreachable]    notifyUnreachable:Should Not Be True
    Should Not Be True    ${output}[notifyOnFlappingstart]    notifyOnFlappingstart:Should Not Be True
    Should Not Be True    ${output}[notifyOnFlappingstop]    notifyOnFlappingstop:Should Not Be True
    Should Not Be True    ${output}[notifyOnFlappingdisabled]    notifyOnFlappingdisabled:Should Not Be True
    Should Not Be True    ${output}[notifyDowntime]    notifyDowntime:Should Not Be True
    Should Not Be True    ${output}[notificationsEnabled]    notificationsEnabled:Should Not Be True
    Should Be Equal As Numbers    ${output}[notificationInterval]    8    notificationInterval
    Should Be Equal As Numbers    ${output}[firstNotificationDelay]    6    firstNotificationDelay
    Should Be True    ${output}[stalkOnUp]    stalkOnUp:Should Be True
    Should Not Be True    ${output}[stalkOnDown]    stalkOnDown:Should Not Be True
    Should Not Be True    ${output}[stalkOnUnreachable]    stalkOnUnreachable:Should Not Be True
    Should Not Be True    ${output}[processPerformanceData]    processPerformanceData:Should Not Be True
    Should Be True    ${output}[have2dCoords]    have2dCoords:Should Be True
    Should Be Equal As Numbers    ${output}[x2d]    50    x2d
    Should Be Equal As Numbers    ${output}[y2d]    50    y2d
    Should Be True    ${output}[have3dCoords]    have3dCoords:Should Be True
    Should Be Equal As Numbers    ${output}[x3d]    3.5    x3d
    Should Be Equal As Numbers    ${output}[y3d]    400    y3d
    Should Be Equal As Numbers    ${output}[z3d]    500    z3d
    Should Be True    ${output}[obsessOverHost]    obsessOverHost:Should Be True
    Should Be True    ${output}[retainStatusInformation]    retainStatusInformation:Should Be True
    Should Be True    ${output}[retainNonstatusInformation]    retainNonstatusInformation:Should Be True
    Should Be Equal As Strings    ${output}[timezone]    GMT+01    timezone
    Should Be Equal As Strings    ${output}[severityLevel]    2    severityLevel
    Should Be Equal As Strings    ${output}[severityId]    2    severityId
    Should Contain    ${output}[tag]    id:6,name:tag22,type:1
    Should Contain    ${output}[tag]    id:7,name:tag28,type:3
    Should Be Equal As Strings    ${output}[iconId]    15    iconId
    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    KEY1    VAL1
    Should Be True    ${ret}    customVariables_KEY1:Should Be True
    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    SNMPCOMMUNITY    pu
    Should Be True    ${ret}    customVariables_SNMPCOMMUNITY:Should Be True
    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    SNMPVERSION    2v
    Should Be True    ${ret}    customVariables_SNMPVERSION:Should Be True

    Ctn Stop Engine
    Ctn Kindly Stop Broker
