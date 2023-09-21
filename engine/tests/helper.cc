/**
 * Copyright 2019 Centreon (https://www.centreon.com/)
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

#include "helper.hh"

#include <com/centreon/engine/checks/checker.hh>
#include <com/centreon/engine/configuration/applier/logging.hh>
#include <com/centreon/engine/configuration/applier/state.hh>
#include "common/configuration/state_helper.hh"
#include "common/log_v2/config.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::engine;
using log_v2 = com::centreon::common::log_v2::log_v2;
using log_v2_config = com::centreon::common::log_v2::config;

extern configuration::state* config;
extern configuration::State pb_config;
extern bool legacy_conf;

using Message = ::google::protobuf::Message;
using Reflection = ::google::protobuf::Reflection;
using FieldDescriptor = ::google::protobuf::FieldDescriptor;

/**
 * @brief Initialize configurations. types is a bitfield of config_type values.
 *
 * @param types What configurations to initialize.
 */
void init_config_state(const config_type type) {
  /* Cleanup */
  if (config) {
    delete config;
    config = nullptr;
  }
  pb_config.Clear();

  /* Legacy case */
  if (type == LEGACY) {
    legacy_conf = true;
    if (config == nullptr)
      config = new configuration::state;

    config->log_file_line(true);
    config->log_file("");

    log_v2_config log_conf(
        "engine-tests", log_v2_config::logger_type::LOGGER_STDOUT,
        config->log_flush_period(), config->log_pid(), config->log_file_line());

    log_v2::instance().apply(log_conf);

    // Hack to instanciate the logger.
    configuration::applier::logging::instance().apply(*config);
  }

  if (type == PROTO) {
    legacy_conf = false;
    pb_config.CopyFrom(configuration::State());
    configuration::state_helper cfg_hlp(&pb_config);
    pb_config.set_log_file_line(true);
    pb_config.set_log_file("");

    log_v2_config log_conf("engine-tests",
                           log_v2_config::logger_type::LOGGER_STDOUT,
                           pb_config.log_flush_period(), pb_config.log_pid(),
                           pb_config.log_file_line());

    log_v2::instance().apply(log_conf);

    // Hack to instanciate the logger.
    configuration::applier::logging::instance().apply(pb_config);
  }

  checks::checker::init(true);
}

void deinit_config_state(void) {
  if (legacy_conf) {
    delete config;
    config = nullptr;
  } else {
    configuration::State new_state;
    pb_config.Swap(&new_state);
  }

  configuration::applier::state::instance().clear();
  checks::checker::deinit();
}

/**
 * @brief This function builds a path to go to the good sub-message, starting
 * from the State pb_config. This is a function useful to debug and to make
 * tests, it is not written to be used in production.
 * Be careful, it is not finished, just developed for currently used cases.
 *
 * @param path A string like this "item1->item2->item3"
 *
 * @return A Path.
 */
configuration::Path build_path(absl::string_view path) {
  configuration::Path retval;
  auto arr = absl::StrSplit(path, "->");
  const Message* conf = &pb_config;
  for (auto& s : arr) {
    const Reflection* refl = conf->GetReflection();
    configuration::Key* key = retval.add_key();
    const FieldDescriptor* field =
        conf->GetDescriptor()->FindFieldByName(std::string(s.data(), s.size()));
    assert(field != nullptr);
    int32_t field_number = field->number();
    key->set_i32(field_number);
    conf = &refl->GetMessage(*conf, field);
  }
  return retval;
}
