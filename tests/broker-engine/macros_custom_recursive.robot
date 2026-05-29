*** Settings ***
Documentation       Verify that custom macros can contain and resolve other macros
...                 (host standard, host custom, service standard, service custom,
...                 resource $USERn$ macros) with recursion depth limited to 2.

Resource            ../resources/import.resource

Suite Setup         Ctn Clean Before Suite
Suite Teardown      Ctn Clean After Suite
Test Setup          Ctn Stop Processes
Test Teardown       Ctn Stop Engine Broker And Save Logs


*** Test Cases ***
CUSTOM_MACRO_RECURSIVE_RESOLUTION
    [Documentation]    Custom macros can reference other macros and get them resolved:
    ...                - host custom containing host standard ($HOSTADDRESS$)
    ...                - host custom containing another host custom ($_HOSTINNER$)
    ...                - host custom containing resource macro ($USER1$)
    ...                - service custom containing service standard ($SERVICEDESC$)
    ...                - service custom containing host standard + host custom + service custom
    ...                - service custom containing $USER1$ + $HOSTADDRESS$
    ...                - recursion stops at depth 2 (3-level chain leaves innermost unresolved)
    [Tags]    engine    macros    MON-193418
    Ctn Config Engine    ${1}    ${1}    ${1}

    # --- Host custom macros ---
    # host standard inside host custom
    Ctn Engine Config Set Value In Hosts    0    host_1    _URL    http://$HOSTADDRESS$/status
    # host custom inside host custom
    Ctn Engine Config Set Value In Hosts    0    host_1    _INNER    inner_value
    Ctn Engine Config Set Value In Hosts    0    host_1    _OUTER    got $_HOSTINNER$
    # resource macro inside host custom
    Ctn Engine Config Set Value In Hosts    0    host_1    _PLUGIN    $USER1$/check_ping
    # host custom used by service custom
    Ctn Engine Config Set Value In Hosts    0    host_1    _HTTPPORT    8080
    # recursion depth chain: TOP -> MID -> DEEP -> $HOSTNAME$
    Ctn Engine Config Set Value In Hosts    0    host_1    _DEEP    $HOSTNAME$
    Ctn Engine Config Set Value In Hosts    0    host_1    _MID    $_HOSTDEEP$
    Ctn Engine Config Set Value In Hosts    0    host_1    _TOP    $_HOSTMID$

    # --- Service custom macros ---
    # service standard inside service custom
    Ctn Engine Config Set Value In Services    0    service_1    _INFO    svc=$SERVICEDESC$
    # host standard + host custom + service custom inside service custom
    Ctn Engine Config Set Value In Services    0    service_1    _ENDPOINT    api/health
    Ctn Engine Config Set Value In Services    0    service_1    _CUSTOMURL    http://$HOSTADDRESS$:$_HOSTHTTPPORT$/$_SERVICEENDPOINT$
    # resource + host standard inside service custom
    Ctn Engine Config Set Value In Services    0    service_1    _CHECK    $USER1$/check_http -H $HOSTADDRESS$

    # Command that echoes all macros in one line, separated by |
    Ctn Engine Config Add Command
    ...    ${0}
    ...    macro_echo_cmd
    ...    /bin/echo $_HOSTURL$|$_HOSTOUTER$|$_HOSTPLUGIN$|$_SERVICEINFO$|$_SERVICECUSTOMURL$|$_SERVICECHECK$|$_HOSTTOP$
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    macro_echo_cmd

    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config BBDO3    ${1}


    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # Force an immediate service check
    Ctn Schedule Forced Service Check    host_1    service_1

    ${result}    Ctn Check Commandline Service With Timeout Rt    host_1    service_1    120
    ...    /bin/echo http://1.0.0.0/status|got inner_value|/usr/lib64/nagios/plugins/check_ping|svc=service_1|http://1.0.0.0:8080/api/health|/usr/lib64/nagios/plugins/check_http -H 1.0.0.0|$HOSTNAME$
    Should Be True    ${result}    Resolved command line not found in DB for service_1

