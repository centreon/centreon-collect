/**
 * Copyright 2022-2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#ifndef CCB_NEB_PRECOMP_HH
#define CCB_NEB_PRECOMP_HH

#include <chrono>
#include <condition_variable>
#include <ctime>
#include <deque>
#include <future>
#include <limits>
#include <list>
#include <map>
#include <stack>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

#include <absl/container/btree_map.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include <absl/strings/string_view.h>

#include <boost/circular_buffer.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/optional.hpp>

#include <boost/asio.hpp>

namespace asio = boost::asio;

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <rapidjson/document.h>

#endif
