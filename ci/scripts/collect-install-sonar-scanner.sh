#!/bin/bash
set -e

# Installing missing requirements
sudo apt-get install unzip || exit

# Cleaning
rm -rf tmp
mkdir tmp
cd tmp

curl https://binaries.sonarsource.com/Distribution/sonar-scanner-cli/sonar-scanner-cli-4.7.0.2747-linux.zip --output sonar-scanner-cli-4.7.0.2747-linux.zip

unzip sonar-scanner-cli-4.7.0.2747-linux.zip
rm -f sonar-scanner-cli-4.7.0.2747-linux.zip
sudo mv sonar-scanner-4.7.0.2747-linux sonar-scanner
