##
## Copyright 2011-2019 Centreon
##
## This file is part of Centreon Engine.
##
## Centreon Engine is free software: you can redistribute it and/or
## modify it under the terms of the GNU General Public License version 2
## as published by the Free Software Foundation.
##
## Centreon Engine is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
## General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with Centreon Engine. If not, see
## <http://www.gnu.org/licenses/>.
##

find_package(gRPC CONFIG REQUIRED)

# Locations definitions
add_definitions(-DDEFAULT_STATUS_FILE="${VAR_DIR}/status.dat")
add_definitions(-DDEFAULT_LOG_FILE="${VAR_DIR}/centengine.log")
add_definitions(-DDEFAULT_LOG_ARCHIVE_PATH="${LOG_ARCHIVE_DIR}")
add_definitions(-DDEFAULT_DEBUG_FILE="${VAR_DIR}/centengine.debug")
add_definitions(-DDEFAULT_RETENTION_FILE="${VAR_DIR}/retention.dat")
add_definitions(-DDEFAULT_COMMAND_FILE="${RW_DIR}/centengine.cmd")
add_definitions(-DDEFAULT_CONFIG_FILE="${PREFIX_CONF}/centengine.cfg")

# Configure files.
configure_file(${CMAKE_SOURCE_DIR}/engine/inc/compatibility/locations.h.in ${CMAKE_BINARY_DIR}/locations.h)
configure_file(${CMAKE_SOURCE_DIR}/engine/inc/compatibility/common.h.in ${CMAKE_BINARY_DIR}/common.h)
configure_file(${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/version.hh.in ${CMAKE_BINARY_DIR}/engine-version.hh)
include_directories(${CMAKE_SOURCE_DIR}/engine/inc/)
include_directories(${CMAKE_SOURCE_DIR}/engine/modules/external_commands/inc/)
include_directories(${CMAKE_BINARY_DIR}/inc/)

set(
  FILES
  # Sources.
  ${CMAKE_SOURCE_DIR}/engine/src/broker/compatibility.cc
  ${CMAKE_SOURCE_DIR}/engine/src/broker/loader.cc
  ${CMAKE_SOURCE_DIR}/engine/src/broker/handle.cc
  ${CMAKE_SOURCE_DIR}/engine/src/checks/checker.cc
  ${CMAKE_SOURCE_DIR}/engine/src/checks/stats.cc
  ${CMAKE_SOURCE_DIR}/engine/src/commands/command.cc
  ${CMAKE_SOURCE_DIR}/engine/src/commands/connector.cc
  ${CMAKE_SOURCE_DIR}/engine/src/commands/environment.cc
  ${CMAKE_SOURCE_DIR}/engine/src/commands/forward.cc
  ${CMAKE_SOURCE_DIR}/engine/src/commands/raw.cc
  ${CMAKE_SOURCE_DIR}/engine/src/commands/result.cc
  ${CMAKE_SOURCE_DIR}/engine/src/compatibility/common.cc
  ${CMAKE_SOURCE_DIR}/engine/src/compatibility/embedded_perl.cc
  ${CMAKE_SOURCE_DIR}/engine/src/compatibility/globals.cc
  ${CMAKE_SOURCE_DIR}/engine/src/compatibility/logging.cc
  ${CMAKE_SOURCE_DIR}/engine/src/compatibility/mmap.cc
  ${CMAKE_SOURCE_DIR}/engine/src/compatibility/sighandlers.cc
  ${CMAKE_SOURCE_DIR}/engine/src/compatibility/utils.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/anomalydetection.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/command.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/connector.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/contact.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/contactgroup.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/daterange.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/group.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/host.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/hostdependency.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/hostescalation.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/hostextinfo.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/hostgroup.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/object.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/parser.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/point_2d.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/point_3d.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/state.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/service.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/servicedependency.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/serviceescalation.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/serviceextinfo.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/servicegroup.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/timeperiod.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/timerange.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/applier/anomalydetection.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/applier/command.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/applier/connector.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/applier/contact.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/applier/contactgroup.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/applier/host.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/applier/hostdependency.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/applier/hostescalation.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/applier/hostgroup.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/applier/globals.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/applier/logging.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/applier/macros.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/applier/scheduler.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/applier/service.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/applier/servicedependency.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/applier/serviceescalation.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/applier/servicegroup.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/applier/state.cc
  ${CMAKE_SOURCE_DIR}/engine/src/configuration/applier/timeperiod.cc

  ${CMAKE_SOURCE_DIR}/engine/src/downtimes/downtime.cc
  ${CMAKE_SOURCE_DIR}/engine/src/downtimes/downtime_manager.cc
  ${CMAKE_SOURCE_DIR}/engine/src/downtimes/host_downtime.cc
  ${CMAKE_SOURCE_DIR}/engine/src/downtimes/service_downtime.cc
  ${CMAKE_SOURCE_DIR}/engine/src/downtimes/downtime_finder.cc
  ${CMAKE_SOURCE_DIR}/engine/src/events/loop.cc
  ${CMAKE_SOURCE_DIR}/engine/src/events/sched_info.cc
  ${CMAKE_SOURCE_DIR}/engine/src/events/timed_event.cc
  ${CMAKE_SOURCE_DIR}/engine/src/logging/broker.cc
  ${CMAKE_SOURCE_DIR}/engine/src/logging/debug_file.cc
  ${CMAKE_SOURCE_DIR}/engine/src/macros/clear_host.cc
  ${CMAKE_SOURCE_DIR}/engine/src/macros/clear_hostgroup.cc
  ${CMAKE_SOURCE_DIR}/engine/src/macros/clear_service.cc
  ${CMAKE_SOURCE_DIR}/engine/src/macros/clear_servicegroup.cc
  ${CMAKE_SOURCE_DIR}/engine/src/macros/grab_host.cc
  ${CMAKE_SOURCE_DIR}/engine/src/macros/grab_service.cc
  ${CMAKE_SOURCE_DIR}/engine/src/macros/grab_value.cc
  ${CMAKE_SOURCE_DIR}/engine/src/macros/misc.cc
  ${CMAKE_SOURCE_DIR}/engine/src/macros/process.cc
  ${CMAKE_SOURCE_DIR}/engine/src/retention/comment.cc
  ${CMAKE_SOURCE_DIR}/engine/src/retention/contact.cc
  ${CMAKE_SOURCE_DIR}/engine/src/retention/downtime.cc
  ${CMAKE_SOURCE_DIR}/engine/src/retention/dump.cc
  ${CMAKE_SOURCE_DIR}/engine/src/retention/host.cc
  ${CMAKE_SOURCE_DIR}/engine/src/retention/info.cc
  ${CMAKE_SOURCE_DIR}/engine/src/retention/parser.cc
  ${CMAKE_SOURCE_DIR}/engine/src/retention/program.cc
  ${CMAKE_SOURCE_DIR}/engine/src/retention/object.cc
  ${CMAKE_SOURCE_DIR}/engine/src/retention/service.cc
  ${CMAKE_SOURCE_DIR}/engine/src/retention/state.cc
  ${CMAKE_SOURCE_DIR}/engine/src/retention/applier/comment.cc
  ${CMAKE_SOURCE_DIR}/engine/src/retention/applier/contact.cc
  ${CMAKE_SOURCE_DIR}/engine/src/retention/applier/downtime.cc
  ${CMAKE_SOURCE_DIR}/engine/src/retention/applier/host.cc
  ${CMAKE_SOURCE_DIR}/engine/src/retention/applier/program.cc
  ${CMAKE_SOURCE_DIR}/engine/src/retention/applier/service.cc
  ${CMAKE_SOURCE_DIR}/engine/src/retention/applier/state.cc
  ${CMAKE_SOURCE_DIR}/engine/src/retention/applier/utils.cc
  ${CMAKE_SOURCE_DIR}/engine/src/anomalydetection.cc
  ${CMAKE_SOURCE_DIR}/engine/src/broker.cc
  ${CMAKE_SOURCE_DIR}/engine/src/checkable.cc
  ${CMAKE_SOURCE_DIR}/engine/src/check_result.cc
  ${CMAKE_SOURCE_DIR}/engine/src/command_manager.cc
  ${CMAKE_SOURCE_DIR}/engine/src/comment.cc
  ${CMAKE_SOURCE_DIR}/engine/src/config.cc
  ${CMAKE_SOURCE_DIR}/engine/src/contact.cc
  ${CMAKE_SOURCE_DIR}/engine/src/contactgroup.cc
  ${CMAKE_SOURCE_DIR}/engine/src/customvariable.cc
  ${CMAKE_SOURCE_DIR}/engine/src/daterange.cc
  ${CMAKE_SOURCE_DIR}/engine/src/dependency.cc
  ${CMAKE_SOURCE_DIR}/engine/src/diagnostic.cc
  ${CMAKE_SOURCE_DIR}/engine/src/exceptions/error.cc
  ${CMAKE_SOURCE_DIR}/engine/src/flapping.cc
  ${CMAKE_SOURCE_DIR}/engine/src/escalation.cc
  ${CMAKE_SOURCE_DIR}/engine/src/globals.cc
  ${CMAKE_SOURCE_DIR}/engine/src/host.cc
  ${CMAKE_SOURCE_DIR}/engine/src/hostdependency.cc
  ${CMAKE_SOURCE_DIR}/engine/src/hostescalation.cc
  ${CMAKE_SOURCE_DIR}/engine/src/hostgroup.cc
  ${CMAKE_SOURCE_DIR}/engine/src/macros.cc
  ${CMAKE_SOURCE_DIR}/engine/src/nebmods.cc
  ${CMAKE_SOURCE_DIR}/engine/src/notification.cc
  ${CMAKE_SOURCE_DIR}/engine/src/notifier.cc
  ${CMAKE_SOURCE_DIR}/engine/src/sehandlers.cc
  ${CMAKE_SOURCE_DIR}/engine/src/service.cc
  ${CMAKE_SOURCE_DIR}/engine/src/servicedependency.cc
  ${CMAKE_SOURCE_DIR}/engine/src/serviceescalation.cc
  ${CMAKE_SOURCE_DIR}/engine/src/servicegroup.cc
  ${CMAKE_SOURCE_DIR}/engine/src/shared.cc
  ${CMAKE_SOURCE_DIR}/engine/src/statistics.cc
  ${CMAKE_SOURCE_DIR}/engine/src/statusdata.cc
  ${CMAKE_SOURCE_DIR}/engine/src/string.cc
  ${CMAKE_SOURCE_DIR}/engine/src/timeperiod.cc
  ${CMAKE_SOURCE_DIR}/engine/src/timerange.cc
  ${CMAKE_SOURCE_DIR}/engine/src/timezone_locker.cc
  ${CMAKE_SOURCE_DIR}/engine/src/timezone_manager.cc
  ${CMAKE_SOURCE_DIR}/engine/src/utils.cc
  ${CMAKE_SOURCE_DIR}/engine/src/xpddefault.cc
  ${CMAKE_SOURCE_DIR}/engine/src/xsddefault.cc
)

# Subdirectories with core features.
add_subdirectory(conf)
add_subdirectory(modules)
add_subdirectory(enginerpc)
include_directories(enginerpc)

# Library engine target.
add_library(cce_core ${LIBRARY_TYPE} ${FILES})

# Link target with required libraries.
target_link_libraries(cce_core CONAN_PKG::json11 centreon-clib)

# centengine target.
add_executable(centengine ${CMAKE_SOURCE_DIR}/engine/src/main.cc)
set_property(TARGET centengine PROPERTY ENABLE_EXPORTS 1)

# Link centengine with required libraries.
target_link_libraries(centengine centreon-clib "-Wl,-whole-archive"  gRPC::grpc++_reflection enginerpc cce_core  "-Wl,-no-whole-archive" CONAN_PKG::grpc)


# centenginestats target.
add_executable(centenginestats ${CMAKE_SOURCE_DIR}/engine/src/centenginestats.cc)
target_link_libraries(centenginestats centreon-clib CONAN_PKG::json11)

# Unit tests.
add_subdirectory(tests)
