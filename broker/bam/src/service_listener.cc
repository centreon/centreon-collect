/**
 * Copyright 2014-2015 Centreon
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

#include "com/centreon/broker/bam/service_listener.hh"

using namespace com::centreon::broker::bam;

/**
 *  Notify of a service status update.
 *
 *  @param[in]  status   Service status.
 *  @param[out] visitor  Visitor.
 */
void service_listener::service_update(const service_state& state
                                      [[maybe_unused]]) {}

/**
 *  Notify of a service status update.
 *
 *  @param[in]  status   Service status.
 *  @param[out] visitor  Visitor.
 */
void service_listener::service_update(
    std::shared_ptr<neb::service_status> const& status,
    io::stream* visitor) {
  (void)status;
  (void)visitor;
}

/**
 *  Notify of a service status update.
 *
 *  @param[in]  status   Service status.
 *  @param[out] visitor  Visitor.
 */
void service_listener::service_update(
    const std::shared_ptr<neb::pb_service>& status,
    io::stream* visitor) {
  (void)status;
  (void)visitor;
}

/**
 *  Notify of a service status update.
 *
 *  @param[in]  status   Service status.
 *  @param[out] visitor  Visitor.
 */
void service_listener::service_update(
    const std::shared_ptr<neb::pb_service_status>& status,
    io::stream* visitor) {
  (void)status;
  (void)visitor;
}

/**
 *  Notify of a protobuf acknowledgement.
 *
 *  @param[in]  ack      Acknowledgement.
 *  @param[out] visitor  Visitor.
 */
void service_listener::service_update(
    const std::shared_ptr<neb::pb_acknowledgement>& ack,
    io::stream* visitor) {
  (void)ack;
  (void)visitor;
}

/**
 *  Notify of an acknowledgement.
 *
 *  @param[in]  ack      Acknowledgement.
 *  @param[out] visitor  Visitor.
 */
void service_listener::service_update(
    std::shared_ptr<neb::acknowledgement> const& ack,
    io::stream* visitor) {
  (void)ack;
  (void)visitor;
}

/**
 *  Notify of a downtime.
 *
 *  @param[in]  dt       Downtime.
 *  @param[out] visitor  Visitor.
 */
void service_listener::service_update(std::shared_ptr<neb::downtime> const& dt,
                                      io::stream* visitor) {
  (void)dt;
  (void)visitor;
}

/**
 *  Notify of a downtime (protobuf).
 *
 *  @param[in]  dt       Downtime.
 *  @param[out] visitor  Visitor.
 */
void service_listener::service_update(
    const std::shared_ptr<neb::pb_downtime>& dt,
    io::stream* visitor) {
  (void)dt;
  (void)visitor;
}
