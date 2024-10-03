/*
 *,
 uint32_t interval_notifCopyright 2019 Centreon (https://www.centreon.com/)
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

#ifndef TEST_ENGINE_HH
#define TEST_ENGINE_HH

#include <gtest/gtest.h>

#include "com/centreon/engine/commands/commands.hh"
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/timeperiod.hh"
#include "com/centreon/engine/timeperiod.hh"
#ifdef LEGACY_CONF
#include "common/engine_legacy_conf/anomalydetection.hh"
#include "common/engine_legacy_conf/contact.hh"
#include "common/engine_legacy_conf/contactgroup.hh"
#include "common/engine_legacy_conf/host.hh"
#include "common/engine_legacy_conf/hostdependency.hh"
#include "common/engine_legacy_conf/hostescalation.hh"
#include "common/engine_legacy_conf/service.hh"
#include "common/engine_legacy_conf/servicedependency.hh"
#include "common/engine_legacy_conf/serviceescalation.hh"
#include "common/engine_legacy_conf/state.hh"
#else
#include "common/engine_conf/anomalydetection_helper.hh"
#include "common/engine_conf/contact_helper.hh"
#include "common/engine_conf/host_helper.hh"
#include "common/engine_conf/hostdependency_helper.hh"
#include "common/engine_conf/hostescalation_helper.hh"
#include "common/engine_conf/service_helper.hh"
#include "common/engine_conf/serviceescalation_helper.hh"
#include "common/engine_conf/timeperiod_helper.hh"
#endif

using namespace com::centreon::engine;

class TestEngine : public ::testing::Test {
 public:
#ifdef LEGACY_CONF
  configuration::contact new_configuration_contact(
      std::string const& name,
      bool full,
      const std::string& notif = "a") const;
  configuration::host new_configuration_host(
      std::string const& hostname,
      std::string const& contacts,
      uint64_t hst_id = 12,
      const std::string_view& connector = "");
  configuration::service new_configuration_service(
      std::string const& hostname,
      std::string const& description,
      std::string const& contacts,
      uint64_t svc_id = 13,
      const std::string_view& connector = "");
  configuration::anomalydetection new_configuration_anomalydetection(
      std::string const& hostname,
      std::string const& description,
      std::string const& contacts,
      uint64_t svc_id = 14,
      uint64_t dependent_svc_id = 13,
      std::string const& thresholds_file = "/tmp/thresholds_file");
  configuration::hostescalation new_configuration_hostescalation(
      std::string const& hostname,
      std::string const& contactgroup,
      uint32_t first_notif = 2,
      uint32_t last_notif = 11,
      uint32_t interval_notif = 9);
  configuration::hostdependency new_configuration_hostdependency(
      std::string const& hostname,
      std::string const& dep_hostname);
  configuration::servicedependency new_configuration_servicedependency(
      std::string const& hostname,
      std::string const& description,
      std::string const& dep_hostname,
      std::string const& dep_description);
  configuration::serviceescalation new_configuration_serviceescalation(
      std::string const& hostname,
      std::string const& svc_desc,
      std::string const& contactgroup);
  configuration::contactgroup new_configuration_contactgroup(
      std::string const& name,
      std::string const& contactname);
#else
  void fill_pb_configuration_contact(configuration::contact_helper* ctct_hlp,
                                     std::string const& name,
                                     bool full,
                                     const std::string& notif = "a") const;
  configuration::Contact new_pb_configuration_contact(
      const std::string& name,
      bool full,
      const std::string& notif = "a") const;
  configuration::Hostdependency new_pb_configuration_hostdependency(
      const std::string& hostname,
      const std::string& dep_hostname);
  void fill_pb_configuration_host(configuration::host_helper* hst_hlp,
                                  std::string const& hostname,
                                  std::string const& contacts,
                                  uint64_t hst_id = 12);
  configuration::Host new_pb_configuration_host(const std::string& hostname,
                                                const std::string& contacts,
                                                uint64_t hst_id = 12);
  configuration::Contactgroup new_pb_configuration_contactgroup(
      const std::string& name,
      const std::string& contactname);
  void fill_pb_configuration_contactgroup(
      configuration::contactgroup_helper* ctct_hlp,
      const std::string& name,
      const std::string& contactname);
  configuration::Service new_pb_configuration_service(
      const std::string& hostname,
      const std::string& description,
      const std::string& contacts,
      uint64_t svc_id = 13);
  configuration::Anomalydetection new_pb_configuration_anomalydetection(
      const std::string& hostname,
      const std::string& description,
      const std::string& contacts,
      uint64_t svc_id = 14,
      uint64_t dependent_svc_id = 13,
      const std::string& thresholds_file = "/tmp/thresholds_file");
  void fill_pb_configuration_service(configuration::service_helper* svc_hlp,
                                     std::string const& hostname,
                                     std::string const& description,
                                     std::string const& contacts,
                                     uint64_t svc_id = 13);
  configuration::Serviceescalation new_pb_configuration_serviceescalation(
      std::string const& hostname,
      std::string const& svc_desc,
      std::string const& contactgroup);
  configuration::Hostescalation new_pb_configuration_hostescalation(
      std::string const& hostname,
      std::string const& contactgroup,
      uint32_t first_notif = 2,
      uint32_t last_notif = 11,
      uint32_t interval_notif = 9);
#endif
  std::unique_ptr<timeperiod> new_timeperiod_with_timeranges(
      const std::string& name,
      const std::string& alias);
};

#endif /* !TEST_ENGINE_HH */
