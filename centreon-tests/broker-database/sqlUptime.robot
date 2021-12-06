*** Settings ***
Resource            ../resources/resources.robot
Suite Setup         Clean Before Suite
Suite Teardown      Clean After Suite
Test Setup          Stop Processes

Documentation       Centreon Broker database stats
Library             Process
Library             DateTime
Library             OperatingSystem
Library             ../resources/BrokerDatabase.py
Library				../resources/Common.py
Library				../resources/Broker.py
Library				../resources/Engine.py

*** Test Cases ***

*** Keywords ***