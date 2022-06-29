*** Settings ***
Resource        ../resources/resources.robot
Suite Setup     Clean Before Suite
Suite Teardown  Clean After Suite
Test Setup      Stop Processes

Documentation   Centreon Broker only start/stop tests
Library Process
Library OperatingSystem
Library ../resources/Broker.py
Library DateTime


*** Test Cases ***
BCL1
        [Documentation] Starting broker with option '-s foobar' should return an error
        [Tags]  Broker  start-stop
        Config Broker   central
        Start Broker With Args  -s foobar
        ${result}=      Wait For Broker
        ${expected}=    Evaluate        "The option -s expects a positive integer" in """${result}"""
        Should be True  ${expected}     msg=expected error 'The option -s expects a positive integer'

BCL2
        [Documentation] Starting broker with option '-s5' should work
        [Tags]  Broker  start-stop
        Config Broker   central
        ${start}=       Get Current Date        exclude_millis=True