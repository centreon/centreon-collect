
add_library(
  external_command
  SHARED
  ${CMAKE_SOURCE_DIR}/src/external_command/commands.cc
  ${CMAKE_SOURCE_DIR}/src/external_command/internal.cc
  ${CMAKE_SOURCE_DIR}/src/external_command/main.cc
  ${CMAKE_SOURCE_DIR}/src/external_command/processing.cc
  ${CMAKE_SOURCE_DIR}/src/external_command/utils.cc
)
target_link_libraries(external_command CONAN_PKG::json11)
target_include_directories(external_command PUBLIC ${CMAKE_BINARY_DIR})
target_include_directories(external_command PUBLIC ${CMAKE_SOURCE_DIR}/include/)
target_compile_features(external_command PUBLIC cxx_std_11)
set_target_properties(external_command PROPERTIES PREFIX "")
install(TARGETS external_command DESTINATION ${CMAKE_INSTALL_LIBDIR})