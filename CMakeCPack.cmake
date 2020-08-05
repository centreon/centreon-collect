find_program(RPMBUILD rpmbuild)
find_program(DEBBUILD dpkg-deb)

set(CPACK_RPM_PACKAGE_DEBUG On)
set(CPACK_PACKAGE_CONTACT "David Boucher <dboucher@centreon.com>")
set(CPACK_PACKAGE_VENDOR "Centreon")
set(CPACK_RPM_PACKAGE_LICENSE "ASL 2.0")
set(CPACK_RPM_PACKAGE_URL "https://github.com/centreon/centreon-collect.git")
set(CPACK_RPM_PACKAGE_GROUP "System Environment/Daemons")
set(CPACK_RPM_PACKAGE_SUMMARY "The Centreon collect softwares mainly composed of Centreon-engine and Centreon-broker")
set(CPACK_RPM_PACKAGE_DESCRIPTION "Centreon Broker is a Centreon Engine/Nagios module that reports events in one or multiple databases.")
set(CPACK_BINARY_RPM On)

set(CPACK_COMPONENTS_ALL broker cbd core storage graphite influxdb clib connector-perl connector-ssh engine-daemon engine-extcommands) # cbmod)
set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(CPACK_PACKAGE_VERSION ${COLLECT_VERSION})
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_PACKAGE_NAME "centreon")
set(PACK_VERSION "${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}")

if (RPMBUILD)
  set(CPACK_GENERATOR ${CPACK_GENERATOR} RPM)
  set(CPACK_RPM_COMPONENT_INSTALL ON)
