/**
 * Copyright 2009-2013,2020-2021 Centreon
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

#include "com/centreon/broker/config/endpoint.hh"

using namespace com::centreon::broker::config;

/**
 * @brief endpoint constructor. It stores the direction of data, way may be
 * input or output, this corresponds to the broker configuration files.
 *
 * @param way io_type::input or io_type::output.
 */
endpoint::endpoint(endpoint::io_type way)
    : _type(way),
      buffering_timeout(0),
      read_timeout((time_t)-1),
      retry_interval(15),
      cache_enabled(false) {}

/**
 *  Copy constructor.
 *
 *  @param[in] other  Object to copy.
 */
endpoint::endpoint(const endpoint& other)
    : _type(other._type),
      buffering_timeout{other.buffering_timeout},
      failovers{other.failovers},
      name{other.name},
      params{other.params},
      read_filters{other.read_filters},
      read_timeout{other.read_timeout},
      retry_interval{other.retry_interval},
      type{other.type},
      write_filters{other.write_filters},
      cache_enabled{other.cache_enabled},
      cfg(other.cfg) {}

/**
 *  Assignment operator.
 *
 *  @param[in] other  Object to copy.
 *
 *  @return This object.
 */
endpoint& endpoint::operator=(const endpoint& other) {
  if (this != &other) {
    buffering_timeout = other.buffering_timeout;
    failovers = other.failovers;
    name = other.name;
    params = other.params;
    read_filters = other.read_filters;
    read_timeout = other.read_timeout;
    retry_interval = other.retry_interval;
    type = other.type;
    write_filters = other.write_filters;
    cache_enabled = other.cache_enabled;
    cfg = other.cfg;
  }
  return *this;
}

/**
 *  Check that two endpoint configurations are equal.
 *
 *  @param[in] other  Object to compare to.
 *
 *  @return True if both objects are equal, false otherwise.
 */
bool endpoint::operator==(const endpoint& other) const {
  return type == other.type && buffering_timeout == other.buffering_timeout &&
         read_timeout == other.read_timeout &&
         retry_interval == other.retry_interval && name == other.name &&
         failovers == other.failovers && read_filters == other.read_filters &&
         write_filters == other.write_filters && params == other.params &&
         cache_enabled == other.cache_enabled && cfg == other.cfg;
}

/**
 *  Check that two endpoint configurations are inequal.
 *
 *  @param[in] other  Object to compare to.
 *
 *  @return True if both objects are not equal, false otherwise.
 */
bool endpoint::operator!=(const endpoint& other) const {
  return !operator==(other);
}

/**
 *  Inequality operator.
 *
 *  @param[in] other  Object to compare to.
 *
 *  @return True if this object is strictly less than the object e.
 */
bool endpoint::operator<(const endpoint& other) const {
  // Check properties that can directly be checked.
  if (type != other.type)
    return type < other.type;
  else if (buffering_timeout != other.buffering_timeout)
    return buffering_timeout < other.buffering_timeout;
  else if (read_timeout != other.read_timeout)
    return read_timeout < other.read_timeout;
  else if (retry_interval != other.retry_interval)
    return retry_interval < other.retry_interval;
  else if (name != other.name)
    return name < other.name;
  else if (failovers != other.failovers)
    return failovers < other.failovers;
  else if (read_filters != other.read_filters)
    return read_filters < other.read_filters;
  else if (write_filters != other.write_filters)
    return write_filters < other.write_filters;
  else if (cache_enabled != other.cache_enabled)
    return cache_enabled < other.cache_enabled;
  else if (cfg != other.cfg)
    return cfg < other.cfg;

  // Need to check all parameters one by one.
  auto it1 = params.begin(), it2 = other.params.begin(), end1 = params.end(),
       end2 = other.params.end();
  while (it1 != end1 && it2 != end2) {
    if (it1->first != it2->first)
      return it1->first < it2->first;
    else if (it1->second != it2->second)
      return it1->second < it2->second;
    ++it1;
    ++it2;
  }
  return it1 == end1 && it2 != end2;
}
