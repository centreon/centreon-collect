/**
 * Copyright 2025-2026 Centreon
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
#include <boost/preprocessor/seq/for_each.hpp>

#include <memory>
#include "absl/synchronization/mutex.h"
#include "bbdo/bam/dimension_ba_bv_relation_event.hh"
#include "bbdo/events.hh"
#include "bbdo/storage/index_mapping.hh"
#include "broker/core/cache/broker_cache.hh"
#include "broker/core/config/applier/state.hh"
#include "com/centreon/broker/neb/bbdo2_to_bbdo3.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "common/engine_conf/state.pb.h"

namespace com::centreon::broker::cache {

/**
 * @brief Constructor
 *
 * @param logger Logger instance
 */
broker_cache::broker_cache(std::shared_ptr<spdlog::logger> logger)
    : _logger{std::move(logger)} {  // logger: log_v2::CORE
  auto& state = config::applier::state::instance();
  if (!state.supports_centralized_conf()) {
    /* Here, we are in legacy mode. We have to load the cache. If Broker is
     * restarted, it must keep the same cache. */
    _load_cache();
  }
}

/**
 * @brief Destructor
 */
broker_cache::~broker_cache() noexcept {
  if (!config::applier::state::instance().supports_centralized_conf()) {
    /* Here, we are in legacy mode. We have to save the cache. If Broker is
     * restarted, it must keep the same cache. */
    _save_cache();
  }
}

/**
 * @brief Merge a configuration state into the cache. Used when a poller
 * established a connection to Broker.
 *
 * @param state Configuration state to merge
 */
void broker_cache::merge(
    const com::centreon::engine::configuration::State& state) {
  absl::WriterMutexLock lck{&_mutex};

  /* Work on instances */
  _instances.insert_or_assign(state.poller_id(), state.poller_name());

  /* Work on severities */
  absl::flat_hash_map<std::pair<uint64_t, uint32_t>, uint32_t> severities;
  for (const engine::configuration::Severity& sev : state.severities()) {
    auto key = std::make_pair(sev.key().id(), sev.key().type());
    severities.emplace(key, sev.level());
  }

  /* Work on hosts */
  auto& index = _hosts.get<by_id>();
  for (const engine::configuration::Host& host : state.hosts()) {
    auto h = std::make_shared<neb::pb_host>();
    _fill_host(&h->mut_obj(), host);
    if (uint64_t severity_id = h->mut_obj().severity_id(); severity_id) {
      uint32_t level =
          severities[{severity_id, engine::configuration::SeverityType::host}];
      _severities.insert_or_assign(std::make_pair(host.host_id(), 0), level);
    }
    auto [it, inserted] = index.insert(h);
    if (!inserted)
      index.replace(it, h);
  }

  /* Work on hostgroups */
  auto& hg_index = _hostgroups.get<by_id>();
  auto fill_hostgroup = [](HostGroup& obj,
                           const engine::configuration::Hostgroup& hg) {
    obj.set_hostgroup_id(hg.hostgroup_id());
    obj.set_name(hg.hostgroup_name());
    obj.set_enabled(true);
    /* Hostgroups are linked to several pollers, so we set poller_id to 0.
     */
    obj.set_poller_id(0);
    obj.set_alias(hg.alias());
  };

  for (const auto& hg : state.hostgroups()) {
    auto found = hg_index.find(hg.hostgroup_id());
    bool inserted = false;
    if (found == hg_index.end()) {
      auto hostgroup = std::make_shared<neb::pb_host_group>();
      fill_hostgroup(hostgroup->mut_obj(), hg);
      std::tie(found, inserted) = hg_index.emplace(
          hostgroup, absl::flat_hash_set<uint64_t>{hg.poller_id()});
    } else {
      /* We can const_cast because keys of the multiindex are in found->first,
       * we don't change found->first here even if the hostgroup changed. */
      absl::flat_hash_set<uint64_t>& set =
          const_cast<absl::flat_hash_set<uint64_t>&>(found->second);
      if (found->first->obj().name() != hg.hostgroup_name()) {
        auto extracted = std::move(
            const_cast<std::pair<std::shared_ptr<neb::pb_host_group>,
                                 absl::flat_hash_set<uint64_t>>&>(*found));
        hg_index.erase(found);
        extracted.first->mut_obj().set_name(hg.hostgroup_name());
        hg_index.insert(std::move(extracted));
      }
      auto& obj = const_cast<HostGroup&>(found->first->mut_obj());
      obj.set_enabled(true);
      obj.set_alias(hg.alias());
      set.insert(hg.poller_id());
    }
    for (const auto& member : hg.members().data()) {
      auto& index = _hosts.get<by_name>();
      auto host_it = index.find(member);
      if (host_it == index.end())
        continue;

      uint64_t host_id = (*host_it)->obj().host_id();
      _host_hostgroups.insert({host_id, found->first});
    }
  }

  /* Work on services */
  auto& index_svc = _services.get<by_id>();
  for (const engine::configuration::Service& svc : state.services()) {
    auto s = std::make_shared<neb::pb_service>();
    _fill_service(&s->mut_obj(), svc);
    if (uint64_t severity_id = s->mut_obj().severity_id(); severity_id) {
      uint32_t level = severities[{
          severity_id, engine::configuration::SeverityType::service}];
      _severities.insert_or_assign(
          std::make_pair(svc.host_id(), svc.service_id()), level);
    }
    auto [it, inserted] = index_svc.insert(s);
    if (!inserted)
      index_svc.replace(it, s);
  }

  /* Work on anomaly detections */
  for (const engine::configuration::Anomalydetection& ad :
       state.anomalydetections()) {
    auto s = std::make_shared<neb::pb_service>();
    _fill_anomaly_detection(&s->mut_obj(), ad);
    if (uint64_t severity_id = s->mut_obj().severity_id(); severity_id) {
      uint32_t level = severities[{
          severity_id, engine::configuration::SeverityType::service}];
      _severities.insert_or_assign(
          std::make_pair(ad.host_id(), ad.service_id()), level);
    }
    auto [it, inserted] = index_svc.insert(s);
    if (!inserted)
      index_svc.replace(it, s);
  }

  /* Work on servicegroups */
  auto& sg_index = _servicegroups.get<by_id>();
  auto fill_servicegroup = [](ServiceGroup& obj,
                              const engine::configuration::Servicegroup& sg) {
    obj.set_servicegroup_id(sg.servicegroup_id());
    obj.set_name(sg.servicegroup_name());
    obj.set_enabled(true);
    obj.set_poller_id(sg.poller_id());
    obj.set_alias(sg.alias());
  };

  for (const auto& sg : state.servicegroups()) {
    auto found = _servicegroups.find(sg.servicegroup_id());
    bool inserted = false;
    if (found == sg_index.end()) {
      auto servicegroup = std::make_shared<neb::pb_service_group>();
      fill_servicegroup(servicegroup->mut_obj(), sg);
      std::tie(found, inserted) = sg_index.emplace(
          servicegroup, absl::flat_hash_set<uint64_t>{sg.poller_id()});
    } else {
      /* We can const_cast because keys of the multiindex are in found->first,
       * we don't change found->first here even if the servicegroup changed.
       */
      absl::flat_hash_set<uint64_t>& set =
          const_cast<absl::flat_hash_set<uint64_t>&>(found->second);
      if (found->first->obj().name() != sg.servicegroup_name()) {
        auto extracted = std::move(
            const_cast<std::pair<std::shared_ptr<neb::pb_service_group>,
                                 absl::flat_hash_set<uint64_t>>&>(*found));
        sg_index.erase(found);
        extracted.first->mut_obj().set_name(sg.servicegroup_name());
        sg_index.insert(std::move(extracted));
      }
      auto& obj = const_cast<ServiceGroup&>(found->first->mut_obj());
      obj.set_enabled(true);
      obj.set_alias(sg.alias());
      set.insert(sg.poller_id());
    }
    for (const auto& member : sg.members().data()) {
      auto& index = _services.get<by_name>();
      auto service_it =
          index.find(std::make_pair(member.first(), member.second()));
      if (service_it == index.end())
        continue;

      uint64_t host_id = (*service_it)->obj().host_id();
      uint64_t service_id = (*service_it)->obj().service_id();
      auto key = std::make_tuple(host_id, service_id, sg.servicegroup_id());

      _service_servicegroups.try_emplace(key, found->first);
    }
  }
}

/**
 * @brief Apply a configuration state difference into the cache. Used when some
 * new Engine configurations are pushed by a user.
 *
 * @param diff Configuration state difference
 */
