#!/usr/bin/python3
"""
** Copyright 2019-2020 Centreon
**
** Licensed under the Apache License, Version 2.0(the "License");
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
"""

from os import listdir
from os.path import isfile, join
import re
import argparse


file_begin_content = """syntax = "proto3";

"""

file_message_centreon_event = """
message centreon_event {
    oneof content {
        bytes buffer = 1;

"""

cc_file_begin_content = """
/*
 * Copyright 2022 Centreon (https://www.centreon.com/)
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
 *
 *  File generated by generate_proto.py
 *
 */

 

#include "grpc_stream.pb.h"
#include "com/centreon/broker/io/protobuf.hh"

#include "com/centreon/broker/grpc/channel.hh"

using namespace com::centreon::broker;

CCB_BEGIN()

namespace grpc {


namespace detail {
/**
 * @brief we pass our protobuf objects to grpc_event without copy
 * so we must avoid that grpc_event delete message of protobuf object
 * This the goal of this struct.
 * At destruction, it releases protobuf object from grpc_event.
 * Destruction of protobuf object is the job of shared_ptr<io::protobuf>
 * @tparam grpc_event_releaser lambda that have to release submessage
 */
template <typename grpc_event_releaser>
struct event_with_data : public channel::event_with_data {
  grpc_event_releaser _releaser;

  event_with_data(const std::shared_ptr<io::data>& bbdo_evt,
                  grpc_event_releaser&& cleaner_lambda)
      : channel::event_with_data(bbdo_evt), _releaser(cleaner_lambda) {}

  ~event_with_data() { _releaser(grpc_event); }
};

template <typename grpc_event_releaser>
std::shared_ptr<event_with_data<grpc_event_releaser>> create_event_with_data(
    std::shared_ptr<io::data> event,
    grpc_event_releaser&& cleaner_lambda) {
  return std::make_shared<event_with_data<grpc_event_releaser>>(
      event, std::move(cleaner_lambda));
}

}  // namespace detail

"""

# cc_file_event_to_protobuf_function = """
# /**
#  * @brief this function set the content of a protobuf message that will be send
#  * on grpc.
#  * stream_content don't have a copy of event, so event mustn't be
#  * deleted before stream_content
#  * event is not const due to protobuf exigences,
#  * nevertheless, this object won't be modified by grpc layers
#  *
#  * @param event to send
#  * @param stream_content object sent on the wire
#  */
# void event_to_protobuf(io::data & event, stream::centreon_event & stream_content) {
#     stream_content.set_destination_id(event.destination_id);
#     stream_content.set_source_id(event.source_id);
#     switch(event.type()) {
# """

cc_file_protobuf_to_event_function = """
/**
 * @brief this function creates a io::protobuf_object from grpc received message
 *
 * @param stream_content message received
 * @return std::shared_ptr<io::data> shared_ptr<io::protobuf<xxx>>, null if
 * unknown content received
 */
std::shared_ptr<io::data> protobuf_to_event(const stream::centreon_event & stream_content) {
    switch(stream_content.content_case()) {
"""

cc_file_create_event_with_data_function = """

#pragma GCC diagnostic ignored "-Wunused-result"

/**
 * @brief this function create a event_with_data structure that will be send on grpc.
 * stream_content don't have a copy of event, so event mustn't be
 * deleted before stream_content
 * event is not const due to protobuf exigences,
 * nevertheless, this object won't be modified by grpc layers
 *
 * @param event to send
 * @return object used for send on the wire
 */
std::shared_ptr<channel::event_with_data> create_event_with_data(const std::shared_ptr<io::data> & event) {
    std::shared_ptr<channel::event_with_data> ret;
    switch(event->type()) {
"""
cc_file_create_event_with_data_function_end = """
    default:
        SPDLOG_LOGGER_ERROR(log_v2::grpc(), "unknown event type: {}", *event);
    }
    if (ret) {
        ret->grpc_event.set_destination_id(event->destination_id);
        ret->grpc_event.set_source_id(event->source_id);
    }
    return ret;
}
"""


parser = argparse.ArgumentParser(
    prog='generate_proto.py', description='generate grpc_stream.proto file by referencing message found in protobuf files')
parser.add_argument('-f', '--proto_file',
                    help='outputs files use by grpc module', required=True)
parser.add_argument('-c', '--cc_file',
                    help='output source file that must be compiled and linked int target library', required=True)
parser.add_argument('-d', '--proto_directory',
                    help='directory where to find protobuf files', required=True, action='append')

args = parser.parse_args()

message_parser = r'^message\s+(\w+)\s+\{'
io_protobuf_parser = r'\/\*\s*(\w+::\w+\s*,\s*\w+::\w+)\s*\*\/'

one_of_index = 2

for directory in args.proto_directory:
    proto_files = [f for f in listdir(
        directory) if f[-6:] == ".proto" and isfile(join(directory, f))]
    for file in proto_files:
        with open(join(directory, file)) as proto_file:
            messages = []
            io_protobuf_match = None
            for line in proto_file.readlines():
                m = re.match(message_parser, line)
                if m is not None and io_protobuf_match is not None:
                    messages.append([m.group(1), io_protobuf_match.group(1)])
                    io_protobuf_match = None
                else:
                    io_protobuf_match = re.match(io_protobuf_parser, line)

            if len(messages) > 0:
                file_begin_content += f"import \"{file}\";\n"
                for mess, id in messages:
                    # proto file
                    file_message_centreon_event += f"        {mess} {mess}_ = {one_of_index};\n"
                    one_of_index += 1
                    lower_mess = mess.lower()
                    # cc file
                    cc_file_protobuf_to_event_function += f"""        case ::stream::centreon_event::k{mess}:
            return std::make_shared<io::protobuf<{mess}, make_type({id})>>(stream_content.{lower_mess}_(), stream_content.source_id(), stream_content.destination_id());
"""
#                     cc_file_event_to_protobuf_function += f"""        case make_type({id}):
#             stream_content.set_allocated_{lower_mess}_(&static_cast<io::protobuf<{mess}, make_type({id})> &>(event).mut_obj());
#             break;
# """
                    cc_file_create_event_with_data_function += f"""        case make_type({id}):
            ret = detail::create_event_with_data(event, [](grpc_event_type & to_clean) {{ (void)to_clean.release_{lower_mess}_(); }});
            ret->grpc_event.set_allocated_{lower_mess}_(&std::static_pointer_cast<io::protobuf<{mess}, make_type({id})>>(event)->mut_obj());
            break;

"""

with open(args.proto_file, 'w', encoding="utf-8") as fp:
    fp.write(file_begin_content)
    fp.write("""

package com.centreon.broker.stream;

""")
    fp.write(file_message_centreon_event)
    fp.write("""
    }
    uint32 destination_id = 126;
    uint32 source_id = 127;
}

service centreon_bbdo {
    //emitter connect to receiver
    rpc exchange(stream centreon_event) returns (stream centreon_event) {}
}
                          
""")

with open(args.cc_file, 'w') as fp:
    fp.write(cc_file_begin_content)
    fp.write(cc_file_protobuf_to_event_function)
    fp.write("""        default:
      SPDLOG_LOGGER_ERROR(log_v2::grpc(), "unknown content type: {} => ignored",
                          stream_content.content_case());
      return std::shared_ptr<io::data>();
    }
}


""")
    fp.write(cc_file_create_event_with_data_function)
    fp.write(cc_file_create_event_with_data_function_end)
    fp.write("""

} //namespace grpc

CCB_END()
             
""")
