/*
** Copyright 2011-2013,2017 Centreon
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

#ifndef CCE_CONFIGURATION_APPLIER_SEVERITY_HH
#define CCE_CONFIGURATION_APPLIER_SEVERITY_HH

#include "com/centreon/engine/namespace.hh"

CCE_BEGIN()

namespace configuration {
class severity;
class state;

namespace applier {
class severity {
 public:
  severity() = default;
  ~severity() noexcept = default;
  severity& operator=(const severity& other) = delete;
  void add_object(const configuration::severity& obj);
  void expand_objects(configuration::state& s);
  void modify_object(const configuration::severity& obj);
  void remove_object(const configuration::severity& obj);
  void resolve_object(const configuration::severity& obj);
};
}  // namespace applier
}  // namespace configuration

CCE_END()

#endif  // !CCE_CONFIGURATION_APPLIER_SEVERITY_HH