void broker_cache::apply(
    const com::centreon::engine::configuration::DiffState& diff) {
  absl::WriterMutexLock lck{&_mutex};

  // /* The easy case: when the diff is not really a diff */
  // if (diff.has_state()) {
  //   merge(diff.state());
  //   return;
  // }

  /* Work on instances */
  //   if (diff.has_poller_name())
  //     _instances.insert_or_assign(diff.poller_id(), diff.poller_name());

  /* Work on severities */
  absl::flat_hash_map<std::pair<uint64_t, uint32_t>, uint32_t> severities;
  for (const engine::configuration::Severity& sev : diff.severities().added()) {
    auto key = std::make_pair(sev.key().id(), sev.key().type());
    _severities.emplace(key, sev.level());
  }
  for (const engine::configuration::Severity& sev :
       diff.severities().modified()) {
    auto key = std::make_pair(sev.key().id(), sev.key().type());
    severities.insert_or_assign(key, sev.level());
  }
  for (const engine::configuration::KeyType& key :
       diff.severities().removed()) {
    auto sev_key = std::make_pair(key.id(), key.type());
    severities.erase(sev_key);
  }

  /* Removing hostgroups */
  auto& hg_index = _hostgroups.get<by_name>();
  for (const auto& hgp : diff.hostgroups().removed()) {
    _logger->debug("Removing hostgroup '{}' from poller {}", hgp.group_name(),
                   hgp.poller_id());
    if (auto hg = hg_index.find(hgp.group_name()); hg != hg_index.end()) {
      /* If the hostgroup is still in the cache, we have to remove all the
       * members of the hostgroup from the cache, because we don't know if they
       * are still members or not. */
      auto& indexed_by_hostgroup = _host_hostgroups.get<by_hostgroup>();
      auto [lower, upper] =
          indexed_by_hostgroup.equal_range(hg->first->obj().hostgroup_id());
      for (; lower != upper;) {
        auto hst = _hosts.find(lower->host_id);
        uint64_t poller_id = 0;
        if (hst != _hosts.end())
          poller_id = (*hst)->obj().instance_id();

        if (poller_id == hgp.poller_id())
          lower = indexed_by_hostgroup.erase(lower);
        else
          ++lower;
      }
      auto& set = const_cast<absl::flat_hash_set<uint64_t>&>(hg->second);
      set.erase(hgp.poller_id());
      if (set.empty()) {
        /* If no poller needs this hostgroup anymore, we can remove it from the
         * cache. */
        hg_index.erase(hg);
      }
    }
  }

  /* Work on hosts */
  auto& h_index = _hosts.get<by_id>();

  /* Adding hosts */
  for (const engine::configuration::Host& host : diff.hosts().added()) {
    auto h = std::make_shared<neb::pb_host>();
    _fill_host(&h->mut_obj(), host);
    auto [it, inserted] = h_index.insert(h);
    if (!inserted)
      h_index.replace(it, h);
  }

  /* Modifying hosts */
  for (const engine::configuration::Host& host : diff.hosts().modified()) {
    auto h = std::make_shared<neb::pb_host>();
    _fill_host(&h->mut_obj(), host);
    auto [it, inserted] = h_index.insert(h);
    if (!inserted)
      h_index.replace(it, h);
  }

  /* Removing hosts */
  for (uint64_t host_id : diff.hosts().removed())
    h_index.erase(host_id);

  /* Work on hostgroups */
  auto feed_hostgroup = [&](const engine::configuration::Hostgroup& hg,
                            bool add) {
    auto& hg_index = _hostgroups.get<by_id>();
    auto found = hg_index.find(hg.hostgroup_id());
    bool inserted = false;
    if (found == hg_index.end()) {
      auto hostgroup = std::make_shared<neb::pb_host_group>();
      auto& obj = hostgroup->mut_obj();
      // fill_hostgroup(hostgroup->mut_obj(), hg);
      obj.set_hostgroup_id(hg.hostgroup_id());
      obj.set_name(hg.hostgroup_name());
      obj.set_enabled(true);
      /* Hostgroups are linked to several pollers, so we set poller_id to 0. */
      obj.set_poller_id(0);
      obj.set_alias(hg.alias());
      std::tie(found, inserted) = hg_index.emplace(
          hostgroup, absl::flat_hash_set<uint64_t>{hg.poller_id()});
    } else {
      absl::flat_hash_set<uint64_t>& set =
          const_cast<absl::flat_hash_set<uint64_t>&>(found->second);
      if (found->first->obj().name() != hg.hostgroup_name()) {
        auto extracted = std::move(
            const_cast<std::pair<std::shared_ptr<neb::pb_host_group>,
                                 absl::flat_hash_set<uint64_t>>&>(*found));
        hg_index.erase(found);
        extracted.first->mut_obj().set_name(hg.hostgroup_name());
        hg_index.insert(std::move(extracted));
      }
      auto& obj = const_cast<HostGroup&>(found->first->mut_obj());
      obj.set_enabled(true);
      obj.set_alias(hg.alias());
      set.insert(hg.poller_id());
    }
    if (!add) {
      /* If it's not an addition, we have to remove all the previous members of
       * the hostgroup from the cache, because we don't know if they are still
       * members or not. */
      auto& indexed_by_hostgroup = _host_hostgroups.get<by_hostgroup>();
      auto [lower, upper] =
          indexed_by_hostgroup.equal_range(found->first->obj().hostgroup_id());
      indexed_by_hostgroup.erase(lower, upper);
    }

    auto& index = _hosts.get<by_name>();
    for (const auto& member : hg.members().data()) {
      auto host_it = index.find(member);
      if (host_it == index.end())
        continue;

      uint64_t host_id = (*host_it)->obj().host_id();

      _logger->debug("Linking host id {} to hostgroup id {}", host_id,
                     hg.hostgroup_id());
      _host_hostgroups.insert({host_id, found->first});
    }
  };

  /* Adding hostgroups */
  for (const auto& hg : diff.hostgroups().added()) {
    _logger->debug("Adding hostgroup '{}' (id {})", hg.hostgroup_name(),
                   hg.hostgroup_id());
    feed_hostgroup(hg, true);
  }

  /* Modifying hostgroups */
  for (const auto& hg : diff.hostgroups().modified()) {
    _logger->debug("Modifying hostgroup '{}' (id {})", hg.hostgroup_name(),
                   hg.hostgroup_id());
    feed_hostgroup(hg, false);
  }

  //   /* Work on services */
  //   auto& s_index = _services.get<by_id>();
  //   auto feed_service = [&](const std::shared_ptr<neb::pb_service>& s,
  //                           const engine::configuration::Service& service) {
  //     _fill_service(&s->mut_obj(), service);
  //     if (uint64_t severity_id = s->mut_obj().severity_id(); severity_id) {
  //       uint32_t level = severities[{
  //           severity_id, engine::configuration::SeverityType::service}];
  //       _severities.insert_or_assign(
  //           std::make_pair(service.host_id(), service.service_id()), level);
  //     }
  //   };
  //
  //   /* Adding services */
  //   for (const engine::configuration::Service& svc : diff.services().added())
  //   {
  //     auto s = std::make_shared<neb::pb_service>();
  //     feed_service(s, svc);
  //     auto [it, inserted] = s_index.insert(s);
  //     if (!inserted)
  //       s_index.replace(it, s);
  //   }
  //
  //   /* Modifying services */
  //   for (const engine::configuration::Service& svc :
  //   diff.services().modified()) {
  //     auto s = std::make_shared<neb::pb_service>();
  //     feed_service(s, svc);
  //     auto [it, inserted] = s_index.insert(s);
  //     if (!inserted)
  //       s_index.replace(it, s);
  //   }
  //
  //   /* Removing services */
  //   for (const auto& key : diff.services().removed())
  //     s_index.erase(std::make_pair(key.host_id(), key.service_id()));
  //
  //   /* Work on anomaly detections */
  //   auto feed_ad = [&](const std::shared_ptr<neb::pb_service>& s,
  //                      const engine::configuration::Anomalydetection& ad) {
  //     _fill_anomaly_detection(&s->mut_obj(), ad);
  //     if (uint64_t severity_id = s->mut_obj().severity_id(); severity_id) {
  //       uint32_t level = severities[{
  //           severity_id, engine::configuration::SeverityType::service}];
  //       _severities.insert_or_assign(
  //           std::make_pair(ad.host_id(), ad.service_id()), level);
  //     }
  //   };
  //
  //   /* Adding anomaly detections */
  //   for (const engine::configuration::Anomalydetection& ad :
  //        diff.anomalydetections().added()) {
  //     auto s = std::make_shared<neb::pb_service>();
  //     feed_ad(s, ad);
  //     auto [it, inserted] = s_index.insert(s);
  //     if (!inserted)
  //       s_index.replace(it, s);
  //   }
  //
  //   /* Modifying anomaly detections */
  //   for (const engine::configuration::Anomalydetection& ad :
  //        diff.anomalydetections().modified()) {
  //     auto s = std::make_shared<neb::pb_service>();
  //     feed_ad(s, ad);
  //     auto [it, inserted] = s_index.insert(s);
  //     if (!inserted)
  //       s_index.replace(it, s);
  //   }
  //
  //   /* Removing anomaly detections */
  //   for (const auto& key : diff.anomalydetections().removed())
  //     s_index.erase(std::make_pair(key.host_id(), key.service_id()));
  //
  //   /* Work on servicegroups */
  //   auto feed_servicegroup = [&](const engine::configuration::Servicegroup&
  //   sg)
  //   {
  //     auto& sg_index = _servicegroups;
  //     auto it = sg_index.find(sg.servicegroup_id());
  //     bool inserted = false;
  //     if (it == sg_index.end()) {
  //       auto servicegroup = std::make_shared<neb::pb_service_group>();
  //       auto& obj = servicegroup->mut_obj();
  //       obj.set_servicegroup_id(sg.servicegroup_id());
  //       obj.set_name(sg.servicegroup_name());
  //       obj.set_enabled(true);
  //       obj.set_poller_id(sg.poller_id());
  //       obj.set_alias(sg.alias());
  //       std::tie(it, inserted) = sg_index.emplace(
  //           servicegroup, absl::flat_hash_set<uint64_t>{sg.poller_id()});
  //     } else {
  //       absl::flat_hash_set<uint64_t>& set =
  //           const_cast<absl::flat_hash_set<uint64_t>&>(it->second);
  //       if (it->first->obj().name() != sg.servicegroup_name()) {
  //         auto extracted = std::move(
  //             const_cast<std::pair<std::shared_ptr<neb::pb_service_group>,
  //                                  absl::flat_hash_set<uint64_t>>&>(*it));
  //         sg_index.erase(it);
  //         extracted.first->mut_obj().set_name(sg.servicegroup_name());
  //         sg_index.insert(std::move(extracted));
  //       }
  //       auto& obj = it->first->mut_obj();
  //       obj.set_enabled(true);
  //       obj.set_alias(sg.alias());
  //       set.insert(sg.poller_id());
  //     }
  //     for (const auto& member : sg.members().data()) {
  //       auto& index = _services.get<by_name>();
  //       auto service_it =
  //           index.find(std::make_pair(member.first(), member.second()));
  //       if (service_it == index.end())
  //         continue;
  //
  //       uint64_t host_id = (*service_it)->obj().host_id();
  //       uint64_t service_id = (*service_it)->obj().service_id();
  //       auto key = std::make_tuple(host_id, service_id,
  //       sg.servicegroup_id());
  //
  //       _service_servicegroups.try_emplace(key, it->first);
  //     }
  //   };
  //
  //   /* Adding servicegroups */
  //   for (const auto& sg : diff.servicegroups().added()) {
  //     feed_servicegroup(sg);
  //   }
  //
  //   /* Modifying servicegroups */
  //   for (const auto& sg : diff.servicegroups().modified()) {
  //     feed_servicegroup(sg);
  //   }
  //
  //   /* Removing servicegroups */
  //   auto& sg_index = _servicegroups.get<by_name>();
  //   for (const auto& sgp : diff.servicegroups().removed())
  //     sg_index.erase(sgp.group_name());
}

