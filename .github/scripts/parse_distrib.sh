#!/bin/bash

#
# Copyright 2025 Centreon
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#
# For more information : contact@centreon.com
#

distrib=$1

if [[ "${distrib}" == "centos7" || "${distrib}" == "el7" ]]; then
    PACKAGE_DISTRIB_SEPARATOR="."
    PACKAGE_VERSION_SEPARATOR="-"
    PACKAGE_DISTRIB_NAME="el7"
    PACKAGE_EXTENSION="rpm"
    DISTRIB_FAMILY="el"
elif [[ "${distrib}" == "alma8" || "${distrib}" == "el8" ]]; then
    PACKAGE_DISTRIB_SEPARATOR="."
    PACKAGE_VERSION_SEPARATOR="-"
    PACKAGE_DISTRIB_NAME="el8"
    PACKAGE_EXTENSION="rpm"
    DISTRIB_FAMILY="el"
elif [[ "${distrib}" == "alma9" || "${distrib}" == "el9" ]]; then
    PACKAGE_DISTRIB_SEPARATOR="."
    PACKAGE_VERSION_SEPARATOR="-"
    PACKAGE_DISTRIB_NAME="el9"
    PACKAGE_EXTENSION="rpm"
    DISTRIB_FAMILY="el"
elif [[ "${distrib}" == "bullseye" ]]; then
    PACKAGE_DISTRIB_SEPARATOR="+"
    PACKAGE_VERSION_SEPARATOR="_"
    PACKAGE_DISTRIB_NAME="deb11u1"
    PACKAGE_EXTENSION="deb"
    DISTRIB_FAMILY="debian"
elif [[ "${distrib}" == "bookworm" ]]; then
    PACKAGE_DISTRIB_SEPARATOR="+"
    PACKAGE_VERSION_SEPARATOR="_"
    PACKAGE_DISTRIB_NAME="deb12u1"
    PACKAGE_EXTENSION="deb"
    DISTRIB_FAMILY="debian"
elif [[ "${distrib}" == "jammy" ]]; then
    PACKAGE_DISTRIB_SEPARATOR="-"
    PACKAGE_VERSION_SEPARATOR="_"
    PACKAGE_DISTRIB_NAME="0ubuntu.22.04"
    PACKAGE_EXTENSION="deb"
    DISTRIB_FAMILY="ubuntu"
elif [[ "${distrib}" == "noble" ]]; then
    PACKAGE_DISTRIB_SEPARATOR="-"
    PACKAGE_VERSION_SEPARATOR="_"
    PACKAGE_DISTRIB_NAME="0ubuntu.24.04"
    PACKAGE_EXTENSION="deb"
    DISTRIB_FAMILY="ubuntu"
else
    echo "::error::Distrib ${distrib} cannot be parsed"
    exit 1
fi
