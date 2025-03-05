/**
 * Copyright 2009-2024 Centreon
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

#include "com/centreon/broker/neb/initial.hh"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/neb/callbacks.hh"
#include "com/centreon/broker/neb/events.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/common.hh"
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/downtimes/service_downtime.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/nebcallbacks.hh"
#include "com/centreon/engine/nebstructs.hh"
#include "com/centreon/engine/objects.hh"
#include "com/centreon/engine/severity.hh"
#include "com/centreon/engine/tag.hh"

// Internal Nagios host list.

using namespace com::centreon::broker;

// NEB module list.
extern "C" {
nebmodule* neb_module_list;
}

/**************************************
 *                                     *
 *          Static Functions           *
 *                                     *
 **************************************/

/**
 *  Send to the global publisher the list of custom variables.
 */

typedef int (*neb_sender)(int, void*);
static void send_custom_variables_list(
    neb_sender sender = neb::callback_custom_variable) {
  // Start log message.
  neb_logger->info("init: beginning custom variables dump");

  // Iterate through all hosts.
  for (host_map::iterator it{com::centreon::engine::host::hosts.begin()},
       end{com::centreon::engine::host::hosts.end()};
       it != end; ++it) {
    // Send all custom variables.
    for (com::centreon::engine::map_customvar::const_iterator
             cit{it->second->custom_variables.begin()},
         cend{it->second->custom_variables.end()};
         cit != cend; ++cit) {
      std::string name{cit->first};
      if (cit->second.is_sent()) {
        // Fill callback struct.
        nebstruct_custom_variable_data nscvd;
        nscvd.type = NEBTYPE_HOSTCUSTOMVARIABLE_ADD;
        nscvd.timestamp.tv_sec = time(nullptr);
        nscvd.var_name = const_cast<char*>(name.c_str());
        nscvd.var_value = const_cast<char*>(cit->second.value().c_str());
        nscvd.object_ptr = it->second.get();

        // Callback.
        sender(NEBCALLBACK_CUSTOM_VARIABLE_DATA, &nscvd);
      }
    }
  }

  // Iterate through all services.
  for (service_map::iterator
           it{com::centreon::engine::service::services.begin()},
       end{com::centreon::engine::service::services.end()};
       it != end; ++it) {
    // Send all custom variables.
    for (com::centreon::engine::map_customvar::const_iterator
             cit{it->second->custom_variables.begin()},
         cend{it->second->custom_variables.end()};
         cit != cend; ++cit) {
      std::string name{cit->first};
      if (cit->second.is_sent()) {
        // Fill callback struct.
        nebstruct_custom_variable_data nscvd{
            .type = NEBTYPE_SERVICECUSTOMVARIABLE_ADD,
            .timestamp = {time(nullptr), 0},
            .var_name = std::string_view(name),
            .var_value = std::string_view(cit->second.value()),
            .object_ptr = it->second.get(),
        };

        // Callback.
        sender(NEBCALLBACK_CUSTOM_VARIABLE_DATA, &nscvd);
      }
    }
  }

  // End log message.
  neb_logger->info("init: end of custom variables dump");
}

static void send_pb_custom_variables_list() {
  send_custom_variables_list(neb::callback_pb_custom_variable);
}

/**
 *  Send to the global publisher the list of downtimes.
 */
static void send_downtimes_list(neb_sender sender = neb::callback_downtime) {
  // Start log message.
  neb_logger->info("init: beginning downtimes dump");

  std::multimap<
      time_t,
      std::shared_ptr<com::centreon::engine::downtimes::downtime>> const& dts{
      com::centreon::engine::downtimes::downtime_manager::instance()
          .get_scheduled_downtimes()};
  // Iterate through all downtimes.
  for (const auto& p : dts) {
    // Fill callback struct.
    nebstruct_downtime_data nsdd;
    memset(&nsdd, 0, sizeof(nsdd));
    nsdd.type = NEBTYPE_DOWNTIME_ADD;
    nsdd.timestamp.tv_sec = time(nullptr);
    nsdd.downtime_type = p.second->get_type();
    nsdd.host_id = p.second->host_id();
    nsdd.service_id =
        p.second->get_type() ==
                com::centreon::engine::downtimes::downtime::service_downtime
            ? std::static_pointer_cast<
                  com::centreon::engine::downtimes::service_downtime>(p.second)
                  ->service_id()
            : 0;
    nsdd.entry_time = p.second->get_entry_time();
    nsdd.author_name = p.second->get_author().c_str();
    nsdd.comment_data = p.second->get_comment().c_str();
    nsdd.start_time = p.second->get_start_time();
    nsdd.end_time = p.second->get_end_time();
    nsdd.fixed = p.second->is_fixed();
    nsdd.duration = p.second->get_duration();
    nsdd.triggered_by = p.second->get_triggered_by();
    nsdd.downtime_id = p.second->get_downtime_id();

    // Callback.
    sender(NEBCALLBACK_DOWNTIME_DATA, &nsdd);
  }

  // End log message.
  neb_logger->info("init: end of downtimes dump");
}

