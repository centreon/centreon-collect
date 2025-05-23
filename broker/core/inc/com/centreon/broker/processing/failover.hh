/**
 * Copyright 2011-2013,2015-2024 Centreon
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

#ifndef CCB_PROCESSING_FAILOVER_HH
#define CCB_PROCESSING_FAILOVER_HH

#include <climits>
#include "com/centreon/broker/io/endpoint.hh"
#include "com/centreon/broker/multiplexing/muxer.hh"

#include "com/centreon/broker/processing/acceptor.hh"

namespace com::centreon::broker {

// Forward declarations.
namespace io {
class properties;
}
namespace stats {
class builder;
}

namespace processing {
/**
 *  @class failover failover.hh "com/centreon/broker/processing/failover.hh"
 *  @brief Failover thread.
 *
 *  Thread that provide failover on output endpoints.
 *
 *  Multiple failover can be forwarded.
 */
class failover : public endpoint {
  friend class stats::builder;
  std::atomic_bool _should_exit;

  std::thread _thread;
  enum _running_state { not_started, running, stopped };
  _running_state _state ABSL_GUARDED_BY(_state_m);
  mutable absl::Mutex _state_m;
  std::shared_ptr<spdlog::logger> _logger;

  void _run();

 public:
  failover(std::shared_ptr<io::endpoint> endp,
           std::shared_ptr<multiplexing::muxer> mux,
           std::string const& name);
  failover(failover const& other) = delete;
  failover& operator=(failover const& other) = delete;
  ~failover();
  void add_secondary_endpoint(std::shared_ptr<io::endpoint> endp);
  time_t get_buffering_timeout() const throw();
  bool get_initialized() const throw();
  time_t get_retry_interval() const throw();
  void start() override;
  void exit() override final;
  bool should_exit() const;
  void set_buffering_timeout(time_t secs);
  void set_failover(std::shared_ptr<processing::failover> fo);
  void set_retry_interval(time_t retry_interval);
  void update() override;
  bool wait_for_all_events_written(unsigned ms_timeout) override;

 protected:
  // From stat_visitable
  std::string const& _get_read_filters() const override;
  std::string const& _get_write_filters() const override;
  uint32_t _get_queued_events() const override;
  virtual void _forward_statistic(nlohmann::json& tree) override;

 private:
  void _launch_failover();
  void _update_status(std::string const& status);

  // Data that doesn't require locking.
  volatile time_t _buffering_timeout;
  std::shared_ptr<io::endpoint> _endpoint;
  std::vector<std::shared_ptr<io::endpoint> > _secondary_endpoints;
  std::shared_ptr<failover> _failover;
  bool _failover_launched;
  volatile bool _initialized;
  time_t _next_timeout;
  volatile time_t _retry_interval = 15;
  std::shared_ptr<multiplexing::muxer> _muxer;
  volatile bool _update;

  // Status.
  std::string _status;
  mutable std::mutex _status_m;

  // Stream.
  std::shared_ptr<io::stream> _stream;
  mutable std::timed_mutex _stream_m;
};
}  // namespace processing

}  // namespace com::centreon::broker

#endif  // !CCB_PROCESSING_FAILOVER_HH
