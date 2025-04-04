/**
 * Copyright 2014-2017, 2021 Centreon
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

#include "com/centreon/broker/bam/configuration/applier/ba.hh"
#include <fmt/format.h>
#include "com/centreon/broker/bam/ba_best.hh"
#include "com/centreon/broker/bam/ba_impact.hh"
#include "com/centreon/broker/bam/ba_ratio_number.hh"
#include "com/centreon/broker/bam/ba_ratio_percent.hh"
#include "com/centreon/broker/bam/ba_worst.hh"
#include "com/centreon/broker/bam/internal.hh"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/broker/neb/host.hh"
#include "com/centreon/broker/neb/service.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::bam::configuration;
using com::centreon::common::log_v2::log_v2;

/**
 * @brief Constructor of an applier of BA.
 *
 * @param logger The logger to use.
 */
applier::ba::ba(const std::shared_ptr<spdlog::logger>& logger)
    : _logger{logger} {}

/**
 *  Copy constructor.
 *
 *  @param[in] other  Object to copy.
 */
applier::ba::ba(applier::ba const& other) {
  _internal_copy(other);
}

/**
 *  Destructor.
 */
applier::ba::~ba() {}

/**
 *  Assignment operator.
 *
 *  @param[in] other  Object to copy.
 *
 *  @return This object.
 */
applier::ba& applier::ba::operator=(applier::ba const& other) {
  if (this != &other)
    _internal_copy(other);
  return *this;
}

/**
 *  Apply configuration.
 *
 *  @param[in] my_bas  BAs to apply.
 *  @param[in] book    The service book.
 */
void applier::ba::apply(const bam::configuration::state::bas& my_bas,
                        service_book& book) {
  //
  // DIFF
  //

  // Objects to delete are items remaining in the
  // set at the end of the iteration.
  std::map<uint32_t, applied> to_delete(_applied);

  // Objects to create are items remaining in the
  // set at the end of the iteration.
  bam::configuration::state::bas to_create(my_bas);

  // Objects to modify are items found but
  // with mismatching configuration.
  std::list<bam::configuration::ba> to_modify;

  // Iterate through configuration.
  for (auto it = to_create.begin(), end = to_create.end(); it != end;) {
    auto cfg_it = to_delete.find(it->first);
    // Found = modify (or not).
    if (cfg_it != to_delete.end()) {
      // Configuration mismatch, modify object.
      if (cfg_it->second.cfg != it->second) {
        if (cfg_it->second.cfg.get_state_source() ==
            it->second.get_state_source())
          to_modify.push_back(it->second);
        else {
          ++it;
          continue;
        }
      }
      to_delete.erase(cfg_it);
      it = to_create.erase(it);
    }
    // Not found = create.
    else
      ++it;
  }

  //
  // OBJECT CREATION/DELETION
  //

  // Delete objects.
  auto bbdo = config::applier::state::instance().get_bbdo_version();
  bool bbdo3_enabled = bbdo.major_v >= 3;
  for (std::map<uint32_t, applied>::iterator it = to_delete.begin(),
                                             end = to_delete.end();
       it != end; ++it) {
    _logger->info("BAM: removing BA {}", it->first);
    std::shared_ptr<io::data> s;
    if (bbdo3_enabled) {
      auto bs = _ba_pb_service(it->first, it->second.cfg.get_host_id(), "",
                               it->second.cfg.get_service_id());
      bs->mut_obj().set_enabled(false);
      s = bs;
    } else {
      auto bs = _ba_service(it->first, it->second.cfg.get_host_id(),
                            it->second.cfg.get_service_id());
      bs->enabled = false;
      s = bs;
    }
    book.unlisten(it->second.cfg.get_host_id(), it->second.cfg.get_service_id(),
                  static_cast<bam::ba*>(it->second.obj.get()));
    _applied.erase(it->first);
    multiplexing::publisher().write(s);
  }
  to_delete.clear();

  // Create new objects.
  for (bam::configuration::state::bas::iterator it = to_create.begin(),
                                                end = to_create.end();
       it != end; ++it) {
    _logger->info("BAM: creating BA {} ('{}')", it->first,
                  it->second.get_name());
    std::shared_ptr<bam::ba> new_ba(_new_ba(it->second, book));
    applied& content(_applied[it->first]);
    content.cfg = it->second;
    content.obj = new_ba;
    if (bbdo3_enabled) {
      std::shared_ptr<neb::pb_host> h(_ba_pb_host(it->second.get_host_id()));
      multiplexing::publisher().write(h);
    } else {
      std::shared_ptr<neb::host> h(_ba_host(it->second.get_host_id()));
      multiplexing::publisher().write(h);
    }
    std::shared_ptr<io::data> s;
    if (bbdo3_enabled)
      s = _ba_pb_service(it->first, it->second.get_host_id(),
                         it->second.get_host_name(),
                         it->second.get_service_id());
    else
      s = _ba_service(it->first, it->second.get_host_id(),
                      it->second.get_service_id());
    multiplexing::publisher().write(s);
  }

  // Modify existing objects.
  for (auto& b : to_modify) {
    std::map<uint32_t, applied>::iterator pos = _applied.find(b.get_id());
    if (pos != _applied.end()) {
      _logger->info("BAM: modifying BA {}", b.get_id());
      pos->second.obj->set_name(b.get_name());
      assert(pos->second.obj->get_state_source() == b.get_state_source());
      pos->second.obj->set_level_warning(b.get_warning_level());
      pos->second.obj->set_level_critical(b.get_critical_level());
      pos->second.cfg = b;
    } else
      _logger->error(
          "BAM: attempting to modify BA {}, however associated object was not "
          "found. This is likely a software bug that you should report to "
          "Centreon Broker developers",
          b.get_id());
  }

  // Set all BA objects as valid. Invalid BAs will be reset as invalid
  // on KPI application.
  for (std::map<uint32_t, applied>::iterator it = _applied.begin(),
                                             end = _applied.end();
       it != end; ++it)
    it->second.obj->set_valid(true);
}

