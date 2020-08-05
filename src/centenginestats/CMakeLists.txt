add_definitions(-DDEFAULT_STATUS_FILE="${VAR_DIR}/status.dat")
add_definitions(-DDEFAULT_CONFIG_FILE="${PREFIX_CONF}/centengine.cfg")

add_executable(centenginestats ${CMAKE_SOURCE_DIR}/src/centenginestats/centenginestats.cc)
target_link_libraries(centenginestats centreon_clib CONAN_PKG::json11)

install(TARGETS centenginestats COMPONENT engine-daemon DESTINATION ${CMAKE_INSTALL_SBINDIR})
