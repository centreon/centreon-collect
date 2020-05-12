set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(gRPC CONFIG REQUIRED)
include_directories(${CMAKE_BINARY_DIR})

get_filename_component(hw_proto "${CMAKE_SOURCE_DIR}/src/ccb_rpc/broker.proto" ABSOLUTE)
get_filename_component(hw_proto_path "${hw_proto}" PATH)

set(hw_proto_srcs "${CMAKE_BINARY_DIR}/broker.pb.cc")
set(hw_proto_hdrs "${CMAKE_BINARY_DIR}/broker.pb.h")
set(hw_grpc_srcs "${CMAKE_BINARY_DIR}/broker.grpc.pb.cc")
set(hw_grpc_hdrs "${CMAKE_BINARY_DIR}/broker.grpc.pb.h")

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
  ccb_rpc
  STATIC

  ${hw_proto_srcs}
  ${hw_proto_hdrs}
  ${hw_grpc_srcs}
  ${hw_grpc_hdrs}
)
target_include_directories(ccb_rpc PUBLIC ${CMAKE_BINARY_DIR})
target_include_directories(ccb_rpc PUBLIC ${CMAKE_SOURCE_DIR}/include/)
target_link_libraries(ccb_rpc PRIVATE CONAN_PKG::protobuf CONAN_PKG::grpc)
target_compile_features(ccb_rpc PUBLIC cxx_std_11)