/**Copyright 2022 Centreon ****Licensed under the Apache License,
    Version 2.0(the "License");
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

#ifndef CCCS_PRECOMP_HH
#define CCCS_PRECOMP_HH

#include <boost/asio.hpp>

#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <sstream>
#include <string>

#include <libssh2.h>

#include <spdlog/common.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include <boost/smart_ptr/shared_array.hpp>

using shared_io_context = std::shared_ptr<asio::io_context>;

using system_clock = std::chrono::system_clock;
using time_point = system_clock::time_point;
using duration = system_clock::duration;

#endif  // CCCS_PRECOMP_HH