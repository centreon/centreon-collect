/**
 * Copyright 2014-2015, 2021-2024 Centreon
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

#include "com/centreon/broker/bam/service_book.hh"

#include "com/centreon/broker/bam/internal.hh"
#include "com/centreon/broker/neb/downtime.hh"
#include "com/centreon/broker/neb/service_status.hh"

using namespace com::centreon::broker::bam;

static constexpr bool time_is_undefined(uint64_t t) {
  return t == 0 || t == static_cast<uint64_t>(-1);
}

/**
 *  Make a service listener listen to service updates.
 *
 *  @param[in]     host_id     Host ID.
 *  @param[in]     service_id  Service ID.
 *  @param[in,out] listnr      Service listener.
 */
void service_book::listen(uint32_t host_id,
                          uint32_t service_id,
                          service_listener* listnr) {
  _logger->trace("BAM: service ({}, {}) added to service book", host_id,
                 service_id);
  auto found = _book.find(std::make_pair(host_id, service_id));
  if (found == _book.end()) {
    service_state_listeners sl{{listnr},
                               {.host_id = host_id, .service_id = service_id}};
    _book[std::make_pair(host_id, service_id)] = sl;
  } else
    _book[std::make_pair(host_id, service_id)].listeners.push_back(listnr);
}

/**
 *  Remove a listener.
 *
 *  @param[in] host_id     Host ID.
 *  @param[in] service_id  Service ID.
 *  @param[in] listnr      Service listener.
 */
void service_book::unlisten(uint32_t host_id,
                            uint32_t service_id,
                            service_listener* listnr) {
  auto found = _book.find(std::make_pair(host_id, service_id));
  if (found != _book.end()) {
    found->second.listeners.remove_if(
        [listnr](service_listener* item) { return item == listnr; });
    if (found->second.listeners.empty())
      _book.erase(found);
  }
}

/**
 * @brief Propagate events of type neb::pb_acknowledgement to the concerned
 * services and then to the corresponding kpi.
 *
 * @param t The event to handle.
 * @param visitor The stream to write into.
 */
void service_book::update(const std::shared_ptr<neb::pb_acknowledgement>& t,
                          io::stream* visitor) {
  auto found =
      _book.find(std::make_pair(t->obj().host_id(), t->obj().service_id()));
  if (found == _book.end())
    return;
  auto& svc_state = found->second.state;
  svc_state.acknowledged = time_is_undefined(t->obj().deletion_time());
  for (auto l : found->second.listeners)
    l->service_update(t, visitor);
}

/**
 * @brief Propagate events of type neb::acknowledgement to the concerned
 * services and then to the corresponding kpi.
 *
 * @param t The event to handle.
 * @param visitor The stream to write into.
 */
void service_book::update(const std::shared_ptr<neb::acknowledgement>& t,
                          io::stream* visitor) {
  auto found = _book.find(std::make_pair(t->host_id, t->service_id));
  if (found == _book.end())
    return;
  auto& svc_state = found->second.state;
  svc_state.acknowledged = t->deletion_time.is_null();
  for (auto l : found->second.listeners)
    l->service_update(t, visitor);
}

/**
 * @brief Propagate events of type neb::downtime to the concerned services
 * and then to the corresponding kpi.
 *
 * @param t The event to handle.
 * @param visitor The stream to write into.
 */
void service_book::update(const std::shared_ptr<neb::downtime>& t,
                          io::stream* visitor) {
  auto found = _book.find(std::make_pair(t->host_id, t->service_id));
  if (found == _book.end())
    return;
  for (auto l : found->second.listeners)
    l->service_update(t, visitor);
}

/**
 * @brief Propagate events of type neb::pb_downtime to the concerned services
 * and then to the corresponding kpi.
 *
 * @param t The event to handle.
 * @param visitor The stream to write into.
 */
void service_book::update(const std::shared_ptr<neb::pb_downtime>& t,
                          io::stream* visitor) {
  auto found =
      _book.find(std::make_pair(t->obj().host_id(), t->obj().service_id()));
  if (found == _book.end())
    return;
  for (auto l : found->second.listeners)
    l->service_update(t, visitor);
}

/**
 * @brief Propagate events of type neb::service_status to the concerned services
 * and then to the corresponding kpi.
 *
 * @param t The event to handle.
 * @param visitor The stream to write into.
 */
