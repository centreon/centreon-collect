##
## Copyright 2011-2013 Centreon
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## For more information : contact@centreon.com
##

# Build package.
if (CPACK_BINARY_DEB OR CPACK_BINARY_RPM)
  string(TOLOWER "${PROJECT_NAME}" PACKAGE_NAME)
  string(REPLACE " " "-" PACKAGE_NAME "${PACKAGE_NAME}")

  set(CPACK_PACKAGE_NAME "${PACKAGE_NAME}")
  set(CPACK_PACKAGE_VENDOR "Centreon")
  set(CPACK_PACKAGE_VERSION_MAJOR "${VERSION_MAJOR}")
  set(CPACK_PACKAGE_VERSION_MINOR "${VERSION_MINOR}")
  set(CPACK_PACKAGE_VERSION_PATCH "${VERSION_PATCH}")
  set(CPACK_PACKAGE_FILE_NAME "${PACKAGE_NAME}-${VERSION}")
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PACKAGE_NAME}")
  set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
  set(CPACK_PACKAGE_CONTACT "Matthieu Kermagoret <mkermagoret@centreon.com>")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Centreon Benchmark Connector")

  # Define sepecific variables for build Debian Package.
  if (CPACK_BINARY_DEB)
    # set(CPACK_DEBIAN_PACKAGE_SECTION "n")
  endif ()

  # Define sepecific variables for build RPM.
  if (CPACK_BINARY_RPM)
    set(CPACK_RPM_PACKAGE_LICENSE "ASL 2.0")
  endif ()

  include(CPack)
endif ()
