*** Settings ***
Resource            ../resources/resources.robot
Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes

Documentation       Centreon Broker database connections status and timestamp through grpc
Library             Process
Library             DateTime
Library             OperatingSystem
Library				../resources/Common.py
Library				../resources/Broker.py
Library				../resources/Engine.py
Library             ../resources/Grpc.py

*** Test Cases ***
StatsDB1
    [Documentation]
    [Tags]      Broker      Database    Grpc    Stats
    Database Status Timestamp Stats Test        conn=1

StatsDB2
    [Documentation]
    [Tags]      Broker      Database    Grpc    Stats
    Database Status Timestamp Stats Test        conn=2

StatsDB3
    [Documentation]
    [Tags]      Broker      Database    Grpc    Stats
    Database Status Timestamp Stats Test        conn=3

StatsDB4
    [Documentation]
    [Tags]      Broker      Database    Grpc    Stats
    Database Status Timestamp Stats Test        conn=4

StatsDB5
    [Documentation]
    [Tags]      Broker      Database    Grpc    Stats
    Database Status Timestamp Stats Test        conn=5

StatsDB6
    [Documentation]
    [Tags]      Broker      Database    Grpc    Stats
    Database Status Timestamp Stats Test        conn=6

*** Keywords ***
Exists Mysql Connections Size In Broker
    ${location}=        Set Variable            /var/lib/centreon-broker/central-broker-master-stats.json
    ${key1}=            Set Variable            mysql manager
    ${key2}=            Set Variable            size
    ${res}=             Stats Exists In File    location=${location}        key1=${key1}
    Should Be True      ${res}
    ${res}=             Stats Exists In File    location=${location}        key1=${key1}        key2=${key2}
    Should Be True      ${res}
Exists Mysql Connections Size In Grpc
    ${component}=       Set Variable            broker
    ${exe}=             Set Variable            GetSqlConnectionSize
    ${key1}=            Set Variable            size
    ${res}=             Stats Exists In Grpc    component=${component}      exe=${exe}          key1=${key1}
    #Log To Console      ${res}
    Should Be True      ${res}
Get Mysql Connections Size From Broker
    ${location}=        Set Variable            /var/lib/centreon-broker/central-broker-master-stats.json
    ${key1}=            Set Variable            mysql manager
    ${key2}=            Set Variable            size
    ${res}=             Get Stats In File       location=${location}        key1=${key1}        key2=${key2}
    [return]            ${res}
Get Mysql Connections Size From Grpc
    ${component}=       Set Variable            broker
    ${exe}=             Set Variable            GetSqlConnectionSize
    ${key1}=            Set Variable            size
    ${res}=             Get Stats In Grpc       component=${component}      exe=${exe}          key1=${key1}
    [return]            ${res}
Database Status Timestamp Stats Test
    [Arguments]     ${conn}
    Config Engine   ${1}
    Config Broker   module
    Config Broker   rrd
    Config Broker   central
    Broker Config Output Set        central     central-broker-master-sql       db_host                 127.0.0.1
    Broker Config Output Set        central     central-broker-master-perfdata  db_host                 127.0.0.1
    Broker Config Output Set        central     central-broker-master-sql       connections_count       ${conn}
    Broker Config Output Set        central     central-broker-master-perfdata  connections_count       ${conn}
    Broker Config Log               central     sql                             trace
    Start Broker
    Start Engine
    Exists Mysql Connections Size In Broker
    Exists Mysql Connections Size In Grpc
    ${mysqlConnectionsSizeBroker}=              Get Mysql Connections Size From Broker
    ${mysqlConnectionsSizeGrpc}=                Get Mysql Connections Size From Grpc
    Should Be Equal As Integers                 ${mysqlConnectionsSizeBroker}                           ${conn}
    Should Be Equal As Integers                 ${mysqlConnectionsSizeGrpc}                             ${conn}
    #
    #FOR
    #END
    #
    Stop Broker
    Stop Engine

*** Variables ***

#broker is connected to the database
#broker lost the connection to the database
#broker restored its connection to the database

#not reported as disconnected when restart after iptables drop
#or when droping iptables while running
#both in sql manager output and grpc stats
#should come from non corrrectly setted variables inside sql connection

#seems to have some issues with multithreading
#values seems to be not set

#RUN	iptables -A INPUT -p tcp --dport ${port} -j DROP
#RUN	iptables -A OUTPUT -p tcp --dport ${port} -j DROP
#RUN	iptables -A FORWARD -p tcp --dport ${port} -j DROP

#RUN	iptables -F
#RUN	iptables -X