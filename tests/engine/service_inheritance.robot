*** Settings ***
Documentation       Centreon Engine verify services inheritance.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
ESI0
    [Documentation]    Verify inheritance service : Service(empty) inherit from template (full) , on Start Engine
    [Tags]    broker    engine    service    MON-148837
    Ctn Config Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1
    Ctn Clear Retention

    # Create files :
    Ctn Create Tags File    ${0}    ${40}
    Ctn Create Severities File    ${0}    ${20}
    Ctn Create Template File    ${0}    service    group_tags    [1, 5]

    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true

    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Config Engine Add Cfg File    ${0}    severities.cfg
    Ctn Config Engine Add Cfg File    ${0}    serviceTemplates.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg

    # Operation in contact group
    Ctn Add Contact Group    ${0}    ${1}    ["John_Doe"]
    Ctn Add Contact Group    ${0}    ${2}    ["U1","U2"]
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    # Operation in service group
    Ctn Add Service Group    ${0}    ${1}    ["service_2"]

    # Operation in services :
    Ctn Add Template To Services    0    service_template_1    [1]
    Ctn Engine Config Delete Value In Service    0    service_1    check_command
    Ctn Engine Config Delete Value In Service    0    service_1    check_period
    Ctn Engine Config Delete Value In Service    0    service_1    max_check_attempts
    Ctn Engine Config Delete Value In Service    0    service_1    check_interval
    Ctn Engine Config Delete Value In Service    0    service_1    retry_interval
    Ctn Engine Config Delete Value In Service    0    service_1    active_checks_enabled
    Ctn Engine Config Delete Value In Service    0    service_1    passive_checks_enabled
    Ctn Engine Config Delete Value In Service    0    service_1    retry_interval

    # Operation in serviceTemplates
    ${config_values}    Create Dictionary
    ...    acknowledgement_timeout    10
    ...    description    service_template_d_1
    ...    service_groups    servicegroup_1
    ...    check_command    checkh2
    ...    check_period    workhours
    ...    event_handler    command_notif
    ...    notification_period    workhours
    ...    contact_groups    contactgroup_1
    ...    contacts    John_Doe
    ...    notes    template_note
    ...    notes_url    template_note_url
    ...    action_url    template_action_url
    ...    icon_image    template_icon_image
    ...    icon_image_alt    template_icon_image_alt
    ...    max_check_attempts    4
    ...    check_interval    2
    ...    retry_interval    1
    ...    recovery_notification_delay    1
    ...    active_checks_enabled    1
    ...    passive_checks_enabled    1
    ...    is_volatile    1
    ...    obsess_over_service    1
    ...    event_handler_enabled    1
    ...    check_freshness    1
    ...    freshness_threshold    123
    ...    low_flap_threshold    83
    ...    high_flap_threshold    126
    ...    flap_detection_enabled    1
    ...    flap_detection_options    all
    ...    notification_options    all
    ...    notifications_enabled    1
    ...    notification_interval    6
    ...    first_notification_delay    3
    ...    stalking_options    all
    ...    process_perf_data    1
    ...    retain_status_information    1
    ...    retain_nonstatus_information    1
    ...    timezone    GMT+01
    ...    severity_id    11
    ...    category_tags    2
    ...    icon_id    1
    ...    _SNMPCOMMUNITY    public

    FOR    ${key}    ${value}    IN    &{config_values}
        Ctn Engine Config Set Value In Services
        ...    0
        ...    service_template_1
        ...    ${key}
        ...    ${value}
        ...    serviceTemplates.cfg
    END

    ${start}    Get Current Date
    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Service Info Grpc    ${1}    ${1}

    Should Be Equal As Strings    ${output}[acknowledgementTimeout]    600    acknowledgementTimeout
    Should Contain    ${output}[servicegroups]    servicegroup_1    service_groups
    Should Contain    ${output}[contactgroups]    contactgroup_1    contactgroups
    Should Contain    ${output}[contacts]    John_Doe    contacts
    Should Be Equal As Strings    ${output}[checkCommand]    checkh2    checkCommand
    Should Be Equal As Strings    ${output}[checkPeriod]    workhours    checkPeriod
    Should Be Equal As Strings    ${output}[eventHandler]    command_notif    eventHandler
    Should Be Equal As Strings    ${output}[notificationPeriod]    workhours    notificationPeriod
    Should Be Equal As Strings    ${output}[notes]    template_note    notes
    Should Be Equal As Strings    ${output}[notesUrl]    template_note_url    notesUrl
    Should Be Equal As Strings    ${output}[actionUrl]    template_action_url    actionUrl
    Should Be Equal As Strings    ${output}[iconImage]    template_icon_image    iconImage
    Should Be Equal As Strings    ${output}[iconImageAlt]    template_icon_image_alt    iconImageAlt
    Should Be Equal As Strings    ${output}[initialState]    OK    initialState should take default value "OK"
    Should Be Equal As Numbers    ${output}[maxCheckAttempts]    4    maxCheckAttempts
    Should Be Equal As Numbers    ${output}[checkInterval]    2    checkInterval
    Should Be Equal As Numbers    ${output}[retryInterval]    1.0    retryInterval
    Should Be Equal As Numbers    ${output}[recoveryNotificationDelay]    1    recoveryNotificationDelay
    Should Be True    ${output}[passiveChecksEnabled]    passiveChecksEnabled:Should Be True
    Should Be True    ${output}[activeChecksEnabled]    activeChecksEnabled:Should Not Be True
    Should Be True    ${output}[isVolatile]    isVolatile:Should Be True
    Should Be True    ${output}[obsessOver]    obsessOverService:Should Be True
    Should Be True    ${output}[eventHandlerEnabled]    eventHandlerEnabled:Should Be True
    Should Be True    ${output}[checkFreshnessEnabled]    checkFreshness:Should Be True
    Should Be Equal As Numbers    ${output}[freshnessThreshold]    123    freshnessThreshold
    Should Be Equal As Numbers    ${output}[lowFlapThreshold]    83.0    lowFlapThreshold
    Should Be Equal As Numbers    ${output}[highFlapThreshold]    126.0    highFlapThreshold
    Should Be True    ${output}[flapDetectionEnabled]    flapDetectionEnabled:Should Be True
    Should Be True    ${output}[flapDetectionOnWarning]    flapDetectionOnWarning:Should Be True
    Should Be True    ${output}[flapDetectionOnUnknown]    flapDetectionOnUnknown:Should Be True
    Should Be True    ${output}[flapDetectionOnCritical]    flapDetectionOnCritical:Should Be True
    Should Be True    ${output}[flapDetectionOnOk]    flapDetectionOnOk:Should Be True
    Should Be True    ${output}[notifyOnUnknown]    notifyOnUnknown:Should Be True
    Should Be True    ${output}[notifyOnWarning]    notifyOnWarning:Should Be True
    Should Be True    ${output}[notifyOnOk]    notifyOnOk:Should Be True
    Should Be True    ${output}[notifyOnFlappingstart]    notifyOnFlappingstart:Should Be True
    Should Be True    ${output}[notifyOnFlappingstop]    notifyOnFlappingstop:Should Be True
    Should Be True    ${output}[notifyOnFlappingdisabled]    notifyOnFlappingdisabled:Should Be True
    Should Be True    ${output}[notifyOnDowntime]    notifyDowntime:Should Be True
    Should Be True    ${output}[notificationsEnabled]    notificationsEnabled:Should Be True
    Should Be Equal As Numbers    ${output}[notificationInterval]    6    notificationInterval
    Should Be Equal As Numbers    ${output}[firstNotificationDelay]    3    firstNotificationDelay
    Should Be True    ${output}[stalkOnOk]    stalkOnOk:Should Be True
    Should Be True    ${output}[stalkOnUnknown]    stalkOnUnknown:Should Be True
    Should Be True    ${output}[stalkOnWarning]    stalkOnWarning:Should Be True
    Should Be True    ${output}[stalkOnCritical]    stalkOnCritical:Should Be True
    Should Be True    ${output}[processPerformanceData]    processPerformanceData:Should Be True
    Should Be Equal As Numbers    ${output}[retainStatusInformation]    1    retainStatusInformation
    Should Be True    ${output}[retainNonstatusInformation]    retainNonstatusInformation:Should Not Be True
    Should Be Equal As Strings    ${output}[timezone]    GMT+01    timezone
    Should Be Equal As Strings    ${output}[severityLevel]    1    severityLevel
    Should Be Equal As Strings    ${output}[severityId]    11    severityId
    Should Contain    ${output}[tag]    id:1,name:tag1,type:0
    Should Contain    ${output}[tag]    id:2,name:tag7,type:2
    Should Be Equal As Strings    ${output}[iconId]    1    iconId

    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    KEY_SERV1_1    VAL_SERV1
    Should Be True    ${ret}    customVariables_KEY_SERV1_1:Should Be True
    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    SNMPCOMMUNITY    public
    Should Be True    ${ret}    customVariables_SNMPCOMMUNITY:Should Be True

    Ctn Stop Engine
    Ctn Kindly Stop Broker

