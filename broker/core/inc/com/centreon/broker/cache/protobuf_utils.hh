/**
 * Copyright 2026 Centreon
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

#ifndef CCB_CACHE_PROTOBUF_UTILS_HH
#define CCB_CACHE_PROTOBUF_UTILS_HH

#include "boost/container/vector.hpp"
#include "boost/interprocess/allocators/private_node_allocator.hpp"
#include "boost/interprocess/interprocess_fwd.hpp"
#include "boost/interprocess/offset_ptr.hpp"

namespace com::centreon::broker::cache {

namespace interprocess = boost::interprocess;
using managed_mapped_file = interprocess::basic_managed_mapped_file<
    char,
    interprocess::rbtree_best_fit<interprocess::null_mutex_family>,
    interprocess::iset_index>;
using segment_manager = managed_mapped_file::segment_manager;
using char_allocator = managed_mapped_file::allocator<char>::type;

using string =
    interprocess::basic_string<char, std::char_traits<char>, char_allocator>;

class message;

using private_int32_allocator =
    interprocess::private_node_allocator<int32_t, segment_manager, 4096>;
using private_int64_allocator =
    interprocess::private_node_allocator<int64_t, segment_manager, 4096>;
using private_uint32_allocator =
    interprocess::private_node_allocator<uint32_t, segment_manager, 4096>;
using private_uint64_allocator =
    interprocess::private_node_allocator<uint64_t, segment_manager, 4096>;
using private_double_allocator =
    interprocess::private_node_allocator<double, segment_manager, 4096>;
using private_float_allocator =
    interprocess::private_node_allocator<float, segment_manager, 4096>;
using private_bool_allocator =
    interprocess::private_node_allocator<bool, segment_manager, 4096>;
using private_string_pointer_allocator =
    interprocess::private_node_allocator<string, segment_manager, 4096>;
using private_message_pointer_allocator =
    interprocess::private_node_allocator<interprocess::offset_ptr<message>,
                                         segment_manager,
                                         4096>;

struct allocators {
  allocators(segment_manager* sgm)
      : char_alloc(sgm),
        int32_alloc(sgm),
        int64_alloc(sgm),
        uint32_alloc(sgm),
        uint64_alloc(sgm),
        double_alloc(sgm),
        float_alloc(sgm),
        bool_alloc(sgm),
        string_alloc(sgm),
        message_alloc(sgm),
        segm_manager(sgm) {}

  char_allocator char_alloc;
  private_int32_allocator int32_alloc;
  private_int64_allocator int64_alloc;
  private_uint32_allocator uint32_alloc;
  private_uint64_allocator uint64_alloc;
  private_double_allocator double_alloc;
  private_float_allocator float_alloc;
  private_bool_allocator bool_alloc;
  private_string_pointer_allocator string_alloc;
  private_message_pointer_allocator message_alloc;
  segment_manager* segm_manager;
};

using int32_vect = boost::container::vector<int32_t, private_int32_allocator>;
using int64_vect = boost::container::vector<int64_t, private_int64_allocator>;
using uint32_vect =
    boost::container::vector<uint32_t, private_uint32_allocator>;
using uint64_vect =
    boost::container::vector<uint64_t, private_uint64_allocator>;
using double_vect = boost::container::vector<double, private_double_allocator>;
using float_vect = boost::container::vector<float, private_float_allocator>;
using bool_vect = boost::container::vector<bool, private_bool_allocator>;
using string_vect =
    boost::container::vector<string, private_string_pointer_allocator>;
using mess_vect = boost::container::vector<interprocess::offset_ptr<message>,
                                           private_message_pointer_allocator>;

using variant_base = std::variant<int32_t,
                                  int64_t,
                                  uint32_t,
                                  uint64_t,
                                  double,
                                  float,
                                  bool,

                                  std::optional<int32_t>,
                                  std::optional<int64_t>,
                                  std::optional<uint32_t>,
                                  std::optional<uint64_t>,
                                  std::optional<double>,
                                  std::optional<float>,
                                  std::optional<bool>,

                                  const string*,
                                  const message*,
                                  const int32_vect*,
                                  const int64_vect*,
                                  const uint32_vect*,
                                  const uint64_vect*,
                                  const double_vect*,
                                  const float_vect*,
                                  const bool_vect*,
                                  const string_vect*,
                                  const mess_vect*>;

struct variant : public variant_base {
  template <typename field_type>
  variant(const field_type& value) : variant_base(value) {}

  variant(const string& value) : variant_base(&value) {}

  variant(const message& value) : variant_base(&value) {}

  variant(
      const boost::container::vector<int32_t, private_int32_allocator>& value)
      : variant_base(&value) {}

  variant(
      const boost::container::vector<int64_t, private_int64_allocator>& value)
      : variant_base(&value) {}

  variant(
      const boost::container::vector<uint32_t, private_uint32_allocator>& value)
      : variant_base(&value) {}

  variant(
      const boost::container::vector<uint64_t, private_uint64_allocator>& value)
      : variant_base(&value) {}

  variant(
      const boost::container::vector<double, private_double_allocator>& value)
      : variant_base(&value) {}

  variant(const boost::container::vector<float, private_float_allocator>& value)
      : variant_base(&value) {}

  variant(const boost::container::vector<bool, private_bool_allocator>& value)
      : variant_base(&value) {}

  variant(const string_vect& value) : variant_base(&value) {}

  variant(const mess_vect& value) : variant_base(&value) {}
};

}  // namespace com::centreon::broker::cache
#endif