/**
 * @brief We use BOOST_PP_SEQ_FOR_EACH to repeat setter without having to write
 * each of them.
 * Example:
 * BOOST_PP_SEQ_FOR_EACH(translate, ,(host_id)(acknowledged))
 * expands to:
 * ```code
 *   obj->set_host_id(in.host_id());
 *   obj->set_acknowledged(in.acknowledged());
 * ```
 */
#define set_proto(attrib) obj->set_##attrib(cfg.attrib());
#define translate(not_used_1, not_used_2, seq_head) set_proto(seq_head)

/**
 * @brief Fill a Host protobuf object from a configuration Host object.
 *
 * @param obj The protobuf Host object to fill
 * @param cfg The configuration Host object to use as source
 */
void broker_cache::_fill_host(Host* obj,
                              const engine::configuration::Host& cfg) {
  if (cfg.poller_id() == 0) {
    _logger->warn("Host '{}' (id {}) has poller_id 0, which is not valid",
                  cfg.host_name(), cfg.host_id());
    return;
  }
  BOOST_PP_SEQ_FOR_EACH(translate, ,
                        (host_id)(action_url)(address)(alias)(check_command)(check_freshness)(check_interval)(check_period)(display_name)(event_handler)(event_handler_enabled)(first_notification_delay)(freshness_threshold)(high_flap_threshold)(icon_image)(icon_image_alt)(low_flap_threshold)(max_check_attempts)(notes)(notes_url)(notification_interval)(notification_period)(obsess_over_host)(retain_nonstatus_information)(retain_status_information)(retry_interval)(statusmap_image)(timezone)(severity_id)(icon_id));
  obj->set_name(cfg.host_name());
  obj->set_active_checks(cfg.checks_active());
  obj->set_passive_checks(cfg.checks_passive());
  obj->set_default_flap_detection(cfg.flap_detection_enabled());
  obj->set_flap_detection_on_down(
      cfg.flap_detection_options() &
      engine::configuration::ActionHostOn::action_hst_down);
  obj->set_flap_detection_on_up(
      cfg.flap_detection_options() &
      engine::configuration::ActionHostOn::action_hst_up);
  obj->set_flap_detection_on_unreachable(
      cfg.flap_detection_options() &
      engine::configuration::ActionHostOn::action_hst_unreachable);
  obj->set_notify_on_down(cfg.notification_options() &
                          engine::configuration::ActionHostOn::action_hst_down);
  obj->set_notify_on_recovery(
      cfg.notification_options() &
      engine::configuration::ActionHostOn::action_hst_up);
  obj->set_notify_on_unreachable(
      cfg.notification_options() &
      engine::configuration::ActionHostOn::action_hst_unreachable);
  obj->set_notify_on_flapping(
      cfg.notification_options() &
      engine::configuration::ActionHostOn::action_hst_flapping);
  obj->set_notify_on_downtime(
      cfg.notification_options() &
      engine::configuration::ActionHostOn::action_hst_downtime);
  obj->set_stalk_on_down(cfg.stalking_options() &
                         engine::configuration::ActionHostOn::action_hst_down);
  obj->set_stalk_on_up(cfg.stalking_options() &
                       engine::configuration::ActionHostOn::action_hst_up);
  obj->set_stalk_on_unreachable(
      cfg.stalking_options() &
      engine::configuration::ActionHostOn::action_hst_unreachable);
  for (const auto& tag : cfg.tags()) {
    auto* t = obj->add_tags();
    t->set_id(tag.first());
    t->set_type(static_cast<TagType>(tag.second()));
  }
  obj->set_instance_id(cfg.poller_id());
}

/**
 * @brief Fill a Service protobuf object from a configuration Service object.
 *
 * @param obj The protobuf Service object to fill
 * @param cfg The configuration Service object to use as source
 */
void broker_cache::_fill_service(Service* obj,
                                 const engine::configuration::Service& cfg) {
  BOOST_PP_SEQ_FOR_EACH(translate, ,
                        (host_id)(service_id)(action_url)(check_command)(check_freshness)(check_interval)(check_period)(display_name)(event_handler)(display_name)(event_handler)(first_notification_delay)(freshness_threshold)(high_flap_threshold)(host_name)(icon_image)(icon_image_alt)(is_volatile)(low_flap_threshold)(max_check_attempts)(notes)(notes_url)(notification_interval)(notification_period)(obsess_over_service)(retain_nonstatus_information)(retain_status_information)(retry_interval)(severity_id)(icon_id));
  obj->set_default_active_checks(cfg.checks_active());
  obj->set_default_passive_checks(cfg.checks_passive());
  obj->set_default_event_handler_enabled(cfg.event_handler_enabled());
  obj->set_default_flap_detection(cfg.flap_detection_enabled());
  obj->set_flap_detection_on_critical(
      cfg.flap_detection_options() &
      engine::configuration::ActionServiceOn::action_svc_critical);
  obj->set_flap_detection_on_ok(
      cfg.flap_detection_options() &
      engine::configuration::ActionServiceOn::action_svc_ok);
  obj->set_flap_detection_on_unknown(
      cfg.flap_detection_options() &
      engine::configuration::ActionServiceOn::action_svc_unknown);
  obj->set_flap_detection_on_warning(
      cfg.flap_detection_options() &
      engine::configuration::ActionServiceOn::action_svc_warning);
  obj->set_notify_on_critical(
      cfg.notification_options() &
      engine::configuration::ActionServiceOn::action_svc_critical);
  obj->set_notify_on_downtime(
      cfg.notification_options() &
      engine::configuration::ActionServiceOn::action_svc_downtime);
  obj->set_notify_on_flapping(
      cfg.notification_options() &
      engine::configuration::ActionServiceOn::action_svc_flapping);
  obj->set_notify_on_recovery(
      cfg.notification_options() &
      engine::configuration::ActionServiceOn::action_svc_ok);
  obj->set_notify_on_unknown(
      cfg.notification_options() &
      engine::configuration::ActionServiceOn::action_svc_unknown);
  obj->set_notify_on_warning(
      cfg.notification_options() &
      engine::configuration::ActionServiceOn::action_svc_warning);
  obj->set_stalk_on_critical(
      cfg.stalking_options() &
      engine::configuration::ActionServiceOn::action_svc_critical);
  obj->set_stalk_on_ok(cfg.stalking_options() &
                       engine::configuration::ActionServiceOn::action_svc_ok);
  obj->set_stalk_on_unknown(
      cfg.stalking_options() &
      engine::configuration::ActionServiceOn::action_svc_unknown);
  obj->set_stalk_on_warning(
      cfg.stalking_options() &
      engine::configuration::ActionServiceOn::action_svc_warning);
  for (const auto& tag : cfg.tags()) {
    auto* t = obj->add_tags();
    t->set_id(tag.first());
    t->set_type(static_cast<TagType>(tag.second()));
  }
  obj->set_description(cfg.service_description());
  uint32_t internal_id;
  if (absl::StartsWith(cfg.host_name(), "_Module_Meta") &&
      absl::StartsWith(cfg.service_description(), "meta_")) {
    obj->set_type(ServiceType::METASERVICE);
    std::string_view internal_id_str =
        absl::ClippedSubstr(cfg.service_description(), 5);
    if (absl::SimpleAtoi(internal_id_str, &internal_id))
      obj->set_internal_id(internal_id);
  } else if (absl::StartsWith(cfg.host_name(), "_Module_BAM") &&
             absl::StartsWith(cfg.service_description(), "ba_")) {
    obj->set_type(ServiceType::BA);
    std::string_view internal_id_str =
        absl::ClippedSubstr(cfg.service_description(), 3);
    if (absl::SimpleAtoi(internal_id_str, &internal_id))
      obj->set_internal_id(internal_id);
  } else
    obj->set_type(ServiceType::SERVICE);
}

/**
 * @brief Fill a Service protobuf object from a configuration Anomaly
 * detection object.
 *
 * @param obj The protobuf Service object to fill
 * @param cfg The configuration Service object to use as source
 */
