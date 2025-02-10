/**
 * Copyright 2023 Centreon
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

#ifndef CCB_GRPC_BRIDGE_HH
#define CCB_GRPC_BRIDGE_HH

#include <spdlog/logger.h>
#include "com/centreon/broker/io/protobuf.hh"
#include "grpc_stream.pb.h"

namespace com::centreon::broker::grpc {
std::shared_ptr<io::data> protobuf_to_event(
    const event_ptr& stream_content,
    const std::shared_ptr<spdlog::logger>& logger);

std::shared_ptr<event_with_data> create_event_with_data(
    const std::shared_ptr<io::data>& event,
    const std::shared_ptr<spdlog::logger>& logger);

}  // namespace com::centreon::broker::grpc

#endif
