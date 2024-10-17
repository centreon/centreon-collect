/**
 * Copyright 2014-2015, 2021-2024 Centreon
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

#ifndef CCB_BAM_SERVICE_LISTENER_HH
#define CCB_BAM_SERVICE_LISTENER_HH

#include "com/centreon/broker/neb/internal.hh"

namespace com::centreon::broker {

// Forward declarations.
namespace neb {
class acknowledgement;
class downtime;
class service_status;
}  // namespace neb

namespace bam {
struct service_state;

/**
 *  @class service_listener service_listener.hh
 * "com/centreon/broker/bam/service_listener.hh"
 *  @brief Listen to service state change.
 *
 *  This interface is used by classes wishing to listen to service
 *  state change.
 */
class service_listener {
 public:
  service_listener() = default;
  service_listener(const service_listener&) = delete;
  virtual ~service_listener() noexcept = default;
  service_listener& operator=(const service_listener&) = delete;
  virtual void service_update(const service_state& s);
  virtual void service_update(std::shared_ptr<neb::pb_service> const& status,
                              io::stream* visitor = nullptr);
  virtual void service_update(
      std::shared_ptr<neb::pb_adaptive_service_status> const& status,
      io::stream* visitor = nullptr);
  virtual void service_update(
      std::shared_ptr<neb::pb_service_status> const& status,
      io::stream* visitor = nullptr);
  virtual void service_update(
      std::shared_ptr<neb::service_status> const& status,
      io::stream* visitor = nullptr);
  virtual void service_update(
      const std::shared_ptr<neb::pb_acknowledgement>& ack,
      io::stream* visitor = nullptr);
  virtual void service_update(std::shared_ptr<neb::acknowledgement> const& ack,
                              io::stream* visitor = nullptr);
  virtual void service_update(std::shared_ptr<neb::downtime> const& dt,
                              io::stream* visitor = nullptr);
  virtual void service_update(const std::shared_ptr<neb::pb_downtime>& dt,
                              io::stream* visitor = nullptr);
};
}  // namespace bam

}  // namespace com::centreon::broker

#endif  // !CCB_BAM_SERVICE_LISTENER_HH
