/**
 * Copyright 2013, 2020-2023 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#include "com/centreon/broker/io/events.hh"

#include <cassert>
#include "bbdo/bbdo/ack.hh"
#include "bbdo/bbdo/stop.hh"
#include "bbdo/bbdo/version_response.hh"
#include "com/centreon/broker/bbdo/factory.hh"
#include "com/centreon/broker/instance_broadcast.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::io;

// Class instance.
static events* _instance(nullptr);

/**
 *  Get class instance.
 *
 *  @return Class instance.
 */
events& events::instance() {
  assert(_instance);
  return *_instance;
}

/**
 *  Load singleton.
 */
void events::load() {
  if (!_instance)
    _instance = new events;
}

/**
 *  Unload singleton.
 */
void events::unload() {
  // Delete operator is NULL-aware.
  delete _instance;
  _instance = nullptr;
}

/**
 * @brief Unregister a category.
 *
 * @param category_id Category ID.
 */
void events::unregister_category(uint16_t category_id) {
  for (auto it = _elements.begin(), end = _elements.end(); it != end;) {
    if (category_of_type(it->first) == category_id)
      it = _elements.erase(it);
    else
      ++it;
  }
}

/**
 * @brief Register an event.
 *
 * @param category_id Category ID.
 * @param event_id Event ID within the category.
 * @param name Name of the event.
 * @param ops event operations
 * @param entries entries of this event
 * @param table The table in the database v3
 * @param table_v2 The table in the database v2 (default one).
 *
 * @return The type of this new event.
 */
uint32_t events::register_event(uint32_t type_id,
                                const std::string& name,
                                event_info::event_operations const* ops,
                                mapping::entry const* entries,
                                const std::string& table_v2) {
  auto found = _elements.find(type_id);
  /* The registration is made only if not already done. */
  if (found == _elements.end())
    _elements.emplace(std::piecewise_construct, std::forward_as_tuple(type_id),
                      std::forward_as_tuple(name, ops, entries, table_v2));
  return type_id;
}

/**
 * @brief Register an event.
 *
 * @param event_id Event ID within the category.
 * @param name Name of the event.
 * @param ops event operations
 * @param table The table in the database v2 (default one).
 *
 * @return The type of this new event.
 */
uint32_t events::register_event(uint32_t type_id,
                                const std::string& name,
                                event_info::event_operations const* ops,
                                const std::string& table) {
  auto found = _elements.find(type_id);
  /* The registration is made only if not already done. */
  if (found == _elements.end())
    _elements.emplace(std::piecewise_construct, std::forward_as_tuple(type_id),
                      std::forward_as_tuple(name, ops, nullptr, table));
  return type_id;
}
/**
 *  Unregister an event.
 *
 *  @param[in] type_id  Type ID.
 */
void events::unregister_event(uint32_t type_id) {
  auto it = _elements.find(type_id);
  if (it != _elements.end())
    _elements.erase(it);
}

/**
 *  Get the content of a category.
 *
 *  @param[in] name  Category name.
 *
 *  @return Category elements.
 */
events::events_container events::get_events_by_category_name(
    const std::string& name) const {
  // Special category matching all registered events.
  if (name == "all")
    return _elements;
  else {
    std::unordered_map<uint32_t, event_info> retval;
    uint16_t cat = category_id(name.c_str());
    for (auto it = _elements.begin(), end = _elements.end(); it != end; ++it) {
      if (category_of_type(it->first) == cat)
        retval.emplace(it->first, it->second);
    }
    if (retval.empty())
      throw msg_fmt("cannot find event category '{}'", name);
    return retval;
  }
}

/**
 *  Get an event information structure.
 *
 *  @param[in] type  Event type ID.
 *
 *  @return Event information structure if found, NULL otherwise.
 */
const event_info* events::get_event_info(uint32_t type) {
  auto it = _elements.find(type);
  if (it != _elements.end())
    return &it->second;
  return nullptr;
}

/**
 *  Get all the events matching this name.
 *
 *  If it's a category name, get the content of the category.
 *  If it's a category name followed by : and the name of an event,
 *  get this event.
 *
 *  @param[in] name  The name.
 *
 *  @return  A list of all the matching events.
 */
events::events_container events::get_matching_events(
    const std::string& name) const {
  size_t num = std::count(name.begin(), name.end(), ':');
  if (num == 0)
    return get_events_by_category_name(name);
  else if (num == 1) {
    size_t place = name.find_first_of(':');
    std::string category_name = name.substr(0, place);
    events::events_container const& events =
        get_events_by_category_name(category_name);
    std::string event_name = name.substr(place + 1);
    for (events::events_container::const_iterator it(events.begin()),
         end(events.end());
         it != end; ++it) {
      if (it->second.get_name() == event_name) {
        events::events_container res;
        res.emplace(it->first, it->second);
        return res;
      }
    }
    throw msg_fmt("core: cannot find event '{}'in '{}'", event_name, name);
  } else
    throw msg_fmt("core: too many ':' in '{}'", name);
}

/**
 *  Default constructor.
 */
events::events() {
  // Register instance_broadcast
  register_event(make_type(io::internal, io::events::de_instance_broadcast),
                 "instance_broadcast", &instance_broadcast::operations,
                 instance_broadcast::entries);

  // Register BBDO events.
  register_event(make_type(io::bbdo, bbdo::de_version_response),
                 "version_response", &bbdo::version_response::operations,
                 bbdo::version_response::entries);
  register_event(make_type(io::bbdo, bbdo::de_welcome), "welcome",
                 &bbdo::pb_welcome::operations);
  register_event(make_type(io::bbdo, bbdo::de_ack), "ack",
                 &bbdo::ack::operations, bbdo::ack::entries);
  register_event(make_type(io::bbdo, bbdo::de_stop), "stop",
                 &bbdo::stop::operations, bbdo::stop::entries);
  register_event(make_type(io::bbdo, bbdo::de_pb_ack), "Ack",
                 &bbdo::pb_ack::operations);
  register_event(make_type(io::bbdo, bbdo::de_pb_stop), "Stop",
                 &bbdo::pb_stop::operations);
  register_event(bbdo::pb_bench::static_type(), "Bench",
                 &bbdo::pb_bench::operations);
  register_event(bbdo::pb_engine_configuration::static_type(),
                 "EngineConfiguration",
                 &bbdo::pb_engine_configuration::operations);
  register_event(neb::pb_instance_broadcast::static_type(), "InstanceBroadcast",
                 &neb::pb_instance_broadcast::operations);

  // Register BBDO protocol.
  io::protocols::instance().reg("BBDO", std::make_shared<bbdo::factory>(), 7,
                                7);
}

/**
 *  Destructor.
 */
events::~events() {
  // Unregister BBDO protocol.
  io::protocols::instance().unreg("BBDO");
}
