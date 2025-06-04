/**
 * Copyright 2024-2025 Centreon
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
#ifndef CCB_NEB_CBMOD_HH
#define CCB_NEB_CBMOD_HH
#include <filesystem>
#include "bbdo/bbdo_version.hh"
#include "com/centreon/broker/neb/acknowledgement.hh"
#include "com/centreon/broker/neb/downtime.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "common/engine_conf/state_helper.hh"

namespace com::centreon::broker {
namespace multiplexing {
class publisher;
}  // namespace multiplexing
namespace neb {
class cbmodimpl;

class cbmod {
  std::shared_ptr<spdlog::logger> _neb_logger;
  std::unique_ptr<cbmodimpl> _impl;
  bool _use_protobuf;

  // Acknowledgements list.
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>,
                      std::shared_ptr<pb_acknowledgement>>
      _acknowledgements;

  // Unstarted downtimes.
  std::unordered_map<uint64_t, std::shared_ptr<neb::pb_downtime>> _downtimes;

 public:
  cbmod(const std::filesystem::path& proto_conf);
  cbmod(const std::string& config_file,
        const std::filesystem::path& proto_conf,
        const std::string& engine_conf_version);
  cbmod& operator=(const cbmod&) = delete;

  virtual ~cbmod() noexcept;
  virtual void write(const std::shared_ptr<io::data>& msg);
  virtual uint64_t poller_id() const;
  virtual const std::string& poller_name() const;

  const bbdo::bbdo_version bbdo_version() const;
  bool use_protobuf() const;
  void add_acknowledgement(const std::shared_ptr<neb::acknowledgement>& ack);
  void add_acknowledgement(const std::shared_ptr<neb::pb_acknowledgement>& ack);
  std::shared_ptr<pb_acknowledgement> find_acknowledgement(
      uint64_t host_id,
      uint64_t service_id) const;
  void remove_acknowledgement(uint64_t host_id, uint64_t service_id);
  size_t acknowledgements_count() const;
  void add_downtime(uint64_t downtime_id,
                    uint64_t host_id,
                    uint64_t service_id,
                    const char* author_name,
                    const char* comment_data,
                    int downtime_type,
                    time_t entry_time,
                    time_t start_time,
                    time_t end_time,
                    uint32_t duration,
                    uint64_t triggered_by,
                    bool fixed);
  void start_downtime(uint64_t downtime_id);
  void stop_downtime(uint64_t downtime_id, bool cancelled);
  void remove_downtime(uint64_t downtime_id);
  void reload();
  std::unique_ptr<com::centreon::engine::configuration::DiffState> diff_state();
  void set_diff_state_applied(const std::string& config_version);
};
}  // namespace neb
}  // namespace com::centreon::broker

#endif /* !CCB_NEB_CBMOD_HH */
