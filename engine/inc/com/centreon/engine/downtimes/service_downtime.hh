/*
 * Copyright 2019 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
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

#ifndef CCE_DOWNTIMES_SERVICE_DOWTIME_HH
#define CCE_DOWNTIMES_SERVICE_DOWTIME_HH

#include "com/centreon/engine/downtimes/downtime.hh"

namespace com::centreon::engine {

namespace downtimes {
class service_downtime : public downtime {
  const uint64_t _service_id;

 public:
  service_downtime(const uint64_t host_id,
                   const uint64_t service_id,
                   time_t entry_time,
                   std::string const& author,
                   std::string const& comment,
                   time_t start_time,
                   time_t end_time,
                   bool fixed,
                   uint64_t triggered_by,
                   int32_t duration,
                   uint64_t downtime_id);
  service_downtime(downtime const& other);
  service_downtime(downtime&& other);
  virtual ~service_downtime();
  uint64_t service_id() const;
  virtual bool is_stale() const override;
  virtual void schedule() override;
  virtual int unschedule() override;
  virtual int subscribe() override;
  virtual int handle() override;
  virtual void print(std::ostream& os) const override;
  virtual void retention(std::ostream& os) const override;
};
}  // namespace downtimes

}

#endif  // !CCE_DOWNTIMES_SERVICE_DOWTIME_HH
