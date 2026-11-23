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
 *
 */

#ifndef CCB_BROKER_NOTIFICATION_DISPATCHER_HH
#define CCB_BROKER_NOTIFICATION_DISPATCHER_HH

#include <spdlog/logger.h>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>

#include "com/centreon/broker/multiplexing/event_sink.hh"
#include "common/notifications/notification_types.hh"

namespace com::centreon::broker {

/**
 * @brief Broker-side notification trigger (notification_mode=broker).
 *
 * Registered as an event_sink on the multiplexing engine: for every batch it
 * looks at the host/service status events and drives the notification_manager,
 * which decides and dispatches the execution to the supervising poller. This is
 * the DECISION trigger; broker_notification_callbacks is the library backend
 * the manager ultimately calls into.
 *
 * The notification_manager is not thread-safe. Every access to it goes
 * through the strand this class owns: the engine posts the batches on it
 * (executor()), and any other caller — an acknowledgement asking for its
 * notification, the notification RPCs — posts its work with the methods below
 * instead of touching the manager from its own thread.
 */
class broker_notification_dispatcher : public multiplexing::event_sink {
  std::shared_ptr<spdlog::logger> _logger;
  boost::asio::strand<boost::asio::io_context::executor_type> _strand;

 public:
  broker_notification_dispatcher(boost::asio::io_context& io_context);
  ~broker_notification_dispatcher() override = default;

  void on_events(const std::deque<std::shared_ptr<io::data>>& events) override;
  boost::asio::any_io_executor executor() override { return _strand; }

  void post_notify(uint64_t host_id,
                   uint64_t service_id,
                   common::notifications::reason_type reason,
                   const std::string& author,
                   const std::string& comment,
                   common::notifications::notification_option options);
  void post_set_notification_number(uint64_t host_id,
                                    uint64_t service_id,
                                    uint32_t number);
};

}  // namespace com::centreon::broker

#endif  // !CCB_BROKER_NOTIFICATION_DISPATCHER_HH
