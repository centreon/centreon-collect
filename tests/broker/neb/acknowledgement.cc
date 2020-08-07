/*
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

#include "com/centreon/broker/neb/acknowledgement.hh"
#include <gtest/gtest.h>

using namespace com::centreon::broker;

TEST(Acknowledgement, Constructor) {
  neb::acknowledgement ack(33, // short acknowledgement_type
                           "author",
                           "comment",
                           time(nullptr), // entry_time
                           2, // host id
                           5, // service id
                           10, // poller id
                           false, // is_sticky
                           true, // notify_contacts
                           true, // persistent_comment
                           44); // state
                    
  ASSERT_EQ(ack.acknowledgement_type, 33);
  ASSERT_EQ(ack.author, "author");
  ASSERT_EQ(ack.comment, "comment");
  ASSERT_EQ(ack.entry_time, time(nullptr));
  ASSERT_EQ(ack.host_id, 2);
  ASSERT_EQ(ack.service_id, 5);
  ASSERT_EQ(ack.poller_id, 10);
  ASSERT_EQ(ack.is_sticky, false);
  ASSERT_EQ(ack.notify_contacts, true);
  ASSERT_EQ(ack.persistent_comment, true);
  ASSERT_EQ(ack.state, 44);
}