void broker_cache::_fill_anomaly_detection(
    Service* obj,
    const engine::configuration::Anomalydetection& cfg) {
  BOOST_PP_SEQ_FOR_EACH(translate, , (host_id)(service_id)(action_url)(check_freshness)(check_interval)(display_name)(event_handler)(display_name)(event_handler)(first_notification_delay)(freshness_threshold)(high_flap_threshold)(host_name)(icon_image)(icon_image_alt)(is_volatile)(low_flap_threshold)(max_check_attempts)(notes)(notes_url)(notification_interval)(notification_period)(obsess_over_service)(retain_nonstatus_information)(retain_status_information)(retry_interval)(severity_id)(icon_id));
  obj->set_default_active_checks(cfg.checks_active());
  obj->set_default_passive_checks(cfg.checks_passive());
  obj->set_default_event_handler_enabled(cfg.event_handler_enabled());
  obj->set_default_flap_detection(cfg.flap_detection_enabled());
  obj->set_flap_detection_on_critical(
      cfg.flap_detection_options() &
      engine::configuration::ActionServiceOn::action_svc_critical);
  obj->set_flap_detection_on_ok(
      cfg.flap_detection_options() &
      engine::configuration::ActionServiceOn::action_svc_ok);
  obj->set_flap_detection_on_unknown(
      cfg.flap_detection_options() &
      engine::configuration::ActionServiceOn::action_svc_unknown);
  obj->set_flap_detection_on_warning(
      cfg.flap_detection_options() &
      engine::configuration::ActionServiceOn::action_svc_warning);
  obj->set_notify_on_critical(
      cfg.notification_options() &
      engine::configuration::ActionServiceOn::action_svc_critical);
  obj->set_notify_on_downtime(
      cfg.notification_options() &
      engine::configuration::ActionServiceOn::action_svc_downtime);
  obj->set_notify_on_flapping(
      cfg.notification_options() &
      engine::configuration::ActionServiceOn::action_svc_flapping);
  obj->set_notify_on_recovery(
      cfg.notification_options() &
      engine::configuration::ActionServiceOn::action_svc_ok);
  obj->set_notify_on_unknown(
      cfg.notification_options() &
      engine::configuration::ActionServiceOn::action_svc_unknown);
  obj->set_notify_on_warning(
      cfg.notification_options() &
      engine::configuration::ActionServiceOn::action_svc_warning);
  obj->set_stalk_on_critical(
      cfg.stalking_options() &
      engine::configuration::ActionServiceOn::action_svc_critical);
  obj->set_stalk_on_ok(cfg.stalking_options() &
                       engine::configuration::ActionServiceOn::action_svc_ok);
  obj->set_stalk_on_unknown(
      cfg.stalking_options() &
      engine::configuration::ActionServiceOn::action_svc_unknown);
  obj->set_stalk_on_warning(
      cfg.stalking_options() &
      engine::configuration::ActionServiceOn::action_svc_warning);
  for (const auto& tag : cfg.tags()) {
    auto* t = obj->add_tags();
    t->set_id(tag.first());
    t->set_type(static_cast<TagType>(tag.second()));
  }
  obj->set_description(cfg.service_description());
  uint32_t internal_id;
  if (absl::StartsWith(cfg.host_name(), "_Module_Meta") &&
      absl::StartsWith(cfg.service_description(), "meta_")) {
    obj->set_type(ServiceType::METASERVICE);
    std::string_view internal_id_str =
        absl::ClippedSubstr(cfg.service_description(), 5);
    if (absl::SimpleAtoi(internal_id_str, &internal_id))
      obj->set_internal_id(internal_id);
  } else if (absl::StartsWith(cfg.host_name(), "_Module_BAM") &&
             absl::StartsWith(cfg.service_description(), "ba_")) {
    obj->set_type(ServiceType::BA);
    std::string_view internal_id_str =
        absl::ClippedSubstr(cfg.service_description(), 3);
    if (absl::SimpleAtoi(internal_id_str, &internal_id))
      obj->set_internal_id(internal_id);
  } else
    obj->set_type(ServiceType::ANOMALY_DETECTION);
}

/**
 * @brief Update an instance in the cache.
 *
 * @param instance The instance to update
 */
void broker_cache::update_instance(
    const std::shared_ptr<neb::pb_instance>& instance) {
  absl::WriterMutexLock l{&_mutex};
  auto& obj = instance->obj();
  if (obj.running())
    _instances.insert_or_assign(obj.instance_id(), obj.name());
  else
    _instances.erase(obj.instance_id());
}

/**
 * @brief Remove an instance from the cache.
 *
 * @param instance_id The ID of the instance to remove
 */
void broker_cache::remove_instance(uint64_t instance_id) {
  absl::WriterMutexLock l{&_mutex};
  _instances.erase(instance_id);

  auto& index_svc = _services.get<by_id>();
  auto& host_by_instance = _hosts.get<by_instance>();
  auto range = host_by_instance.equal_range(instance_id);
  for (auto it = range.first; it != range.second;) {
    auto lower =
        index_svc.lower_bound(std::make_pair((*it)->obj().host_id(), 0));
    auto upper =
        index_svc.upper_bound(std::make_pair((*it)->obj().host_id() + 1, 0));
    index_svc.erase(lower, upper);
    it = host_by_instance.erase(it);
  }
}

/**
 * @brief Update a servicegroup in the cache.
 *
 * @param servicegroup The servicegroup to update
 */
void broker_cache::update_servicegroup(
    const std::shared_ptr<neb::pb_service_group>& servicegroup) {
  absl::WriterMutexLock l{&_mutex};

  const auto& hg_id = servicegroup->obj().servicegroup_id();
  const auto& poller_id = servicegroup->obj().poller_id();

  _logger->debug(
      "Updating service group '{}' (id {}) for poller {} in Broker cache.",
      servicegroup->obj().name(), hg_id, poller_id);

  if (servicegroup->obj().enabled()) {
    auto& hg_index = _servicegroups.get<by_id>();
    auto found = hg_index.find(hg_id);
    if (found != hg_index.end()) {
      // The element already exists, we update it
      auto& pollers = const_cast<absl::flat_hash_set<uint64_t>&>(found->second);
      pollers.insert(poller_id);
      if (found->first->obj().name() != servicegroup->obj().name()) {
        auto extracted = std::move(
            const_cast<std::pair<std::shared_ptr<neb::pb_service_group>,
                                 absl::flat_hash_set<uint64_t>>&>(*found));
        hg_index.erase(found);
        extracted.first->mut_obj().set_name(servicegroup->obj().name());
        bool inserted;
        std::tie(found, inserted) = hg_index.insert(std::move(extracted));
      }
      ServiceGroup& obj = found->first->mut_obj();
      obj.set_servicegroup_id(servicegroup->obj().servicegroup_id());
      obj.set_name(servicegroup->obj().name());
      obj.set_enabled(servicegroup->obj().enabled());
      obj.set_poller_id(servicegroup->obj().poller_id());
      obj.set_alias(servicegroup->obj().alias());
    } else {
      // The element is missing, we create it and insert it
      auto filled_servicegroup = std::make_shared<neb::pb_service_group>();
      ServiceGroup& obj = filled_servicegroup->mut_obj();
      obj.set_servicegroup_id(servicegroup->obj().servicegroup_id());
      obj.set_name(servicegroup->obj().name());
      obj.set_enabled(servicegroup->obj().enabled());
      obj.set_poller_id(servicegroup->obj().poller_id());
      obj.set_alias(servicegroup->obj().alias());
      _servicegroups.emplace(filled_servicegroup,
                             absl::flat_hash_set<uint64_t>{poller_id});
    }
  } else if (auto found = _servicegroups.get<by_id>().find(hg_id);
             found != _servicegroups.end()) {
    const_cast<absl::flat_hash_set<uint64_t>&>(found->second).erase(poller_id);
    if (found->second.empty())
      _servicegroups.erase(found);
  }
}

/**
 * @brief Update a hostgroup in the cache.
 *
 * @param hostgroup The hostgroup to update
 */
void broker_cache::update_hostgroup(
    const std::shared_ptr<neb::pb_host_group>& hostgroup) {
  absl::WriterMutexLock l{&_mutex};

  const auto& hg_id = hostgroup->obj().hostgroup_id();
  const auto& poller_id = hostgroup->obj().poller_id();

  _logger->debug(
      "Updating host group '{}' (id {}) for poller {} in Broker cache.",
      hostgroup->obj().name(), hg_id, poller_id);

  if (hostgroup->obj().enabled()) {
    auto& hg_index = _hostgroups.get<by_id>();
    auto found = hg_index.find(hg_id);
    if (found != hg_index.end()) {
      // The element already exists, we update it
      _logger->debug(
          "Host group '{}' (id {}) already exists in Broker cache, updating "
          "it.",
          hostgroup->obj().name(), hg_id);
      auto& pollers = const_cast<absl::flat_hash_set<uint64_t>&>(found->second);
      pollers.insert(poller_id);
      if (found->first->obj().name() != hostgroup->obj().name()) {
        auto extracted = std::move(
            const_cast<std::pair<std::shared_ptr<neb::pb_host_group>,
                                 absl::flat_hash_set<uint64_t>>&>(*found));
        hg_index.erase(found);
        extracted.first->mut_obj().set_name(hostgroup->obj().name());
        bool inserted;
        std::tie(found, inserted) = hg_index.insert(std::move(extracted));
      }
      HostGroup& obj = found->first->mut_obj();
      obj.set_hostgroup_id(hostgroup->obj().hostgroup_id());
      obj.set_name(hostgroup->obj().name());
      obj.set_enabled(hostgroup->obj().enabled());
      /* The poller ID is not updated because a hostgroup can be linked to
       * several pollers, so no sense to keep it or replace it. */
      obj.set_poller_id(0);
      obj.set_alias(hostgroup->obj().alias());
    } else {
      // The element is missing, we create it and insert it
      _logger->debug(
          "Host group '{}' (id {}) does not exist in Broker cache, adding "
          "it.",
          hostgroup->obj().name(), hg_id);
      auto filled_hostgroup = std::make_shared<neb::pb_host_group>();
      HostGroup& obj = filled_hostgroup->mut_obj();
      obj.set_hostgroup_id(hostgroup->obj().hostgroup_id());
      obj.set_name(hostgroup->obj().name());
      obj.set_enabled(hostgroup->obj().enabled());
      /* The poller ID is not set because a hostgroup can be linked to several
       * pollers, so no sense to keep it or replace it. */
      obj.set_poller_id(0);
      obj.set_alias(hostgroup->obj().alias());
      _hostgroups.emplace(filled_hostgroup,
                          absl::flat_hash_set<uint64_t>{poller_id});
    }
  } else if (auto found = _hostgroups.get<by_id>().find(hg_id);
             found != _hostgroups.end()) {
    _logger->debug(
        "Host group '{}' (id {}) is disabled for poller {}. Removing it from "
        "Broker cache.",
        hostgroup->obj().name(), hg_id, poller_id);
    const_cast<absl::flat_hash_set<uint64_t>&>(found->second).erase(poller_id);
    if (found->second.empty()) {
      _logger->info("Removing host group '{}' (id {}) from Broker cache.",
                    found->first->obj().name(), hg_id);
      _hostgroups.erase(found);
    }
  }
}

/**
 * @brief Update a hostgroup member in the cache.
 *
 * @param hostgroup_member The hostgroup member to update
 */
