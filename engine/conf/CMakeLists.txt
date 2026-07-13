#
# Copyright 2011-2013 Merethis
# Copyright 2020-2021 Centreon
#
# This file is part of Centreon Engine.
#
# Centreon Engine is free software: you can redistribute it and/or
# modify it under the terms of the GNU General Public License version 2
# as published by the Free Software Foundation.
#
# Centreon Engine is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Centreon Engine. If not, see
# <http://www.gnu.org/licenses/>.
#

# Set directories.
set(SRC_DIR "${PROJECT_SOURCE_DIR}/conf")

# Configure files.
configure_file("${SRC_DIR}/centengine.cfg.in"
  "${SRC_DIR}/centengine.cfg")
configure_file("${SRC_DIR}/resource.cfg.in"
  "${SRC_DIR}/resource.cfg")
configure_file("${SRC_DIR}/commands.cfg.in"
  "${SRC_DIR}/commands.cfg")
configure_file("${SRC_DIR}/timeperiods.cfg.in"
  "${SRC_DIR}/timeperiods.cfg")

# Install files if necessary.
option(WITH_SAMPLE_CONFIG "Install sample configuration files." ON)
if (WITH_SAMPLE_CONFIG)
  install(DIRECTORY "${SRC_DIR}/"
    DESTINATION "${PREFIX_ENGINE_CONF}"
    COMPONENT "runtime"
    FILES_MATCHING PATTERN "*.cfg")

  install(CODE "
  function(my_chown user group file)
    if (APPLE OR (UNIX AND NOT CYGWIN))
      execute_process(COMMAND \"chown\" \"\${user}:\${group}\" \"\${file}\")
    endif ()
  endfunction()

  my_chown(\"${USER_ENGINE}\" \"${USER_ENGINE}\" \"${PREFIX_ENGINE_CONF}/centengine.cfg\")
  my_chown(\"${USER_ENGINE}\" \"${USER_ENGINE}\" \"${PREFIX_ENGINE_CONF}/resource.cfg\")
  my_chown(\"${USER_ENGINE}\" \"${USER_ENGINE}\" \"${PREFIX_ENGINE_CONF}/commands.cfg\")
  my_chown(\"${USER_ENGINE}\" \"${USER_ENGINE}\" \"${PREFIX_ENGINE_CONF}/timeperiods.cfg\")
  ")
endif ()