ESI1
    [Documentation]    Verify inheritance service : Service(full) inherit from template (full) , on Start Engine
    [Tags]    broker    engine    service    MON-148837
    Ctn Config Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1
    Ctn Clear Retention

    # Create files :
    Ctn Create Tags File    ${0}    ${40}
    Ctn Create Severities File    ${0}    ${20}
    Ctn Create Template File    ${0}    service    group_tags    [1, 5]

    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true

    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Config Engine Add Cfg File    ${0}    severities.cfg
    Ctn Config Engine Add Cfg File    ${0}    serviceTemplates.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg

    # Operation in contact group
    Ctn Add Contact Group    ${0}    ${1}    ["John_Doe"]
    Ctn Add Contact Group    ${0}    ${2}    ["U1","U2"]
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    # Operation in service group
    Ctn Add Service Group    ${0}    ${1}    ["service_2"]
    Ctn Add Service Group    ${0}    ${2}    ["service_3"]

    # Operation in services :
    Ctn Add Template To Services    0    service_template_1    [1]
    Ctn Engine Config Delete Value In Service    0    service_1    check_command
    Ctn Engine Config Delete Value In Service    0    service_1    check_period
    Ctn Engine Config Delete Value In Service    0    service_1    max_check_attempts
    Ctn Engine Config Delete Value In Service    0    service_1    check_interval
    Ctn Engine Config Delete Value In Service    0    service_1    retry_interval
    Ctn Engine Config Delete Value In Service    0    service_1    active_checks_enabled
    Ctn Engine Config Delete Value In Service    0    service_1    passive_checks_enabled
    Ctn Engine Config Delete Value In Service    0    service_1    retry_interval

    ${config_values}    Create Dictionary
    ...    acknowledgement_timeout    10
    ...    service_groups    servicegroup_1
    ...    check_command    checkh2
    ...    check_period    workhours
    ...    event_handler    command_notif
    ...    notification_period    workhours
    ...    contact_groups    contactgroup_1
    ...    contacts    John_Doe
    ...    notes    note
    ...    notes_url    note_url
    ...    action_url    action_url
    ...    icon_image    icon_image
    ...    icon_image_alt    icon_image_alt
    ...    max_check_attempts    4
    ...    check_interval    2
    ...    retry_interval    1
    ...    recovery_notification_delay    1
    ...    active_checks_enabled    1
    ...    passive_checks_enabled    1
    ...    is_volatile    1
    ...    obsess_over_service    1
    ...    event_handler_enabled    1
    ...    check_freshness    1
    ...    freshness_threshold    123
    ...    low_flap_threshold    83
    ...    high_flap_threshold    126
    ...    flap_detection_enabled    1
    ...    flap_detection_options    all
    ...    notification_options    all
    ...    notifications_enabled    1
    ...    notification_interval    6
    ...    first_notification_delay    3
    ...    stalking_options    all
    ...    process_perf_data    1
    ...    retain_status_information    1
    ...    retain_nonstatus_information    1
    ...    timezone    GMT+01
    ...    severity_id    11
    ...    category_tags    2
    ...    icon_id    1
    ...    _SNMPCOMMUNITY    public

    FOR    ${key}    ${value}    IN    &{config_values}
        Ctn Engine Config Set Value In Services
        ...    0
        ...    service_1
        ...    ${key}
        ...    ${value}
        ...    services.cfg
    END

    # Operation in serviceTemplates
    ${config_values_tmpl}    Create Dictionary
    ...    acknowledgement_timeout    5
    ...    service_groups    servicegroup_2
    ...    check_command    checkh3
    ...    check_period    none
    ...    event_handler    checkh4
    ...    notification_period    none
    ...    contact_groups    contactgroup_2
    ...    contacts    U1
    ...    notes    template_note
    ...    notes_url    template_note_url
    ...    action_url    template_action_url
    ...    icon_image    template_icon_image
    ...    icon_image_alt    template_icon_image_alt
    ...    max_check_attempts    2
    ...    check_interval    1
    ...    retry_interval    2
    ...    recovery_notification_delay    2
    ...    active_checks_enabled    0
    ...    passive_checks_enabled    0
    ...    is_volatile    0
    ...    obsess_over_service    0
    ...    event_handler_enabled    0
    ...    check_freshness    0
    ...    freshness_threshold    23
    ...    low_flap_threshold    17
    ...    high_flap_threshold    17
    ...    flap_detection_enabled    0
    ...    flap_detection_options    warning
    ...    notification_options    warning
    ...    notifications_enabled    0
    ...    notification_interval    2
    ...    first_notification_delay    1
    ...    stalking_options    warning
    ...    process_perf_data    0
    ...    retain_status_information    0
    ...    retain_nonstatus_information    0
    ...    timezone    GMT+02
    ...    severity_id    13
    ...    category_tags    1
    ...    icon_id    10
    ...    _SNMPCOMMUNITY    private

    FOR    ${key}    ${value}    IN    &{config_values_tmpl}
        Ctn Engine Config Set Value In Services
        ...    0
        ...    service_template_1
        ...    ${key}
        ...    ${value}
        ...    serviceTemplates.cfg
    END

    ${start}    Get Current Date
    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Service Info Grpc    ${1}    ${1}

    Should Be Equal As Strings    ${output}[acknowledgementTimeout]    600    acknowledgementTimeout
    Should Contain    ${output}[servicegroups]    servicegroup_1    service_groups
    Should Contain    ${output}[contactgroups]    contactgroup_1    contactgroups
    Should Contain    ${output}[contacts]    John_Doe    contacts
    Should Be Equal As Strings    ${output}[checkCommand]    checkh2    checkCommand
    Should Be Equal As Strings    ${output}[checkPeriod]    workhours    checkPeriod
    Should Be Equal As Strings    ${output}[eventHandler]    command_notif    eventHandler
    Should Be Equal As Strings    ${output}[notificationPeriod]    workhours    notificationPeriod
    Should Be Equal As Strings    ${output}[notes]    note    notes
    Should Be Equal As Strings    ${output}[notesUrl]    note_url    notesUrl
    Should Be Equal As Strings    ${output}[actionUrl]    action_url    actionUrl
    Should Be Equal As Strings    ${output}[iconImage]    icon_image    iconImage
    Should Be Equal As Strings    ${output}[iconImageAlt]    icon_image_alt    iconImageAlt
    Should Be Equal As Strings    ${output}[initialState]    OK    initialState should take default value "OK"
    Should Be Equal As Numbers    ${output}[maxCheckAttempts]    4    maxCheckAttempts
    Should Be Equal As Numbers    ${output}[checkInterval]    2    checkInterval
    Should Be Equal As Numbers    ${output}[retryInterval]    1.0    retryInterval
    Should Be Equal As Numbers    ${output}[recoveryNotificationDelay]    1    recoveryNotificationDelay
    Should Be True    ${output}[passiveChecksEnabled]    passiveChecksEnabled:Should Be True
    Should Be True    ${output}[activeChecksEnabled]    activeChecksEnabled:Should Not Be True
    Should Be True    ${output}[isVolatile]    isVolatile:Should Be True
    Should Be True    ${output}[obsessOver]    obsessOverService:Should Be True
    Should Be True    ${output}[eventHandlerEnabled]    eventHandlerEnabled:Should Be True
    Should Be True    ${output}[checkFreshnessEnabled]    checkFreshness:Should Be True
    Should Be Equal As Numbers    ${output}[freshnessThreshold]    123    freshnessThreshold
    Should Be Equal As Numbers    ${output}[lowFlapThreshold]    83.0    lowFlapThreshold
    Should Be Equal As Numbers    ${output}[highFlapThreshold]    126.0    highFlapThreshold
    Should Be True    ${output}[flapDetectionEnabled]    flapDetectionEnabled:Should Be True
    Should Be True    ${output}[flapDetectionOnWarning]    flapDetectionOnWarning:Should Be True
    Should Be True    ${output}[flapDetectionOnUnknown]    flapDetectionOnUnknown:Should Be True
    Should Be True    ${output}[flapDetectionOnCritical]    flapDetectionOnCritical:Should Be True
    Should Be True    ${output}[flapDetectionOnOk]    flapDetectionOnOk:Should Be True
    Should Be True    ${output}[notifyOnUnknown]    notifyOnUnknown:Should Be True
    Should Be True    ${output}[notifyOnWarning]    notifyOnWarning:Should Be True
    Should Be True    ${output}[notifyOnOk]    notifyOnOk:Should Be True
    Should Be True    ${output}[notifyOnFlappingstart]    notifyOnFlappingstart:Should Be True
    Should Be True    ${output}[notifyOnFlappingstop]    notifyOnFlappingstop:Should Be True
    Should Be True    ${output}[notifyOnFlappingdisabled]    notifyOnFlappingdisabled:Should Be True
    Should Be True    ${output}[notifyOnDowntime]    notifyDowntime:Should Be True
    Should Be True    ${output}[notificationsEnabled]    notificationsEnabled:Should Be True
    Should Be Equal As Numbers    ${output}[notificationInterval]    6    notificationInterval
    Should Be Equal As Numbers    ${output}[firstNotificationDelay]    3    firstNotificationDelay
    Should Be True    ${output}[stalkOnOk]    stalkOnOk:Should Be True
    Should Be True    ${output}[stalkOnUnknown]    stalkOnUnknown:Should Be True
    Should Be True    ${output}[stalkOnWarning]    stalkOnWarning:Should Be True
    Should Be True    ${output}[stalkOnCritical]    stalkOnCritical:Should Be True
    Should Be True    ${output}[processPerformanceData]    processPerformanceData:Should Be True
    Should Be Equal As Numbers    ${output}[retainStatusInformation]    1    retainStatusInformation
    Should Be True    ${output}[retainNonstatusInformation]    retainNonstatusInformation:Should Not Be True
    Should Be Equal As Strings    ${output}[timezone]    GMT+01    timezone
    Should Be Equal As Strings    ${output}[severityLevel]    1    severityLevel
    Should Be Equal As Strings    ${output}[severityId]    11    severityId
    Should Contain    ${output}[tag]    id:1,name:tag1,type:0
    Should Contain    ${output}[tag]    id:2,name:tag7,type:2
    Should Be Equal As Strings    ${output}[iconId]    1    iconId

    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    KEY_SERV1_1    VAL_SERV1
    Should Be True    ${ret}    customVariables_KEY_SERV1_1:Should Be True
    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    SNMPCOMMUNITY    public
    Should Be True    ${ret}    customVariables_SNMPCOMMUNITY:Should Be True

    Ctn Stop Engine
    Ctn Kindly Stop Broker

