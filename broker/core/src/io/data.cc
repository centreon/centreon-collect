/**
 * Copyright 2011 Centreon
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

#include "com/centreon/broker/io/data.hh"
#include <cassert>
#include "bbdo/events.hh"
#include "com/centreon/broker/io/events.hh"
#include "fmt/base.h"

using namespace com::centreon::broker::io;

uint32_t data::broker_id(0);

/**
 *  Constructor.
 */
data::data(uint32_t type)
    : _type(type), source_id(broker_id), destination_id(0) {
  assert(type);
}

/**
 *  Copy constructor.
 *
 *  @param[in] other  Object to copy.
 */
data::data(data const& other)
    : _type(other._type),
      source_id(other.source_id),
      destination_id(other.destination_id) {}

/**
 *  Assignment operator.
 *
 *  @param[in] other  Object to copy.
 *
 *  @return This object.
 */
data& data::operator=(data const& other) {
  if (this != &other) {
    source_id = other.source_id;
    destination_id = other.destination_id;
  }
  return *this;
}

/**
 * @brief dump obj content to std::ostream
 *
 * @param s
 */
void data::dump(fmt::format_context::iterator& stream) const {
  const auto event_info = io::events::instance().get_event_info(_type);
  fmt::format_to(stream, "\"cat\":\"{}\",",
                 category_name(data_category(_type >> 16)));
  if (event_info) {
    fmt::format_to(stream, "\"elem\":\"{}\",", event_info->get_name());
  } else {
    fmt::format_to(stream, "\"elem\":\"{}\",", _type & 0x0FFFF);
  }
  fmt::format_to(stream, "\"src_id\":{},\"dst_id\":{},\"broker_id\":{}",
                 source_id, destination_id, broker_id);
}
