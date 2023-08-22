*** Settings ***
Documentation       Importation of all resources

# collection
Library             DatabaseLibrary
Library             Process
Library             OperatingSystem
Library             DateTime
Library             Collections
Library             DateTime
Library             String
# creation
Resource            resources.resource
Resource            db_variables.resource
# Python Library
Library             Engine.py
Library             Broker.py
Library             Bench.py
Library             Common.py
Library             specific-duplication.py
Library             db_conf.py
