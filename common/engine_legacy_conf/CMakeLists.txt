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

if(LEGACY_ENGINE)
  add_library(
    engine_legacy_conf STATIC
    anomalydetection.cc
    command.cc
    connector.cc
    contact.cc
    contactgroup.cc
    customvariable.cc
    daterange.cc
    group.cc
    host.cc
    hostdependency.cc
    hostescalation.cc
    hostgroup.cc
    object.cc
    parser.cc
    point_2d.cc
    point_3d.cc
    service.cc
    servicedependency.cc
    serviceescalation.cc
    servicegroup.cc
    severity.cc
    state.cc
    tag.cc
    timeperiod.cc)

  add_dependencies(engine_legacy_conf pb_neb_lib pb_common_lib pb_bam_lib pb_bam_state_lib)
  include_directories(${CMAKE_SOURCE_DIR}/common/inc)

  target_precompile_headers(engine_legacy_conf PRIVATE
                            ${CMAKE_SOURCE_DIR}/common/precomp_inc/precomp.hh)
  target_link_libraries(engine_legacy_conf log_v2 absl::any absl::log
                        absl::base absl::bits)
endif()