void broker_cache::update_hostgroup_member(
    const std::shared_ptr<neb::pb_host_group_member>& hostgroup_member) {
  absl::WriterMutexLock l{&_mutex};

  const auto& hgm_obj = hostgroup_member->obj();
  SPDLOG_LOGGER_DEBUG(_logger,
                      "Processing host group member (group_name: '{}', "
                      "group_id: {}, host_id: "
                      "{}, enabled: {})",
                      hgm_obj.name(), hgm_obj.hostgroup_id(), hgm_obj.host_id(),
                      hgm_obj.enabled());
  auto key = std::make_pair(hgm_obj.host_id(), hgm_obj.hostgroup_id());

  if (hgm_obj.enabled()) {
    auto found = _hostgroups.get<by_id>().find(hgm_obj.hostgroup_id());
    bool inserted;
    if (found == _hostgroups.get<by_id>().end()) {
      auto hg = std::make_shared<neb::pb_host_group>();
      auto& obj = hg->mut_obj();
      obj.set_hostgroup_id(hgm_obj.hostgroup_id());
      obj.set_name(hgm_obj.name());
      obj.set_enabled(true);
      /* The poller ID is not set because a hostgroup can be linked to several
       * pollers, so no sense to keep it or replace it. */
      obj.set_poller_id(0);
      std::tie(found, inserted) = _hostgroups.insert(
          {hg, absl::flat_hash_set<uint64_t>{hgm_obj.poller_id()}});
    }
    auto [it, inserted2] =
        _host_hostgroups.insert({hgm_obj.host_id(), found->first});
    if (!inserted2) {
      assert(it->hostgroup->obj().hostgroup_id() == hgm_obj.hostgroup_id());
      auto& obj = const_cast<HostGroup&>(it->hostgroup->mut_obj());
      obj.set_name(hgm_obj.name());
    }
  } else {
    _host_hostgroups.erase(key);
  }
}

/**
 * @brief Update a servicegroup member in the cache.
 *
 * @param servicegroup_member The servicegroup member to update
 */
void broker_cache::update_servicegroup_member(
    const std::shared_ptr<neb::pb_service_group_member>& servicegroup_member) {
  absl::WriterMutexLock l{&_mutex};

  const auto& sgm_obj = servicegroup_member->obj();
  SPDLOG_LOGGER_DEBUG(_logger,
                      "Processing service group member (group_name: '{}', "
                      "group_id: {}, service_id: "
                      "{}, enabled: {})",
                      sgm_obj.name(), sgm_obj.servicegroup_id(),
                      sgm_obj.service_id(), sgm_obj.enabled());
  auto key = std::make_tuple(sgm_obj.host_id(), sgm_obj.service_id(),
                             sgm_obj.servicegroup_id());

  if (sgm_obj.enabled()) {
    auto [it, inserted] = _service_servicegroups.try_emplace(
        key, std::make_shared<neb::pb_service_group>());

    auto& obj = it->second->mut_obj();
    obj.set_servicegroup_id(sgm_obj.servicegroup_id());
    obj.set_name(sgm_obj.name());
    obj.set_enabled(true);
    obj.set_poller_id(sgm_obj.poller_id());
  } else
    _service_servicegroups.erase(key);
}

void broker_cache::update_metric_mapping(
    const std::shared_ptr<storage::pb_metric_mapping>& mm) {
  absl::WriterMutexLock l{&_mutex};
  _metric_mappings[mm->obj().metric_id()] = mm;
}

/**
 * @brief Add a host to the cache (used in legacy mode).
 *
 * @param host The host to add
 */
void broker_cache::update_host(const std::shared_ptr<neb::pb_host>& host) {
  absl::WriterMutexLock l{&_mutex};
  auto& index = _hosts.get<by_id>();
  auto& h = host->obj();
  SPDLOG_LOGGER_DEBUG(_logger, "Processing host '{}' of id {} enabled {}",
                      h.name(), h.host_id(), h.enabled());
  auto it = index.find(h.host_id());
  if (h.enabled()) {
    if (it != index.end())
      index.replace(it, host);
    else
      index.insert(host);
  } else {
    if (it != index.end())
      index.erase(it);
  }
}

/**
 * @brief Remove a host from the cache (used in legacy mode).
 *
 * @param host_id The ID of the host to remove
 */
void broker_cache::remove_host(uint64_t host_id) {
  absl::WriterMutexLock l(&_mutex);
  auto& index = _hosts.get<by_id>();
  auto found = index.find(host_id);
  if (found != index.end())
    index.erase(found);
}

/**
 * @brief Update a host in the cache.
 *
 * @param status The host status used to update the host.
 */
void broker_cache::update_host(
    const std::shared_ptr<neb::pb_host_status>& status) {
  auto& hs = status->obj();
  uint64_t host_id = hs.host_id();
  bool updated = false;
  {
    absl::WriterMutexLock l{&_mutex};
    auto& index = _hosts.get<by_id>();
    auto found = index.find(host_id);
    if (found != index.end()) {
      auto& hst = found->get()->mut_obj();
      hst.set_checked(hs.checked());
      hst.set_check_type(static_cast<Host_CheckType>(hs.check_type()));
      hst.set_state(static_cast<Host_State>(hs.state()));
      hst.set_state_type(static_cast<Host_StateType>(hs.state_type()));
      hst.set_last_state_change(hs.last_state_change());
      hst.set_last_hard_state(static_cast<Host_State>(hs.last_hard_state()));
      hst.set_last_hard_state_change(hs.last_hard_state_change());
      hst.set_last_time_up(hs.last_time_up());
      hst.set_last_time_down(hs.last_time_down());
      hst.set_last_time_unreachable(hs.last_time_unreachable());
      hst.set_output(hs.output());
      hst.set_perfdata(hs.perfdata());
      hst.set_flapping(hs.flapping());
      hst.set_percent_state_change(hs.percent_state_change());
      hst.set_latency(hs.latency());
      hst.set_execution_time(hs.execution_time());
      hst.set_last_check(hs.last_check());
      hst.set_next_check(hs.next_check());
      hst.set_should_be_scheduled(hs.should_be_scheduled());
      hst.set_check_attempt(hs.check_attempt());
      hst.set_notification_number(hs.notification_number());
      hst.set_no_more_notifications(hs.no_more_notifications());
      hst.set_last_notification(hs.last_notification());
      hst.set_next_host_notification(hs.next_host_notification());
      hst.set_acknowledgement_type(hs.acknowledgement_type());
      hst.set_scheduled_downtime_depth(hs.scheduled_downtime_depth());
      updated = true;
    } else {
      _logger->warn(
          "Attempt to update host ({}) in Broker cache, but it does not exist.",
          host_id);
    }
  }

  if (updated)
    _logger->debug("Updated host status for host '{}' in Broker cache.",
                   host_id);
  else
    _logger->debug(
        "Host status for host '{}' was not updated in Broker cache because "
        "host does not exist in cache.",
        host_id);
}

/**
 * @brief Update a host in the cache.
 *
 * @param host The adaptive host used to update the host.
 */
void broker_cache::update_host(
    const std::shared_ptr<neb::pb_adaptive_host>& host) {
  absl::WriterMutexLock l{&_mutex};
  auto& ah = host->obj();
  auto& index = _hosts.get<by_id>();
  auto found = index.find(ah.host_id());
  if (found != index.end()) {
    auto& h = found->get()->mut_obj();
    _logger->debug("Updating adaptive host for host '{}' in Broker cache.",
                   ah.host_id());
    if (ah.has_notify())
      h.set_notify(ah.notify());
    if (ah.has_active_checks())
      h.set_active_checks(ah.active_checks());
    if (ah.has_should_be_scheduled())
      h.set_should_be_scheduled(ah.should_be_scheduled());
    if (ah.has_passive_checks())
      h.set_passive_checks(ah.passive_checks());
    if (ah.has_event_handler_enabled())
      h.set_event_handler_enabled(ah.event_handler_enabled());
    if (ah.has_flap_detection())
      h.set_flap_detection(ah.flap_detection());
    if (ah.has_obsess_over_host())
      h.set_obsess_over_host(ah.obsess_over_host());
    if (ah.has_event_handler())
      h.set_event_handler(ah.event_handler());
    if (ah.has_check_command())
      h.set_check_command(ah.check_command());
    if (ah.has_check_interval())
      h.set_check_interval(ah.check_interval());
    if (ah.has_retry_interval())
      h.set_retry_interval(ah.retry_interval());
    if (ah.has_max_check_attempts())
      h.set_max_check_attempts(ah.max_check_attempts());
    if (ah.has_check_freshness())
      h.set_check_freshness(ah.check_freshness());
    if (ah.has_check_period())
      h.set_check_period(ah.check_period());
    if (ah.has_notification_period())
      h.set_notification_period(ah.notification_period());
  } else
    _logger->warn(
        "Cannot update cache for host {}, it does not exist in the cache",
        ah.host_id());
}

/**
 * @brief Update a host in the cache.
 *
 * @param status The adaptive host status used to update the host.
 */
void broker_cache::update_host(
    const std::shared_ptr<neb::pb_adaptive_host_status>& status) {
  absl::WriterMutexLock l{&_mutex};
  auto& hs = status->obj();
  auto& index = _hosts.get<by_id>();
  auto found = index.find(hs.host_id());
  if (found != index.end()) {
    auto& hst = found->get()->mut_obj();
    _logger->debug(
        "Updating adaptive host status for host '{}' in Broker cache.",
        hs.host_id());
    if (hs.has_scheduled_downtime_depth())
      hst.set_scheduled_downtime_depth(hs.scheduled_downtime_depth());
    if (hs.has_acknowledgement_type())
      hst.set_acknowledgement_type(hs.acknowledgement_type());
    if (hs.has_notification_number())
      hst.set_notification_number(hs.notification_number());
  } else {
    _logger->warn(
        "Attempt to update host ({}) in Broker cache, but it does not exist.",
        hs.host_id());
  }
}

/**
 * @brief Add a service to the cache (used in legacy mode).
 *
 * @param svc The service to add
 */
void broker_cache::update_service(const std::shared_ptr<neb::pb_service>& svc) {
  absl::WriterMutexLock l{&_mutex};

  auto& index = _services.get<by_id>();
  auto& s = svc->obj();
  SPDLOG_LOGGER_DEBUG(
      _logger, "Processing service ({}, {}) (description:{}) enabled {}",
      s.host_id(), s.service_id(), s.description(), s.enabled());

  auto it = index.find(std::make_pair(s.host_id(), s.service_id()));
  if (s.enabled()) {
    if (it != index.end())
      index.replace(it, svc);
    else
      index.insert(svc);
  } else {
    if (it != index.end())
      index.erase(it);
  }
}

/**
 * @brief Remove a service from the cache (used in legacy mode).
 *
 * @param host_id Host ID of the service to remove
 * @param service_id Service ID of the service to remove
 */
