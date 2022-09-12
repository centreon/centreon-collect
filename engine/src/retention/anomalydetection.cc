/*
** Copyright 2022 Centreon
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include "com/centreon/engine/retention/anomalydetection.hh"

using com::centreon::engine::map_customvar;
using com::centreon::engine::opt;
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

static constexpr absl::string_view _sensitivity_key("sensitivity");
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
