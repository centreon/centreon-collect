/*
 * Copyright 2022 Centreon (https://www.centreon.com/)
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

#ifndef CCB_GRPC_PRECOMP_HH
#define CCB_GRPC_PRECOMP_HH
#include "../../precomp_inc/precomp.hpp"

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <asio.hpp>

#include <grpc/grpc.h>
#include <grpcpp/alarm.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/impl/codegen/client_callback.h>
#include <grpcpp/security/credentials.h>

#include "com/centreon/broker/io/endpoint.hh"
#include "com/centreon/broker/io/raw.hh"
#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using system_clock = std::chrono::system_clock;
using time_point = system_clock::time_point;
using duration = system_clock::duration;
using unique_lock = std::unique_lock<std::mutex>;

#endif  // CCB_GRPC_PRECOMP_HH
