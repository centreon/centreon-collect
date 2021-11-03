##
## Copyright 2021 Centreon
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## For more information : contact@centreon.com
##


macro(get_protobuf_files name)
    set(proto_file "${name}.proto")
    set(full_proto_file "${CMAKE_SOURCE_DIR}/protobuf/${name}.proto")
    add_custom_command(OUTPUT "${CMAKE_SOURCE_DIR}/protobuf/${name}.pb.cc" "${CMAKE_SOURCE_DIR}/protobuf/${name}.pb.h"
        DEPENDS ${full_proto_file}
        COMMENT "Generating interface files of the protobuf file ${proto_file}"
        COMMAND ${PROTOBUF_PREFIX}/bin/protoc
        ARGS --cpp_out=${CMAKE_SOURCE_DIR}/protobuf --proto_path=${CMAKE_SOURCE_DIR}/protobuf ${proto_file}
        VERBATIM
        )
  set_source_files_properties("${CMAKE_SOURCE_DIR}/protobuf/${name}.pb.cc" PROPERTIES GENERATED TRUE)
  set_source_files_properties("${CMAKE_SOURCE_DIR}/protobuf/${name}.pb.h" PROPERTIES GENERATED TRUE)
  set(proto_${name}
      "${CMAKE_SOURCE_DIR}/protobuf/${name}.pb.cc"
      "${CMAKE_SOURCE_DIR}/protobuf/${name}.pb.h")
endmacro()
