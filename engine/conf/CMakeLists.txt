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

# Set directories.
set(SRC_DIR "")

# Configure files.
configure_file(${CMAKE_SOURCE_DIR}/engine/conf/centengine.cfg.in ${CMAKE_BINARY_DIR}/centengine.cfg)
configure_file(${CMAKE_SOURCE_DIR}/engine/conf/resource.cfg.in ${CMAKE_BINARY_DIR}/resource.cfg)
configure_file(${CMAKE_SOURCE_DIR}/engine/conf/objects/commands.cfg.in ${CMAKE_BINARY_DIR}/objects/commands.cfg)
configure_file(${CMAKE_SOURCE_DIR}/engine/conf/objects/contacts.cfg.in ${CMAKE_BINARY_DIR}/objects/contacts.cfg)
configure_file(${CMAKE_SOURCE_DIR}/engine/conf/objects/localhost.cfg.in ${CMAKE_BINARY_DIR}/objects/localhost.cfg)
configure_file(${CMAKE_SOURCE_DIR}/engine/conf/objects/printer.cfg.in ${CMAKE_BINARY_DIR}/objects/printer.cfg)
configure_file(${CMAKE_SOURCE_DIR}/engine/conf/objects/switch.cfg.in ${CMAKE_BINARY_DIR}/objects/switch.cfg)
configure_file(${CMAKE_SOURCE_DIR}/engine/conf/objects/templates.cfg.in ${CMAKE_BINARY_DIR}/objects/templates.cfg)
configure_file(${CMAKE_SOURCE_DIR}/engine/conf/objects/timeperiods.cfg.in ${CMAKE_BINARY_DIR}/objects/timeperiods.cfg)
configure_file(${CMAKE_SOURCE_DIR}/engine/conf/objects/windows.cfg.in ${CMAKE_BINARY_DIR}/objects/windows.cfg)
