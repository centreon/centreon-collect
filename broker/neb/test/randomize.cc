/**
 * Copyright 2011, 2019-2021 Centreon (https://www.centreon.com/)
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

#include "randomize.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/mapping/entry.hh"
#include "com/centreon/broker/mapping/property.hh"
#include "com/centreon/broker/mapping/source.hh"
#include "com/centreon/broker/neb/events.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

static std::list<char*> generated;

/**
 *  Randomize an object.
 *
 *  @param[out]    t        Base object.
 *  @param[out]    values   Generated values.
 */
namespace com::centreon::broker {
void randomize(io::data& t, std::vector<randval>* values) {
  using namespace com::centreon::exceptions;
  using namespace com::centreon::broker;
  io::event_info const* info(io::events::instance().get_event_info(t.type()));
  if (!info)
    throw msg_fmt("cannot find mapping for type {}", t.type());
  for (mapping::entry const* current_entry(info->get_mapping());
       !current_entry->is_null(); ++current_entry) {
    randval r;
    switch (current_entry->get_type()) {
      case mapping::source::BOOL: {
        r.b = ((rand() % 2) ? true : false);
        current_entry->set_bool(t, r.b);
      } break;
      case mapping::source::DOUBLE: {
        r.d = rand() + (rand() / 100000.0);
        current_entry->set_double(t, r.d);
      } break;
      case mapping::source::INT: {
        r.i = rand();
        current_entry->set_int(t, r.i);
      } break;
      case mapping::source::SHORT: {
        r.s = rand();
        current_entry->set_short(t, r.s);
      } break;
      case mapping::source::STRING: {
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), "%d", rand());
        size_t size = strlen(buffer);
        r.S = new char[size + 1];
        generated.push_back(r.S);
        strncpy(r.S, buffer, size + 1);
        r.S[size] = 0;
        current_entry->set_string(t, r.S);
      } break;
      case mapping::source::TIME: {
        r.t = rand();
        current_entry->set_time(t, r.t);
      } break;
      case mapping::source::UINT: {
        r.u = rand();
        current_entry->set_uint(t, r.u);
      } break;
    }
    if (values)
      values->push_back(r);
  }
  return;
}

/**
 *  Initialize randomization engine.
 */
void randomize_init() {
  io::protocols::load();
  io::events::load();
  io::events& e(io::events::instance());
  e.register_event(make_type(io::neb, neb::de_acknowledgement),
                   "acknowledgement", &neb::acknowledgement::operations,
                   neb::acknowledgement::entries);
  e.register_event(make_type(io::neb, neb::de_custom_variable),
                   "custom_variable", &neb::custom_variable::operations,
                   neb::custom_variable::entries);
  e.register_event(make_type(io::neb, neb::de_custom_variable_status),
                   "custom_variable_status",
                   &neb::custom_variable_status::operations,
                   neb::custom_variable_status::entries);
  e.register_event(make_type(io::neb, neb::de_downtime), "downtime",
                   &neb::downtime::operations, neb::downtime::entries);
  e.register_event(make_type(io::neb, neb::de_host_check), "host_check",
                   &neb::host_check::operations, neb::host_check::entries);
  e.register_event(make_type(io::neb, neb::de_host), "host",
                   &neb::host::operations, neb::host::entries);
  e.register_event(make_type(io::neb, neb::de_host_parent), "host_parent",
                   &neb::host_parent::operations, neb::host_parent::entries);
  e.register_event(make_type(io::neb, neb::de_host_status), "host_status",
                   &neb::host_status::operations, neb::host_status::entries);
  e.register_event(make_type(io::neb, neb::de_instance), "instance",
                   &neb::instance::operations, neb::instance::entries);
  e.register_event(make_type(io::neb, neb::de_instance_status),
                   "instance_status", &neb::instance_status::operations,
                   neb::instance_status::entries);
  e.register_event(make_type(io::neb, neb::de_log_entry), "log_entry",
                   &neb::log_entry::operations, neb::log_entry::entries);
  e.register_event(make_type(io::neb, neb::de_service_check), "service_check",
                   &neb::service_check::operations,
                   neb::service_check::entries);
  e.register_event(make_type(io::neb, neb::de_service), "service",
                   &neb::service::operations, neb::service::entries);
  e.register_event(make_type(io::neb, neb::de_service_status), "service_status",
                   &neb::service_status::operations,
                   neb::service_status::entries);
}

/**
 *  Delete memory used for generation.
 */
void randomize_cleanup() {
  for (std::list<char*>::iterator it(generated.begin()), end(generated.end());
       it != end; ++it)
    delete[] *it;
  generated.clear();
  io::events::unload();
  io::protocols::unload();
}

}  // namespace com::centreon::broker
