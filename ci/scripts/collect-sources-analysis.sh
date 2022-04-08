#!/bin/bash
set -e

echo "current location"
pwd

# Moving files in order to be able to use absolute path specified in compile_commands.json
sudo mv build src
ls -lart

sonar-scanner -X -Dsonar.projectVersion="$VERSION"
