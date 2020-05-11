# Find GNU TLS.
find_file(
  GNUTLS_FOUND
  "gnutls.h"
  PATHS "/usr/gnutls2/include/gnutls"
  NO_DEFAULT_PATH)
if (GNUTLS_FOUND)
  set(GNUTLS_INCLUDE_DIR "/usr/gnutls2/include")
  set(GNUTLS_LIBRARIES "/usr/gnutls2/lib/libgnutls.so")
else ()
  include(FindGnuTLS)
  if (NOT GNUTLS_FOUND)
    message(FATAL_ERROR "Could not find GNU TLS.")
  endif ()
endif ()
