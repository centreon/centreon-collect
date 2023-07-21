/*
** Copyright 2023 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include <gtest/gtest.h>
#include <regex>

#include "com/centreon/common/node_allocator.hh"

using namespace com::centreon::common;

struct dummy {
  int int_value;
};

TEST(node_allocator, deallocate_twice) {
  using my_alloc = node_allocator<dummy, std::allocator<uint8_t>, 200>;
  std::allocator<uint8_t> uchar_alloc;
  my_alloc alloc(uchar_alloc);

  std::vector<dummy*> to_free;
  for (unsigned ii = 0; ii < 10005; ++ii) {
    to_free.push_back(alloc.allocate());
  }

  ASSERT_THROW(alloc.deallocate(reinterpret_cast<dummy*>(5)),
               my_alloc::no_allocated_exception);
  ASSERT_THROW(alloc.deallocate(reinterpret_cast<dummy*>(
                   reinterpret_cast<char*>(to_free[0]) + 1)),
               my_alloc::bad_aligned_exception);

  for (auto node_to_free : to_free) {
    ASSERT_NO_THROW(alloc.deallocate(node_to_free));
  }

  std::ostringstream dumped;
  dumped << alloc;
  std::cout << alloc;

  // we must have only strings like chunk_begin:0x559c5f29ec28 allocated:0
  // free:200
  std::regex not_empty_regex("allocated:[1-9]");
  ASSERT_FALSE(std::regex_search(dumped.str(), not_empty_regex));

  ASSERT_THROW(alloc.deallocate(reinterpret_cast<dummy*>(5)),
               my_alloc::no_allocated_exception);
  ASSERT_THROW(alloc.deallocate(reinterpret_cast<dummy*>(
                   reinterpret_cast<char*>(to_free[0]) + 1)),
               my_alloc::bad_aligned_exception);
  ASSERT_THROW(alloc.deallocate(to_free[0]), my_alloc::yet_freed_exception);
}

struct alignas(64) big_align {
  std::string toto;
};

TEST(node_allocator, big_aligned) {
  using my_alloc = node_allocator<big_align, std::allocator<uint8_t>, 200>;
  std::allocator<uint8_t> uchar_alloc;
  my_alloc alloc(uchar_alloc);

  std::vector<big_align*> to_free;
  for (unsigned ii = 0; ii < 1000; ++ii) {
    big_align* allocated = alloc.allocate();
    to_free.push_back(allocated);
    ASSERT_EQ(size_t(allocated) % 64, 0);
  }

  for (unsigned ii = 0; ii < 115; ++ii) {
    ASSERT_NO_THROW(alloc.deallocate(to_free[ii]));
  }

  for (unsigned ii = 0; ii < 1000; ++ii) {
    big_align* allocated = alloc.allocate();
    to_free.push_back(allocated);
    ASSERT_EQ(size_t(allocated) % 64, 0);
  }
}
