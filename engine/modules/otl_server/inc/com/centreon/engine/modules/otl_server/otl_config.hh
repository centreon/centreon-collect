/*
** Copyright 2024 Centreon
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

#ifndef CCE_MOD_OTL_SERVER_OTLCONFIG_HH
#define CCE_MOD_OTL_SERVER_OTLCONFIG_HH

#include "grpc_config.hh"

namespace com::centreon::engine::modules::otl_server {
class otl_config {
  grpc_config::pointer _conf;

 public:
  otl_config(const std::string_view& file_path);
};

}  // namespace com::centreon::engine::modules::otl_server
#endif