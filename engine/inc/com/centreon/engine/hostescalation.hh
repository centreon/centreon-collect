/*
** Copyright 2011-2019 Centreon
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

#ifndef CCE_HOSTESCALATION_HH
#define CCE_HOSTESCALATION_HH
#include "com/centreon/engine/escalation.hh"

/* Forward declaration. */
namespace com::centreon::engine {
class host;
class hostescalation;
}  // namespace com::centreon::engine

typedef std::unordered_multimap<
    std::string,
    std::shared_ptr<com::centreon::engine::hostescalation>>
    hostescalation_mmap;

namespace com::centreon::engine {
class hostescalation : public escalation {
 public:
  hostescalation(std::string const& host_name,
                 uint32_t first_notification,
                 uint32_t last_notification,
                 double notification_interval,
                 std::string const& escalation_period,
                 uint32_t escalate_on,
                 Uuid const& uuid);
  virtual ~hostescalation();

  std::string const& get_hostname() const;
  bool is_viable(int state, uint32_t notification_number) const override;
  void resolve(int& w, int& e);

  static hostescalation_mmap hostescalations;

 private:
  std::string _hostname;
};

}  // namespace com::centreon::engine

#endif  // !CCE_HOSTESCALATION_HH
