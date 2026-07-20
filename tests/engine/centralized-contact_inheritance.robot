*** Settings ***
Documentation       Centreon Engine verify contacts inheritance with centralized configuration.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Save Logs If Failed


*** Test Cases ***
CECI0
    [Documentation]    Given a centralized Engine configuration where John_Doe is empty and inherits from a full contact template
    ...    And the template defines all notification settings, addresses, and custom variables
    ...    When Engine and Broker are started
    ...    Then John_Doe resolves with all values from the template
    [Tags]    engine    contact    MON-151074
    Ctn Config Centralized Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Clear Retention

    # Create files :
    Ctn Create Template File    ${0}    contact    address1    ["dummy_address_1"]

    # Add Command :
    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true

    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    contactTemplates.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg

    # Operation in contact group
    Ctn Add Contact Group    ${0}    ${1}    ["U1"]
    Ctn Add Contact Group    ${0}    ${2}    ["U1","U2"]

    # Operation in Contact :
    Ctn Add Template To Contact    0    contact_template_1    [John_Doe]

    Ctn Engine Config Delete Value In Contact    0    John_Doe    alias
    Ctn Engine Config Delete Value In Contact    0    John_Doe    email
    Ctn Engine Config Delete Value In Contact    0    John_Doe    host_notification_period
    Ctn Engine Config Delete Value In Contact    0    John_Doe    service_notification_period
    Ctn Engine Config Delete Value In Contact    0    John_Doe    host_notification_options
    Ctn Engine Config Delete Value In Contact    0    John_Doe    service_notification_options
    Ctn Engine Config Delete Value In Contact    0    John_Doe    host_notifications_enabled
    Ctn Engine Config Delete Value In Contact    0    John_Doe    service_notifications_enabled

    # Delete unnecessary fields:
    Ctn Engine Config Delete Value In Contact    0    contact_template_1    active_checks_enabled    contactTemplates.cfg
    Ctn Engine Config Delete Value In Contact    0    contact_template_1    passive_checks_enabled    contactTemplates.cfg

    # Operation in contactTemplates
    ${config_values}    Create Dictionary
    ...    alias    contact_template_d_1
    ...    contact_groups    contactgroup_1
    ...    email    template@gmail.com
    ...    pager    templatepager
    ...    host_notification_period    workhours
    ...    host_notification_commands    command_notif
    ...    service_notification_period    workhours
    ...    service_notification_commands    command_notif
    ...    host_notification_options    all
    ...    service_notification_options    all
    ...    host_notifications_enabled    1
    ...    service_notifications_enabled    1
    ...    can_submit_commands    1
    ...    retain_status_information    1
    ...    retain_nonstatus_information    1
    ...    timezone    GMT+01
    ...    address2    dummy_address_2
    ...    address3    dummy_address_3
    ...    address4    dummy_address_4
    ...    address5    dummy_address_5
    ...    address6    dummy_address_6
    ...    _SNMPCOMMUNITY    public

    FOR    ${key}    ${value}    IN    &{config_values}
        Ctn Engine Config Set Value In Contacts    0    contact_template_1    ${key}    ${value}    contactTemplates.cfg
    END

    ${start}    Ctn Get Round Current Date
    Ctn Clear Retention
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Contact Info Grpc    John_Doe

    Should Be Equal As Strings    ${output}[alias]    John_Doe    alias
    Should Contain    ${output}[contactGroups]    contactgroup_1    contact_groups
    Should Be Equal As Strings    ${output}[email]    template@gmail.com    email
    Should Be Equal As Strings    ${output}[pager]    templatepager    pager
    Should Be Equal As Strings    ${output}[hostNotificationPeriod]    workhours    host_notification_period
    Should Contain    ${output}[hostNotificationCommands]    command_notif    host_notification_commands
    Should Be Equal As Strings    ${output}[serviceNotificationPeriod]    workhours    service_notification_period
    Should Contain    ${output}[serviceNotificationCommands]    command_notif    service_notification_commands
    Should Be True    ${output}[hostNotificationOnUp]    host_notification_on_up:Should Be True
    Should Be True    ${output}[hostNotificationOnDown]    host_notification_on_down:Should Be True
    Should Be True    ${output}[hostNotificationOnUnreachable]    host_notification_on_unreachable:Should Be True
    Should Be True    ${output}[hostNotificationOnFlappingstart]    host_notification_on_flappingstart:Should Be True
    Should Be True    ${output}[hostNotificationOnFlappingstop]    host_notification_on_flappingstop:Should Be True
    Should Be True    ${output}[hostNotificationOnFlappingdisabled]    host_notification_on_flappingdisabled:Should Be True
    Should Be True    ${output}[hostNotificationOnDowntime]    host_notification_on_downtime:Should Be True
    Should Be True    ${output}[serviceNotificationOnOk]    service_notification_on_ok:Should Be True
    Should Be True    ${output}[serviceNotificationOnWarning]    service_notification_on_warning:Should Be True
    Should Be True    ${output}[serviceNotificationOnCritical]    service_notification_on_critical:Should Be True
    Should Be True    ${output}[serviceNotificationOnUnknown]    service_notification_on_unknown:Should Be True
    Should Be True    ${output}[serviceNotificationOnFlappingstart]    service_notification_on_flappingstart:Should Be True
    Should Be True    ${output}[serviceNotificationOnFlappingstop]    service_notification_on_flappingstop:Should Be True
    Should Be True    ${output}[serviceNotificationOnFlappingdisabled]    service_notification_on_flappingdisabled:Should Be True
    Should Be True    ${output}[serviceNotificationOnDowntime]    service_notification_on_downtime:Should Be True
    Should Be True    ${output}[hostNotificationsEnabled]    host_notifications_enabled:Should Be True
    Should Be True    ${output}[serviceNotificationsEnabled]    service_notifications_enabled:Should Be True
    Should Be True    ${output}[canSubmitCommands]    can_submit_commands:Should Be True
    Should Be True    ${output}[retainStatusInformation]    retain_status_information:Should Be True
    Should Be True    ${output}[retainNonstatusInformation]    retain_nonstatus_information:Should Be True
    Should Be Equal As Strings    ${output}[timezone]    GMT+01    timezone
    Should Contain    ${output}[addresses]    dummy_address_1
    Should Contain    ${output}[addresses]    dummy_address_2
    Should Contain    ${output}[addresses]    dummy_address_3
    Should Contain    ${output}[addresses]    dummy_address_4
    Should Contain    ${output}[addresses]    dummy_address_5
    Should Contain    ${output}[addresses]    dummy_address_6

    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    SNMPCOMMUNITY    public
    Should Be True    ${ret}    customVariables_SNMPCOMMUNITY:Should Be True

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CECI1
    [Documentation]    Given a centralized Engine configuration where John_Doe is full and inherits from a full contact template
    ...    And both the contact and template define all notification settings and addresses
    ...    When Engine and Broker are started
    ...    Then John_Doe's own values take precedence over the template's values for all fields
    [Tags]    engine    contact    MON-151074
    Ctn Config Centralized Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Clear Retention

    # Create files :
    Ctn Create Template File    ${0}    contact    address1    ["template_address_1"]

    # Add Command :
    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true

    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    contactTemplates.cfg
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg

    # Operation in contact group
    Ctn Add Contact Group    ${0}    ${1}    ["U1"]
    Ctn Add Contact Group    ${0}    ${2}    ["U1","U2"]

    # Operation in Contact :
    Ctn Add Template To Contact    0    contact_template_1    [John_Doe]

    Ctn Engine Config Delete Value In Contact    0    John_Doe    alias
    Ctn Engine Config Delete Value In Contact    0    John_Doe    email
    Ctn Engine Config Delete Value In Contact    0    John_Doe    host_notification_period
    Ctn Engine Config Delete Value In Contact    0    John_Doe    service_notification_period
    Ctn Engine Config Delete Value In Contact    0    John_Doe    host_notification_options
    Ctn Engine Config Delete Value In Contact    0    John_Doe    service_notification_options
    Ctn Engine Config Delete Value In Contact    0    John_Doe    host_notifications_enabled
    Ctn Engine Config Delete Value In Contact    0    John_Doe    service_notifications_enabled

    Ctn Engine Config Delete Value In Contact    0    contact_template_1    active_checks_enabled    contactTemplates.cfg
    Ctn Engine Config Delete Value In Contact    0    contact_template_1    passive_checks_enabled    contactTemplates.cfg

    ${config_values}    Create Dictionary
    ...    alias    John_Doe_alias
    ...    contact_groups    contactgroup_1
    ...    email    John_Doe@gmail.com
    ...    pager    John_Doepager
    ...    host_notification_period    workhours
    ...    host_notification_commands    command_notif
    ...    service_notification_period    workhours
    ...    service_notification_commands    command_notif
    ...    host_notification_options    none
    ...    service_notification_options    none
    ...    host_notifications_enabled    1
    ...    service_notifications_enabled    1
    ...    can_submit_commands    1
    ...    retain_status_information    1
    ...    retain_nonstatus_information    1
    ...    timezone    GMT+01
    ...    address1    dummy_address_1
    ...    address2    dummy_address_2
    ...    address3    dummy_address_3
    ...    address4    dummy_address_4
    ...    address5    dummy_address_5
    ...    address6    dummy_address_6
    ...    _SNMPCOMMUNITY    public

    FOR    ${key}    ${value}    IN    &{config_values}
        Ctn Engine Config Set Value In Contacts    0    John_Doe    ${key}    ${value}
    END
     # Operation in contactTemplates
    ${config_values_tmp}    Create Dictionary
    ...    alias    contact_template_d_1
    ...    contact_groups    contactgroup_2
    ...    email    template@gmail.com
    ...    pager    templatepager
    ...    host_notification_period    none
    ...    host_notification_commands    checkh1
    ...    service_notification_period    none
    ...    service_notification_commands    checkh1
    ...    host_notification_options    all
    ...    service_notification_options    all
    ...    host_notifications_enabled    0
    ...    service_notifications_enabled    0
    ...    can_submit_commands    0
    ...    retain_status_information    0
    ...    retain_nonstatus_information    0
    ...    timezone    GMT+01
    ...    address2    template_address_2
    ...    address3    template_address_3
    ...    address4    template_address_4
    ...    address5    template_address_5
    ...    address6    template_address_6
    ...    _key    value

    FOR    ${key}    ${value}    IN    &{config_values_tmp}
        Ctn Engine Config Set Value In Contacts    0    contact_template_1    ${key}    ${value}    contactTemplates.cfg
    END

    ${start}    Ctn Get Round Current Date
    Ctn Clear Retention
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    ${output}    Ctn Get Contact Info Grpc    John_Doe

    Should Be Equal As Strings    ${output}[alias]    John_Doe_alias    alias
    Should Contain    ${output}[contactGroups]    contactgroup_1    contact_groups
    Should Be Equal As Strings    ${output}[email]    John_Doe@gmail.com    email
    Should Be Equal As Strings    ${output}[pager]    John_Doepager    pager
    Should Be Equal As Strings    ${output}[hostNotificationPeriod]    workhours    host_notification_period
    Should Contain    ${output}[hostNotificationCommands]    command_notif    host_notification_commands
    Should Be Equal As Strings    ${output}[serviceNotificationPeriod]    workhours    service_notification_period
    Should Contain    ${output}[serviceNotificationCommands]    command_notif    service_notification_commands
    Should Not Be True    ${output}[hostNotificationOnUp]    host_notification_on_up:Should Not Be True
    Should Not Be True    ${output}[hostNotificationOnDown]    host_notification_on_down:Should Not Be True
    Should Not Be True    ${output}[hostNotificationOnUnreachable]    host_notification_on_unreachable:Should Not Be True
    Should Not Be True    ${output}[hostNotificationOnFlappingstart]    host_notification_on_flappingstart:Should Not Be True
    Should Not Be True    ${output}[hostNotificationOnFlappingstop]    host_notification_on_flappingstop:Should Not Be True
    Should Not Be True    ${output}[hostNotificationOnFlappingdisabled]    host_notification_on_flappingdisabled:Should Not Be True
    Should Not Be True    ${output}[hostNotificationOnDowntime]    host_notification_on_downtime:Should Not Be True
    Should Not Be True    ${output}[serviceNotificationOnOk]    service_notification_on_ok:Should Not Be True
    Should Not Be True    ${output}[serviceNotificationOnWarning]    service_notification_on_warning:Should Not Be True
    Should Not Be True    ${output}[serviceNotificationOnCritical]    service_notification_on_critical:Should Not Be True
    Should Not Be True    ${output}[serviceNotificationOnUnknown]    service_notification_on_unknown:Should Not Be True
    Should Not Be True    ${output}[serviceNotificationOnFlappingstart]    service_notification_on_flappingstart:Should Not Be True
    Should Not Be True    ${output}[serviceNotificationOnFlappingstop]    service_notification_on_flappingstop:Should Not Be True
    Should Not Be True    ${output}[serviceNotificationOnFlappingdisabled]    service_notification_on_flappingdisabled:Should Not Be True
    Should Not Be True    ${output}[serviceNotificationOnDowntime]    service_notification_on_downtime:Should Not Be True
    Should Be True    ${output}[hostNotificationsEnabled]    host_notifications_enabled:Should Be True
    Should Be True    ${output}[serviceNotificationsEnabled]    service_notifications_enabled:Should Be True
    Should Be True    ${output}[canSubmitCommands]    can_submit_commands:Should Be True
    Should Be True    ${output}[retainStatusInformation]    retain_status_information:Should Be True
    Should Be True    ${output}[retainNonstatusInformation]    retain_nonstatus_information:Should Be True
    Should Be Equal As Strings    ${output}[timezone]    GMT+01    timezone
    Should Contain    ${output}[addresses]    dummy_address_1
    Should Contain    ${output}[addresses]    dummy_address_2
    Should Contain    ${output}[addresses]    dummy_address_3
    Should Contain    ${output}[addresses]    dummy_address_4
    Should Contain    ${output}[addresses]    dummy_address_5
    Should Contain    ${output}[addresses]    dummy_address_6

    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    SNMPCOMMUNITY    public
    Should Be True    ${ret}    customVariables_SNMPCOMMUNITY:Should Be True
    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    key    value
    Should Be True    ${ret}    customVariables_key:Should Be True

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CECI2
    [Documentation]    Given a centralized Engine started with a minimal contact configuration
    ...    And after start, John_Doe is made empty and a full contact template is added
    ...    When the new configuration is sent to Engine via Broker notification
    ...    Then John_Doe resolves with all values from the template
    [Tags]    engine    contact    MON-151074
    Ctn Config Centralized Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    central    bbdo    info
    Ctn Clear Retention

    # Create files :
    Ctn Create Template File    ${0}    contact    address1    ["dummy_address_1"]

    # Add Command :
    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true

    # Add necessarily files :
    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg

    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    checkh2
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    checkh2

    ${start}    Ctn Get Round Current Date
    Ctn Clear Retention
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # Add templates files :
    Ctn Config Engine Add Cfg File    ${0}    contactTemplates.cfg

    # Operation in contact group
    Ctn Add Contact Group    ${0}    ${1}    ["U1"]
    Ctn Add Contact Group    ${0}    ${2}    ["U1","U2"]

    # Operation in Contact :
    Ctn Add Template To Contact    0    contact_template_1    [John_Doe]

    Ctn Engine Config Delete Value In Contact    0    John_Doe    alias
    Ctn Engine Config Delete Value In Contact    0    John_Doe    email
    Ctn Engine Config Delete Value In Contact    0    John_Doe    host_notification_period
    Ctn Engine Config Delete Value In Contact    0    John_Doe    service_notification_period
    Ctn Engine Config Delete Value In Contact    0    John_Doe    host_notification_options
    Ctn Engine Config Delete Value In Contact    0    John_Doe    service_notification_options
    Ctn Engine Config Delete Value In Contact    0    John_Doe    host_notifications_enabled
    Ctn Engine Config Delete Value In Contact    0    John_Doe    service_notifications_enabled
    Ctn Engine Config Delete Value In Contact    0    John_Doe    host_notification_commands
    Ctn Engine Config Delete Value In Contact    0    John_Doe    service_notification_commands

    Ctn Engine Config Delete Value In Contact    0    contact_template_1    active_checks_enabled    contactTemplates.cfg
    Ctn Engine Config Delete Value In Contact    0    contact_template_1    passive_checks_enabled    contactTemplates.cfg

    # Operation in ContactTemplates
    ${config_values}    Create Dictionary
    ...    alias    contact_template_d_1
    ...    contact_groups    contactgroup_1
    ...    email    template@gmail.com
    ...    pager    templatepager
    ...    host_notification_period    workhours
    ...    host_notification_commands    command_notif
    ...    service_notification_period    workhours
    ...    service_notification_commands    command_notif
    ...    host_notification_options    all
    ...    service_notification_options    all
    ...    host_notifications_enabled    1
    ...    service_notifications_enabled    1
    ...    can_submit_commands    1
    ...    retain_status_information    1
    ...    retain_nonstatus_information    1
    ...    timezone    GMT+01
    ...    address2    dummy_address_2
    ...    address3    dummy_address_3
    ...    address4    dummy_address_4
    ...    address5    dummy_address_5
    ...    address6    dummy_address_6
    ...    _SNMPCOMMUNITY    public

    FOR    ${key}    ${value}    IN    &{config_values}
        Ctn Engine Config Set Value In Contacts    0    contact_template_1    ${key}    ${value}    contactTemplates.cfg
    END

    # Sending the new configuration to Engine.
    ${start}    Ctn Get Round Current Date
    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    ${output}    Ctn Get Contact Info Grpc    John_Doe

    Should Be Equal As Strings    ${output}[alias]    John_Doe    alias
    Should Contain    ${output}[contactGroups]    contactgroup_1    contact_groups
    Should Be Equal As Strings    ${output}[email]    template@gmail.com    email
    Should Be Equal As Strings    ${output}[pager]    templatepager    pager
    Should Be Equal As Strings    ${output}[hostNotificationPeriod]    workhours    host_notification_period
    Should Contain    ${output}[hostNotificationCommands]    command_notif    host_notification_commands
    Should Be Equal As Strings    ${output}[serviceNotificationPeriod]    workhours    service_notification_period
    Should Contain    ${output}[serviceNotificationCommands]    command_notif    service_notification_commands
    Should Be True    ${output}[hostNotificationOnUp]    host_notification_on_up:Should Be True
    Should Be True    ${output}[hostNotificationOnDown]    host_notification_on_down:Should Be True
    Should Be True    ${output}[hostNotificationOnUnreachable]    host_notification_on_unreachable:Should Be True
    Should Be True    ${output}[hostNotificationOnFlappingstart]    host_notification_on_flappingstart:Should Be True
    Should Be True    ${output}[hostNotificationOnFlappingstop]    host_notification_on_flappingstop:Should Be True
    Should Be True    ${output}[hostNotificationOnFlappingdisabled]    host_notification_on_flappingdisabled:Should Be True
    Should Be True    ${output}[hostNotificationOnDowntime]    host_notification_on_downtime:Should Be True
    Should Be True    ${output}[serviceNotificationOnOk]    service_notification_on_ok:Should Be True
    Should Be True    ${output}[serviceNotificationOnWarning]    service_notification_on_warning:Should Be True
    Should Be True    ${output}[serviceNotificationOnCritical]    service_notification_on_critical:Should Be True
    Should Be True    ${output}[serviceNotificationOnUnknown]    service_notification_on_unknown:Should Be True
    Should Be True    ${output}[serviceNotificationOnFlappingstart]    service_notification_on_flappingstart:Should Be True
    Should Be True    ${output}[serviceNotificationOnFlappingstop]    service_notification_on_flappingstop:Should Be True
    Should Be True    ${output}[serviceNotificationOnFlappingdisabled]    service_notification_on_flappingdisabled:Should Be True
    Should Be True    ${output}[serviceNotificationOnDowntime]    service_notification_on_downtime:Should Be True
    Should Be True    ${output}[hostNotificationsEnabled]    host_notifications_enabled:Should Be True
    Should Be True    ${output}[serviceNotificationsEnabled]    service_notifications_enabled:Should Be True
    Should Be True    ${output}[canSubmitCommands]    can_submit_commands:Should Be True
    Should Be True    ${output}[retainStatusInformation]    retain_status_information:Should Be True
    Should Be True    ${output}[retainNonstatusInformation]    retain_nonstatus_information:Should Be True
    Should Be Equal As Strings    ${output}[timezone]    GMT+01    timezone
    Should Contain    ${output}[addresses]    dummy_address_1
    Should Contain    ${output}[addresses]    dummy_address_2
    Should Contain    ${output}[addresses]    dummy_address_3
    Should Contain    ${output}[addresses]    dummy_address_4
    Should Contain    ${output}[addresses]    dummy_address_5
    Should Contain    ${output}[addresses]    dummy_address_6

    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    SNMPCOMMUNITY    public
    Should Be True    ${ret}    customVariables_SNMPCOMMUNITY:Should Be True

    Ctn Stop Engine
    Ctn Kindly Stop Broker

