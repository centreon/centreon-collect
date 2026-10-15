/**
 * Copyright 2026 Centreon
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

#ifndef CC_OTLP_PRECOMP_HH
#define CC_OTLP_PRECOMP_HH

#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include <nlohmann/json.hpp>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/strings/ascii.h>
#include <absl/strings/match.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_split.h>
#include <absl/synchronization/mutex.h>

#include <boost/asio.hpp>
#include <boost/container/string.hpp>
#include <boost/container/vector.hpp>
#include <boost/interprocess/allocators/private_node_allocator.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>

#include <grpcpp/grpcpp.h>

#include "com/centreon/exceptions/msg_fmt.hh"

/* common/pool.hh and other shared headers refer to asio unqualified. */
namespace asio = boost::asio;

#endif  // CC_OTLP_PRECOMP_HH