/**
 *  Send to the global publisher the list of downtimes.
 */
static void send_pb_downtimes_list() {
  send_downtimes_list(neb::callback_pb_downtime);
}

/**
 *  Send to the global publisher the list of host groups within Engine.
 */
static void send_host_group_list(
    neb_sender group_sender = neb::callback_group,
    neb_sender group_member_sender = neb::callback_group_member) {
  // Start log message.
  neb_logger->info("init: beginning host group dump");

  // Loop through all host groups.
  for (hostgroup_map::const_iterator
           it{com::centreon::engine::hostgroup::hostgroups.begin()},
       end{com::centreon::engine::hostgroup::hostgroups.end()};
       it != end; ++it) {
    // Fill callback struct.
    nebstruct_group_data nsgd;
    memset(&nsgd, 0, sizeof(nsgd));
    nsgd.type = NEBTYPE_HOSTGROUP_ADD;
    nsgd.object_ptr = it->second.get();

    // Callback.
    group_sender(NEBCALLBACK_GROUP_DATA, &nsgd);

    // Dump host group members.
    for (host_map_unsafe::const_iterator hit{it->second->members.begin()},
         hend{it->second->members.end()};
         hit != hend; ++hit) {
      // Fill callback struct.
      nebstruct_group_member_data nsgmd;
      memset(&nsgmd, 0, sizeof(nsgmd));
      nsgmd.type = NEBTYPE_HOSTGROUPMEMBER_ADD;
      nsgmd.object_ptr = hit->second;
      nsgmd.group_ptr = it->second.get();

      // Callback.
      group_member_sender(NEBCALLBACK_GROUP_MEMBER_DATA, &nsgmd);
    }
  }

  // End log message.
  neb_logger->info("init: end of host group dump");
}

static void send_pb_host_group_list() {
  send_host_group_list(neb::callback_pb_group, neb::callback_pb_group_member);
}

/**
 * @brief When centengine is started, send severities in bulk.
 */
static void send_severity_list() {
  /* Start log message. */
  neb_logger->info("init: beginning severity dump");

  for (auto it = com::centreon::engine::severity::severities.begin(),
            end = com::centreon::engine::severity::severities.end();
       it != end; ++it) {
    broker_adaptive_severity_data(NEBTYPE_SEVERITY_ADD, it->second.get());
  }
}

/**
 * @brief When centengine is started, send tags in bulk.
 */
static void send_tag_list() {
  /* Start log message. */
  neb_logger->info("init: beginning tag dump");

  for (auto it = com::centreon::engine::tag::tags.begin(),
            end = com::centreon::engine::tag::tags.end();
       it != end; ++it) {
    broker_adaptive_tag_data(NEBTYPE_TAG_ADD, it->second.get());
  }
}

/**
 *  Send to the global publisher the list of hosts within Nagios.
 */
static void send_host_list(neb_sender sender = neb::callback_host) {
  // Start log message.
  neb_logger->info("init: beginning host dump");

  // Loop through all hosts.
  for (host_map::iterator it{com::centreon::engine::host::hosts.begin()},
       end{com::centreon::engine::host::hosts.end()};
       it != end; ++it) {
    // Fill callback struct.
    nebstruct_adaptive_host_data nsahd;
    memset(&nsahd, 0, sizeof(nsahd));
    nsahd.type = NEBTYPE_HOST_ADD;
    nsahd.modified_attribute = MODATTR_ALL;
    nsahd.object_ptr = it->second.get();

    // Callback.
    sender(NEBCALLBACK_ADAPTIVE_HOST_DATA, &nsahd);
  }

  // End log message.
  neb_logger->info("init: end of host dump");
}

/**
 *  Send to the global publisher the list of hosts within Nagios.
 */
static void send_pb_host_list() {
  send_host_list(neb::callback_pb_host);
}

/**
 *  Send to the global publisher the list of host parents within Nagios.
 */
static void send_host_parents_list(neb_sender sender = neb::callback_relation) {
  // Start log message.
  neb_logger->info("init: beginning host parents dump");

  try {
    // Loop through all hosts.
    for (const auto& [_, sptr_host] : com::centreon::engine::host::hosts) {
      // Loop through all parents.
      for (const auto& [_, sptr_host_parent] : sptr_host->parent_hosts) {
        // Fill callback struct.
        nebstruct_relation_data nsrd;
        memset(&nsrd, 0, sizeof(nsrd));
        nsrd.type = NEBTYPE_PARENT_ADD;
        nsrd.hst = sptr_host_parent.get();
        nsrd.dep_hst = sptr_host.get();

        // Callback.
        sender(NEBTYPE_PARENT_ADD, &nsrd);
      }
    }
  } catch (std::exception const& e) {
    neb_logger->error("init: error occurred while dumping host parents: {}",
                      e.what());
  } catch (...) {
    neb_logger->error(
        "init: unknown error occurred while dumping host parents");
  }

  // End log message.
  neb_logger->info("init: end of host parents dump");
}

