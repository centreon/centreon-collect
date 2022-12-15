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

#ifndef CCB_GRPC_TEST__INCLUDE_HH
#define CCB_GRPC_TEST__INCLUDE_HH

#include <condition_variable>
#include <mutex>
#include <queue>
#include <set>

#include <absl/strings/string_view.h>
#include <gtest/gtest.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <nlohmann/json.hpp>

#include <boost/asio.hpp>

#include "../src/grpc_stream.pb.h"

#include <grpc/impl/codegen/compression_types.h>
#include <grpcpp/create_channel.h>

using system_clock = std::chrono::system_clock;
using time_point = system_clock::time_point;
using duration = system_clock::duration;
using unique_lock = std::unique_lock<std::mutex>;

#include "com/centreon/broker/grpc/acceptor.hh"
#include "com/centreon/broker/grpc/stream.hh"
#include "com/centreon/broker/io/raw.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/pool.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

#endif  // CCB_GRPC_TEST__INCLUDE_HH
