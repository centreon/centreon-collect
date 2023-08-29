*** Settings ***
Documentation       Importation of all resources

# collection
Library             DatabaseLibrary
# Library             Process
# Library             OperatingSystem
Library             DateTime
Library             Collections
Library             DateTime
Library             Examples
Library             String
Library             Examples
Library             HttpCtrl.Server
# creation
Resource            resources.resource
# # Python Library
# Library             Engine.py
# Library             Broker.py
# Library             Bench.py
# Library             Common.py
# Library             specific-duplication.py
