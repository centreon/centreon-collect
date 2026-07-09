/*
** Copyright 2011-2013 Centreon
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

#ifndef CCB_IO_DATA_HH
#define CCB_IO_DATA_HH

#include "google/protobuf/io/zero_copy_stream.h"
namespace com::centreon::broker::io {
/**
 *  @class data data.hh "com/centreon/broker/io/data.hh"
 *  @brief Data abstraction.
 *
 *  Data is the core element that is transmitted through Centreon
 *  Broker. It is an interface that is implemented by all specific
 *  module data that wish to be transmitted by the multiplexing
 *  engine.
 */
class data {
  const uint32_t _type;

 public:
  data() = delete;
  data(uint32_t type = 0);
  data(data const& other);
  virtual ~data() = default;
  data& operator=(data const& other);
  constexpr uint32_t type() const noexcept { return _type; }

  virtual void dump(fmt::format_context::iterator& stream) const;

  uint32_t source_id;
  uint32_t destination_id;

  static uint32_t broker_id;
};

using data_read_handler = std::function<void(const std::shared_ptr<data>&)>;

}  // namespace com::centreon::broker::io

namespace fmt {
template <class event_type>
struct formatter<
    event_type,
    char,
    std::enable_if_t<
        std::is_base_of_v<com::centreon::broker::io::data, event_type>>> {
  constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const event_type& event,
              FormatContext& ctx) const -> decltype(ctx.out()) {
    auto out = ctx.out();
    *(out++) = '{';
    event.dump(out);
    *(out++) = '}';
    return out;
  }
};

}  // namespace fmt

#endif  // !CCB_IO_DATA_HH
