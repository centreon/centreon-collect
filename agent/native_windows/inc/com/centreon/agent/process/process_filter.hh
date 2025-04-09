/**
 * Copyright 2025 Centreon
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

#ifndef CENTREON_AGENT_CHECK_PROCESS_FILTER_HH
#define CENTREON_AGENT_CHECK_PROCESS_FILTER_HH

#include "filter.hh"

#include "process_data.hh"

namespace com::centreon::agent::process {
class process_data;

/**
 * @brief process_filter
 * This class is used to filter process data and get a field_mask
 * (process_field) that will be used to get the only needed process data
 *
 */
class process_filter {
  filters::filter_combinator _filter;
  unsigned _fields_mask;

  void _create_checker(filter* f);

  void _set_label_compare_to_value(filters::label_compare_to_value* filt);

  template <class filter_type>
  void _set_getter(filter_type* filt);

 public:
  process_filter(const std::string_view filter_str,
                 const std::shared_ptr<spdlog::logger>& logger);

  void set_filter_logger(const std::shared_ptr<spdlog::logger>& logger) {
    _filter.set_logger(logger);
  }

  bool check(const process_data& data) const;

  unsigned get_fields_mask() const { return _fields_mask; }

  const filters::filter_combinator& get_filter() const { return _filter; }
};

}  // namespace com::centreon::agent::process

#endif
