# Copyright 2023 Centreon
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

set(STATS_EXPORTER "15-stats_exporter")
set(STATS_EXPORTER
    ${STATS_EXPORTER}
    PARENT_SCOPE)
add_library(${STATS_EXPORTER} SHARED src/main.cc)
set_target_properties(${STATS_EXPORTER} PROPERTIES PREFIX "")
target_link_libraries(${STATS_EXPORTER} CONAN_PKG::opentelemetry-cpp CONAN_PKG::spdlog)
target_precompile_headers(${STATS_EXPORTER} PRIVATE precomp_inc/precomp.hpp)

# Install rule.
install(TARGETS ${STATS_EXPORTER} LIBRARY DESTINATION "${PREFIX_MODULES}")
