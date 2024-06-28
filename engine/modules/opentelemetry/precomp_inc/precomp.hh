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

#ifndef CCE_MOD_OTL_SERVER_PRECOMP_HH
#define CCE_MOD_OTL_SERVER_PRECOMP_HH

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include <absl/base/thread_annotations.h>
#include <absl/container/btree_map.h>
#include <absl/container/btree_set.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include <re2/re2.h>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
namespace asio = boost::asio;
#include <boost/algorithm/string.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/optional.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <fmt/chrono.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <grpcpp/grpcpp.h>

#include <rapidjson/document.h>

#include "opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"

#include "com/centreon/common/http/http_server.hh"
#include "com/centreon/common/rapidjson_helper.hh"

#include "com/centreon/exceptions/msg_fmt.hh"

#endif