void broker_cache::remove_service(uint64_t host_id, uint64_t service_id) {
  absl::WriterMutexLock l(&_mutex);
  auto& index = _services.get<by_id>();
  auto found = index.find(std::make_pair(host_id, service_id));
  if (found != index.end())
    index.erase(found);
}

/**
 * @brief Update a service in the cache.
 *
 * @param status The service status used to update the service.
 */
void broker_cache::update_service(
    const std::shared_ptr<neb::pb_service_status>& status) {
  absl::WriterMutexLock l{&_mutex};

  const auto& obj = status->obj();
  _logger->debug("Processing service status ({}, {})", obj.host_id(),
                 obj.service_id());

  auto& index = _services.get<by_id>();
  auto it = index.find(std::make_pair(obj.host_id(), obj.service_id()));
  if (it == index.end()) {
    _logger->warn(
        "Attempt to update service ({}, {}) in cache, but it does not exist.",
        obj.host_id(), obj.service_id());
    return;
  }

  auto& svc = it->get()->mut_obj();
  svc.set_checked(obj.checked());
  svc.set_check_type(static_cast<Service_CheckType>(obj.check_type()));
  svc.set_state(static_cast<Service_State>(obj.state()));
  svc.set_state_type(static_cast<Service_StateType>(obj.state_type()));
  svc.set_last_state_change(obj.last_state_change());
  svc.set_last_hard_state(static_cast<Service_State>(obj.last_hard_state()));
  svc.set_last_hard_state_change(obj.last_hard_state_change());
  svc.set_last_time_ok(obj.last_time_ok());
  svc.set_last_time_warning(obj.last_time_warning());
  svc.set_last_time_critical(obj.last_time_critical());
  svc.set_last_time_unknown(obj.last_time_unknown());
  svc.set_output(obj.output());
  svc.set_perfdata(obj.perfdata());
  svc.set_flapping(obj.flapping());
  svc.set_percent_state_change(obj.percent_state_change());
  svc.set_latency(obj.latency());
  svc.set_execution_time(obj.execution_time());
  svc.set_last_check(obj.last_check());
  svc.set_next_check(obj.next_check());
  svc.set_should_be_scheduled(obj.should_be_scheduled());
  svc.set_check_attempt(obj.check_attempt());
  svc.set_notification_number(obj.notification_number());
  svc.set_no_more_notifications(obj.no_more_notifications());
  svc.set_last_notification(obj.last_notification());
  svc.set_next_notification(obj.next_notification());
  svc.set_acknowledgement_type(obj.acknowledgement_type());
  svc.set_scheduled_downtime_depth(obj.scheduled_downtime_depth());
}

/**
 * @brief Update a service in the cache.
 *
 * @param svc The adaptive service used to update the service.
 */
void broker_cache::update_service(
    const std::shared_ptr<neb::pb_adaptive_service>& svc) {
  absl::WriterMutexLock l{&_mutex};

  auto& as = svc->obj();
  auto& index = _services.get<by_id>();
  auto it = index.find(std::make_pair(as.host_id(), as.service_id()));
  if (it != _services.end()) {
    auto& s = it->get()->mut_obj();
    if (as.has_notify())
      s.set_notify(as.notify());
    if (as.has_active_checks())
      s.set_active_checks(as.active_checks());
    if (as.has_should_be_scheduled())
      s.set_should_be_scheduled(as.should_be_scheduled());
    if (as.has_passive_checks())
      s.set_passive_checks(as.passive_checks());
    if (as.has_event_handler_enabled())
      s.set_event_handler_enabled(as.event_handler_enabled());
    if (as.has_flap_detection_enabled())
      s.set_flap_detection(as.flap_detection_enabled());
    if (as.has_obsess_over_service())
      s.set_obsess_over_service(as.obsess_over_service());
    if (as.has_event_handler())
      s.set_event_handler(as.event_handler());
    if (as.has_check_command())
      s.set_check_command(as.check_command());
    if (as.has_check_interval())
      s.set_check_interval(as.check_interval());
    if (as.has_retry_interval())
      s.set_retry_interval(as.retry_interval());
    if (as.has_max_check_attempts())
      s.set_max_check_attempts(as.max_check_attempts());
    if (as.has_check_freshness())
      s.set_check_freshness(as.check_freshness());
    if (as.has_check_period())
      s.set_check_period(as.check_period());
    if (as.has_notification_period())
      s.set_notification_period(as.notification_period());
  } else {
    SPDLOG_LOGGER_WARN(
        _logger,
        "Cannot update cache for service ({}, {}), it does not exist in "
        "the cache",
        as.host_id(), as.service_id());
  }
}

/**
 * @brief Update a service in the cache.
 *
 * @param ass The adaptive service status used to update the service.
 */
void broker_cache::update_service(
    const std::shared_ptr<neb::pb_adaptive_service_status>& ass) {
  absl::WriterMutexLock l{&_mutex};

  const auto& obj = ass->obj();

  SPDLOG_LOGGER_DEBUG(_logger, "Processing adaptive service status ({}, {})",
                      obj.host_id(), obj.service_id());
  auto& index = _services.get<by_id>();

  auto it = index.find(std::make_pair(obj.host_id(), obj.service_id()));
  if (it == _services.end()) {
    _logger->warn(
        "Attempt to update service ({}, {}) in global cache, but it does not "
        "exist.",
        obj.host_id(), obj.service_id());
    return;
  }

  auto& svc = it->get()->mut_obj();
  if (obj.has_acknowledgement_type())
    svc.set_acknowledgement_type(obj.acknowledgement_type());
  if (obj.has_scheduled_downtime_depth())
    svc.set_scheduled_downtime_depth(obj.scheduled_downtime_depth());
  if (obj.has_notification_number())
    svc.set_notification_number(obj.notification_number());
}

void broker_cache::update_severity(
    const std::shared_ptr<neb::pb_custom_variable>& evt) {
  auto& obj = evt->obj();
  if (obj.name() == "CRITICALITY_LEVEL") {
    uint32_t level = 0;
    uint64_t host_id = obj.host_id();
    uint64_t service_id = obj.service_id();
    if (!absl::SimpleAtoi(obj.value(), &level)) {
      _logger->warn(
          "Cannot update severity for resource ({}, {}) in Broker cache: "
          "invalid severity level '{}'",
          obj.host_id(), obj.service_id(), obj.value());
      return;
    }
    absl::WriterMutexLock l(&_mutex);
    if (service_id)
      _severities.insert_or_assign({host_id, service_id}, level);
    else
      _severities.insert_or_assign({host_id, 0}, level);
  }
}

void broker_cache::update_dimension_ba_bv_relation(
    const std::shared_ptr<bam::pb_dimension_ba_bv_relation_event>& rel) {
  absl::WriterMutexLock l(&_mutex);
  const auto& obj = rel->obj();
  _dimension_ba_bv_relations.emplace(std::make_pair(obj.ba_id(), obj.bv_id()));
}

std::vector<uint64_t> broker_cache::dimension_bvs_for_ba(uint64_t ba_id) const {
  absl::ReaderMutexLock l(&_mutex);
  std::vector<uint64_t> retval;
  auto lower = _dimension_ba_bv_relations.lower_bound({ba_id, 0});
  auto upper = _dimension_ba_bv_relations.upper_bound({ba_id + 1, 0});
  retval.reserve(std::distance(lower, upper));
  for (auto it = lower; it != upper; ++it)
    retval.push_back(it->second);

  return retval;
}

/**
 * @brief Update a dimension business view event in the cache.
 *
 * @param bv_event The dimension business view event to update
 */
void broker_cache::update_dimension_bv_event(
    const std::shared_ptr<bam::pb_dimension_bv_event>& bv_event) {
  absl::WriterMutexLock l(&_mutex);
  const auto& obj = bv_event->obj();
  _dimension_bvs.insert_or_assign(obj.bv_id(), bv_event);
}

/**
 * @brief Get the dimension business view event of the given ID from the
 * cache.
 *
 * @param bv_id The business view ID of the desired event.
 *
 * @return A shared pointer to the event, nullptr if not found.
 */
std::shared_ptr<bam::pb_dimension_bv_event> broker_cache::dimension_bv(
    uint64_t bv_id) const {
  absl::ReaderMutexLock l(&_mutex);
  auto found = _dimension_bvs.find(bv_id);
  if (found == _dimension_bvs.end())
    return nullptr;
  else
    return found->second;
}

void broker_cache::update_dimension_ba_event(
    const std::shared_ptr<bam::pb_dimension_ba_event>& ba_event) {
  absl::WriterMutexLock l(&_mutex);
  const auto& obj = ba_event->obj();
  _dimension_bas.insert_or_assign(obj.ba_id(), ba_event);
}

std::shared_ptr<bam::pb_dimension_ba_event> broker_cache::dimension_ba(
    uint64_t ba_id) const {
  absl::ReaderMutexLock l(&_mutex);
  auto found = _dimension_bas.find(ba_id);
  if (found == _dimension_bas.end())
    return nullptr;
  else
    return found->second;
}

/**
 * @brief Get the poller instance of the given ID from the cache.
 *
 * @param instance_id The poller ID of the desired instance.
 *
 * @return A shared pointer to the instance, nullptr if not found.
 */
std::string broker_cache::instance(uint64_t instance_id) const {
  absl::ReaderMutexLock l{&_mutex};
  auto found = _instances.find(instance_id);
  if (found == _instances.end())
    return "";
  else
    return found->second;
}

/**
 * @brief Get the host of the given name from the cache.
 *
 * @param host_name The host name of the desired host.
 *
 * @return A shared pointer to the host, nullptr if not found.
 */
std::shared_ptr<neb::pb_host> broker_cache::host(
    const std::string& host_name) const {
  absl::ReaderMutexLock l{&_mutex};
  auto& index = _hosts.get<by_name>();
  auto found = index.find(host_name);
  if (found == index.end())
    return nullptr;
  else
    return *found;
}

/**
 * @brief Get the host of the given ID from the cache.
 *
 * @param host_id The host ID of the desired host.
 *
 * @return A shared pointer to the host, nullptr if not found.
 */