endif ()
if (DEBBUILD)
  set(CPACK_GENERATOR ${CPACK_GENERATOR} DEB)
  set(CPACK_DEB_COMPONENT_INSTALL ON)
  execute_process(COMMAND dpkg --print-architecture
                  OUTPUT_VARIABLE ARCH
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
endif ()

#centreon-broker
set(CPACK_PACKAGE_BROKER_PACKAGE_NAME "centreon-broker-${PACK_VERSION}")
set(CPACK_RPM_BROKER_FILE_NAME "${CPACK_PACKAGE_BROKER_PACKAGE_NAME}.rpm")
set(CPACK_DEBIAN_BROKER_FILE_NAME "centreon-broker_${PACK_VERSION}_${ARCH}.deb")
set(CPACK_RPM_BROKER_DEFAULT_USER "centreon-broker")
set(CPACK_RPM_BROKER_DEFAULT_GROUP "centreon-broker")

#centreon-broker-cbd
set(CPACK_PACKAGE_CBD_PACKAGE_NAME "centreon-broker-cbd-${PACK_VERSION}")
set(CPACK_DEBIAN_CBD_FILE_NAME "centreon-broker-cbd_${PACK_VERSION}_${ARCH}.deb")
set(CPACK_DEBIAN_CBD_PACKAGE_DEPENDS "centreon-broker (= ${PACK_VERSION}), libmariadb3")
set(CPACK_RPM_CBD_FILE_NAME "${CPACK_PACKAGE_CBD_PACKAGE_NAME}.rpm")
set(CPACK_RPM_CBD_PACKAGE_REQUIRES "centreon-broker = ${PACK_VERSION}")

#centreon-broker-core
set(CPACK_PACKAGE_CORE_PACKAGE_NAME "centreon-broker-core-${PACK_VERSION}")
set(CPACK_DEBIAN_CORE_FILE_NAME "centreon-broker-core_${PACK_VERSION}_${ARCH}.deb")
set(CPACK_DEBIAN_CORE_PACKAGE_DEPENDS "centreon-broker (= ${PACK_VERSION}), libgnutls-openssl27 (>= 2.0)")
set(CPACK_RPM_CORE_FILE_NAME "${CPACK_PACKAGE_CORE_PACKAGE_NAME}.rpm")
set(CPACK_RPM_CORE_PACKAGE_REQUIRES "centreon-broker = ${PACK_VERSION}, centreon-broker-storage = ${PACK_VERSION}, gnutls >= 2.0, lua >= 5.1")

#centreon-broker-storage
set(CPACK_PACKAGE_STORAGE_PACKAGE_NAME "centreon-broker-storage-${PACK_VERSION}")
set(CPACK_DEBIAN_STORAGE_FILE_NAME "centreon-broker-storage_${PACK_VERSION}_${ARCH}.deb")
set(CPACK_DEBIAN_STORAGE_PACKAGE_DEPENDS "centreon-broker-core (= ${PACK_VERSION})")
set(CPACK_RPM_STORAGE_FILE_NAME "${CPACK_PACKAGE_STORAGE_PACKAGE_NAME}.rpm")
set(CPACK_RPM_STORAGE_SUMMARY "Write Centreon performance data to Mariadb database.")
set(CPACK_RPM_STORAGE_GROUP "Application/Communications")
set(CPACK_RPM_STORAGE_PACKAGE_REQUIRES "centreon-broker-core = ${PACK_VERSION}")

#centreon-broker-graphite
set(CPACK_PACKAGE_GRAPHITE_PACKAGE_NAME "centreon-broker-graphite-${PACK_VERSION}")
set(CPACK_DEBIAN_GRAPHITE_FILE_NAME "centreon-broker-graphite_${PACK_VERSION}_${ARCH}.deb")
set(CPACK_DEBIAN_GRAPHITE_PACKAGE_DEPENDS "centreon-broker-core (= ${PACK_VERSION})")
set(CPACK_RPM_GRAPHITE_FILE_NAME "${CPACK_PACKAGE_GRAPHITE_PACKAGE_NAME}.rpm")
set(CPACK_RPM_GRAPHITE_SUMMARY "Write Centreon performance data to Graphite.")
set(CPACK_RPM_GRAPHITE_GROUP "Application/Communications")
set(CPACK_RPM_GRAPHITE_PACKAGE_REQUIRES "centreon-broker-core = ${PACK_VERSION}")

#centreon-broker-influxdb
set(CPACK_PACKAGE_INFLUXDB_PACKAGE_NAME "centreon-broker-influxdb-${PACK_VERSION}")
set(CPACK_DEBIAN_INFLUXDB_FILE_NAME "centreon-broker-influxdb_${PACK_VERSION}_${ARCH}.deb")
set(CPACK_DEBIAN_INFLUXDB_PACKAGE_DEPENDS "centreon-broker-core (= ${PACK_VERSION})")
set(CPACK_RPM_INFLUXDB_FILE_NAME "${CPACK_PACKAGE_INFLUXDB_PACKAGE_NAME}.rpm")
set(CPACK_RPM_INFLUXDB_SUMMARY "Write Centreon performance data to InfluxDB.")
set(CPACK_RPM_INFLUXDB_GROUP "Application/Communications")
set(CPACK_RPM_INFLUXDB_PACKAGE_REQUIRES "centreon-broker-core = ${PACK_VERSION}")

#centreon-clib
set(CPACK_PACKAGE_CLIB_PACKAGE_NAME "centreon-clib-${PACK_VERSION}")
set(CPACK_DEBIAN_CLIB_FILE_NAME "centreon-clib_${PACK_VERSION}_${ARCH}.deb")
set(CPACK_DEBIAN_CLIB_PACKAGE_DEPENDS "libstdc++6 (>= 8.3.0-6)")
set(CPACK_RPM_CLIB_FILE_NAME "${CPACK_PACKAGE_CLIB_PACKAGE_NAME}.rpm")
set(CPACK_RPM_CLIB_PACKAGE_NAME "centreon-clib")
set(CPACK_RPM_CLIB_PACKAGE_GROUP "Development/Libraries")
set(CPACK_RPM_CLIB_PACKAGE_LICENSE "ASL 2.0")
set(CPACK_RPM_CLIB_PACKAGE_SUMMARY "Centreon core library.")
set(CPACK_RPM_CLIB_PACKAGE_URL "https://github.com/centreon/centreon-collect")
set(CPACK_RPM_CLIB_PACKAGE_PACKAGER "David BOUCHER <dboucher@centreon.com>")
set(CPACK_RPM_CLIB_PACKAGE_VENDOR "Centreon")
set(CPACK_RPM_CLIB_PACKAGE_DESCRIPTION "Centreon Clib is a common library for all Centreon products written in C/C++.")
set(CPACK_RPM_CLIB_PACKAGE_REQUIRES "libstdc++ >= 4.8.5-39")

#centreon-connector-perl
set(CPACK_PACKAGE_CONNECTOR-PERL_PACKAGE_NAME "centreon-connector-perl-${PACK_VERSION}")
set(CPACK_DEBIAN_CONNECTOR-PERL_FILE_NAME "centreon-connector-perl_${PACK_VERSION}_${ARCH}.deb")
set(CPACK_DEBIAN_CONNECTOR-PERL_PACKAGE_DEPENDS "centreon-clib (= ${PACK_VERSION}), perl")
set(CPACK_RPM_CONNECTOR-PERL_FILE_NAME "${CPACK_PACKAGE_CONNECTOR-PERL_PACKAGE_NAME}.rpm")
set(CPACK_RPM_CONNECTOR-PERL_PACKAGE_REQUIRES "centreon-clib = ${PACK_VERSION}, perl")

#centreon-connector-ssh
set(CPACK_PACKAGE_CONNECTOR-SSH_PACKAGE_NAME "centreon-connector-ssh-${PACK_VERSION}")
set(CPACK_DEBIAN_CONNECTOR-SSH_FILE_NAME "centreon-connector-ssh_${PACK_VERSION}_${ARCH}.deb")
set(CPACK_DEBIAN_CONNECTOR-SSH_PACKAGE_DEPENDS "centreon-clib (= ${PACK_VERSION}), libssl")
set(CPACK_RPM_CONNECTOR-SSH_FILE_NAME "${CPACK_PACKAGE_CONNECTOR-SSH_PACKAGE_NAME}.rpm")
set(CPACK_RPM_CONNECTOR-SSH_PACKAGE_REQUIRES "centreon-clib = ${PACK_VERSION}, libgcrypt, libssh2 >= 1.4")

#centreon-engine-daemon
set(CPACK_PACKAGE_ENGINE-DAEMON_PACKAGE_NAME "centreon-engine-daemon-${PACK_VERSION}")
set(CPACK_DEBIAN_ENGINE-DAEMON_FILE_NAME "centreon-engine-daemon_${PACK_VERSION}_${ARCH}.deb")
set(CPACK_DEBIAN_ENGINE-DAEMON_PACKAGE_DEPENDS "centreon-clib (= ${PACK_VERSION})")
set(CPACK_RPM_ENGINE-DAEMON_FILE_NAME "${CPACK_PACKAGE_ENGINE-DAEMON_PACKAGE_NAME}.rpm")
set(CPACK_RPM_ENGINE-DAEMON_REQUIRES "centreon-clib = ${PACK_VERSION}")

#centreon-engine-extcommands
set(CPACK_PACKAGE_ENGINE-EXTCOMMANDS_PACKAGE_NAME "centreon-engine-extcommands-${PACK_VERSION}")
set(CPACK_DEBIAN_ENGINE-EXTCOMMANDS_FILE_NAME "centreon-engine-extcommands_${PACK_VERSION}_${ARCH}.deb")
set(CPACK_DEBIAN_ENGINE-EXTCOMMANDS_PACKAGE_DEPENDS "centreon-engine-daemon (= ${PACK_VERSION})")
set(CPACK_RPM_ENGINE-EXTCOMMANDS_FILE_NAME ${CPACK_PACKAGE_ENGINE-EXTCOMMANDS_PACKAGE_NAME}.rpm)
set(CPACK_RPM_ENGINE-EXTCOMMANDS_DEPENDS "centreon-engine-daemon = ${PACK_VERSION}")

include(CPack)
