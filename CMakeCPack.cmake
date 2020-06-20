find_program(RPMBUILD rpmbuild)

set(CPACK_RPM_PACKAGE_DEBUG On)
set(CPACK_PACKAGE_CONTACT "David Boucher <dboucher@centreon.com>")
set(CPACK_PACKAGE_VENDOR "Centreon")
set(CPACK_RPM_PACKAGE_LICENSE "ASL 2.0")
set(CPACK_RPM_PACKAGE_URL "https://github.com/centreon/centreon-collect.git")
set(CPACK_RPM_PACKAGE_GROUP "System Environment/Daemons")
set(CPACK_RPM_PACKAGE_SUMMARY "The Centreon collect softwares mainly composed of Centreon-engine and Centreon-broker")
set(CPACK_RPM_PACKAGE_DESCRIPTION "Centreon Broker is a Centreon Engine/Nagios module that reports events in one or multiple databases.")
set(CPACK_BINARY_RPM On)

set(CPACK_COMPONENTS_ALL broker cbd core graphite influxdb clib connector-perl connector-ssh engine-daemon engine-extcommands) # storage cbmod devel)
set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(CPACK_PACKAGE_VERSION ${COLLECT_VERSION})
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_PACKAGE_NAME "centreon")

if (RPMBUILD)
  set(CPACK_GENERATOR RPM)
  set(CPACK_RPM_COMPONENT_INSTALL ON)
else ()
  set(CPACK_GENERATOR DEB)
  set(CPACK_DEB_COMPONENT_INSTALL ON)
endif ()

#centreon-broker
set(CPACK_PACKAGE_BROKER_PACKAGE_NAME "centreon-broker-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}")
set(CPACK_RPM_BROKER_FILE_NAME "${CPACK_PACKAGE_BROKER_PACKAGE_NAME}.rpm")
set(CPACK_DEBIAN_BROKER_FILE_NAME "${CPACK_PACKAGE_BROKER_PACKAGE_NAME}.deb")
set(CPACK_RPM_BROKER_DEFAULT_USER "centreon-broker")
set(CPACK_RPM_BROKER_DEFAULT_GROUP "centreon-broker")

#centreon-broker-cbd
set(CPACK_PACKAGE_CBD_PACKAGE_NAME "centreon-broker-cbd-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}")
set(CPACK_DEBIAN_CBD_FILE_NAME "${CPACK_PACKAGE_CBD_PACKAGE_NAME}.deb")
set(CPACK_RPM_CBD_FILE_NAME "${CPACK_PACKAGE_CBD_PACKAGE_NAME}.rpm")

#centreon-broker-core
set(CPACK_PACKAGE_CORE_PACKAGE_NAME "centreon-broker-core-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}")
set(CPACK_DEBIAN_CORE_FILE_NAME "${CPACK_PACKAGE_CORE_PACKAGE_NAME}.deb")
set(CPACK_RPM_CORE_FILE_NAME "${CPACK_PACKAGE_CORE_PACKAGE_NAME}.rpm")
set(CPACK_RPM_CORE_PACKAGE_REQUIRES "centreon-broker = ${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}, centreon-broker-storage = ${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}, gnutls >= 2.0")

#centreon-broker-graphite
set(CPACK_PACKAGE_GRAPHITE_PACKAGE_NAME "centreon-broker-graphite-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}")
set(CPACK_DEBIAN_GRAPHITE_FILE_NAME "${CPACK_PACKAGE_GRAPHITE_PACKAGE_NAME}.deb")
set(CPACK_RPM_GRAPHITE_FILE_NAME "${CPACK_PACKAGE_GRAPHITE_PACKAGE_NAME}.rpm")
set(CPACK_RPM_GRAPHITE_SUMMARY "Write Centreon performance data to Graphite.")
set(CPACK_RPM_GRAPHITE_GROUP "Application/Communications")
set(CPACK_RPM_CORE_PACKAGE_REQUIRES "centreon-broker-core = ${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}")

#centreon-clib
set(CPACK_PACKAGE_clib_PACKAGE_NAME "centreon-clib-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}")
set(CPACK_DEBIAN_clib_FILE_NAME "${CPACK_PACKAGE_clib_PACKAGE_NAME}.deb")
set(CPACK_RPM_clib_FILE_NAME "${CPACK_PACKAGE_clib_PACKAGE_NAME}.rpm")
set(CPACK_RPM_clib_PACKAGE_NAME "centreon-clib")
set(CPACK_RPM_clib_PACKAGE_GROUP "Development/Libraries")
set(CPACK_RPM_clib_PACKAGE_LICENSE "ASL 2.0")
set(CPACK_RPM_clib_PACKAGE_SUMMARY "Centreon core library.")
set(CPACK_RPM_clib_PACKAGE_URL "https://github.com/centreon/centreon-clib")
set(CPACK_RPM_clib_PACKAGE_PACKAGER "David BOUCHER <dboucher@centreon.com>")
set(CPACK_RPM_clib_PACKAGE_VENDOR "Centreon")
set(CPACK_RPM_clib_PACKAGE_DESCRIPTION "Centreon Clib is a common library for all Centreon products written in C/C++.")

#centreon-connector-perl
set(CPACK_PACKAGE_connector-perl_PACKAGE_NAME "centreon-connector-perl")
set(CPACK_DEBIAN_connector-perl_FILE_NAME "${CPACK_PACKAGE_connector-perl_PACKAGE_NAME}.deb")
set(CPACK_RPM_connector-perl_FILE_NAME "${CPACK_PACKAGE_connector-perl_PACKAGE_NAME}.rpm")

#centreon-connector-ssh
set(CPACK_PACKAGE_connector-ssh_PACKAGE_NAME "centreon-connector-ssh")
set(CPACK_DEBIAN_connector-ssh_FILE_NAME "${CPACK_PACKAGE_connector-ssh_PACKAGE_NAME}.deb")
set(CPACK_RPM_connector-ssh_FILE_NAME "${CPACK_PACKAGE_connector-ssh_PACKAGE_NAME}.rpm")

#centreon-engine-daemon
set(CPACK_PACKAGE_engine-daemon_PACKAGE_NAME "centreon-engine-daemon")
set(CPACK_DEBIAN_engine-daemon_FILE_NAME "${CPACK_PACKAGE_engine-daemon_PACKAGE_NAME}.deb")
set(CPACK_RPM_engine-daemon_FILE_NAME "${CPACK_PACKAGE_engine-daemon_PACKAGE_NAME}.rpm")
#set(CPACK_RPM_engine-daemon_REQUIRES centreon-clib)

#centreon-engine-extcommands
set(CPACK_PACKAGE_engine-extcommands_PACKAGE_NAME "centreon-engine-extcommands")
set(CPACK_DEBIAN_engine-extcommands_FILE_NAME ${CPACK_PACKAGE_engine-extcommands_PACKAGE_NAME}.deb)
set(CPACK_RPM_engine-extcommands_FILE_NAME ${CPACK_PACKAGE_engine-extcommands_PACKAGE_NAME}.rpm)

include(CPack)