std::shared_ptr<neb::pb_host> broker_cache::host(uint64_t host_id) const {
  absl::ReaderMutexLock l{&_mutex};
  auto& index = _hosts.get<by_id>();
  auto found = index.find(host_id);
  if (found == index.end())
    return nullptr;
  else
    return *found;
}

/**
 * @brief Get the list of host IDs present in the cache.
 *
 * @return A vector of host IDs.
 */
std::vector<uint64_t> broker_cache::host_ids() const {
  absl::ReaderMutexLock l{&_mutex};
  std::vector<uint64_t> retval;
  auto& index = _hosts.get<by_id>();
  retval.reserve(index.size());
  for (const auto& host : _hosts)
    retval.push_back(host->obj().host_id());
  return retval;
}

/**
 * @brief Get the service of the given host ID and service ID from the cache.
 *
 * @param host_id The host ID of the desired service.
 * @param service_id The service ID of the desired service.
 *
 * @return A shared pointer to the service, nullptr if not found.
 */
std::shared_ptr<neb::pb_service> broker_cache::service(
    uint64_t host_id,
    uint64_t service_id) const {
  absl::ReaderMutexLock l{&_mutex};
  auto& index = _services.get<by_id>();
  auto found = index.find(std::make_pair(host_id, service_id));
  if (found == index.end())
    return nullptr;
  else
    return *found;
}

std::shared_ptr<storage::pb_index_mapping> broker_cache::get_index_mapping(
    uint64_t index_id) const {
  absl::ReaderMutexLock l{&_mutex};
  auto& index = _index_mappings.get<by_id>();
  auto found = index.find(index_id);
  if (found == index.end())
    return nullptr;
  else
    return *found;
}

std::shared_ptr<storage::pb_index_mapping> broker_cache::get_index_mapping(
    uint64_t host_id,
    uint64_t service_id) const {
  absl::ReaderMutexLock l{&_mutex};
  auto& index = _index_mappings.get<by_service>();
  auto found = index.find(std::make_pair(host_id, service_id));
  if (found == index.end())
    return nullptr;
  else
    return *found;
}

std::shared_ptr<storage::pb_metric_mapping> broker_cache::get_metric_mapping(
    uint64_t metric_id) const {
  absl::ReaderMutexLock l{&_mutex};
  auto found = _metric_mappings.find(metric_id);
  if (found == _metric_mappings.end())
    return nullptr;
  else
    return found->second;
}

/**
 * @brief Get the list of service IDs present in the cache.
 *
 * @return A vector of pairs of host ID and service ID.
 */
std::vector<std::pair<uint64_t, uint64_t>> broker_cache::service_ids() const {
  absl::ReaderMutexLock l{&_mutex};
  std::vector<std::pair<uint64_t, uint64_t>> retval;
  retval.reserve(_services.size());
  for (const auto& id : _services) {
    auto p = std::make_pair(id->obj().host_id(), id->obj().service_id());
    retval.push_back(p);
  }
  return retval;
}

/**
 * @brief Get the hostgroup of the given ID from the cache.
 *
 * @param hostgroup_id The hostgroup ID of the desired hostgroup.
 *
 * @return A shared pointer to the hostgroup, nullptr if not found.
 */
std::shared_ptr<neb::pb_host_group> broker_cache::hostgroup(
    uint64_t hostgroup_id) const {
  absl::ReaderMutexLock l{&_mutex};
  auto& index = _hostgroups.get<by_id>();
  auto found = index.find(hostgroup_id);
  if (found == index.end())
    return nullptr;
  else
    return found->first;
}

/**
 * @brief Get the servicegroup of the given ID from the cache.
 *
 * @param servicegroup_id The servicegroup ID of the desired servicegroup.
 *
 * @return A shared pointer to the servicegroup, nullptr if not found.
 */
std::shared_ptr<neb::pb_service_group> broker_cache::servicegroup(
    uint64_t servicegroup_id) const {
  absl::ReaderMutexLock l{&_mutex};
  auto found = _servicegroups.find(servicegroup_id);
  if (found == _servicegroups.end())
    return nullptr;
  else
    return found->first;
}

/**
 * @brief Get the hostgroups a host belongs to from the cache.
 *
 * @param host_id The host ID of the desired host
 *
 * @return A vector of shared pointers to the hostgroups the host belongs to.
 */
std::vector<std::shared_ptr<neb::pb_host_group>> broker_cache::hostgroups(
    uint64_t host_id) const {
  absl::ReaderMutexLock l{&_mutex};
  std::vector<std::shared_ptr<neb::pb_host_group>> retval;
  auto& indexed_by_host = _host_hostgroups.get<by_host>();
  auto range = indexed_by_host.equal_range(host_id);
  retval.reserve(std::distance(range.first, range.second));
  for (auto it = range.first; it != range.second; ++it)
    retval.push_back(it->hostgroup);
  return retval;
}

std::vector<uint64_t> broker_cache::hostgroup_members(
    uint64_t hostgroup_id) const {
  absl::ReaderMutexLock l{&_mutex};
  std::vector<uint64_t> retval;
  auto& indexed_by_hostgroup = _host_hostgroups.get<by_hostgroup>();
  auto range = indexed_by_hostgroup.equal_range(hostgroup_id);
  retval.reserve(std::distance(range.first, range.second));
  for (auto it = range.first; it != range.second; ++it)
    retval.push_back(it->host_id);
  return retval;
}

/**
 * @brief Get the servicegroups a service belongs to from the cache.
 *
 * @param host_id The host ID of the service
 * @param service_id The service ID of the service
 *
 * @return A vector of shared pointers to the servicegroups the service
 * belongs to.
 */
std::vector<std::shared_ptr<neb::pb_service_group>> broker_cache::servicegroups(
    uint64_t host_id,
    uint64_t service_id) const {
  absl::ReaderMutexLock l{&_mutex};
  std::vector<std::shared_ptr<neb::pb_service_group>> retval;
  auto lower = _service_servicegroups.lower_bound({host_id, service_id, 0});
  auto upper = _service_servicegroups.upper_bound({host_id, service_id + 1, 0});

  retval.reserve(std::distance(lower, upper));
  for (; lower != upper; ++lower)
    retval.push_back(lower->second);
  return retval;
}

