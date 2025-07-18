/**
 * Copyright 2024 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#ifndef CCB_HTTP_CLIENT_PRECOMP_HH
#define CCB_HTTP_CLIENT_PRECOMP_HH

#include <algorithm>
#include <chrono>
#include <deque>
#include <forward_list>
#include <fstream>
#include <functional>
#include <list>
#include <map>
#include <mutex>
#include <queue>
#include <regex>
#include <set>
#include <string>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <absl/container/flat_hash_set.h>
#include <absl/hash/hash.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_replace.h>
#include <absl/strings/str_split.h>
#include <absl/synchronization/mutex.h>

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <spdlog/spdlog.h>

#include <boost/asio.hpp>
#ifndef _WIN32
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#endif
#include <boost/container/flat_set.hpp>

#include "com/centreon/exceptions/msg_fmt.hh"

namespace asio = boost::asio;

#endif  // CCB_HTTP_CLIENT_PRECOMP_HH