CUSTOM_MACRO_RECURSIVE_RESOLUTION_WITH_ONE_DOLLAR
    [Documentation]    Custom macros can reference other macros and get them resolved:
    ...                - host custom containing host standard ($HOSTADDRESS$)
    ...                - host custom containing another host custom ($_HOSTINNER$)
    ...                - host custom containing resource macro ($USER1$)
    ...                - service custom containing service standard ($SERVICEDESC$)
    ...                - service custom containing host standard + host custom + service custom
    ...                - service custom containing $USER1$ + $HOSTADDRESS$
    ...                - recursion stops at depth 2 (3-level chain leaves innermost unresolved)
    [Tags]    engine    macros    MON-193418
    Ctn Config Engine    ${1}    ${1}    ${1}

    # --- Host custom macros ---
    # host standard inside host custom
    Ctn Engine Config Set Value In Hosts    0    host_1    _URL    http://$HOSTADDRESS$/status
    # host custom inside host custom
    Ctn Engine Config Set Value In Hosts    0    host_1    _INNER    inner_value
    Ctn Engine Config Set Value In Hosts    0    host_1    _OUTER    got $_HOSTINNER$
    # resource macro inside host custom
    Ctn Engine Config Set Value In Hosts    0    host_1    _PLUGIN    $USER1$/check_ping
    # host custom used by service custom
    Ctn Engine Config Set Value In Hosts    0    host_1    _HTTPPORT    8080
    # recursion depth chain: TOP -> MID -> DEEP -> $HOSTNAME$
    Ctn Engine Config Set Value In Hosts    0    host_1    _DEEP    $HOSTNAME$
    Ctn Engine Config Set Value In Hosts    0    host_1    _MID    $_HOSTDEEP$
    Ctn Engine Config Set Value In Hosts    0    host_1    _TOP    $_HOSTMID$|--storage '^/$'

    # --- Service custom macros ---
    # service standard inside service custom
    Ctn Engine Config Set Value In Services    0    service_1    _INFO    svc=$SERVICEDESC$
    # host standard + host custom + service custom inside service custom
    Ctn Engine Config Set Value In Services    0    service_1    _ENDPOINT    api/health
    Ctn Engine Config Set Value In Services    0    service_1    _CUSTOMURL    http://$HOSTADDRESS$:$_HOSTHTTPPORT$/$_SERVICEENDPOINT$
    # resource + host standard inside service custom
    Ctn Engine Config Set Value In Services    0    service_1    _CHECK    $USER1$/check_http -H $HOSTADDRESS$

    # Command that echoes all macros in one line, separated by |
    Ctn Engine Config Add Command
    ...    ${0}
    ...    macro_echo_cmd
    ...    /bin/echo $_HOSTURL$|$_HOSTOUTER$|$_HOSTPLUGIN$|$_SERVICEINFO$|$_SERVICECUSTOMURL$|$_SERVICECHECK$|$_HOSTTOP$
    Ctn Engine Config Replace Value In Services    ${0}    service_1    check_command    macro_echo_cmd

    Ctn Config Broker    module
    Ctn Config Broker    central
    Ctn Config Broker    rrd
    Ctn Config BBDO3    ${1}


    Ctn Clear Retention

    ${start}    Ctn Get Round Current Date
    Ctn Start Broker
    Ctn Start Engine
    Ctn Wait For Engine To Be Ready    ${start}    ${1}

    # Force an immediate service check
    Ctn Schedule Forced Service Check    host_1    service_1

    ${result}    Ctn Check Commandline Service With Timeout Rt    host_1    service_1    120
    ...    /bin/echo http://1.0.0.0/status|got inner_value|/usr/lib64/nagios/plugins/check_ping|svc=service_1|http://1.0.0.0:8080/api/health|/usr/lib64/nagios/plugins/check_http -H 1.0.0.0|$HOSTNAME$|--storage '^/$'
    Should Be True    ${result}    Resolved command line not found in DB for service_1