void broker_cache::_publish(const std::shared_ptr<io::data>& evt) {
  uint32_t type = evt->type();
  switch (type) {
    case make_type(io::neb, neb::de_host):
      update_host(
          std::static_pointer_cast<neb::pb_host>(neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::neb, neb::de_service):
      update_service(
          std::static_pointer_cast<neb::pb_service>(neb::bbdo2_to_bbdo3(evt)));
      break;
    case neb::pb_host::static_type():
      update_host(std::static_pointer_cast<neb::pb_host>(evt));
      break;
    case neb::pb_host_status::static_type():
      update_host(std::static_pointer_cast<neb::pb_host_status>(evt));
      break;
    case neb::pb_adaptive_host::static_type():
      update_host(std::static_pointer_cast<neb::pb_adaptive_host>(evt));
      break;
    case neb::pb_adaptive_host_status::static_type():
      update_host(std::static_pointer_cast<neb::pb_adaptive_host_status>(evt));
      break;
    case neb::pb_service::static_type():
      update_service(std::static_pointer_cast<neb::pb_service>(evt));
      break;
    case neb::pb_service_status::static_type():
      update_service(std::static_pointer_cast<neb::pb_service_status>(evt));
      break;
    case neb::pb_adaptive_service::static_type():
      update_service(std::static_pointer_cast<neb::pb_adaptive_service>(evt));
      break;
    case neb::pb_adaptive_service_status::static_type():
      update_service(
          std::static_pointer_cast<neb::pb_adaptive_service_status>(evt));
      break;
    case neb::pb_instance::static_type():
      update_instance(std::static_pointer_cast<neb::pb_instance>(evt));
      break;
    case make_type(io::neb, neb::de_instance):
      update_instance(
          std::static_pointer_cast<neb::pb_instance>(neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::neb, neb::de_host_group):
      update_hostgroup(std::static_pointer_cast<neb::pb_host_group>(
          neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::neb, neb::de_pb_host_group):
      update_hostgroup(std::static_pointer_cast<neb::pb_host_group>(evt));
      break;
    case neb::pb_host_group_member::static_type():
      update_hostgroup_member(
          std::static_pointer_cast<neb::pb_host_group_member>(evt));
      break;
    case make_type(io::neb, neb::de_host_group_member):
      update_hostgroup_member(
          std::static_pointer_cast<neb::pb_host_group_member>(
              neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::neb, neb::de_service_group):
      update_servicegroup(std::static_pointer_cast<neb::pb_service_group>(
          neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::neb, neb::de_pb_service_group):
      update_servicegroup(std::static_pointer_cast<neb::pb_service_group>(evt));
      break;
    case make_type(io::neb, neb::de_service_group_member):
      update_servicegroup_member(
          std::static_pointer_cast<neb::pb_service_group_member>(
              neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::neb, neb::de_pb_service_group_member):
      update_servicegroup_member(
          std::static_pointer_cast<neb::pb_service_group_member>(evt));
      break;
    case make_type(io::storage, storage::de_pb_metric_mapping):
      update_metric_mapping(
          std::static_pointer_cast<storage::pb_metric_mapping>(evt));
      break;
    case make_type(io::storage, storage::de_metric_mapping):
      update_metric_mapping(
          std::static_pointer_cast<storage::pb_metric_mapping>(
              neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::neb, neb::de_pb_custom_variable):
      update_severity(std::static_pointer_cast<neb::pb_custom_variable>(evt));
      break;
    case make_type(io::neb, neb::de_custom_variable):
      update_severity(std::static_pointer_cast<neb::pb_custom_variable>(
          neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::bam, bam::de_pb_dimension_ba_bv_relation_event):
      update_dimension_ba_bv_relation(
          std::static_pointer_cast<bam::pb_dimension_ba_bv_relation_event>(
              evt));
      break;
    case make_type(io::bam, bam::de_dimension_ba_bv_relation_event):
      update_dimension_ba_bv_relation(
          std::static_pointer_cast<bam::pb_dimension_ba_bv_relation_event>(
              neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::bam, bam::de_pb_dimension_ba_event):
      update_dimension_ba_event(
          std::static_pointer_cast<bam::pb_dimension_ba_event>(evt));
      break;
    case make_type(io::bam, bam::de_dimension_ba_event):
      update_dimension_ba_event(
          std::static_pointer_cast<bam::pb_dimension_ba_event>(
              neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::bam, bam::de_pb_dimension_bv_event):
      update_dimension_bv_event(
          std::static_pointer_cast<bam::pb_dimension_bv_event>(evt));
      break;
    case make_type(io::bam, bam::de_dimension_bv_event):
      update_dimension_bv_event(
          std::static_pointer_cast<bam::pb_dimension_bv_event>(
              neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::storage, storage::de_index_mapping):
      update_index_mapping(std::static_pointer_cast<storage::pb_index_mapping>(
          neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::storage, storage::de_pb_index_mapping):
      update_index_mapping(
          std::static_pointer_cast<storage::pb_index_mapping>(evt));
      break;
    case make_type(io::neb, neb::de_pb_global_diff_state):
      apply(std::static_pointer_cast<neb::pb_global_diff_state>(evt)->obj());
      break;
    default:
      break;
  }
}

/**
 * @brief Publish data to the cache. This is used by the multiplexing Engine
 * to keep retro compatibility with the legacy Broker. It is called at the
 * same time the muxers publish() are called. The goal is to populate the
 * cache.
 *
 * @param to_publish
 */
void broker_cache::publish(const std::shared_ptr<io::data>& to_publish) {
  _publish(to_publish);
}

/**
 * @brief Publish data to the cache. This is used by the multiplexing Engine
 * to keep retro compatibility with the legacy Broker. It is called at the
 * same time the muxers publish() are called. The goal is to populate the
 * cache.
 *
 * @param to_publish
 */
void broker_cache::publish(
    const std::deque<std::shared_ptr<io::data>>& to_publish) {
  for (auto evt : to_publish) {
    _publish(evt);
  }
}

/**
 * @brief Get the severity level of a resource (host or service) from the cache.
 *
 * @param host_id The host ID of the resource
 * @param service_id The service ID of the resource (0 for host severity,
 * non-zero for service severity)
 *
 * @return The severity level of the resource, or 0 if not found or not set.
 */
uint32_t broker_cache::severity(uint64_t host_id, uint64_t service_id) const {
  absl::ReaderMutexLock l{&_mutex};
  if (service_id == 0) {
    // Host severity
    auto host_it = _hosts.get<by_id>().find(host_id);
    if (host_it != _hosts.get<by_id>().end()) {
      uint64_t severity_id = host_it->get()->obj().severity_id();
      if (severity_id) {
        auto severity_it = _severities.find({severity_id, 0});
        if (severity_it != _severities.end())
          return severity_it->second;
      } else
        _logger->warn("Host {} has no severity set", host_id);
    } else {
      _logger->warn("Host {} not found in cache when looking for severity",
                    host_id);
    }
  } else {
    // Service severity
    auto svc_it =
        _services.get<by_id>().find(std::make_pair(host_id, service_id));
    if (svc_it != _services.get<by_id>().end()) {
      uint64_t severity_id = svc_it->get()->obj().severity_id();
      if (severity_id) {
        auto severity_it = _severities.find({severity_id, 1});
        if (severity_it != _severities.end())
          return severity_it->second;
      } else
        _logger->warn("Service ({}, {}) has no severity set", host_id,
                      service_id);
    } else {
      _logger->warn(
          "Service ({}, {}) not found in cache when looking for severity",
          host_id, service_id);
    }
  }
  return 0;
}

/**
 * @brief Load the cache from a file. This is used to persist the cache across
 * restarts of the broker. The cache is loaded from a binary file using Protocol
 * Buffers serialization.
 */
void broker_cache::_load_cache() {
  _logger->info("broker_cache: loading cache...");
  auto& state = config::applier::state::instance();
  std::filesystem::path cache_file{fmt::format("{}.cache", state.cache_dir())};
  std::ifstream ifs{cache_file, std::ios::binary};
  if (ifs) {
    BrokerCache to_load;
    if (!to_load.ParseFromIstream(&ifs)) {
      _logger->error("broker_cache: cannot parse cache file '{}'",
                     cache_file.string());
    } else {
      absl::WriterMutexLock lck{&_mutex};
      for (const auto& inst_pair : to_load.instances())
        _instances.insert({inst_pair.id(), inst_pair.name()});

      for (const auto& host : to_load.hosts()) {
        auto h = std::make_shared<neb::pb_host>();
        h->mut_obj().CopyFrom(host);
        _hosts.get<by_id>().insert(h);
      }
      for (const auto& svc : to_load.services()) {
        auto s = std::make_shared<neb::pb_service>();
        s->mut_obj().CopyFrom(svc);
        _services.get<by_id>().insert(s);
      }
      for (const auto& hgp : to_load.hostgroups()) {
        auto hst_grp = std::make_shared<neb::pb_host_group>();
        auto poller_ids = absl::flat_hash_set<uint64_t>();
        hst_grp->mut_obj().CopyFrom(hgp.hostgroup());
        /* The poller_id field is not saved in the cache, so we set it to 0
         * here. The actual poller IDs are stored in the poller_ids set, which
         * is populated from the pollers() field of the host group in the cache
         * file. */
        hst_grp->mut_obj().set_poller_id(0);
        for (const auto& poller_id : hgp.pollers())
          poller_ids.insert(poller_id);
        _hostgroups.get<by_id>().insert(
            std::make_pair(hst_grp, std::move(poller_ids)));
        for (uint64_t host_id : hgp.hosts()) {
          _host_hostgroups.insert({host_id, hst_grp});
        }
      }

      for (const auto& sgp : to_load.servicegroups()) {
        auto svc_grp = std::make_shared<neb::pb_service_group>();
        auto poller_ids = absl::flat_hash_set<uint64_t>();
        svc_grp->mut_obj().CopyFrom(sgp.servicegroup());
        for (const auto& poller_id : sgp.pollers())
          poller_ids.insert(poller_id);
        _servicegroups.get<by_id>().insert(
            std::make_pair(svc_grp, std::move(poller_ids)));
        for (const auto& id : sgp.services()) {
          _service_servicegroups[std::make_tuple(
              id.host_id(), id.service_id(),
              svc_grp->obj().servicegroup_id())] = svc_grp;
        }
      }
      _logger->info("broker_cache: cache loaded from file '{}'",
                    cache_file.string());
    }
  } else {
    _logger->info("broker_cache: cache file '{}' does not exist",
                  cache_file.string());
  }
}

/**
 * @brief Save the cache to a file. This is used to persist the cache across
 * restarts of the broker. The cache is saved in a binary format using Protocol
 * Buffers serialization.
 */
void broker_cache::_save_cache() {
  _logger->info("broker_cache: saving cache...");
  BrokerCache to_save;
  {
    absl::ReaderMutexLock lck{&_mutex};
    for (const auto& [id, name] : _instances) {
      auto* inst_pair = to_save.add_instances();
      inst_pair->set_id(id);
      inst_pair->set_name(name);
    }
    for (const auto& host : _hosts) {
      auto* h = to_save.add_hosts();
      h->CopyFrom(host->obj());
    }
    for (const auto& svc : _services) {
      auto* s = to_save.add_services();
      s->CopyFrom(svc->obj());
    }
    absl::flat_hash_map<uint64_t, std::list<uint64_t>> hostgroup_hosts;
    for (const auto& p : _host_hostgroups)
      hostgroup_hosts[p.hostgroup->obj().hostgroup_id()].push_back(p.host_id);
    for (const auto& hg : _hostgroups) {
      auto* hst_grp = to_save.add_hostgroups();
      hst_grp->mutable_hostgroup()->CopyFrom(hg.first->obj());
      for (uint64_t poller_id : hg.second)
        hst_grp->add_pollers(poller_id);
      for (const uint64_t host_id :
           hostgroup_hosts[hg.first->obj().hostgroup_id()])
        hst_grp->add_hosts(host_id);
    }

    absl::flat_hash_map<uint64_t, std::list<std::pair<uint64_t, uint64_t>>>
        servicegroup_services;
    for (const auto& t : _service_servicegroups)
      servicegroup_services[std::get<2>(t.first)].emplace_back(
          std::make_pair(std::get<0>(t.first), std::get<1>(t.first)));
    for (const auto& hg : _servicegroups) {
      auto* svc_grp = to_save.add_servicegroups();
      svc_grp->mutable_servicegroup()->CopyFrom(hg.first->obj());
      for (uint64_t poller_id : hg.second)
        svc_grp->add_pollers(poller_id);
      for (const auto& [host_id, service_id] :
           servicegroup_services[hg.first->obj().servicegroup_id()]) {
        auto* s = svc_grp->add_services();
        s->set_host_id(host_id);
        s->set_service_id(service_id);
      }
    }
  }
  /* Saving the BrokerCache */
  std::filesystem::path cache_file{
      fmt::format("{}.cache", config::applier::state::instance().cache_dir())};
  std::ofstream ofs{cache_file, std::ios::binary | std::ios::trunc};
  if (!ofs) {
    _logger->error("broker_cache: cannot open cache file '{}'",
                   cache_file.string());
  } else {
    if (!to_save.SerializeToOstream(&ofs)) {
      _logger->error("broker_cache: cannot serialize cache to file '{}'",
                     cache_file.string());
    } else {
      _logger->info("broker_cache: cache saved to file '{}'",
                    cache_file.string());
    }
  }
}

void broker_cache::update_index_mapping(
    const std::shared_ptr<storage::pb_index_mapping>& index_mapping) {
  absl::WriterMutexLock l{&_mutex};
  auto& obj = index_mapping->obj();
  auto& index = _index_mappings.get<by_id>();
  auto found = index.find(obj.index_id());
  if (found != index.end())
    index.replace(found, index_mapping);
  else
    index.insert(index_mapping);
}

void broker_cache::remove_index_mapping(uint64_t host_id, uint64_t service_id) {
  absl::WriterMutexLock l{&_mutex};
  auto& index = _index_mappings.get<by_service>();
  index.erase(std::make_pair(host_id, service_id));
}
}  // namespace com::centreon::broker::cache
