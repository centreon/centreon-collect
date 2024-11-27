/**
 * Copyright 2013,2017,2020-2024 Centreon
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

#ifndef CCB_BBDO_INTERNAL_HH
#define CCB_BBDO_INTERNAL_HH

#include "bbdo/bbdo.pb.h"
#include "bbdo/events.hh"
#include "bbdo/extcmd.pb.h"
#include "com/centreon/broker/io/protobuf.hh"

#define BBDO_VERSION_MAJOR 2
#define BBDO_VERSION_MINOR 0
#define BBDO_VERSION_PATCH 0
constexpr uint32_t BBDO_HEADER_SIZE = 16u;

namespace com::centreon::broker {
namespace bbdo {

using pb_welcome =
    com::centreon::broker::io::protobuf<Welcome,
                                        make_type(io::bbdo, bbdo::de_welcome)>;

using pb_ack =
    com::centreon::broker::io::protobuf<Ack,
                                        make_type(io::bbdo, bbdo::de_pb_ack)>;

using pb_stop =
    com::centreon::broker::io::protobuf<Stop,
                                        make_type(io::bbdo, bbdo::de_pb_stop)>;

using pb_engine_configuration = com::centreon::broker::io::protobuf<
    EngineConfiguration,
    make_type(io::bbdo, bbdo::de_pb_engine_configuration)>;

using pb_bench = com::centreon::broker::io::
    protobuf<Bench, make_type(io::extcmd, extcmd::de_pb_bench)>;

// Load/unload of BBDO protocol.
void load();
void unload();

}  // namespace bbdo

namespace local {
using pb_stop = com::centreon::broker::io::
    protobuf<Stop, make_type(io::local, local::de_pb_stop)>;
}

}  // namespace com::centreon::broker

#endif  // !CCB_BBDO_INTERNAL_HH
