/*
** Copyright 2011-2013 Merethis
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

#ifndef CCE_RETENTION_STATE_HH
#define CCE_RETENTION_STATE_HH

#include "com/centreon/engine/retention/anomalydetection.hh"
#include "com/centreon/engine/retention/comment.hh"
#include "com/centreon/engine/retention/contact.hh"
#include "com/centreon/engine/retention/downtime.hh"
#include "com/centreon/engine/retention/host.hh"
#include "com/centreon/engine/retention/info.hh"
#include "com/centreon/engine/retention/program.hh"
#include "com/centreon/engine/retention/service.hh"

namespace com::centreon::engine {

namespace retention {
class state {
 public:
  state();
  ~state() noexcept;
  state(state const& right);
  state& operator=(state const& right);
  bool operator==(state const& right) const noexcept;
  bool operator!=(state const& right) const noexcept;
  list_comment& comments() noexcept;
  list_comment const& comments() const noexcept;
  list_contact& contacts() noexcept;
  list_contact const& contacts() const noexcept;
  list_downtime& downtimes() noexcept;
  list_downtime const& downtimes() const noexcept;
  program& globals() noexcept;
  program const& globals() const noexcept;
  list_host& hosts() noexcept;
  list_host const& hosts() const noexcept;
  info& informations() noexcept;
  info const& informations() const noexcept;
  list_service& services() noexcept;
  list_service const& services() const noexcept;
  list_anomalydetection& anomalydetection() noexcept {
    return _anomalydetection;
  }
  list_anomalydetection const& anomalydetection() const {
    return _anomalydetection;
  }

 private:
  list_comment _comments;
  list_contact _contacts;
  list_downtime _downtimes;
  list_host _hosts;
  info _info;
  program _globals;
  list_service _services;
  list_anomalydetection _anomalydetection;
};
}  // namespace retention

}

#endif  // !CCE_RETENTION_STATE_HH
