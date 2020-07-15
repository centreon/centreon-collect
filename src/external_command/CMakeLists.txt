
add_library(
  externalcmd
  SHARED
  ${CMAKE_SOURCE_DIR}/src/external_command/commands.cc
  ${CMAKE_SOURCE_DIR}/src/external_command/internal.cc
  ${CMAKE_SOURCE_DIR}/src/external_command/main.cc
  ${CMAKE_SOURCE_DIR}/src/external_command/processing.cc
  ${CMAKE_SOURCE_DIR}/src/external_command/utils.cc
)
target_link_libraries(externalcmd CONAN_PKG::json11 CONAN_PKG::fmt)
target_include_directories(externalcmd PUBLIC ${CMAKE_BINARY_DIR})
target_include_directories(externalcmd PUBLIC ${CMAKE_SOURCE_DIR}/include/)
target_compile_features(externalcmd PUBLIC cxx_std_11)
set_target_properties(externalcmd PROPERTIES PREFIX "")
install(TARGETS externalcmd COMPONENT engine-extcommands DESTINATION ${CMAKE_INSTALL_LIBDIR}/centreon-engine)