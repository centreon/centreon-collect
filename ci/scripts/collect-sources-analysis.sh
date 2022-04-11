#!/bin/bash
set -e

sonar-scanner -Dsonar.projectVersion="$VERSION"
