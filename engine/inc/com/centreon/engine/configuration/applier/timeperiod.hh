/*
** Copyright 2011-2013,2017,2023 Centreon
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

#ifndef CCE_CONFIGURATION_APPLIER_TIMEPERIOD_HH
#define CCE_CONFIGURATION_APPLIER_TIMEPERIOD_HH

#include "com/centreon/engine/timeperiod.hh"
#include "configuration/state.pb.h"

// Forward declaration.
CCE_BEGIN()

namespace configuration {
// Forward declarations.
class state;
class timeperiod;

namespace applier {
class timeperiod {
 public:
  /**
   * @brief Default constructor.
   */
  timeperiod() = default;
  timeperiod(timeperiod const& right);
  /**
   * @brief Destructor.
   */
  ~timeperiod() noexcept = default;
  timeperiod& operator=(timeperiod const&) = delete;
  void add_object(const configuration::Timeperiod& obj);
  void add_object(const configuration::timeperiod& obj);
  void expand_objects(configuration::state& s);
  void expand_objects(configuration::State& s);
  void modify_object(configuration::timeperiod const& obj);
  void remove_object(ssize_t idx);
  void remove_object(configuration::timeperiod const& obj);
  void resolve_object(const configuration::Timeperiod& obj);
  void resolve_object(configuration::timeperiod const& obj);

 private:
  void _add_exclusions(std::set<std::string> const& exclusions,
                       com::centreon::engine::timeperiod* tp);
};
}  // namespace applier
}  // namespace configuration

CCE_END()

#endif  // !CCE_CONFIGURATION_APPLIER_TIMEPERIOD_HH
