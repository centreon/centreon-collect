*** Settings ***
Resource            ../resources/resources.robot
Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes

Documentation       Centreon Broker database connections status and timestamp through grpc
Library             Process
Library             DateTime
Library             OperatingSystem
#Library             ../resources/BrokerDatabase.py
Library				../resources/Common.py
Library				../resources/Broker.py
Library				../resources/Engine.py

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
    #
    Stop Broker
    Stop Engine


*** Variables ***