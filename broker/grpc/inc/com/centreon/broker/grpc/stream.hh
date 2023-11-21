/*
** Copyright 2022 Centreon
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

#ifndef CCB_GRPC_STREAM_HH__
#define CCB_GRPC_STREAM_HH__

#include "channel.hh"

namespace com::centreon::broker {

namespace grpc {

class accepted_service;
class stream : public io::stream {
 protected:
  std::string _hostport;
  bool _accept;

  std::shared_ptr<channel> _channel;

  /* Logger */
  std::shared_ptr<spdlog::logger> _logger;

 public:
  stream(const grpc_config::pointer& conf);
  stream(const std::shared_ptr<accepted_service>&);

  stream& operator=(const stream&) = delete;
  stream(const stream&) = delete;

  ~stream() noexcept;

  bool read(std::shared_ptr<io::data>& d, time_t deadline) override;
  bool read(std::shared_ptr<io::data>& d,
            const system_clock::time_point& deadline);
  bool read(std::shared_ptr<io::data>& d,
            const system_clock::duration& timeout);
  int32_t write(std::shared_ptr<io::data> const& d) override;

  int32_t flush() override;
  int32_t stop() override;

  bool is_down() const;

  bool wait_for_all_events_written(unsigned ms_timeout) override;
};
}  // namespace grpc

}  // namespace com::centreon::broker

#endif  // !CCB_GRPC_STREAM_HH
