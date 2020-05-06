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

  # Headers.
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/broker/compatibility.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/broker/handle.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/broker/loader.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/commands/command.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/commands/command_listener.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/commands/connector.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/commands/environment.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/commands/forward.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/commands/raw.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/commands/result.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/broker.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/comments.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/common.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/config.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/downtime.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/embedded_perl.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/epn_nagios.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/events.h
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/anomalydetection.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/command.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/connector.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/contactgroup.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/contact.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/daterange.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/file_info.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/group.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/host.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/hostdependency.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/hostescalation.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/hostextinfo.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/hostgroup.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/object.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/parser.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/point_2d.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/point_3d.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/servicedependency.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/serviceescalation.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/serviceextinfo.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/servicegroup.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/service.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/state.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/timeperiod.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/timerange.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/applier/anomalydetection.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/applier/command.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/applier/connector.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/applier/contact.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/applier/contactgroup.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/applier/difference.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/applier/host.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/applier/hostdependency.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/applier/hostescalation.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/applier/hostgroup.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/applier/globals.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/applier/logging.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/applier/macros.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/applier/scheduler.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/applier/service.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/applier/servicedependency.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/applier/serviceescalation.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/applier/servicegroup.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/applier/state.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/configuration/applier/timeperiod.hh
  ${CMAKE_BINARY_DIR}/locations.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/logging.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/macros.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/mmap.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/nagios.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/nebcallbacks.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/neberrors.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/nebmods.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/nebmodules.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/nebstructs.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/netutils.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/objects.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/perfdata.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/protoapi.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/shared.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/sighandlers.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/sretention.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/statusdata.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/xcddefault.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/xodtemplate.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/xpddefault.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/xrddefault.h
  ${CMAKE_SOURCE_DIR}/engine/inc/compatibility/xsddefault.h
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/downtimes/downtime.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/downtimes/downtime_manager.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/downtimes/host_downtime.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/downtimes/service_downtime.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/downtimes/downtime_finder.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/events/loop.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/events/sched_info.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/events/timed_event.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/logging/logger.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/logging/broker.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/logging/debug_file.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/macros/clear_host.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/macros/clear_hostgroup.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/macros/clear_service.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/macros/clear_servicegroup.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/macros/grab.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/macros/grab_host.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/macros/grab_service.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/macros/grab_value.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/macros/misc.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/macros/process.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/retention/comment.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/retention/contact.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/retention/downtime.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/retention/dump.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/retention/host.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/retention/info.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/retention/parser.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/retention/program.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/retention/object.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/retention/service.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/retention/state.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/retention/applier/comment.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/retention/applier/contact.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/retention/applier/downtime.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/retention/applier/host.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/retention/applier/program.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/retention/applier/service.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/retention/applier/state.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/retention/applier/utils.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/anomalydetection.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/broker.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/checkable.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/check_result.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/circular_buffer.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/command_manager.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/comment.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/common.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/config.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/contact.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/contactgroup.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/customvariable.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/daterange.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/dependency.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/diagnostic.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/exceptions/error.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/escalation.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/flapping.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/globals.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/host.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/hostdependency.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/hostescalation.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/hostgroup.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/logging.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/macros.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/nebcallbacks.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/neberrors.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/nebmods.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/nebmodules.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/nebstructs.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/notification.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/notifier.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/objects.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/opt.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/sehandlers.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/service.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/servicedependency.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/serviceescalation.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/servicegroup.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/shared.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/statistics.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/statusdata.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/string.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/timeperiod.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/timerange.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/timezone_locker.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/timezone_manager.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/utils.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/version.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/xpddefault.hh
  ${CMAKE_SOURCE_DIR}/engine/inc/com/centreon/engine/xsddefault.hh
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
