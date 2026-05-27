/**
 * Copyright 2011 - 2013, 2026 Centreon (https://www.centreon.com/)
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
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/comment.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/statusdata.hh"
#include "com/centreon/engine/string.hh"
#include "common/downtimes/downtime_manager.hh"
#include "common/downtimes/host_downtime.hh"
#include "common/downtimes/service_downtime.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::string;

namespace com::centreon::common::downtimes {
downtime::downtime(downtime::type type,
                   const uint64_t host_id,
                   time_t entry_time,
                   const std::string& author,
                   const std::string& comment,
                   time_t start_time,
                   time_t end_time,
                   bool fixed,
                   uint64_t triggered_by,
                   uint32_t duration,
                   uint64_t downtime_id)
    : _type{type},
      _host_id{host_id},
      _entry_time{entry_time},
      _author{author},
      _comment{comment},
      _start_time{start_time},
      _end_time{end_time},
      _fixed{fixed},
      _triggered_by{triggered_by},
      _duration{duration},
      _downtime_id{downtime_id},
      _in_effect{false},
      _comment_id{0},
      _start_flex_downtime{0},
      _incremented_pending_downtime{false} {
  /* don't add triggered downtimes that don't have a valid parent */
  if (triggered_by > 0 && !downtime_manager::instance().find_downtime(
                              downtime::any_downtime, triggered_by))
    throw engine_error()
        << "can not add triggered host downtime without a valid parent";
}

downtime::~downtime() {}

downtime::type downtime::get_type() const {
  return _type;
}

uint64_t downtime::host_id() const {
  return _host_id;
}

const char* downtime::service_description() const {
  return nullptr;
}

const std::string& downtime::get_comment() const {
  return _comment;
}

const std::string& downtime::get_author() const {
  return _author;
}

uint64_t downtime::get_downtime_id() const {
  return _downtime_id;
}

uint64_t downtime::get_triggered_by() const {
  return _triggered_by;
}

bool downtime::is_fixed() const {
  return _fixed;
}

time_t downtime::get_entry_time() const {
  return _entry_time;
}

time_t downtime::get_start_time() const {
  return _start_time;
}

time_t downtime::get_end_time() const {
  return _end_time;
}

uint32_t downtime::get_duration() const {
  return _duration;
}

bool downtime::is_in_effect() const {
  return _in_effect;
}

void downtime::_set_in_effect(bool in_effect) {
  _in_effect = in_effect;
}

uint64_t downtime::_get_comment_id() const {
  return _comment_id;
}

void downtime::start_flex_downtime() {
  _start_flex_downtime = true;
}
}  // namespace com::centreon::common::downtimes

/* handles scheduled downtime (id passed from timed event queue) */
int handle_scheduled_downtime_by_id(uint64_t downtime_id) {
  using namespace com::centreon::common::downtimes;
  std::shared_ptr<downtime> temp_downtime{
      downtime_manager::instance().find_downtime(downtime::any_downtime,
                                                 downtime_id)};
  /* find the downtime entry */
  if (!temp_downtime)
    return ERROR;

  /* handle the downtime */
  return temp_downtime->handle();
}

std::ostream& operator<<(std::ostream& os,
                         com::centreon::common::downtimes::downtime const& dt) {
  dt.print(os);
  return os;
}
