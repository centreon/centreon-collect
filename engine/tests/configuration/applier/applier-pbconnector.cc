/*
 * Copyright 2017-2019,2023 Centreon (https://www.centreon.com/)
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
#include <gtest/gtest.h>
#include "../../timeperiod/utils.hh"
#include "com/centreon/engine/commands/connector.hh"
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/connector.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::configuration::applier;

class ApplierPbConnector : public ::testing::Test {
 public:
  void SetUp() override { init_config_state(); }

  void TearDown() override { deinit_config_state(); }
};

// Given a connector applier
// And a configuration connector just with a name
// Then the applier add_object adds the connector in the configuration set
// and in the connectors map.
TEST_F(ApplierPbConnector, PbUnusableConnectorFromConfig) {
  configuration::applier::connector aply;
  configuration::Connector cnn;
  configuration::connector_helper cnn_hlp(&cnn);
  cnn.set_connector_name("connector");
  aply.add_object(cnn);
  ASSERT_EQ(commands::connector::connectors.size(), 1u);
}

// Given a connector applier already applied
// When the connector is modified from the configuration,
// Then the modify_object() method updated correctly the connector.
TEST_F(ApplierPbConnector, PbModifyConnector) {
  configuration::applier::connector aply;
  configuration::Connector cnn;
  configuration::connector_helper cnn_hlp(&cnn);
  cnn.set_connector_name("connector");
  cnn.set_connector_line("perl");

  aply.add_object(cnn);

  cnn.set_connector_line("date");
  configuration::Connector* old =
      pb_indexed_config.mut_connectors().at("connector").get();
  aply.modify_object(old, cnn);

  connector_map::iterator found_con =
      commands::connector::connectors.find("connector");
  ASSERT_FALSE(found_con == commands::connector::connectors.end());
  ASSERT_FALSE(!found_con->second);

  ASSERT_EQ(found_con->second->get_name(), "connector");
  ASSERT_EQ(found_con->second->get_command_line(), "date");
}

// Given simple connector applier already applied
// When the connector is removed from the configuration,
// Then the connector is totally removed.
TEST_F(ApplierPbConnector, PbRemoveConnector) {
  configuration::applier::connector aply;
  configuration::Connector cnn;
  configuration::connector_helper cnn_hlp(&cnn);
  cnn.set_connector_name("connector");
  cnn.set_connector_line("echo 1");

  aply.add_object(cnn);
  aply.remove_object("connector");
  ASSERT_TRUE(pb_indexed_config.connectors().size() == 0);
  ASSERT_TRUE(commands::connector::connectors.size() == 0);
}
