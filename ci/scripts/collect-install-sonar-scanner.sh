#!/bin/bash
set -e

sudo apt-get install unzip || exit

rm -rf tmp
mkdir tmp
cd tmp
sudo rm -rf /root/.sonar

curl https://binaries.sonarsource.com/Distribution/sonar-scanner-cli/sonar-scanner-cli-4.7.0.2747-linux.zip --output sonar-scanner-cli-4.7.0.2747-linux.zip

unzip sonar-scanner-cli-4.7.0.2747-linux.zip
rm sonar-scanner-cli-4.7.0.2747-linux.zip
sudo mv sonar-scanner-4.7.0.2747-linux sonar-scanner
