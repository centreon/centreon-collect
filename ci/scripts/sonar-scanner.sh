#!/bin/bash
set -e

sudo apt-get install unzip || exit

echo "Cleaning..."
rm -rf tmp
mkdir tmp
cd tmp
rm -rf /root/.sonar/cache

echo "Downloading sonar-scanner..."
curl https://binaries.sonarsource.com/Distribution/sonar-scanner-cli/sonar-scanner-cli-4.7.0.2747-linux.zip --output sonar-scanner-cli-4.7.0.2747-linux.zip

echo "Unziping..."
unzip sonar-scanner-cli-4.7.0.2747-linux.zip
rm sonar-scanner-cli-4.7.0.2747-linux.zip
sudo mv sonar-scanner-4.7.0.2747-linux sonar-scanner

echo "Installation completed"
