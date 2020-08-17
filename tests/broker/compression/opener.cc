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

#include "com/centreon/broker/compression/opener.hh"
#include <gtest/gtest.h>

using namespace com::centreon::broker;

class test_opener : public io::endpoint {
    public:
      std::shared_ptr<io::stream> _open(std::shared_ptr<io::stream> stream);
      int32_t _level;
      uint32_t _size;
};

/**
 *  Check compression opener's constructor with parameters.
 */
TEST(Opener, ParamCtor) {
  compression::opener o(1,  // _level
                        0); // _size
  test_opener* test_o = reinterpret_cast<test_opener*>(&o);

  ASSERT_EQ(test_o->_level, 1);
  ASSERT_EQ(test_o->_size, 0);
}