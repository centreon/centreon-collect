
add_library(
  cce_external_command
  SHARED
  ${CMAKE_SOURCE_DIR}/src/cce_external_command/commands.cc
  ${CMAKE_SOURCE_DIR}/src/cce_external_command/internal.cc
  ${CMAKE_SOURCE_DIR}/src/cce_external_command/main.cc
  ${CMAKE_SOURCE_DIR}/src/cce_external_command/processing.cc
  ${CMAKE_SOURCE_DIR}/src/cce_external_command/utils.cc
)
target_include_directories(cce_external_command PUBLIC ${CMAKE_BINARY_DIR})
target_include_directories(cce_external_command PUBLIC ${CMAKE_SOURCE_DIR}/include/)
target_compile_features(cce_external_command PUBLIC cxx_std_11)