/**
 *  Find BA by its ID.
 *
 *  @param[in] id BA ID.
 *
 *  @return Shared pointer to the applied BA object.
 */
std::shared_ptr<bam::ba> applier::ba::find_ba(uint32_t id) const {
  auto it = _applied.find(id);
  return it != _applied.end() ? it->second.obj : std::shared_ptr<bam::ba>();
}

/**
 *  Visit each applied BA.
 *
 *  @param[out] visitor  Visitor that will receive status.
 */
void applier::ba::visit(io::stream* visitor) {
  for (std::map<uint32_t, applied>::iterator it(_applied.begin()),
       end(_applied.end());
       it != end; ++it)
    it->second.obj->visit(visitor);
}

/**
 *  Get the virtual BA host of a BA.
 *
 *  @param[in] host_id  Host ID.
 *
 *  @return Virtual BA host.
 */
std::shared_ptr<neb::host> applier::ba::_ba_host(uint32_t host_id) {
  std::shared_ptr<neb::host> h(new neb::host);
  h->poller_id =
      com::centreon::broker::config::applier::state::instance().poller_id();
  h->host_id = host_id;
  h->host_name = fmt::format("_Module_BAM_{}", h->poller_id);
  h->last_update = time(nullptr);
  return h;
}

/**
 *  Get the virtual BA host of a BA.
 *
 *  @param[in] host_id  Host ID.
 *
 *  @return Virtual BA host.
 */
std::shared_ptr<neb::pb_host> applier::ba::_ba_pb_host(uint32_t host_id) {
  auto h = std::make_shared<neb::pb_host>();
  auto& o = h->mut_obj();
  o.set_instance_id(
      com::centreon::broker::config::applier::state::instance().poller_id());
  o.set_host_id(host_id);
  o.set_name(fmt::format("_Module_BAM_{}", o.instance_id()));
  o.set_last_update(time(nullptr));
  o.set_enabled(true);
  return h;
}

/**
 *  Get the virtual BA service of a BA.
 *
 *  @param[in] ba_id       BA ID.
 *  @param[in] host_id     Host ID.
 *  @param[in] service_id  Service ID.
 *
 *  @return Virtual BA service.
 */
std::shared_ptr<neb::service> applier::ba::_ba_service(uint32_t ba_id,
                                                       uint32_t host_id,
                                                       uint32_t service_id,
                                                       bool in_downtime) {
  _logger->trace("_ba_service ba {}, service {}:{} with downtime {}", ba_id,
                 host_id, service_id, in_downtime);
  auto s{std::make_shared<neb::service>()};
  s->host_id = host_id;
  s->service_id = service_id;
  s->service_description = fmt::format("ba_{}", ba_id);
  s->display_name = s->service_description;
  s->last_update = time(nullptr);
  s->downtime_depth = in_downtime ? 1 : 0;
  return s;
}

/**
 *  Get the virtual BA service of a BA.
 *
 *  @param[in] ba_id       BA ID.
 *  @param[in] host_id     Host ID.
 *  @param[in] service_id  Service ID.
 *
 *  @return Virtual BA service.
 */
std::shared_ptr<neb::pb_service> applier::ba::_ba_pb_service(
    uint32_t ba_id,
    uint32_t host_id,
    const std::string& host_name,
    uint32_t service_id,
    bool in_downtime) {
  _logger->trace("_ba_pb_service ba {}, service {}:{} with downtime {}", ba_id,
                 host_id, service_id, in_downtime);
  auto s{std::make_shared<neb::pb_service>()};
  auto& o = s->mut_obj();
  o.set_host_id(host_id);
  o.set_service_id(service_id);
  o.set_description(fmt::format("ba_{}", ba_id));
  o.set_host_name(host_name);
  o.set_type(BA);
  o.set_internal_id(ba_id);
  o.set_display_name(o.description());
  o.set_last_update(time(nullptr));
  o.set_scheduled_downtime_depth(in_downtime ? 1 : 0);
  o.set_max_check_attempts(1);
  o.set_enabled(true);
  return s;
}

