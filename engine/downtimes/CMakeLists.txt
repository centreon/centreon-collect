#
# Copyright 2011-2019 Centreon
#
# This file is part of Centreon Scheduler.
#
# Centreon Scheduler is free software: you can redistribute it and/or
# modify it under the terms of the GNU General Public License version 2
# as published by the Free Software Foundation.
#
# Centreon Scheduler is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Centreon Scheduler. If not, see
# <http://www.gnu.org/licenses/>.
#
include_directories(
  ${PROJECT_BINARY_DIR}
)

add_library(downtimes_lib STATIC
  # Sources.
  downtime.cc
  downtime_manager.cc
  host_downtime.cc
  service_downtime.cc
  downtime_finder.cc
)

target_precompile_headers(downtimes_lib PRIVATE ${PROJECT_SOURCE_DIR}/precomp_inc/precomp.hh)
add_dependencies(downtimes_lib pb_neb_lib engine_conf process_stat)