void service_book::update(const std::shared_ptr<neb::service_status>& t,
                          io::stream* visitor) {
  auto found = _book.find(std::make_pair(t->host_id, t->service_id));
  if (found == _book.end())
    return;
  auto& svc_state = found->second.state;
  svc_state.current_state = static_cast<State>(t->current_state);
  svc_state.last_hard_state = static_cast<State>(t->last_hard_state);
  svc_state.last_check = t->last_check;
  svc_state.state_type = t->state_type;

  for (auto l : found->second.listeners)
    l->service_update(t, visitor);
}

/**
 * @brief Propagate events of type neb::service_status to the concerned services
 * and then to the corresponding kpi.
 *
 * @param t The event to handle.
 * @param visitor The stream to write into.
 */
void service_book::update(
    const std::shared_ptr<neb::pb_adaptive_service_status>& t,
    io::stream* visitor) {
  auto obj = t->obj();
  auto found = _book.find(std::make_pair(obj.host_id(), obj.service_id()));
  if (found == _book.end())
    return;

  for (auto l : found->second.listeners)
    l->service_update(t, visitor);
}

/**
 * @brief Propagate events of type pb_service to the
 * concerned services and then to the corresponding kpi.
 *
 * @param t The event to handle.
 * @param visitor The stream to write into.
 */
void service_book::update(const std::shared_ptr<neb::pb_service>& t,
                          io::stream* visitor) {
  auto found =
      _book.find(std::make_pair(t->obj().host_id(), t->obj().service_id()));
  if (found == _book.end())
    return;
  auto& svc_state = found->second.state;
  auto& o = t->obj();
  svc_state.last_check =
      time_is_undefined(o.last_check()) ? std::time(nullptr) : o.last_check();
  svc_state.last_hard_state = static_cast<State>(o.last_hard_state());
  svc_state.current_state = static_cast<State>(o.state());
  svc_state.state_type = o.state_type();

  for (auto l : found->second.listeners)
    l->service_update(t, visitor);
}

/**
 * @brief Propagate events of type pb_service_status to the
 * concerned services and then to the corresponding kpi.
 *
 * @param t The event to handle.
 * @param visitor The stream to write into.
 */
void service_book::update(const std::shared_ptr<neb::pb_service_status>& t,
                          io::stream* visitor) {
  auto found =
      _book.find(std::make_pair(t->obj().host_id(), t->obj().service_id()));
  if (found == _book.end())
    return;
  auto& svc_state = found->second.state;
  auto& o = t->obj();
  svc_state.last_check =
      time_is_undefined(o.last_check()) ? std::time(nullptr) : o.last_check();
  svc_state.last_hard_state = static_cast<State>(o.last_hard_state());
  svc_state.current_state = static_cast<State>(o.state());
  svc_state.state_type = o.state_type();

  for (auto l : found->second.listeners)
    l->service_update(t, visitor);
}

/**
 * @brief Save the services states from the service_book to the cache. The cache
 * must be open with write mode, otherwise, this function throws an exception.
 *
 * @param cache The persistent cache to receive data.
 */
void service_book::save_to_cache(persistent_cache& cache) const {
  auto to_save_ptr = std::make_shared<pb_services_book_state>();
  ServicesBookState& to_save = to_save_ptr->mut_obj();
  for (auto it = _book.begin(); it != _book.end(); ++it) {
    auto& state = it->second.state;
    auto* svc = to_save.add_service();
    svc->set_host_id(state.host_id);
    svc->set_service_id(state.service_id);
    svc->set_current_state(state.current_state);
    svc->set_last_hard_state(state.last_hard_state);
    svc->set_last_check(state.last_check);
    svc->set_state_type(state.state_type);
    svc->set_acknowledged(state.acknowledged);
  }
  cache.add(to_save_ptr);
}

/**
 * @brief Restore from the ServicesBookState the states of all the services
 * involved in BAs.
 *
 * @param state A ServicesBookState get from the cache.
 */
void service_book::apply_services_state(const ServicesBookState& state) {
  _logger->trace("BAM: applying services state from cache");
  for (auto& svc : state.service()) {
    auto found = _book.find(std::make_pair(svc.host_id(), svc.service_id()));
    if (found == _book.end())
      continue;
    auto& svc_state = found->second.state;
    svc_state.current_state = svc.current_state();
    svc_state.last_hard_state = svc.last_hard_state();
    svc_state.last_check = svc.last_check();
    svc_state.state_type = svc.state_type();
    svc_state.acknowledged = svc.acknowledged();
    for (auto l : found->second.listeners)
      l->service_update(svc_state);
  }
  _logger->trace("BAM: Services state applied from cache");
}
