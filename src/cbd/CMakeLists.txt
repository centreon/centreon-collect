link_directories(${CONAN_LIB_DIRS_GRPC})
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_BINARY_DIR})

configure_file(${CMAKE_SOURCE_DIR}/config/poller-module.json.in
  ${CMAKE_BINARY_DIR}/poller-module.json
  @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/config/central-broker.json.in
  ${CMAKE_BINARY_DIR}/central-broker.json
  @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/config/central-rrd.json.in
  ${CMAKE_BINARY_DIR}/central-rrd.json
  @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/script/systemd/cbd.service.in
  ${CMAKE_BINARY_DIR}/cbd.service
  @ONLY)

# Standalone binary.
add_executable(cbd
  ${CMAKE_SOURCE_DIR}/src/cbd/main.cc
  ${CMAKE_SOURCE_DIR}/src/cbd/config/applier/init.cc
  ${CMAKE_SOURCE_DIR}/src/cbd/config/applier/logger.cc
  ${CMAKE_SOURCE_DIR}/src/cbd/broker_impl.cc
  ${CMAKE_SOURCE_DIR}/src/cbd/brokerrpc.cc
  ${CMAKE_BINARY_DIR}/central-broker.json
  ${CMAKE_BINARY_DIR}/central-rrd.json
  ${CMAKE_BINARY_DIR}/poller-module.json
  )

#Flags needed to include all symbols in binary.
target_link_libraries(cbd
  "-Wl,--export-dynamic" "-Wl,--whole-archive" ccb_core ccb_sql grpc++_reflection "-Wl,--no-whole-archive"
  CONAN_PKG::json11 CONAN_PKG::spdlog CONAN_PKG::fmt CONAN_PKG::openssl
  CONAN_PKG::grpc CONAN_PKG::protobuf CONAN_PKG::protoc_installer ccb_rpc)
set_target_properties(cbd PROPERTIES COMPILE_FLAGS "-fPIC")

install(FILES ${CMAKE_BINARY_DIR}/cbd.service COMPONENT cbd DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system)
install(FILES ${CMAKE_BINARY_DIR}/central-broker.json COMPONENT cbd DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/centreon-broker)
install(FILES ${CMAKE_BINARY_DIR}/central-rrd.json COMPONENT cbd DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/centreon-broker)
install(FILES ${CMAKE_BINARY_DIR}/poller-module.json DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/centreon-broker)
install(TARGETS cbd COMPONENT cbd DESTINATION ${CMAKE_INSTALL_SBINDIR})