ESI2
    [Documentation]    Verify inheritance service : Service(empty) inherit from template (full) , on Reload Engine
    [Tags]    broker    engine    service    MON-148837
    Ctn Config Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

    # Create files :
    Ctn Create Tags File    ${0}    ${40}
    Ctn Create Severities File    ${0}    ${20}
    Ctn Create Template File    ${0}    service    group_tags    [1, 5]

    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true

    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Config Engine Add Cfg File    ${0}    severities.cfg
    Ctn Config Engine Add Cfg File    ${0}    serviceTemplates.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg

    # Operation in contact group
    Ctn Add Contact Group    ${0}    ${1}    ["John_Doe"]
    Ctn Add Contact Group    ${0}    ${2}    ["U1","U2"]
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    # Operation in service group
    Ctn Add Service Group    ${0}    ${1}    ["service_2"]

    ${start}    Get Current Date
    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # Operation in services :
    Ctn Add Template To Services    0    service_template_1    [1]
    Ctn Engine Config Delete Value In Service    0    service_1    check_command
    Ctn Engine Config Delete Value In Service    0    service_1    check_period
    Ctn Engine Config Delete Value In Service    0    service_1    max_check_attempts
    Ctn Engine Config Delete Value In Service    0    service_1    check_interval
    Ctn Engine Config Delete Value In Service    0    service_1    retry_interval
    Ctn Engine Config Delete Value In Service    0    service_1    active_checks_enabled
    Ctn Engine Config Delete Value In Service    0    service_1    passive_checks_enabled
    Ctn Engine Config Delete Value In Service    0    service_1    retry_interval

    # Operation in serviceTemplates
    ${config_values}    Create Dictionary
    ...    acknowledgement_timeout    10
    ...    description    service_template_d_1
    ...    service_groups    servicegroup_1
    ...    check_command    checkh2
    ...    check_period    workhours
    ...    event_handler    command_notif
    ...    notification_period    workhours
    ...    contact_groups    contactgroup_1
    ...    contacts    John_Doe
    ...    notes    template_note
    ...    notes_url    template_note_url
    ...    action_url    template_action_url
    ...    icon_image    template_icon_image
    ...    icon_image_alt    template_icon_image_alt
    ...    max_check_attempts    4
    ...    check_interval    2
    ...    retry_interval    1
    ...    recovery_notification_delay    1
    ...    active_checks_enabled    1
    ...    passive_checks_enabled    1
    ...    is_volatile    1
    ...    obsess_over_service    1
    ...    event_handler_enabled    1
    ...    check_freshness    1
    ...    freshness_threshold    123
    ...    low_flap_threshold    83
    ...    high_flap_threshold    126
    ...    flap_detection_enabled    1
    ...    flap_detection_options    all
    ...    notification_options    all
    ...    notifications_enabled    1
    ...    notification_interval    6
    ...    first_notification_delay    3
    ...    stalking_options    all
    ...    process_perf_data    1
    ...    retain_status_information    1
    ...    retain_nonstatus_information    1
    ...    timezone    GMT+01
    ...    severity_id    11
    ...    category_tags    2
    ...    icon_id    1
    ...    _SNMPCOMMUNITY    public

    FOR    ${key}    ${value}    IN    &{config_values}
        Ctn Engine Config Set Value In Services
        ...    0
        ...    service_template_1
        ...    ${key}
        ...    ${value}
        ...    serviceTemplates.cfg
    END

    ${start}    Get Current Date
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
    ${output}    Ctn Get Service Info Grpc    ${1}    ${1}

    Should Be Equal As Strings    ${output}[acknowledgementTimeout]    600    acknowledgementTimeout
    Should Contain    ${output}[servicegroups]    servicegroup_1    service_groups
    Should Contain    ${output}[contactgroups]    contactgroup_1    contactgroups
    Should Contain    ${output}[contacts]    John_Doe    contacts
    Should Be Equal As Strings    ${output}[checkCommand]    checkh2    checkCommand
    Should Be Equal As Strings    ${output}[checkPeriod]    workhours    checkPeriod
    Should Be Equal As Strings    ${output}[eventHandler]    command_notif    eventHandler
    Should Be Equal As Strings    ${output}[notificationPeriod]    workhours    notificationPeriod
    Should Be Equal As Strings    ${output}[notes]    template_note    notes
    Should Be Equal As Strings    ${output}[notesUrl]    template_note_url    notesUrl
    Should Be Equal As Strings    ${output}[actionUrl]    template_action_url    actionUrl
    Should Be Equal As Strings    ${output}[iconImage]    template_icon_image    iconImage
    Should Be Equal As Strings    ${output}[iconImageAlt]    template_icon_image_alt    iconImageAlt
    Should Be Equal As Strings    ${output}[initialState]    OK    initialState should take default value "OK"
    Should Be Equal As Numbers    ${output}[maxCheckAttempts]    4    maxCheckAttempts
    Should Be Equal As Numbers    ${output}[checkInterval]    2    checkInterval
    Should Be Equal As Numbers    ${output}[retryInterval]    1.0    retryInterval
    Should Be Equal As Numbers    ${output}[recoveryNotificationDelay]    1    recoveryNotificationDelay
    Should Be True    ${output}[passiveChecksEnabled]    passiveChecksEnabled:Should Be True
    Should Be True    ${output}[activeChecksEnabled]    activeChecksEnabled:Should Not Be True
    Should Be True    ${output}[isVolatile]    isVolatile:Should Be True
    Should Be True    ${output}[obsessOver]    obsessOverService:Should Be True
    Should Be True    ${output}[eventHandlerEnabled]    eventHandlerEnabled:Should Be True
    Should Be True    ${output}[checkFreshnessEnabled]    checkFreshness:Should Be True
    Should Be Equal As Numbers    ${output}[freshnessThreshold]    123    freshnessThreshold
    Should Be Equal As Numbers    ${output}[lowFlapThreshold]    83.0    lowFlapThreshold
    Should Be Equal As Numbers    ${output}[highFlapThreshold]    126.0    highFlapThreshold
    Should Be True    ${output}[flapDetectionEnabled]    flapDetectionEnabled:Should Be True
    Should Be True    ${output}[flapDetectionOnWarning]    flapDetectionOnWarning:Should Be True
    Should Be True    ${output}[flapDetectionOnUnknown]    flapDetectionOnUnknown:Should Be True
    Should Be True    ${output}[flapDetectionOnCritical]    flapDetectionOnCritical:Should Be True
    Should Be True    ${output}[flapDetectionOnOk]    flapDetectionOnOk:Should Be True
    Should Be True    ${output}[notifyOnUnknown]    notifyOnUnknown:Should Be True
    Should Be True    ${output}[notifyOnWarning]    notifyOnWarning:Should Be True
    Should Be True    ${output}[notifyOnOk]    notifyOnOk:Should Be True
    Should Be True    ${output}[notifyOnFlappingstart]    notifyOnFlappingstart:Should Be True
    Should Be True    ${output}[notifyOnFlappingstop]    notifyOnFlappingstop:Should Be True
    Should Be True    ${output}[notifyOnFlappingdisabled]    notifyOnFlappingdisabled:Should Be True
    Should Be True    ${output}[notifyOnDowntime]    notifyDowntime:Should Be True
    Should Be True    ${output}[notificationsEnabled]    notificationsEnabled:Should Be True
    Should Be Equal As Numbers    ${output}[notificationInterval]    6    notificationInterval
    Should Be Equal As Numbers    ${output}[firstNotificationDelay]    3    firstNotificationDelay
    Should Be True    ${output}[stalkOnOk]    stalkOnOk:Should Be True
    Should Be True    ${output}[stalkOnUnknown]    stalkOnUnknown:Should Be True
    Should Be True    ${output}[stalkOnWarning]    stalkOnWarning:Should Be True
    Should Be True    ${output}[stalkOnCritical]    stalkOnCritical:Should Be True
    Should Be True    ${output}[processPerformanceData]    processPerformanceData:Should Be True
    Should Be Equal As Numbers    ${output}[retainStatusInformation]    1    retainStatusInformation
    Should Be True    ${output}[retainNonstatusInformation]    retainNonstatusInformation:Should Not Be True
    Should Be Equal As Strings    ${output}[timezone]    GMT+01    timezone
    Should Be Equal As Strings    ${output}[severityLevel]    1    severityLevel
    Should Be Equal As Strings    ${output}[severityId]    11    severityId
    Should Contain    ${output}[tag]    id:1,name:tag1,type:0
    Should Contain    ${output}[tag]    id:2,name:tag7,type:2
    Should Be Equal As Strings    ${output}[iconId]    1    iconId

    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    KEY_SERV1_1    VAL_SERV1
    Should Be True    ${ret}    customVariables_KEY_SERV1_1:Should Be True
    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    SNMPCOMMUNITY    public
    Should Be True    ${ret}    customVariables_SNMPCOMMUNITY:Should Be True

    Ctn Stop Engine
    Ctn Kindly Stop Broker

