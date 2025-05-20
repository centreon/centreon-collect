/**
 * Copyright 2024 Centreon
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

#ifndef CA_PRECOMP_HH
#define CA_PRECOMP_HH

#include <deque>
#include <iostream>
#include <limits>
#include <list>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/chrono.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <absl/base/thread_annotations.h>
#include <absl/container/btree_map.h>
#include <absl/container/btree_set.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/strings/ascii.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_split.h>
#include <absl/synchronization/mutex.h>

#include <re2/re2.h>

#include <boost/asio.hpp>

namespace asio = boost::asio;

#include <boost/algorithm/string.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/process.hpp>
#include <boost/program_options.hpp>

#include <rapidjson/document.h>

#include "com/centreon/exceptions/msg_fmt.hh"

namespace multi_index = boost::multi_index;

#endif
