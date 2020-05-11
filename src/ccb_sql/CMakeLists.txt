# Version.
include_directories(${CMAKE_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR}/include)

include(${CMAKE_SOURCE_DIR}/cmake/FindMariaDB.cmake)

# Broker vars
# Core library.
add_library(ccb_sql
  STATIC
  # Sources.
  ${CMAKE_SOURCE_DIR}/src/ccb_sql/mysql.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_sql/database/mysql_bind.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_sql/database/mysql_column.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_sql/database/mysql_error.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_sql/mysql_manager.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_sql/database/mysql_result.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_sql/database/mysql_stmt.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_sql/mysql_connection.cc
  ${CMAKE_SOURCE_DIR}/src/ccb_sql/query_preparator.cc
)

# Static libraries.
set_target_properties(ccb_sql PROPERTIES COMPILE_FLAGS "${MYSQL_CFLAGS} -fPIC")
target_link_libraries(ccb_sql CONAN_PKG::json11 CONAN_PKG::spdlog ${MYSQL_LIBS})