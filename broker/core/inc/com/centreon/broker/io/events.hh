/**
 * Copyright 2013-2023 Centreon
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

#ifndef CCB_IO_EVENTS_HH
#define CCB_IO_EVENTS_HH

#include "bbdo/events.hh"
#include "com/centreon/broker/io/event_info.hh"
#include "common/log_v2/log_v2.hh"

namespace com::centreon::broker::io {
/**
 *  @class events events.hh "com/centreon/broker/io/events.hh"
 *  @brief Data events registration.
 *
 *  Maintain the set of existing events.
 */
class events {
 public:
  using events_container = std::unordered_map<uint32_t, event_info>;
  // Internal events used by the core.
  enum internal_event_category { de_raw = 1, de_instance_broadcast, de_buffer };
  // Extcmd events used by the core.
  enum extcmd_event_category {
    de_command_request = 1,
    de_command_result,
  };

  template <uint16_t category, uint16_t element>
  struct data_type {
    enum { value = static_cast<uint32_t>(category << 16 | element) };
  };

  // Singleton.
  static events& instance();
  static void load();
  static void unload();

  // Category.
  void unregister_category(uint16_t category_id);

  // Events.
  uint32_t register_event(uint32_t type_id,
                          std::string const& name = std::string(),
                          event_info::event_operations const* ops = nullptr,
                          mapping::entry const* entries = nullptr,
                          std::string const& table_v2 = std::string());
  uint32_t register_event(uint32_t type_id,
                          std::string const& name,
                          event_info::event_operations const* ops,
                          const std::string& table);

  void unregister_event(uint32_t type_id);

  // Event browsing.
  events_container get_events_by_category_name(std::string const& name) const;
  event_info const* get_event_info(uint32_t type);
  events_container get_matching_events(std::string const& name) const;

 private:
  std::shared_ptr<spdlog::logger> _logger;

  events();
  events(const events&) = delete;
  ~events();
  events& operator=(events const& other);

  events_container _elements;
};
}  // namespace com::centreon::broker::io

#endif  // !CCB_IO_EVENTS_HH
