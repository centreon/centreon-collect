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
#include "com/centreon/engine/log_v2.hh"

using namespace com::centreon::engine;

extern configuration::state* config;
extern configuration::State pb_config;

using Message = ::google::protobuf::Message;
using Reflection = ::google::protobuf::Reflection;
using FieldDescriptor = ::google::protobuf::FieldDescriptor;

void init_config_state(void) {
  if (config == nullptr)
    config = new configuration::state;

  config->log_file_line(true);
  config->log_file("");

  pb_config.CopyFrom(configuration::State());
  pb_config.set_log_file_line(true);
  pb_config.set_log_file("");

  // Hack to instanciate the logger.
  configuration::applier::logging::instance().apply(*config);
  log_v2::instance()->apply(*config);

  checks::checker::init(true);
}

void deinit_config_state(void) {
  delete config;
  config = nullptr;

  configuration::State new_state;
  pb_config.Swap(&new_state);
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
