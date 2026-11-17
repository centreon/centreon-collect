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
#ifndef CCB_NEB_CBMOD_TEST_HH
#define CCB_NEB_CBMOD_TEST_HH

#include "com/centreon/broker/neb/cbmod.hh"
#include "common/log_v2/log_v2.hh"

using com::centreon::common::log_v2::log_v2;

namespace com::centreon::broker::neb {

class cbmod_test : public com::centreon::broker::neb::cbmod {
  const std::string _poller_name = "test";

 public:
  cbmod_test() : cbmod() {}
  ~cbmod_test() noexcept = default;

  /* Events written while record is set, for tests checking what engine sends
   * to broker. */
  bool record = false;
  std::vector<std::shared_ptr<io::data>> written;

  void write(const std::shared_ptr<io::data>& d) override {
    if (record)
      written.push_back(d);
  }
  /* Ids returned by poller_id() (the Broker uid when set) and
   * original_poller_id() (the 'poller_id' key), for tests of the BAM virtual
   * host lookup. */
  uint64_t poller_id_value = 1;
  uint64_t original_poller_id_value = 1;

  uint64_t poller_id() const { return poller_id_value; }
  uint64_t original_poller_id() const override {
    return original_poller_id_value;
  }
  const std::string& poller_name() const { return _poller_name; }
};
}  // namespace com::centreon::broker::neb

#endif /* !CCB_NEB_CBMOD_TEST_HH */
