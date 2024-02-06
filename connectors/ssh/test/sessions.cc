/**
 * Copyright 2020 Centreon (https://www.centreon.com/)
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

#include "com/centreon/connector/ssh/sessions/credentials.hh"

using namespace com::centreon::connector::ssh::sessions;

TEST(SSHSession, Assign) {
  // Base object.
  credentials creds1;
  creds1.set_host("localhost");
  creds1.set_user("root");
  creds1.set_password("random words");

  // Copy object.
  credentials creds2;
  creds2 = creds1;

  // Reset base object.
  creds1.set_host("centreon.com");
  creds1.set_user("daemon");
  creds1.set_password("please let me in");

  // Check.
  ASSERT_EQ(creds1.get_host(), "centreon.com");
  ASSERT_EQ(creds1.get_user(), "daemon");
  ASSERT_EQ(creds1.get_password(), "please let me in");
  ASSERT_EQ(creds2.get_host(), "localhost");
  ASSERT_EQ(creds2.get_user(), "root");
  ASSERT_EQ(creds2.get_password(), "random words");
}

TEST(SSHSession, CtorCopy) {
  // Base object.
  credentials creds1;
  creds1.set_host("localhost");
  creds1.set_user("root");
  creds1.set_password("random words");

  // Copy object.
  credentials creds2(creds1);

  // Reset base object.
  creds1.set_host("centreon.com");
  creds1.set_user("daemon");
  creds1.set_password("please let me in");

  // Check.
  ASSERT_EQ(creds1.get_host(), "centreon.com");
  ASSERT_EQ(creds1.get_user(), "daemon");
  ASSERT_EQ(creds1.get_password(), "please let me in");
  ASSERT_EQ(creds2.get_host(), "localhost");
  ASSERT_EQ(creds2.get_user(), "root");
  ASSERT_EQ(creds2.get_password(), "random words");
}

TEST(SSHSession, Default) {
  // Object.
  credentials creds;

  // Check.
  ASSERT_TRUE(creds.get_host().empty());
  ASSERT_TRUE(creds.get_key().empty());
  ASSERT_TRUE(creds.get_password().empty());
  ASSERT_EQ(creds.get_port(), 22);
  ASSERT_TRUE(creds.get_user().empty());
  ASSERT_EQ(creds, credentials());
  ASSERT_EQ(creds, credentials());
}

TEST(SSHSession, Values) {
  // Base object.
  credentials creds("localhost", "root", "random words");

  // Check.
  ASSERT_EQ(creds.get_host(), "localhost");
  ASSERT_EQ(creds.get_user(), "root");
  ASSERT_EQ(creds.get_password(), "random words");
}

TEST(SSHSession, Host) {
  // Object.
  com::centreon::connector::ssh::sessions::credentials creds;

  // Checks.
  creds.set_host("localhost");
  for (unsigned int i = 0; i < 100; ++i)
    ASSERT_EQ(creds.get_host(), "localhost");
  creds.set_host("www.centreon.com");
  ASSERT_EQ(creds.get_host(), "www.centreon.com");
  creds.set_host("www.merethis.com");
  for (unsigned int i = 0; i < 1000; ++i)
    ASSERT_EQ(creds.get_host(), "www.merethis.com");
}

TEST(SSHSession, LessThan) {
  // Objects.
  credentials creds1("AAA", "GGG", "VVV");
  credentials creds2("BBB", "HHH", "XXX");
  credentials creds3("AAA", "HHH", "QQQ");
  credentials creds4("AAA", "GGG", "ZZZ");
  credentials creds5(creds1);

  // Checks.
  ASSERT_TRUE(creds1 < creds2);
  ASSERT_TRUE(creds1 < creds3);
  ASSERT_TRUE(creds1 < creds4);
  ASSERT_FALSE(creds1 < creds5);
  ASSERT_FALSE(creds5 < creds1);
}

TEST(SSHSession, Password) {
  // Object.
  com::centreon::connector::ssh::sessions::credentials creds;

  // Checks.
  creds.set_password("mysimplepassword");
  for (unsigned int i = 0; i < 100; ++i)
    ASSERT_EQ(creds.get_password(), "mysimplepassword");
  creds.set_password("thisis_42kinda*complicat3d");
  ASSERT_EQ(creds.get_password(), "thisis_42kinda*complicat3d");
  creds.set_password("a/very{secure%p4ssw0rd;");
  for (unsigned int i = 0; i < 1000; ++i)
    ASSERT_EQ(creds.get_password(), "a/very{secure%p4ssw0rd;");
}

TEST(SSHSession, User) {
  // Object.
  com::centreon::connector::ssh::sessions::credentials creds;

  // Checks.
  creds.set_user("root");
  for (unsigned int i = 0; i < 100; ++i)
    ASSERT_EQ(creds.get_user(), "root");
  creds.set_user("Centreon");
  ASSERT_EQ(creds.get_user(), "Centreon");
  creds.set_user("Merethis");
  for (unsigned int i = 0; i < 1000; ++i)
    ASSERT_EQ(creds.get_user(), "Merethis");
}
