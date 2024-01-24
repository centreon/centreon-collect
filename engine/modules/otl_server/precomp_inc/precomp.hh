/*
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

#include <absl/container/btree_map.h>

#include <boost/asio.hpp>
namespace asio = boost::asio;
#include <boost/algorithm/string.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/optional.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <grpcpp/grpcpp.h>

#include <rapidjson/document.h>

#include "opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h"

#include "com/centreon/engine/log_v2.hh"

#endif
