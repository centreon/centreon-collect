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

set(proto_files
    rebuild.proto
)

set(output)
foreach(f ${proto_files})
  string(REGEX REPLACE "/\\.proto$/g" "" name ${f})
  add_custom_command(
      DEPENDS "${f}"
      COMMENT "Generating interface files of the proto file ${f}"
      OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/${name}.pb.cc" "${CMAKE_CURRENT_SOURCE_DIR}/${name}.pb.h"
      COMMAND ${PROTOBUF_PREFIX}/bin/protoc
      ARGS --cpp_out="${CMAKE_CURRENT_SOURCE_DIR}" --proto_path="${CMAKE_CURRENT_SOURCE_DIR}" "${f}"
      )
  set(output ${output} "${CMAKE_CURRENT_SOURCE_DIR}/${name}.pb.cc" "${CMAKE_CURRENT_SOURCE_DIR}/${name}.pb.h")
endforeach()

add_custom_target(centreon_proto DEPENDS "${output}")
