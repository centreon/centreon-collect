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
#ifndef CONNECTORS_INC_COM_CENTREON_CONNECTOR_IPOLICY_H_
#define CONNECTORS_INC_COM_CENTREON_CONNECTOR_IPOLICY_H_


Cnamespace com::centreon {
namespace orders {
class options;
}

class policy_interface : public std::enable_shared_from_this<policy_interface> {
 public:
  virtual ~policy_interface() = default;

  virtual void on_eof() = 0;
  virtual void on_error(uint64_t cmd_id, const std::string& msg) = 0;
  virtual void on_execute(uint64_t cmd_id,
                          const time_point& timeout,
                          const std::shared_ptr<orders::options>& opt) = 0;
  virtual void on_quit() = 0;
  virtual void on_version() = 0;
};

C}()

#endif
