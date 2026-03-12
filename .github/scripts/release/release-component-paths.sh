#!/usr/bin/env bash

declare -A COMPONENT_PATHS

COMPONENT_PATHS[centreon-collect]="
.version.centreon-collect
bbdo
broker
ccc
clib
connectors
custom-triplets
engine
grpc
packaging/centreon-collect
overlays
selinux/centreon-broker
selinux/centreon-engine
cmake.sh
cmake-vcpkg.sh
CMakeLists.txt
CMakeListsLinux.txt
vcpkg.json
"

COMPONENT_PATHS[centreon-gorgone]="
gorgone
.version.centreon-gorgone
packaging/centreon-gorgone
"

COMPONENT_PATHS[centreon-monitoring-agent]="
agent
.version.centreon-monitoring-agent
"

COMPONENT_PATHS[centreon-common]="
packaging/centreon-common
.version.centreon-common
"
