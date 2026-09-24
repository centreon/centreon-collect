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

CUSTOM_MACRO_RESOLUTION_WITH_ONE_DOLLAR
    [Documentation]    Custom macros with one dollar can be resolved an dollar is not removed
    [Tags]    engine    macros    MON-200062
    Ctn Config Engine    ${1}    ${1}    ${1}

    # --- Host custom macros ---
    Ctn Engine Config Set Value In Hosts    0    host_1    _SNMPVERSION        2c
    Ctn Engine Config Set Value In Hosts    0    host_1    _SNMPCOMMUNITY      public
    
    # --- Service custom macros ---
    Ctn Engine Config Set Value In Services    0    service_1    _DISKNAME        ^/$
    Ctn Engine Config Set Value In Services    0    service_1    _WARNING        80
    Ctn Engine Config Set Value In Services    0    service_1    _CRITICAL        90
    Ctn Engine Config Set Value In Services    0    service_1    _EXTRAOPTIONS    --filter-perfdata='storage.space|used|free'
    
    # Command that echoes all macros in one line, separated by |
    Ctn Engine Config Add Command
    ...    ${0}
    ...    macro_echo_cmd
    ...    /bin/echo --plugin=os::linux::snmp::plugin --mode=storage --hostname=$HOSTADDRESS$ --snmp-version='$_HOSTSNMPVERSION$' --snmp-community='$_HOSTSNMPCOMMUNITY$' --storage '$_SERVICEDISKNAME$' --name --display-transform-src='$_SERVICETRANSFORMSRC$' --display-transform-dst='$_SERVICETRANSFORMDST$' --warning-usage='$_SERVICEWARNING$' --critical-usage='$_SERVICECRITICAL$' $_SERVICEEXTRAOPTIONS$
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
    ...    /bin/echo --plugin=os::linux::snmp::plugin --mode=storage --hostname=1.0.0.0 --snmp-version='2c' --snmp-community='public' --storage '^/$' --name --display-transform-src='' --display-transform-dst='' --warning-usage='80' --critical-usage='90' --filter-perfdata='storage.space|used|free'
    Should Be True    ${result}    Resolved command line not found in DB for service_1


CUSTOM_MACRO_RESOLUTION_WITH_FALSE_MACRO
    [Documentation]    Given engine is configured with host and service custom macros whose values contain dollar signs
    ...                When a service check runs using old-style $MACRO$ syntax with a non-existent macro at the end of the command
    ...                Then all macros are resolved correctly, dollar signs inside values are preserved, and the non-existent macro expands to empty
    [Tags]    engine    macros    MON-200203
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_macros    trace

    # --- Host custom macros ---
    Ctn Engine Config Set Value In Hosts    0    host_1    _NRPEPORT       5666
    Ctn Engine Config Set Value In Hosts    0    host_1    _NRPETIMEOUT      30
    Ctn Engine Config Set Value In Hosts    0    host_1    _NRPEEXTRAOPTIONS  -u -m 8192
    
    # --- Service custom macros ---
    Ctn Engine Config Set Value In Services    0    service_1    _FILTER         name in ('MSSQL$SIG','MSSQL$RH')
    Ctn Engine Config Set Value In Services    0    service_1    _SERVICE        *
    Ctn Engine Config Set Value In Services    0    service_1    _EXCLUDE        ok=state_is_ok()
    Ctn Engine Config Set Value In Services    0    service_1    _WARNING        not state_is_perfect()
    Ctn Engine Config Set Value In Services    0    service_1    _CRITICAL       not state_is_ok()
    Ctn Engine Config Set Value In Services    0    service_1    _TOPSYNTAX      \${problem_list}
    Ctn Engine Config Set Value In Services    0    service_1    _DETAILSYNTAX   {state} (\${start_type})
    Ctn Engine Config Set Value In Services    0    service_1    _EXTRAOPTIONS   'perf-config=none'
    
    # Command that echoes all macros in one line, separated by |
    Ctn Engine Config Add Command
    ...    ${0}
    ...    macro_echo_cmd
    ...    /bin/echo -H $HOSTADDRESS$ -p $_HOSTNRPEPORT$ -t $_HOSTNRPETIMEOUT$ $_HOSTNRPEEXTRAOPTIONS$ -c check_service -a "filter=$_SERVICEFILTER$" "service=$_SERVICESERVICE$" "exclude=$_SERVICEEXCLUDE$" "warning=$_SERVICEWARNING$" "critical=$_SERVICECRITICAL$" 'top-syntax=$_SERVICETOPSYNTAX$' 'detail-syntax=$_SERVICEDETAILSYNTAX$' $_SERVICEEXTRAOPTIONS$$_SERVICEEXTRAOPTIONSEMPTY$
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
    ...    /bin/echo -H 1.0.0.0 -p 5666 -t 30 -u -m 8192 -c check_service -a "filter=name in ('MSSQL$SIG','MSSQL$RH')" "service=*" "exclude=ok=state_is_ok()" "warning=not state_is_perfect()" "critical=not state_is_ok()" 'top-syntax=\${problem_list}' 'detail-syntax={state} (\${start_type})' 'perf-config=none'
    Should Be True    ${result}    Resolved command line not found in DB for service_1

