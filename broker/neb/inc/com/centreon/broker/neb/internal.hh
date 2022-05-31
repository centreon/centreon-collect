/*
** Copyright 2009-2015, 2021 Centreon
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

#ifndef CCB_NEB_INTERNAL_HH
#define CCB_NEB_INTERNAL_HH

#include "bbdo/events.hh"
#include "bbdo/host.pb.h"
#include "bbdo/service.pb.h"
#include "bbdo/severity.pb.h"
#include "bbdo/tag.pb.h"
#include "com/centreon/broker/io/protobuf.hh"
#include "com/centreon/broker/misc/pair.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/broker/namespace.hh"
#include "com/centreon/broker/neb/callback.hh"

CCB_BEGIN()

namespace neb {
// Forward declaration.
class acknowledgement;

// Configuration file.
extern std::string gl_configuration_file;

// Sender object.
extern multiplexing::publisher gl_publisher;

// Registered callbacks.
extern std::list<std::unique_ptr<neb::callback>> gl_registered_callbacks;

// Acknowledgement list.
extern std::unordered_map<std::pair<uint32_t, uint32_t>, neb::acknowledgement>
    gl_acknowledgements;

using pb_service =
    io::protobuf<Service, make_type(io::neb, neb::de_pb_service)>;
using pb_adaptive_service =
    io::protobuf<AdaptiveService,
                 make_type(io::neb, neb::de_pb_adaptive_service)>;

using pb_service_status =
    io::protobuf<ServiceStatus, make_type(io::neb, neb::de_pb_service_status)>;

using pb_host_status =
    io::protobuf<HostStatus, make_type(io::neb, neb::de_pb_host_status)>;
using pb_host = io::protobuf<Host, make_type(io::neb, neb::de_pb_host)>;
using pb_adaptive_host =
    io::protobuf<AdaptiveHost, make_type(io::neb, neb::de_pb_adaptive_host)>;

using pb_severity =
    io::protobuf<Severity, make_type(io::neb, neb::de_pb_severity)>;
using pb_tag = io::protobuf<Tag, make_type(io::neb, neb::de_pb_tag)>;

}  // namespace neb

CCB_END()

#endif  // !CCB_NEB_INTERNAL_HH
