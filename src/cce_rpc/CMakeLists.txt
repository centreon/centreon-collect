set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(gRPC CONFIG REQUIRED)
include_directories(${CMAKE_BINARY_DIR})

get_filename_component(hw_proto "${CMAKE_SOURCE_DIR}/src/cce_rpc/engine.proto" ABSOLUTE)
get_filename_component(hw_proto_path "${hw_proto}" PATH)

set(hw_proto_srcs "${CMAKE_BINARY_DIR}/engine.pb.cc")
set(hw_proto_hdrs "${CMAKE_BINARY_DIR}/engine.pb.h")
set(hw_grpc_srcs "${CMAKE_BINARY_DIR}/engine.grpc.pb.cc")
set(hw_grpc_hdrs "${CMAKE_BINARY_DIR}/engine.grpc.pb.h")

add_custom_command(
  OUTPUT "${hw_proto_srcs}" "${hw_proto_hdrs}" "${hw_grpc_srcs}" "${hw_grpc_hdrs}"
  COMMAND protobuf::protoc
  ARGS --grpc_out "${CMAKE_BINARY_DIR}"
  --cpp_out "${CMAKE_BINARY_DIR}"
  -I "${hw_proto_path}"
  --plugin=protoc-gen-grpc="$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
  "${hw_proto}"
  DEPENDS "${hw_proto}")

add_library(
  cce_rpc
  STATIC

  ${hw_proto_srcs}
  ${hw_proto_hdrs}
  ${hw_grpc_srcs}
  ${hw_grpc_hdrs}
)
target_include_directories(cce_rpc PUBLIC ${CMAKE_BINARY_DIR})
target_include_directories(cce_rpc PUBLIC ${CMAKE_SOURCE_DIR}/include/)
target_link_libraries(cce_rpc PUBLIC CONAN_PKG::protobuf CONAN_PKG::grpc)
target_compile_features(cce_rpc PUBLIC cxx_std_11)