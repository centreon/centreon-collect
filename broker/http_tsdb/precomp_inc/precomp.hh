/*
** Copyright 2022 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CC_INFLUX_DB_PRECOMP_HH
#define CC_INFLUX_DB_PRECOMP_HH

#include <condition_variable>
#include <deque>
#include <future>
#include <list>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/strings/match.h>

#include <nlohmann/json.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/exception/diagnostic_information.hpp>

// with this define boost::interprocess doesn't need Boost.DataTime
#define BOOST_DATE_TIME_NO_LIB 1
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>

using system_clock = std::chrono::system_clock;
using time_point = system_clock::time_point;
using duration = system_clock::duration;

namespace asio = boost::asio;

#endif
