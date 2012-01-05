##
## Copyright 2011 Merethis
##
## This file is part of Centreon Clib.
##
## Centreon Clib is free software: you can redistribute it
## and/or modify it under the terms of the GNU Affero General Public
## License as published by the Free Software Foundation, either version
## 3 of the License, or (at your option) any later version.
##
## Centreon Clib is distributed in the hope that it will be
## useful, but WITHOUT ANY WARRANTY; without even the implied warranty
## of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
## Affero General Public License for more details.
##
## You should have received a copy of the GNU Affero General Public
## License along with Centreon Clib. If not, see
## <http://www.gnu.org/licenses/>.
##

# Build package.
if (CPACK_BINARY_DEB OR CPACK_BINARY_RPM)
  string(TOLOWER "${PROJECT_NAME}" PACKAGE_NAME)
  string(REPLACE " " "-" PACKAGE_NAME "${PACKAGE_NAME}")

  set(CPACK_PACKAGE_NAME "${PACKAGE_NAME}")
  set(CPACK_PACKAGE_VENDOR "Merethis")
  set(CPACK_PACKAGE_VERSION_MAJOR "${VERSION_MAJOR}")
  set(CPACK_PACKAGE_VERSION_MINOR "${VERSION_MINOR}")
  set(CPACK_PACKAGE_VERSION_PATCH "${VERSION_PATCH}")
  set(CPACK_PACKAGE_FILE_NAME "${PACKAGE_NAME}-${VERSION}")
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PACKAGE_NAME}")
  set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/license.txt")
  set(CPACK_PACKAGE_CONTACT "Dorian Guillois <dguillois@merethis.com>")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Centreon Connector ICMP")

  # Define sepecific variables for build Debian Package.
  if (CPACK_BINARY_DEB)
    set(CPACK_DEBIAN_PACKAGE_SECTION "net")
  endif ()

  # Define sepecific variables for build RPM.
  if (CPACK_BINARY_RPM)
    set(CPACK_RPM_PACKAGE_LICENSE "AGPLv3")
  endif ()

  include(CPack)
endif ()