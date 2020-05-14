link_directories(${CONAN_LIB_DIRS_GRPC})
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_BINARY_DIR})

# Standalone binary.
add_executable(cbd
  ${CMAKE_SOURCE_DIR}/src/cbd/main.cc
  ${CMAKE_SOURCE_DIR}/src/cbd/config/applier/init.cc
  ${CMAKE_SOURCE_DIR}/src/cbd/config/applier/logger.cc
  ${CMAKE_SOURCE_DIR}/src/cbd/broker_impl.cc
  ${CMAKE_SOURCE_DIR}/src/cbd/brokerrpc.cc
  )

#Flags needed to include all symbols in binary.
target_link_libraries(cbd
  "-Wl,--export-dynamic" "-Wl,--whole-archive" ccb_core ccb_sql grpc++_reflection "-Wl,--no-whole-archive"
  CONAN_PKG::json11 CONAN_PKG::spdlog CONAN_PKG::fmt CONAN_PKG::openssl
  CONAN_PKG::grpc CONAN_PKG::protobuf CONAN_PKG::protoc_installer ccb_rpc)
set_target_properties(cbd PROPERTIES COMPILE_FLAGS "-fPIC")