CUSTOM_MACRO_RESOLUTION_WITH_NEW_STYLE_MACRO
    [Documentation]    Given engine is configured with host and service custom macros, one value embedding a new-style non-existent macro {{$TITI$}}
    ...                When a service check runs using new-style {{$MACRO$}} syntax with a non-existent macro at the end of the command
    ...                Then all macros are resolved correctly, dollar signs inside values are preserved, and non-existent macros expand to empty
    [Tags]    engine    macros    MON-200203
    Ctn Config Engine    ${1}    ${1}    ${1}
    Ctn Engine Config Set Value    ${0}    log_level_macros    trace

    # --- Host custom macros ---
    Ctn Engine Config Set Value In Hosts    0    host_1    _NRPEPORT       5666
    Ctn Engine Config Set Value In Hosts    0    host_1    _NRPETIMEOUT      30
    Ctn Engine Config Set Value In Hosts    0    host_1    _NRPEEXTRAOPTIONS  -u -m 8192
    
    # --- Service custom macros ---
    Ctn Engine Config Set Value In Services    0    service_1    _FILTER         name in ('MSSQL$SIG','MSSQL$RH')
    Ctn Engine Config Set Value In Services    0    service_1    _SERVICE        *
    Ctn Engine Config Set Value In Services    0    service_1    _EXCLUDE        ok=state_is_ok()
    Ctn Engine Config Set Value In Services    0    service_1    _WARNING        not state_is_perfect()
    Ctn Engine Config Set Value In Services    0    service_1    _CRITICAL       not state_is_ok()
    Ctn Engine Config Set Value In Services    0    service_1    _TOPSYNTAX      \${problem_list}
    Ctn Engine Config Set Value In Services    0    service_1    _DETAILSYNTAX   {state} (\${start_type})
    Ctn Engine Config Set Value In Services    0    service_1    _EXTRAOPTIONS   'perf-config=none{{$TITI$}}'
    
    # Command that echoes all macros in one line, separated by |
    Ctn Engine Config Add Command
    ...    ${0}
    ...    macro_echo_cmd
    ...    /bin/echo -H {{$HOSTADDRESS$}} -p {{$_HOSTNRPEPORT$}} -t {{$_HOSTNRPETIMEOUT$}} {{$_HOSTNRPEEXTRAOPTIONS$}} -c check_service -a "filter={{$_SERVICEFILTER$}}" "service={{$_SERVICESERVICE$}}" "exclude={{$_SERVICEEXCLUDE$}}" "warning={{$_SERVICEWARNING$}}" "critical={{$_SERVICECRITICAL$}}" 'top-syntax={{$_SERVICETOPSYNTAX$}}' 'detail-syntax={{$_SERVICEDETAILSYNTAX$}}' {{$_SERVICEEXTRAOPTIONS$}}{{$_SERVICEEXTRAOPTIONSEMPTY$}}
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
    ...    /bin/echo -H 1.0.0.0 -p 5666 -t 30 -u -m 8192 -c check_service -a "filter=name in ('MSSQL$SIG','MSSQL$RH')" "service=*" "exclude=ok=state_is_ok()" "warning=not state_is_perfect()" "critical=not state_is_ok()" 'top-syntax=\${problem_list}' 'detail-syntax={state} (\${start_type})' 'perf-config=none'
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
