/*
 * Copyright 2020 Centreon (https://www.centreon.com/)
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
#ifndef CONNECTORS_PERL_INC_COM_CENTREON_CONNECTOR_PERL_LOG_V2_H_
#define CONNECTORS_PERL_INC_COM_CENTREON_CONNECTOR_PERL_LOG_V2_H_

#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include <memory>

class log_v2 {
 private:
  std::shared_ptr<spdlog::logger> _core_log;
  log_v2();
  ~log_v2();

 public:
  static log_v2& instance();

  void set_level(spdlog::level::level_enum level);
  void switch_to_stdout();
  void switch_to_file(std::string const& filename);

  static std::shared_ptr<spdlog::logger> core();
};

#endif  // CONNECTORS_PERL_INC_COM_CENTREON_CONNECTOR_PERL_LOG_V2_H_
