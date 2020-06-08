##
## Copyright 2020 Centreon
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

set(stats_src
    centenginestats_client.cc
    main.cc

    centenginestats_client.hh
)

include_directories(${CMAKE_SOURCE_DIR}/include)
add_executable(centenginestats2 ${stats_src})
target_link_libraries(centenginestats2 cce_rpc)