ESI3
    [Documentation]    Verify inheritance service : Service(full) inherit from template (full) , on Reload Engine
    [Tags]    broker    engine    service    MON-148837
    Ctn Config Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Config BBDO3    1

    # Create files :
    Ctn Create Tags File    ${0}    ${40}
    Ctn Create Severities File    ${0}    ${20}
    Ctn Create Template File    ${0}    service    group_tags    [1, 5]

    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true

    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    tags.cfg
    Ctn Config Engine Add Cfg File    ${0}    severities.cfg
    Ctn Config Engine Add Cfg File    ${0}    serviceTemplates.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg
    Ctn Config Engine Add Cfg File    ${0}    servicegroups.cfg

    # Operation in contact group
    Ctn Add Contact Group    ${0}    ${1}    ["John_Doe"]
    Ctn Add Contact Group    ${0}    ${2}    ["U1","U2"]
    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    command_notif
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    command_notif

    # Operation in service group
    Ctn Add Service Group    ${0}    ${1}    ["service_2"]
    Ctn Add Service Group    ${0}    ${2}    ["service_3"]

    ${start}    Get Current Date
    Ctn Clear Retention
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # Operation in services :
    Ctn Add Template To Services    0    service_template_1    [1]
    Ctn Engine Config Delete Value In Service    0    service_1    check_command
    Ctn Engine Config Delete Value In Service    0    service_1    check_period
    Ctn Engine Config Delete Value In Service    0    service_1    max_check_attempts
    Ctn Engine Config Delete Value In Service    0    service_1    check_interval
    Ctn Engine Config Delete Value In Service    0    service_1    retry_interval
    Ctn Engine Config Delete Value In Service    0    service_1    active_checks_enabled
    Ctn Engine Config Delete Value In Service    0    service_1    passive_checks_enabled
    Ctn Engine Config Delete Value In Service    0    service_1    retry_interval

    ${config_values}    Create Dictionary
    ...    acknowledgement_timeout    10
    ...    service_groups    servicegroup_1
    ...    check_command    checkh2
    ...    check_period    workhours
    ...    event_handler    command_notif
    ...    notification_period    workhours
    ...    contact_groups    contactgroup_1
    ...    contacts    John_Doe
    ...    notes    note
    ...    notes_url    note_url
    ...    action_url    action_url
    ...    icon_image    icon_image
    ...    icon_image_alt    icon_image_alt
    ...    max_check_attempts    4
    ...    check_interval    2
    ...    retry_interval    1
    ...    recovery_notification_delay    1
    ...    active_checks_enabled    1
    ...    passive_checks_enabled    1
    ...    is_volatile    1
    ...    obsess_over_service    1
    ...    event_handler_enabled    1
    ...    check_freshness    1
    ...    freshness_threshold    123
    ...    low_flap_threshold    83
    ...    high_flap_threshold    126
    ...    flap_detection_enabled    1
    ...    flap_detection_options    all
    ...    notification_options    all
    ...    notifications_enabled    1
    ...    notification_interval    6
    ...    first_notification_delay    3
    ...    stalking_options    all
    ...    process_perf_data    1
    ...    retain_status_information    1
    ...    retain_nonstatus_information    1
    ...    timezone    GMT+01
    ...    severity_id    11
    ...    category_tags    2
    ...    icon_id    1
    ...    _SNMPCOMMUNITY    public

    FOR    ${key}    ${value}    IN    &{config_values}
        Ctn Engine Config Set Value In Services
        ...    0
        ...    service_1
        ...    ${key}
        ...    ${value}
        ...    services.cfg
    END

    # Operation in serviceTemplates
    ${config_values_tmpl}    Create Dictionary
    ...    acknowledgement_timeout    5
    ...    service_groups    servicegroup_2
    ...    check_command    checkh3
    ...    check_period    none
    ...    event_handler    checkh4
    ...    notification_period    none
    ...    contact_groups    contactgroup_2
    ...    contacts    U1
    ...    notes    template_note
    ...    notes_url    template_note_url
    ...    action_url    template_action_url
    ...    icon_image    template_icon_image
    ...    icon_image_alt    template_icon_image_alt
    ...    max_check_attempts    2
    ...    check_interval    1
    ...    retry_interval    2
    ...    recovery_notification_delay    2
    ...    active_checks_enabled    0
    ...    passive_checks_enabled    0
    ...    is_volatile    0
    ...    obsess_over_service    0
    ...    event_handler_enabled    0
    ...    check_freshness    0
    ...    freshness_threshold    23
    ...    low_flap_threshold    17
    ...    high_flap_threshold    17
    ...    flap_detection_enabled    0
    ...    flap_detection_options    warning
    ...    notification_options    warning
    ...    notifications_enabled    0
    ...    notification_interval    2
    ...    first_notification_delay    1
    ...    stalking_options    warning
    ...    process_perf_data    0
    ...    retain_status_information    0
    ...    retain_nonstatus_information    0
    ...    timezone    GMT+02
    ...    severity_id    13
    ...    category_tags    1
    ...    icon_id    10
    ...    _SNMPCOMMUNITY    private

    FOR    ${key}    ${value}    IN    &{config_values_tmpl}
        Ctn Engine Config Set Value In Services
        ...    0
        ...    service_template_1
        ...    ${key}
        ...    ${value}
        ...    serviceTemplates.cfg
    END

    ${start}    Get Current Date
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

    ${output}    Ctn Get Service Info Grpc    ${1}    ${1}

    Should Be Equal As Strings    ${output}[acknowledgementTimeout]    600    acknowledgementTimeout
    Should Contain    ${output}[servicegroups]    servicegroup_1    service_groups
    Should Contain    ${output}[contactgroups]    contactgroup_1    contactgroups
    Should Contain    ${output}[contacts]    John_Doe    contacts
    Should Be Equal As Strings    ${output}[checkCommand]    checkh2    checkCommand
    Should Be Equal As Strings    ${output}[checkPeriod]    workhours    checkPeriod
    Should Be Equal As Strings    ${output}[eventHandler]    command_notif    eventHandler
    Should Be Equal As Strings    ${output}[notificationPeriod]    workhours    notificationPeriod
    Should Be Equal As Strings    ${output}[notes]    note    notes
    Should Be Equal As Strings    ${output}[notesUrl]    note_url    notesUrl
    Should Be Equal As Strings    ${output}[actionUrl]    action_url    actionUrl
    Should Be Equal As Strings    ${output}[iconImage]    icon_image    iconImage
    Should Be Equal As Strings    ${output}[iconImageAlt]    icon_image_alt    iconImageAlt
    Should Be Equal As Strings    ${output}[initialState]    OK    initialState should take default value "OK"
    Should Be Equal As Numbers    ${output}[maxCheckAttempts]    4    maxCheckAttempts
    Should Be Equal As Numbers    ${output}[checkInterval]    2    checkInterval
    Should Be Equal As Numbers    ${output}[retryInterval]    1.0    retryInterval
    Should Be Equal As Numbers    ${output}[recoveryNotificationDelay]    1    recoveryNotificationDelay
    Should Be True    ${output}[passiveChecksEnabled]    passiveChecksEnabled:Should Be True
    Should Be True    ${output}[activeChecksEnabled]    activeChecksEnabled:Should Not Be True
    Should Be True    ${output}[isVolatile]    isVolatile:Should Be True
    Should Be True    ${output}[obsessOver]    obsessOverService:Should Be True
    Should Be True    ${output}[eventHandlerEnabled]    eventHandlerEnabled:Should Be True
    Should Be True    ${output}[checkFreshnessEnabled]    checkFreshness:Should Be True
    Should Be Equal As Numbers    ${output}[freshnessThreshold]    123    freshnessThreshold
    Should Be Equal As Numbers    ${output}[lowFlapThreshold]    83.0    lowFlapThreshold
    Should Be Equal As Numbers    ${output}[highFlapThreshold]    126.0    highFlapThreshold
    Should Be True    ${output}[flapDetectionEnabled]    flapDetectionEnabled:Should Be True
    Should Be True    ${output}[flapDetectionOnWarning]    flapDetectionOnWarning:Should Be True
    Should Be True    ${output}[flapDetectionOnUnknown]    flapDetectionOnUnknown:Should Be True
    Should Be True    ${output}[flapDetectionOnCritical]    flapDetectionOnCritical:Should Be True
    Should Be True    ${output}[flapDetectionOnOk]    flapDetectionOnOk:Should Be True
    Should Be True    ${output}[notifyOnUnknown]    notifyOnUnknown:Should Be True
    Should Be True    ${output}[notifyOnWarning]    notifyOnWarning:Should Be True
    Should Be True    ${output}[notifyOnOk]    notifyOnOk:Should Be True
    Should Be True    ${output}[notifyOnFlappingstart]    notifyOnFlappingstart:Should Be True
    Should Be True    ${output}[notifyOnFlappingstop]    notifyOnFlappingstop:Should Be True
    Should Be True    ${output}[notifyOnFlappingdisabled]    notifyOnFlappingdisabled:Should Be True
    Should Be True    ${output}[notifyOnDowntime]    notifyDowntime:Should Be True
    Should Be True    ${output}[notificationsEnabled]    notificationsEnabled:Should Be True
    Should Be Equal As Numbers    ${output}[notificationInterval]    6    notificationInterval
    Should Be Equal As Numbers    ${output}[firstNotificationDelay]    3    firstNotificationDelay
    Should Be True    ${output}[stalkOnOk]    stalkOnOk:Should Be True
    Should Be True    ${output}[stalkOnUnknown]    stalkOnUnknown:Should Be True
    Should Be True    ${output}[stalkOnWarning]    stalkOnWarning:Should Be True
    Should Be True    ${output}[stalkOnCritical]    stalkOnCritical:Should Be True
    Should Be True    ${output}[processPerformanceData]    processPerformanceData:Should Be True
    Should Be Equal As Numbers    ${output}[retainStatusInformation]    1    retainStatusInformation
    Should Be True    ${output}[retainNonstatusInformation]    retainNonstatusInformation:Should Not Be True
    Should Be Equal As Strings    ${output}[timezone]    GMT+01    timezone
    Should Be Equal As Strings    ${output}[severityLevel]    1    severityLevel
    Should Be Equal As Strings    ${output}[severityId]    11    severityId
    Should Contain    ${output}[tag]    id:1,name:tag1,type:0
    Should Contain    ${output}[tag]    id:2,name:tag7,type:2
    Should Be Equal As Strings    ${output}[iconId]    1    iconId

    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    KEY_SERV1_1    VAL_SERV1
    Should Be True    ${ret}    customVariables_KEY_SERV1_1:Should Be True
    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    SNMPCOMMUNITY    public
    Should Be True    ${ret}    customVariables_SNMPCOMMUNITY:Should Be True

    Ctn Stop Engine
    Ctn Kindly Stop Broker
