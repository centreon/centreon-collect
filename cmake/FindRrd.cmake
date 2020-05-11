# Find librrd.
include(FindPkgConfig)
if (PKG_CONFIG_FOUND)
  pkg_check_modules(LIBRRD REQUIRED "librrd")
  if (LIBRRD_INCLUDE_DIRS)
    include_directories("${LIBRRD_INCLUDE_DIRS}")
    set(LIBRRD_INCLUDE_DIRS "${LIBRRD_INCLUDE_DIRS}" PARENT_SCOPE)
  endif ()
else ()
  # Find rrd.h
  find_path(LIBRRD_INCLUDE_DIR "rrd.h")
  if (NOT LIBRRD_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find librrd's headers.")
  endif ()
  include_directories("${LIBRRD_INCLUDE_DIR}")
  set(LIBRRD_INCLUDE_DIRS "${LIBRRD_INCLUDE_DIR}" PARENT_SCOPE)

  # Find librrd.
  find_library(LIBRRD_LDFLAGS "rrd")
  if (NOT LIBRRD_LDFLAGS)
    message(FATAL_ERROR "Could not find librrd's library.")
  endif ()
  set(LIBRRD_LDFLAGS "${LIBRRD_LDFLAGS}" PARENT_SCOPE)
endif ()
