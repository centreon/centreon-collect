/*
 * Copyright 2021 Centreon (https://www.centreon.com/)
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
#include "com/centreon/engine/severity.hh"

using namespace com::centreon;
using namespace com::centreon::engine;

severity_map severity::severities;

severity::severity(int32_t id, int32_t level, const std::string& name)
    : _id{static_cast<uint32_t>(id)},
      _level{static_cast<uint32_t>(level)},
      _name{name} {}

uint32_t severity::id() const {
  return _id;
}

uint32_t severity::level() const {
  return _level;
}

void severity::set_level(uint32_t level) {
  _level = level;
}

const std::string& severity::name() const {
  return _name;
}

void severity::set_name(const std::string& name) {
  _name = name;
}
