/**
 * Copyright 2009-2013,2015 Centreon
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

#ifndef CCB_NEB_HOST_HH
#define CCB_NEB_HOST_HH

#include "com/centreon/broker/neb/host_service.hh"
#include "com/centreon/broker/neb/host_status.hh"

namespace com::centreon::broker {

namespace neb {
/**
 *  @class host host.hh "com/centreon/broker/neb/host.hh"
 *  @brief Host within the scheduling engine.
 *
 *  The scheduling engine has two main objects that can be
 *  manipulated : host and service. An host object holds every
 *  parameter related to a host machine.
 */
class host : public host_service, public host_status {
 public:
  host();
  host(host_status const& hs);
  host(host const& other);
  ~host();
  host& operator=(host const& other);
  constexpr static uint32_t static_type() {
    return io::events::data_type<io::neb, neb::de_host>::value;
  }

  std::string address;
  std::string alias;
  bool flap_detection_on_down;
  bool flap_detection_on_unreachable;
  bool flap_detection_on_up;
  std::string host_name;
  bool notify_on_down;
  bool notify_on_unreachable;
  uint32_t poller_id;
  bool stalk_on_down;
  bool stalk_on_unreachable;
  bool stalk_on_up;
  std::string statusmap_image;
  std::string timezone;

  static mapping::entry const entries[];
  static io::event_info::event_operations const operations;

 private:
  void _internal_copy(host const& other);
  void _zero_initialize();
};
}  // namespace neb

}  // namespace com::centreon::broker

#endif  // !CCB_NEB_HOST_HH