/**
 *  Copy internal data members.
 *
 *  @param[in] other  Object to copy.
 */
void applier::ba::_internal_copy(applier::ba const& other) {
  _applied = other._applied;
}

/**
 *  Create new BA object.
 *
 *  @param[in] cfg BA configuration.
 *
 *  @return New BA object.
 */
std::shared_ptr<bam::ba> applier::ba::_new_ba(configuration::ba const& cfg,
                                              service_book& book) {
  std::shared_ptr<bam::ba> obj;
  switch (cfg.get_state_source()) {
    case configuration::ba::state_source_impact:
      obj = std::make_shared<bam::ba_impact>(cfg.get_id(), cfg.get_host_id(),
                                             cfg.get_service_id(), false,
                                             _logger);
      break;
    case configuration::ba::state_source_best:
      obj =
          std::make_shared<bam::ba_best>(cfg.get_id(), cfg.get_host_id(),
                                         cfg.get_service_id(), false, _logger);
      break;
    case configuration::ba::state_source_worst:
      obj =
          std::make_shared<bam::ba_worst>(cfg.get_id(), cfg.get_host_id(),
                                          cfg.get_service_id(), false, _logger);
      break;
    case configuration::ba::state_source_ratio_percent:
      obj = std::make_shared<bam::ba_ratio_percent>(
          cfg.get_id(), cfg.get_host_id(), cfg.get_service_id(), false,
          _logger);
      break;
    case configuration::ba::state_source_ratio_number:
      obj = std::make_shared<bam::ba_ratio_number>(
          cfg.get_id(), cfg.get_host_id(), cfg.get_service_id(), false,
          _logger);
      break;
    default:
      /* Should not arrive */
      assert(1 == 0);
      break;
  }
  obj->set_name(cfg.get_name());
  obj->set_level_warning(cfg.get_warning_level());
  obj->set_level_critical(cfg.get_critical_level());
  obj->set_downtime_behaviour(cfg.get_downtime_behaviour());
  if (cfg.get_opened_event().obj().ba_id())
    obj->set_initial_event(cfg.get_opened_event());
  book.listen(cfg.get_host_id(), cfg.get_service_id(), obj.get());
  return obj;
}

/**
 *  Save inherited downtime to the cache.
 *
 *  @param[in] cache  The cache.
 */
void applier::ba::save_to_cache(persistent_cache& cache) {
  for (std::map<uint32_t, applied>::const_iterator it = _applied.begin(),
                                                   end = _applied.end();
       it != end; ++it) {
    it->second.obj->save_inherited_downtime(cache);
  }
}

/**
 *  Load inherited downtime from cache.
 *
 *  @param[in] cache  The cache.
 */
void applier::ba::apply_inherited_downtime(const inherited_downtime& dwn) {
  auto bbdo = config::applier::state::instance().get_bbdo_version();
  bool bbdo3_enabled = bbdo.major_v >= 3;

  std::map<uint32_t, applied>::iterator found = _applied.find(dwn.ba_id);
  if (found != _applied.end()) {
    _logger->debug("BAM: found an inherited downtime for BA {}", found->first);
    found->second.obj->set_inherited_downtime(dwn);
    std::shared_ptr<io::data> s;
    if (bbdo3_enabled)
      s = _ba_pb_service(found->first, found->second.cfg.get_host_id(),
                         found->second.cfg.get_host_name(),
                         found->second.cfg.get_service_id(), dwn.in_downtime);
    else
      s = _ba_service(found->first, found->second.cfg.get_host_id(),
                      found->second.cfg.get_service_id(), dwn.in_downtime);
    multiplexing::publisher().write(s);
  }
}

void applier::ba::apply_inherited_downtime(const pb_inherited_downtime& dwn) {
  auto bbdo = config::applier::state::instance().get_bbdo_version();
  bool bbdo3_enabled = bbdo.major_v >= 3;
  std::map<uint32_t, applied>::iterator found =
      _applied.find(dwn.obj().ba_id());
  if (found != _applied.end()) {
    _logger->debug("BAM: found an inherited downtime for BA {}", found->first);
    found->second.obj->set_inherited_downtime(dwn);
    std::shared_ptr<io::data> s;
    if (bbdo3_enabled)
      s = _ba_pb_service(found->first, found->second.cfg.get_host_id(),
                         found->second.cfg.get_host_name(),
                         found->second.cfg.get_service_id(),
                         dwn.obj().in_downtime());
    else
      s = _ba_service(found->first, found->second.cfg.get_host_id(),
                      found->second.cfg.get_service_id(),
                      dwn.obj().in_downtime());
    multiplexing::publisher().write(s);
  }
}
