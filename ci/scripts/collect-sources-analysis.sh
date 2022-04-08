#!/bin/bash
set -e

# Moving files in order to be able to use absolute path specified in compile_commands.json
mkdir src

ls -lart
sudo find / -type d -name ".conan" 2>/dev/null

sudo mv build src

sonar-scanner -Dsonar.projectVersion="$VERSION"
