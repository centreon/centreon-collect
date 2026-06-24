#
# Copyright 2026 Centreon
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

find_package(benchmark REQUIRED)

# POC benchmark: compares the current struct-tm/mktime implementations of the
# timeperiods calendar helpers against an Abseil (CivilDay) rewrite.
add_executable(common_timeperiod_calc_bench timeperiod_calc.cc)

target_compile_features(common_timeperiod_calc_bench PRIVATE cxx_std_17)
target_compile_options(common_timeperiod_calc_bench PRIVATE -O2)

target_link_libraries(common_timeperiod_calc_bench
    PRIVATE
    benchmark::benchmark
    timeperiods
    engine_conf
    absl::time
    absl::flat_hash_map
    spdlog::spdlog
    fmt::fmt)
