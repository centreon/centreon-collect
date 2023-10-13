/**
 * Copyright 2022 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef CCE_RETENTION_ANOMALYDETECTION_HH
#define CCE_RETENTION_ANOMALYDETECTION_HH

#include "service.hh"

namespace com::centreon::engine::retention {

class anomalydetection : public service {
 protected:
  opt<double> _sensitivity;

 public:
  using pointer = std::shared_ptr<anomalydetection>;

  anomalydetection();
  anomalydetection(anomalydetection const& right);
  anomalydetection& operator=(anomalydetection const& right);
  bool operator==(anomalydetection const& right) const throw();
  bool operator!=(anomalydetection const& right) const throw();
  bool set(char const* key, char const* value) override;

  opt<double> const& sensitivity() const { return _sensitivity; }
};

typedef std::list<anomalydetection::pointer> list_anomalydetection;

}  // namespace com::centreon::engine::retention

#endif
