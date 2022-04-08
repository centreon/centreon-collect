#!/bin/bash
set -e

# Moving files in order to be able to use absolute path specified in compile_commands.json
mkdir src
mv build src

sonar-scanner -Dsonar.projectVersion="$VERSION"
