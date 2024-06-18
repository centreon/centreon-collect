/**
 * Copyright 2022-2024 Centreon
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
#include "com/centreon/engine/retention/anomalydetection.hh"

using com::centreon::common::opt;
using com::centreon::engine::map_customvar;
using namespace com::centreon::engine::retention;

anomalydetection::anomalydetection() : service(object::anomalydetection) {}

anomalydetection::anomalydetection(anomalydetection const& right)
    : service(right), _sensitivity(right._sensitivity) {}

anomalydetection& anomalydetection::operator=(anomalydetection const& right) {
  service::operator=(right);
  _sensitivity = right._sensitivity;
  return *this;
}

bool anomalydetection::operator==(anomalydetection const& right) const throw() {
  if (!(static_cast<const service&>(*this) ==
        static_cast<const service&>(right))) {
    return false;
  }
  return _sensitivity == right._sensitivity;
}

bool anomalydetection::operator!=(anomalydetection const& right) const throw() {
  return !operator==(right);
}

static constexpr std::string_view _sensitivity_key("sensitivity");
bool anomalydetection::set(char const* key, char const* value) {
  if (_sensitivity_key == key) {
    double sensitivity;
    if (absl::SimpleAtod(value, &sensitivity)) {
      _sensitivity.set(sensitivity);
      return true;
    }
    return false;
  }
  return service::set(key, value);
}
