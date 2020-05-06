##
## Copyright 2011-2013 Merethis
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

include_directories(${CMAKE_SOURCE_DIR}/engine/modules/external_commands/inc/)
# mod_externalcmd target.
add_library(
  externalcmd
  SHARED

  # Sources.
  ${CMAKE_SOURCE_DIR}/engine/modules/external_commands/src/commands.cc
  ${CMAKE_SOURCE_DIR}/engine/modules/external_commands/src/internal.cc
  ${CMAKE_SOURCE_DIR}/engine/modules/external_commands/src/main.cc
  ${CMAKE_SOURCE_DIR}/engine/modules/external_commands/src/processing.cc
  ${CMAKE_SOURCE_DIR}/engine/modules/external_commands/src/utils.cc

  # Headers.
  ${CMAKE_SOURCE_DIR}/engine/modules/external_commands/inc/com/centreon/engine/modules/external_commands/commands.hh
  ${CMAKE_SOURCE_DIR}/engine/modules/external_commands/inc/com/centreon/engine/modules/external_commands/internal.hh
  ${CMAKE_SOURCE_DIR}/engine/modules/external_commands/inc/com/centreon/engine/modules/external_commands/processing.hh
  ${CMAKE_SOURCE_DIR}/engine/modules/external_commands/inc/com/centreon/engine/modules/external_commands/utils.hh
)
# Prettier name.
set_property(TARGET externalcmd PROPERTY PREFIX "")

set(EXTERNALCMD_MODULE ${EXTERNALCMD_MODULE} PARENT_SCOPE)
# Link target with libraries.
target_link_libraries(externalcmd centreon-clib CONAN_PKG::json11)

# Install rule.
install(TARGETS "externalcmd"
  DESTINATION "${PREFIX_LIB}"
  COMPONENT "runtime")
