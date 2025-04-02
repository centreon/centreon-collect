#
# Copyright 2011-2013 Merethis
# Copyright 2014,2017-2024 Centreon
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# For more information : contact@centreon.com
#
# Set directories.
set(SRC_DIR "${SRC_DIR}/applier")
set(INC_DIR "${INC_DIR}/applier")

# Set files.
set(
  FILES
  ${FILES}

  # Sources.
  "${SRC_DIR}/anomalydetection.cc"
  "${SRC_DIR}/command.cc"
  "${SRC_DIR}/connector.cc"
  "${SRC_DIR}/contact.cc"
  "${SRC_DIR}/contactgroup.cc"
  "${SRC_DIR}/host.cc"
  "${SRC_DIR}/hostdependency.cc"
  "${SRC_DIR}/hostescalation.cc"
  "${SRC_DIR}/hostgroup.cc"
  "${SRC_DIR}/globals.cc"
  "${SRC_DIR}/logging.cc"
  "${SRC_DIR}/macros.cc"
  "${SRC_DIR}/scheduler.cc"
  "${SRC_DIR}/service.cc"
  "${SRC_DIR}/servicedependency.cc"
  "${SRC_DIR}/serviceescalation.cc"
  "${SRC_DIR}/servicegroup.cc"
  "${SRC_DIR}/severity.cc"
  "${SRC_DIR}/state.cc"
  "${SRC_DIR}/tag.cc"
  "${SRC_DIR}/timeperiod.cc"

  # Headers.
  "${INC_DIR}/anomalydetection.hh"
  "${INC_DIR}/command.hh"
  "${INC_DIR}/connector.hh"
  "${INC_DIR}/contact.hh"
  "${INC_DIR}/contactgroup.hh"
  "${INC_DIR}/host.hh"
  "${INC_DIR}/hostdependency.hh"
  "${INC_DIR}/hostescalation.hh"
  "${INC_DIR}/hostgroup.hh"
  "${INC_DIR}/globals.hh"
  "${INC_DIR}/logging.hh"
  "${INC_DIR}/macros.hh"
  "${INC_DIR}/scheduler.hh"
  "${INC_DIR}/service.hh"
  "${INC_DIR}/servicedependency.hh"
  "${INC_DIR}/serviceescalation.hh"
  "${INC_DIR}/servicegroup.hh"
  "${INC_DIR}/severity.hh"
  "${INC_DIR}/state.hh"
  "${INC_DIR}/tag.hh"
  "${INC_DIR}/timeperiod.hh"

  PARENT_SCOPE
)