CECI3
    [Documentation]    Given a centralized Engine started with a minimal contact configuration
    ...    And after start, John_Doe is made full with its own values and a full contact template is added
    ...    When the new configuration is sent to Engine via Broker notification
    ...    Then John_Doe's own values take precedence over the template's values for all fields
    [Tags]    engine    contact    MON-151074
    Ctn Config Centralized Engine    ${1}    ${5}    ${5}
    Ctn Config Broker    rrd
    Ctn Config Broker    central
    Ctn Config Broker    module
    Ctn Broker Config Log    central    bbdo    info
    Ctn Clear Retention

    # Create files :
    Ctn Create Template File    ${0}    contact    address1    ["template_address_1"]

    # Add Command :
    Ctn Engine Config Add Command    0    command_notif    /usr/bin/true

    # Add necessarily files :

    Ctn Config Engine Add Cfg File    ${0}    contacts.cfg
    Ctn Config Engine Add Cfg File    ${0}    contactgroups.cfg

    Ctn Engine Config Set Value In Contacts    0    John_Doe    host_notification_commands    checkh2
    Ctn Engine Config Set Value In Contacts    0    John_Doe    service_notification_commands    checkh2

    ${start}    Ctn Get Round Current Date
    Ctn Clear Retention
    Ctn Start Broker    newGeneration=True
    Ctn Start Engine    newGeneration=True
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # Add templates files :
    Ctn Config Engine Add Cfg File    ${0}    contactTemplates.cfg

    # Operation in contact group
    Ctn Add Contact Group    ${0}    ${1}    ["U1"]
    Ctn Add Contact Group    ${0}    ${2}    ["U1","U2"]

    # Operation in Contact :
    Ctn Add Template To Contact    0    contact_template_1    [John_Doe]

    Ctn Engine Config Delete Value In Contact    0    John_Doe    alias
    Ctn Engine Config Delete Value In Contact    0    John_Doe    email
    Ctn Engine Config Delete Value In Contact    0    John_Doe    host_notification_period
    Ctn Engine Config Delete Value In Contact    0    John_Doe    service_notification_period
    Ctn Engine Config Delete Value In Contact    0    John_Doe    host_notification_options
    Ctn Engine Config Delete Value In Contact    0    John_Doe    service_notification_options
    Ctn Engine Config Delete Value In Contact    0    John_Doe    host_notifications_enabled
    Ctn Engine Config Delete Value In Contact    0    John_Doe    service_notifications_enabled
    Ctn Engine Config Delete Value In Contact    0    John_Doe    host_notification_commands
    Ctn Engine Config Delete Value In Contact    0    John_Doe    service_notification_commands

    Ctn Engine Config Delete Value In Contact    0    contact_template_1    active_checks_enabled    contactTemplates.cfg
    Ctn Engine Config Delete Value In Contact    0    contact_template_1    passive_checks_enabled    contactTemplates.cfg

    ${config_values}    Create Dictionary
    ...    alias    John_Doe_alias
    ...    contact_groups    contactgroup_1
    ...    email    John_Doe@gmail.com
    ...    pager    John_Doepager
    ...    host_notification_period    workhours
    ...    host_notification_commands    command_notif
    ...    service_notification_period    workhours
    ...    service_notification_commands    command_notif
    ...    host_notification_options    all
    ...    service_notification_options    all
    ...    host_notifications_enabled    1
    ...    service_notifications_enabled    1
    ...    can_submit_commands    1
    ...    retain_status_information    1
    ...    retain_nonstatus_information    1
    ...    timezone    GMT+01
    ...    address1    dummy_address_1
    ...    address2    dummy_address_2
    ...    address3    dummy_address_3
    ...    address4    dummy_address_4
    ...    address5    dummy_address_5
    ...    address6    dummy_address_6
    ...    _SNMPCOMMUNITY    public

    FOR    ${key}    ${value}    IN    &{config_values}
        Ctn Engine Config Set Value In Contacts    0    John_Doe    ${key}    ${value}
    END

    # Operation in contactTemplates
    ${config_values_tmp}    Create Dictionary
    ...    alias    contact_template_d_1
    ...    contact_groups    contactgroup_2
    ...    email    template@gmail.com
    ...    pager    templatepager
    ...    host_notification_period    none
    ...    host_notification_commands    checkh1
    ...    service_notification_period    none
    ...    service_notification_commands    checkh1
    ...    host_notification_options    none
    ...    service_notification_options    none
    ...    host_notifications_enabled    0
    ...    service_notifications_enabled    0
    ...    can_submit_commands    0
    ...    retain_status_information    0
    ...    retain_nonstatus_information    0
    ...    timezone    GMT+01
    ...    address2    template_address_2
    ...    address3    template_address_3
    ...    address4    template_address_4
    ...    address5    template_address_5
    ...    address6    template_address_6
    ...    _key    value

    FOR    ${key}    ${value}    IN    &{config_values_tmp}
        Ctn Engine Config Set Value In Contacts    0    contact_template_1    ${key}    ${value}    contactTemplates.cfg
    END
    # Sending the new configuration to Engine.
    ${start}    Ctn Get Round Current Date
    Ctn Notify Broker Of Engine Config Change    0
    ${content}    Create List    received diff state ack from poller 1
    ${result}    Ctn Find In Log With Timeout    ${centralLog}    ${start}    ${content}    30
    Should Be True    ${result}    The broker must receive a diff state ack from the poller 1.

    ${output}    Ctn Get Contact Info Grpc    John_Doe

    Should Be Equal As Strings    ${output}[alias]    John_Doe_alias    alias
    Should Contain    ${output}[contactGroups]    contactgroup_1    contact_groups
    Should Be Equal As Strings    ${output}[email]    John_Doe@gmail.com    email
    Should Be Equal As Strings    ${output}[pager]    John_Doepager    pager
    Should Be Equal As Strings    ${output}[hostNotificationPeriod]    workhours    host_notification_period
    Should Contain    ${output}[hostNotificationCommands]    command_notif    host_notification_commands
    Should Be Equal As Strings    ${output}[serviceNotificationPeriod]    workhours    service_notification_period
    Should Contain    ${output}[serviceNotificationCommands]    command_notif    service_notification_commands
    Should Be True    ${output}[hostNotificationOnUp]    host_notification_on_up:Should Be True
    Should Be True    ${output}[hostNotificationOnDown]    host_notification_on_down:Should Be True
    Should Be True    ${output}[hostNotificationOnUnreachable]    host_notification_on_unreachable:Should Be True
    Should Be True    ${output}[hostNotificationOnFlappingstart]    host_notification_on_flappingstart:Should Be True
    Should Be True    ${output}[hostNotificationOnFlappingstop]    host_notification_on_flappingstop:Should Be True
    Should Be True    ${output}[hostNotificationOnFlappingdisabled]    host_notification_on_flappingdisabled:Should Be True
    Should Be True    ${output}[hostNotificationOnDowntime]    host_notification_on_downtime:Should Be True
    Should Be True    ${output}[serviceNotificationOnOk]    service_notification_on_ok:Should Be True
    Should Be True    ${output}[serviceNotificationOnWarning]    service_notification_on_warning:Should Be True
    Should Be True    ${output}[serviceNotificationOnCritical]    service_notification_on_critical:Should Be True
    Should Be True    ${output}[serviceNotificationOnUnknown]    service_notification_on_unknown:Should Be True
    Should Be True    ${output}[serviceNotificationOnFlappingstart]    service_notification_on_flappingstart:Should Be True
    Should Be True    ${output}[serviceNotificationOnFlappingstop]    service_notification_on_flappingstop:Should Be True
    Should Be True    ${output}[serviceNotificationOnFlappingdisabled]    service_notification_on_flappingdisabled:Should Be True
    Should Be True    ${output}[serviceNotificationOnDowntime]    service_notification_on_downtime:Should Be True
    Should Be True    ${output}[hostNotificationsEnabled]    host_notifications_enabled:Should Be True
    Should Be True    ${output}[serviceNotificationsEnabled]    service_notifications_enabled:Should Be True
    Should Be True    ${output}[canSubmitCommands]    can_submit_commands:Should Be True
    Should Be True    ${output}[retainStatusInformation]    retain_status_information:Should Be True
    Should Be True    ${output}[retainNonstatusInformation]    retain_nonstatus_information:Should Be True
    Should Be Equal As Strings    ${output}[timezone]    GMT+01    timezone
    Should Contain    ${output}[addresses]    dummy_address_1
    Should Contain    ${output}[addresses]    dummy_address_2
    Should Contain    ${output}[addresses]    dummy_address_3
    Should Contain    ${output}[addresses]    dummy_address_4
    Should Contain    ${output}[addresses]    dummy_address_5
    Should Contain    ${output}[addresses]    dummy_address_6

    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    SNMPCOMMUNITY    public
    Should Be True    ${ret}    customVariables_SNMPCOMMUNITY:Should Be True
    ${ret}    Ctn Check Key Value Existence    ${output}[customVariables]    key    value
    Should Be True    ${ret}    customVariables_key:Should Be True

    Ctn Stop Engine
    Ctn Kindly Stop Broker
