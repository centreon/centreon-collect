/*
 * Copyright 2018 - 2019 Centreon (https://www.centreon.com/)
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
#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/contactgroup.hh"
#include "com/centreon/engine/contactgroup.hh"
#include "helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine::configuration::applier;

class ApplierPbContactgroup : public ::testing::Test {
 public:
  void SetUp() override { init_config_state(); }

  void TearDown() override { deinit_config_state(); }
};

// Given a contactgroup applier
// And a configuration contactgroup
// When we modify the contactgroup configuration with a non existing
// contactgroup
// Then an exception is thrown.
TEST_F(ApplierPbContactgroup, ModifyUnexistingContactgroupFromConfig) {
  configuration::applier::contactgroup aply;
  configuration::Contactgroup cg;
  configuration::contactgroup_helper hlp(&cg);
  cg.set_contactgroup_name("test");
  fill_string_group(cg.mutable_members(), "contact");
  auto* new_cg = pb_indexed_config.state().add_contactgroups();
  new_cg->CopyFrom(cg);
  ASSERT_THROW(aply.modify_object(new_cg, cg), std::exception);
}

// Given a contactgroup applier
// And a configuration contactgroup in configuration
// When we modify the contactgroup configuration
// Then the applier modify_object updates the contactgroup.
TEST_F(ApplierPbContactgroup, ModifyContactgroupFromConfig) {
  configuration::applier::contact caply;
  configuration::applier::contactgroup aply;
  configuration::Contactgroup cg;
  configuration::contactgroup_helper cg_hlp(&cg);
  cg.set_contactgroup_name("test");
  configuration::Contact ct;
  configuration::contact_helper ct_hlp(&ct);
  ct.set_contact_name("contact");

  caply.add_object(ct);

  fill_string_group(cg.mutable_members(), "contact");
  cg.set_alias("test");
  aply.add_object(cg);
  auto it =
      std::find_if(pb_indexed_config.state().mutable_contactgroups()->begin(),
                   pb_indexed_config.state().mutable_contactgroups()->end(),
                   [](const configuration::Contactgroup& cg) {
                     return cg.contactgroup_name() == "test";
                   });

  ASSERT_TRUE(it->alias() == "test");

  cg.set_alias("test_renamed");
  aply.modify_object(&*it, cg);

  it = std::find_if(pb_indexed_config.state().mutable_contactgroups()->begin(),
                    pb_indexed_config.state().mutable_contactgroups()->end(),
                    [](const configuration::Contactgroup& cg) {
                      return cg.contactgroup_name() == "test";
                    });
  ASSERT_TRUE(it->alias() == "test_renamed");
}

// Given a contactgroup applier
// And a configuration contactgroup in configuration
// When we change remove the configuration
// Then it is really removed
TEST_F(ApplierPbContactgroup, RemoveContactgroupFromConfig) {
  configuration::applier::contact caply;
  configuration::applier::contactgroup aply;
  configuration::Contactgroup cg;
  configuration::contactgroup_helper cg_hlp(&cg);
  cg.set_contactgroup_name("test");
  configuration::Contact ct;
  configuration::contact_helper ct_hlp(&ct);
  ct.set_contact_name("contact");

  caply.add_object(ct);
  fill_string_group(cg.mutable_members(), "contact");
  aply.add_object(cg);
  ASSERT_FALSE(engine::contactgroup::contactgroups.empty());

  aply.remove_object<std::string>({0, "test"});
  ASSERT_TRUE(engine::contactgroup::contactgroups.empty());
}

// Given an empty contactgroup
// When the resolve_object() method is called
// Then no warning, nor error are given
TEST_F(ApplierPbContactgroup, ResolveEmptyContactgroup) {
  configuration::error_cnt err;
  configuration::applier::contactgroup aplyr;
  configuration::Contactgroup grp;
  configuration::contactgroup_helper hlp(&grp);
  grp.set_contactgroup_name("test");
  aplyr.add_object(grp);
  aplyr.expand_objects(pb_indexed_config.state());
  aplyr.resolve_object(grp, err);
  ASSERT_EQ(err.config_warnings, 0);
  ASSERT_EQ(err.config_errors, 0);
}

// Given a contactgroup with a non-existing contact
// When the resolve_object() method is called
// Then an exception is thrown
// And the method returns 1 error
TEST_F(ApplierPbContactgroup, ResolveInexistentContact) {
  configuration::applier::contactgroup aplyr;
  configuration::Contactgroup grp;
  configuration::contactgroup_helper grp_hlp(&grp);
  grp.set_contactgroup_name("test");
  fill_string_group(grp.mutable_members(), "non_existing_contact");
  ASSERT_THROW(aplyr.add_object(grp), std::exception);
}

// Given a contactgroup with a contact
// When the resolve_object() method is called
// Then the contact is really added to the contact group.
TEST_F(ApplierPbContactgroup, ResolveContactgroup) {
  configuration::error_cnt err;
  configuration::applier::contact aply;
  configuration::applier::contactgroup aply_grp;
  configuration::Contactgroup grp;
  configuration::contactgroup_helper hlp(&grp);
  grp.set_contactgroup_name("test_group");
  configuration::Contact ctct;
  configuration::contact_helper c_hlp(&ctct);
  ctct.set_contact_name("test");
  aply.add_object(ctct);
  fill_string_group(ctct.mutable_contactgroups(), "test_group");
  fill_string_group(grp.mutable_members(), "test");
  aply_grp.add_object(grp);
  aply_grp.expand_objects(pb_indexed_config.state());
  ASSERT_NO_THROW(aply_grp.resolve_object(grp, err));
}

// Given a contactgroup with a contact already configured
// And a second contactgroup configuration
// When we set the first one as contactgroup member to the second
// Then the parse method returns true and set the first one contacts
// to the second one.
TEST_F(ApplierPbContactgroup, SetContactgroupMembers) {
  configuration::error_cnt err;
  configuration::applier::contact aply;
  configuration::applier::contactgroup aply_grp;
  configuration::Contactgroup grp;
  configuration::contactgroup_helper grp_hlp(&grp);
  grp.set_contactgroup_name("test_group");
  configuration::Contact ctct;
  configuration::contact_helper ctct_hlp(&ctct);
  ctct.set_contact_name("test");
  aply.add_object(ctct);
  fill_string_group(grp.mutable_members(), "test");
  aply_grp.add_object(grp);
  aply_grp.expand_objects(pb_indexed_config.state());
  aply_grp.resolve_object(grp, err);
  ASSERT_EQ(grp.members().data().size(), 1);

  configuration::Contactgroup grp1;
  configuration::contactgroup_helper grp1_hlp(&grp1);
  grp1.set_contactgroup_name("big_group");
  fill_string_group(grp1.mutable_contactgroup_members(), "test_group");
  aply_grp.add_object(grp1);
  aply_grp.expand_objects(pb_indexed_config.state());

  // grp1 must be reload because the expand_objects reload them totally.
  bool found = false;
  for (auto& cg : pb_indexed_config.state().contactgroups()) {
    if (cg.contactgroup_name() == "big_group") {
      ASSERT_EQ(cg.members().data().size(), 1);
      found = true;
      break;
    }
  }
  ASSERT_TRUE(found);
}

TEST_F(ApplierPbContactgroup, ContactRemove) {
  configuration::error_cnt err;
  configuration::applier::contact aply;
  configuration::applier::contactgroup aply_grp;
  configuration::Contactgroup grp;
  configuration::contactgroup_helper grp_hlp(&grp);
  grp.set_contactgroup_name("test_group");

  configuration::Contact ctct;
  configuration::contact_helper ctct_hlp(&ctct);
  ctct.set_contact_name("test");
  aply.add_object(ctct);

  configuration::Contact ctct2;
  configuration::contact_helper ctct2_hlp(&ctct2);
  ctct2.set_contact_name("test2");
  aply.add_object(ctct2);

  grp_hlp.hook("members", "test, test2");
  aply_grp.add_object(grp);
  aply_grp.expand_objects(pb_indexed_config.state());
  aply_grp.resolve_object(grp, err);
  ASSERT_EQ(
      engine::contactgroup::contactgroups["test_group"]->get_members().size(),
      2u);

  int idx2 = 0;
  while (pb_indexed_config.state().contacts()[idx2].contact_name() != "test2") {
    idx2++;
    ASSERT_LE(idx2, pb_indexed_config.state().contacts().size());
  }

  aply.remove_object<std::string>({idx2, "test2"});
  ASSERT_EQ(
      engine::contactgroup::contactgroups["test_group"]->get_members().size(),
      1u);
  grp_hlp.hook("members", "test");
  //  grp.parse("members", "test");
  int idx = 0;
  while (pb_indexed_config.state().contactgroups()[idx].contactgroup_name() !=
         "test_group") {
    idx++;
    ASSERT_LE(idx, pb_indexed_config.state().contactgroups().size());
  }
  aply_grp.modify_object(
      &pb_indexed_config.state().mutable_contactgroups()->at(idx), grp);
}

// Given a contactgroup applier
// And a configuration contactgroup in configuration
// When we modify members in the contactgroup configuration
// Then the applier modify_object updates the contactgroup.
TEST_F(ApplierPbContactgroup, ModifyMembersContactgroupFromConfig) {
  configuration::applier::contact caply;
  configuration::applier::contactgroup aply;
  configuration::Contactgroup cg;
  configuration::contactgroup_helper cg_hlp(&cg);
  cg.set_contactgroup_name("test");
  configuration::Contact ct;
  configuration::contact_helper ct_hlp(&ct);
  ct.set_contact_name("contact");

  configuration::Contact ct1;
  configuration::contact_helper ct_hlp1(&ct1);
  ct1.set_contact_name("contact1");
  caply.add_object(ct);
  caply.add_object(ct1);

  fill_string_group(cg.mutable_members(), "contact,contact1");
  cg.set_alias("test");
  aply.add_object(cg);
  auto it =
      std::find_if(pb_indexed_config.state().mutable_contactgroups()->begin(),
                   pb_indexed_config.state().mutable_contactgroups()->end(),
                   [](const configuration::Contactgroup& cg) {
                     return cg.contactgroup_name() == "test";
                   });

  ASSERT_TRUE(it->alias() == "test");

  fill_string_group(cg.mutable_members(), "contact1");
  aply.modify_object(&*it, cg);

  it = std::find_if(pb_indexed_config.state().mutable_contactgroups()->begin(),
                    pb_indexed_config.state().mutable_contactgroups()->end(),
                    [](const configuration::Contactgroup& cg) {
                      return cg.contactgroup_name() == "test";
                    });
  ASSERT_TRUE(it->members().data().size() == 2);
  ASSERT_TRUE(it->members().data()[0] == "contact");
  ASSERT_TRUE(it->members().data()[1] == "contact1");
}