/**
 *  Send to the global publisher the list of host parents within Nagios.
 */
static void send_pb_host_parents_list() {
  send_host_parents_list(neb::callback_pb_relation);
}

/**
 *  Send to the global publisher the list of service groups within Engine.
 */
static void send_service_group_list(
    neb_sender group_sender = neb::callback_group,
    neb_sender group_member_sender = neb::callback_group_member) {
  // Start log message.
  neb_logger->info("init: beginning service group dump");

  // Loop through all service groups.
  for (servicegroup_map::const_iterator
           it{com::centreon::engine::servicegroup::servicegroups.begin()},
       end{com::centreon::engine::servicegroup::servicegroups.end()};
       it != end; ++it) {
    // Fill callback struct.
    nebstruct_group_data nsgd;
    memset(&nsgd, 0, sizeof(nsgd));
    nsgd.type = NEBTYPE_SERVICEGROUP_ADD;
    nsgd.object_ptr = it->second.get();

    // Callback.
    group_sender(NEBCALLBACK_GROUP_DATA, &nsgd);

    // Dump service group members.
    for (service_map_unsafe::const_iterator sit{it->second->members.begin()},
         send{it->second->members.end()};
         sit != send; ++sit) {
      // Fill callback struct.
      nebstruct_group_member_data nsgmd;
      memset(&nsgmd, 0, sizeof(nsgmd));
      nsgmd.type = NEBTYPE_SERVICEGROUPMEMBER_ADD;
      nsgmd.object_ptr = sit->second;
      nsgmd.group_ptr = it->second.get();

      // Callback.
      group_member_sender(NEBCALLBACK_GROUP_MEMBER_DATA, &nsgmd);
    }
  }

  // End log message.
  neb_logger->info("init: end of service groups dump");
}

static void send_pb_service_group_list() {
  send_service_group_list(neb::callback_pb_group,
                          neb::callback_pb_group_member);
}

/**
 *  Send to the global publisher the list of services within Nagios.
 */
static void send_service_list(neb_sender sender = neb::callback_service) {
  // Start log message.
  neb_logger->info("init: beginning service dump");

  // Loop through all services.
  for (service_map::const_iterator
           it{com::centreon::engine::service::services.begin()},
       end{com::centreon::engine::service::services.end()};
       it != end; ++it) {
    // Fill callback struct.
    nebstruct_adaptive_service_data nsasd;
    memset(&nsasd, 0, sizeof(nsasd));
    nsasd.type = NEBTYPE_SERVICE_ADD;
    nsasd.modified_attribute = MODATTR_ALL;
    nsasd.object_ptr = it->second.get();

    // Callback.
    sender(NEBCALLBACK_ADAPTIVE_SERVICE_DATA, &nsasd);
  }

  // End log message.
  neb_logger->info("init: end of services dump");
}

/**
 *  Send to the global publisher the list of services within Nagios.
 */
static void send_pb_service_list() {
  send_service_list(neb::callback_pb_service);
}

/**
 *  Send the instance configuration loaded event.
 */
static void send_instance_configuration() {
  neb_logger->info(
      "init: sending initial instance configuration loading event, poller id: "
      "{}",
      config::applier::state::instance().poller_id());
  std::shared_ptr<neb::instance_configuration> ic(
      new neb::instance_configuration);
  ic->loaded = true;
  ic->poller_id = config::applier::state::instance().poller_id();
  neb::gl_publisher.write(ic);
}

/**
 *  Send the instance configuration loaded event.
 */
static void send_pb_instance_configuration() {
  neb_logger->info(
      "init: sending initial instance configuration loading event");
  auto ic = std::make_shared<neb::pb_instance_configuration>();
  ic->mut_obj().set_loaded(true);
  ic->mut_obj().set_poller_id(config::applier::state::instance().poller_id());
  neb::gl_publisher.write(ic);
}

/**
 *  Send initial configuration to the global publisher.
 */
void neb::send_initial_configuration() {
  SPDLOG_LOGGER_INFO(neb_logger, "init: send poller conf");
  send_severity_list();
  send_tag_list();
  send_host_list();
  send_service_list();
  send_custom_variables_list();
  send_downtimes_list();
  send_host_parents_list();
  send_host_group_list();
  send_service_group_list();
  send_instance_configuration();
}

/**
 *  Send initial configuration to the global publisher.
 */
void neb::send_initial_pb_configuration() {
//  if (config::applier::state::instance().broker_needs_update()) {
    SPDLOG_LOGGER_INFO(neb_logger, "init: sending poller configuration");
    send_severity_list();
    send_tag_list();
    send_pb_host_list();
    send_pb_service_list();
    send_pb_custom_variables_list();
    send_pb_downtimes_list();
    send_pb_host_parents_list();
    send_pb_host_group_list();
    send_pb_service_group_list();
//  } else {
//    SPDLOG_LOGGER_INFO(neb_logger,
//                       "init: No need to send poller configuration");
//  }
  send_pb_instance_configuration();
}
