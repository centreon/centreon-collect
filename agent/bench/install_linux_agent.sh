#!/bin/bash

#
# Copyright 2024 Centreon
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

# This script is used to install the Centreon Monitoring Agent on a Linux system

set -x

sed -i s/^SELINUX=.*$/SELINUX=disabled/ /etc/selinux/config

dnf install -y dnf-plugins-core
dnf install -y epel-release
/usr/bin/crb enable

dnf install -y dnf-plugins-core
dnf config-manager --add-repo https://packages.centreon.com/rpm-standard/24.10/el9/centreon-24.10.repo

dnf install -y centreon-monitoring-agent

