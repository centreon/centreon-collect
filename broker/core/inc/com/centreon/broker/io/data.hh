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

#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

namespace io {
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
  /**
   * @brief this sub class is used to dump data or inherited by using
   * dump_more_detail instead of dump
   * example:
   * @code {.c++}
   * io::data event;
   * log_v2::core()->trace("event:{}", io::data::dump_detail{event});
   * @endcode
   */
  struct dump_detail {
    const data& to_dump;
  };

  /**
   * @brief same as dump_detail
   * dump in json format
   *
   */
  struct dump_json {
    const data& to_dump;
  };

  data() = delete;
  data(uint32_t type = 0);
  data(data const& other);
  virtual ~data() = default;
  data& operator=(data const& other);
  constexpr uint32_t type() const noexcept { return _type; }

  virtual void dump(std::ostream& s) const;
  virtual void dump_more_detail(std::ostream& s) const;
  virtual void dump_to_json(std::ostream& s) const;

  uint32_t source_id;
  uint32_t destination_id;

  static uint32_t broker_id;
};

inline std::ostream& operator<<(std::ostream& s, const data& d) {
  d.dump(s);
  return s;
}

inline std::ostream& operator<<(std::ostream& s, const data::dump_detail& d) {
  d.to_dump.dump_more_detail(s);
  return s;
}

inline std::ostream& operator<<(std::ostream& s, const data::dump_json& d) {
  d.to_dump.dump_to_json(s);
  return s;
}

using data_read_handler = std::function<void(const std::shared_ptr<data>&)>;

}  // namespace io

CCB_END()

namespace fmt {
template <>
struct formatter<com::centreon::broker::io::data> : ostream_formatter {};

template <>
struct formatter<com::centreon::broker::io::data::dump_detail>
    : ostream_formatter {};

template <>
struct formatter<com::centreon::broker::io::data::dump_json>
    : ostream_formatter {};

}  // namespace fmt

#endif  // !CCB_IO_DATA_HH
