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
#include <memory>
#include "bbdo/bbdo_version.hh"
#include "com/centreon/broker/neb/acknowledgement.hh"
//#include "state.pb.h"

namespace com::centreon::broker {
namespace multiplexing {
class publisher;
}  // namespace multiplexing
namespace neb {
class cbmodimpl;

class cbmod {
  std::shared_ptr<spdlog::logger> _neb_logger;
  std::unique_ptr<cbmodimpl> _impl;
  std::filesystem::path _proto_conf;
  bool _use_protobuf;

  // Engine case
//  mutable absl::Mutex _diff_state_m;
//  std::unique_ptr<com::centreon::engine::configuration::DiffState> _diff_state;

  // Acknowledgements list.
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>,
                      std::shared_ptr<pb_acknowledgement>>
      _acknowledgements;

 public:
  // Downtime internal structure.
  struct private_downtime_params {
    bool cancelled;
    time_t deletion_time;
    time_t end_time;
    bool started;
    time_t start_time;
  };

 private:
  // Unstarted downtimes.
  std::unordered_map<uint32_t, private_downtime_params> _downtimes;

 public:
  cbmod();
  cbmod(const std::string& config_file);
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
  private_downtime_params& get_downtime(uint32_t downtime_id);
  void remove_downtime(uint32_t downtime_id);
};
}  // namespace neb
}  // namespace com::centreon::broker

#endif /* !CCB_NEB_CBMOD_HH */
