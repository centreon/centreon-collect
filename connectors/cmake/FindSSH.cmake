# We will use pkg-config if available.
include(FindPkgConfig)
if (PKG_CONFIG_FOUND)
  pkg_check_modules(LIBSSH2 QUIET "libssh2")     # Will be used below.
  pkg_check_modules(LIBGCRYPT QUIET "libgcrypt") # Will be used below.
endif ()

# Find libssh2's headers.
if (WITH_LIBSSH2_INCLUDE_DIR)
  find_file(
    LIBSSH2_HEADER_FOUND
    "libssh2.h"
    PATHS "${WITH_LIBSSH2_INCLUDE_DIR}"
    NO_DEFAULT_PATH)
  if (NOT LIBSSH2_HEADER_FOUND)
    message(FATAL_ERROR "Could not find libssh2's headers in ${WITH_LIBSSH2_INCLUDE_DIR}.")
  endif ()
  include_directories("${WITH_LIBSSH2_INCLUDE_DIR}")
elseif (LIBSSH2_FOUND) # Was libssh2 detected with pkg-config ?
  if (CMAKE_CXX_FLAGS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LIBSSH2_CFLAGS}")
  else ()
    set(CMAKE_CXX_FLAGS "${LIBSSH2_CFLAGS}")
  endif ()
else ()
  find_path(LIBSSH2_INCLUDE_DIR "libssh2.h")
  if (NOT LIBSSH2_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find libssh2's headers (try WITH_LIBSSH2_INCLUDE_DIR).")
  endif ()
  include_directories("${LIBSSH2_INCLUDE_DIR}")
endif ()

# Find libssh2's library.
if (WITH_LIBSSH2_LIBRARIES)
  set(LIBSSH2_LIBRARIES "${WITH_LIBSSH2_LIBRARIES}")
elseif (WITH_LIBSSH2_LIBRARY_DIR)
  find_library(
    LIBSSH2_LIBRARIES
    "ssh2"
    PATHS "${WITH_LIBSSH2_LIBRARY_DIR}"
    NO_DEFAULT_PATH)
  if (NOT LIBSSH2_LIBRARIES)
    message(FATAL_ERROR "Could not find libssh2's library in ${WITH_LIBSSH2_LIBRARY_DIR}.")
  endif ()
elseif (LIBSSH2_FOUND) # Was libssh2 detected with pkg-config ?
  set(LIBSSH2_LIBRARIES "${LIBSSH2_LDFLAGS}")
else ()
  find_library(LIBSSH2_LIBRARIES "ssh2")
  if (NOT LIBSSH2_LIBRARIES)
    message(FATAL_ERROR "Could not find libssh2's library (try WITH_LIBSSH2_LIBRARY_DIR or WITH_LIBSSH2_LIBRARIES).")
  endif ()
endif ()

# Check if libssh2 is using libgcrypt or OpenSSL.
if (WITH_LIBSSH2_WITH_LIBGCRYPT)
  set(LIBSSH2_WITH_LIBGCRYPT "${WITH_LIBSSH2_WITH_LIBGCRYPT}")
else ()
  set(LIBSSH2_WITH_LIBGCRYPT 1)
endif ()
if (LIBSSH2_WITH_LIBGCRYPT)
  # Find libgcrypt's required header.
  if (WITH_LIBGCRYPT_INCLUDE_DIR)
    find_file(
      LIBGCRYPT_HEADER_FOUND
      "gcrypt.h"
      PATHS "${WITH_LIBGCRYPT_INCLUDE_DIR}"
      NO_DEFAULT_PATH)
    if (NOT LIBGCRYPT_HEADER_FOUND)
      message(FATAL_ERROR "Could not find libgcrypt's headers in ${WITH_LIBGCRYPT_INCLUDE_DIR}.")
    endif ()
    include_directories("${WITH_LIBGCRYPT_INCLUDE_DIR}")
  else ()
    find_file(LIBGCRYPT_HEADER_FOUND "gcrypt.h")
    if (NOT LIBGCRYPT_HEADER_FOUND)
      message(FATAL_ERROR "Could not find libgcrypt's headers (try WITH_LIBGCRYPT_INCLUDE_DIR).")
    endif ()
  endif ()

  # Find libgcrypt's library.
  if (WITH_LIBGCRYPT_LIBRARIES)
    set(LIBGCRYPT_LIBRARIES "${WITH_LIBGCRYPT_LIBRARIES}")
  elseif (WITH_LIBGCRYPT_LIBRARY_DIR)
    find_library(
      LIBGCRYPT_LIBRARIES
      "gcrypt"
      PATHS "${WITH_LIBGCRYPT_LIBRARY_DIR}"
      NO_DEFAULT_PATH)
    if (NOT LIBGCRYPT_LIBRARIES)
      message(FATAL_ERROR "Could not find libgcrypt's library in ${WITH_LIBGCRYPT_LIBRARY_DIR}.")
    endif ()
  elseif (LIBGCRYPT_FOUND) # Was libgcrypt detected with pkg-config ?
    set(LIBGCRYPT_LIBRARIES "${LIBGCRYPT_LDFLAGS}")
  else ()
    find_library(LIBGCRYPT_LIBRARIES "gcrypt")
    if (NOT LIBGCRYPT_LIBRARIES)
      message(FATAL_ERROR "Could not find libgcrypt's library (try WITH_LIBGCRYPT_LIBRARY_DIR).")
    endif ()
  endif ()

  # Add macro.
  add_definitions(-DLIBSSH2_WITH_LIBGCRYPT)
endif ()
