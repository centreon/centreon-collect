/**
 * Copyright 2014-2015, 2024 Centreon
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

#ifndef CCB_BAM_SERVICE_BOOK_HH
#define CCB_BAM_SERVICE_BOOK_HH

#include <unordered_map>
#include "com/centreon/broker/bam/service_state.hh"
#include "com/centreon/broker/persistent_cache.hh"

namespace com::centreon::broker {

// Forward declarations.
namespace neb {
class acknowledgement;
class downtime;
class service_status;
}  // namespace neb

namespace bam {
// Forward declarations.
class service_listener;

/**
 *  @class service_book service_book.hh
 * "com/centreon/broker/bam/service_book.hh"
 *  @brief Propagate service updates.
 *
 *  Propagate updates of services to service listeners.
 */
class service_book {
  struct service_state_listeners {
    std::list<service_listener*> listeners;
    service_state state;
  };
  std::unordered_map<std::pair<uint64_t, uint64_t>,
                     service_state_listeners,
                     absl::Hash<std::pair<uint64_t, uint64_t>>>
      _book;
  std::shared_ptr<spdlog::logger> _logger;

 public:
  service_book(const std::shared_ptr<spdlog::logger>& logger)
      : _logger{logger} {}
  ~service_book() noexcept = default;
  service_book(const service_book&) = delete;
  service_book& operator=(const service_book&) = delete;
  void listen(uint32_t host_id, uint32_t service_id, service_listener* listnr);
  void unlisten(uint32_t host_id,
                uint32_t service_id,
                service_listener* listnr);
  void update(const std::shared_ptr<neb::acknowledgement>& t,
              io::stream* visitor = nullptr);
  void update(const std::shared_ptr<neb::pb_acknowledgement>& t,
              io::stream* visitor = nullptr);
  void update(const std::shared_ptr<neb::downtime>& t,
              io::stream* visitor = nullptr);
  void update(const std::shared_ptr<neb::pb_downtime>& t,
              io::stream* visitor = nullptr);
  void update(const std::shared_ptr<neb::service_status>& t,
              io::stream* visitor = nullptr);
  void update(const std::shared_ptr<neb::pb_service>& t,
              io::stream* visitor = nullptr);
  void update(const std::shared_ptr<neb::pb_service_status>& t,
              io::stream* visitor = nullptr);
  void update(const std::shared_ptr<neb::pb_adaptive_service_status>& t,
              io::stream* visitor = nullptr);
  void save_to_cache(persistent_cache& cache) const;
  void apply_services_state(const ServicesBookState& state);
};
}  // namespace bam

}  // namespace com::centreon::broker

#endif  // !CCB_BAM_SERVICE_BOOK_HH
