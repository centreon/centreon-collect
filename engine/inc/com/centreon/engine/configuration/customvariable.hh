/*
 * Copyright 2023 Centreon (https://www.centreon.com/)
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
#ifndef CCE_CONFIGURATION_CUSTOMVARIABLE_HH
#define CCE_CONFIGURATION_CUSTOMVARIABLE_HH
#include <string>

namespace com::centreon::engine::configuration {
/**
 * @class customvariable customvariable.hh
 * "com/centreon/engine/configuration/customvariable.hh"
 * @brief This class represents a customvariable configuration.
 *
 * It is lighter than the engine::customvariable class and also it is
 * separated from the engine core because the configuration module must be
 * loadable from cbd.
 */
class customvariable {
  std::string _value;
  bool _is_sent;

 public:
  customvariable(std::string const& value = "", bool is_sent = false);
  bool operator==(const customvariable& other) const;
  ~customvariable() noexcept = default;
  void set_sent(bool sent);
  bool is_sent() const;
  void set_value(const std::string& value);
  const std::string& value() const;
};
}  // namespace com::centreon::engine::configuration

#endif /* !CCE_CONFIGURATION_CUSTOMVARIABLE_HH */
