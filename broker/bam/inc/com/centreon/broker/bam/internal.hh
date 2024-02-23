/**
 * Copyright 2024 Centreon
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

#ifndef CCB_BAM_INTERNAL_HH
#define CCB_BAM_INTERNAL_HH

#include "broker.pb.h"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protobuf.hh"
#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

/* We have to declare the pb_ba_info also here because we don't control the
 * order things are created. If the bam stream is created before brokerrpc, its
 * muxer will be declared with known events (so without pb_ba_info) and if we
 * want pb_ba_info to be known, thenwe have to force its declaration. */
namespace extcmd {
using pb_ba_info =
    io::protobuf<BaInfo, make_type(io::extcmd, extcmd::de_ba_info)>;
}  // namespace extcmd

CCB_END()
#endif /* !CCB_BAM_INTERNAL_HH */
