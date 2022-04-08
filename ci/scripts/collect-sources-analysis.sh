#!/bin/bash
set -e

ls -lart
sudo find / -type d -name ".conan" 2>/dev/null
echo "current location"
pwd

# Moving files in order to be able to use absolute path specified in compile_commands.json
sudo mv build src

sonar-scanner -X -Dsonar.projectVersion="$VERSION"
