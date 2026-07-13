*** Settings ***
Documentation       Importation of all resources

# Collection
Library             Telnet
Library             String
Library             DateTime
Library             Collections
Library             DatabaseLibrary
Library             Examples
Library             HttpCtrl.Server
Library             RequestsLibrary
# Creation
Resource            resources.resource
# Python Library
Library             Engine.py
Library             Broker.py
#Library             Bench.py
Library             Common.py
Library             specific-duplication.py
