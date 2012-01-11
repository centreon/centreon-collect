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

# Set directories.
set(TEST_DIR "${PROJECT_SOURCE_DIR}/test")

# check_critical
set(TEST_NAME "check_critical")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# check_copy
set(TEST_NAME "check_copy")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# check_current_host_check
set(TEST_NAME "check_current_host_check")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# check_ctor
set(TEST_NAME "check_ctor")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# check_hosts
set(TEST_NAME "check_hosts")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# check_max_completion_time
set(TEST_NAME "check_max_completion_time")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# check_min_host_alive
set(TEST_NAME "check_min_host_alive")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# check_nb_packet
set(TEST_NAME "check_nb_packet")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# check_packet_data_size
set(TEST_NAME "check_packet_data_size")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# check_packet_interval
set(TEST_NAME "check_packet_interval")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# check_target_interval
set(TEST_NAME "check_target_interval")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# check_warning
set(TEST_NAME "check_warning")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# check_dispatch_ctor
set(TEST_NAME "check_dispatch_ctor")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# check_dispatch_max_concurrent_checks
set(TEST_NAME "check_dispatch_max_concurrent_checks")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# check_dispatch_submit
set(TEST_NAME "check_dispatch_submit")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# host_copy
set(TEST_NAME "host_copy")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# host_ctor
set(TEST_NAME "host_ctor")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# host_factory
set(TEST_NAME "host_factory")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# host_get_roundtrip_avg
set(TEST_NAME "host_get_roundtrip_avg")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# host_get_roundtrip_max
set(TEST_NAME "host_get_roundtrip_max")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# host_get_roundtrip_min
set(TEST_NAME "host_get_roundtrip_min")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# host_has_lost_packet
set(TEST_NAME "host_has_lost_packet")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# host_has_recv_packet
set(TEST_NAME "host_has_recv_packet")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# host_has_send_packet
set(TEST_NAME "host_has_send_packet")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# icmp_info
set(TEST_NAME "icmp_info")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# packet_address
set(TEST_NAME "packet_address")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# packet_code
set(TEST_NAME "packet_code")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# packet_copy
set(TEST_NAME "packet_copy")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# packet_ctor
set(TEST_NAME "packet_ctor")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# packet_host_id
set(TEST_NAME "packet_host_id")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# packet_id
set(TEST_NAME "packet_id")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# packet_recv_time
set(TEST_NAME "packet_recv_time")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# packet_sequence
set(TEST_NAME "packet_sequence")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# packet_type
set(TEST_NAME "packet_type")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# request_execute
set(TEST_NAME "request_execute")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# request_quit
set(TEST_NAME "request_quit")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# request_version
set(TEST_NAME "request_version")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# result_ctor
set(TEST_NAME "result_ctor")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")

# result_add_data
set(TEST_NAME "result_add_data")
add_executable("${TEST_NAME}" "${TEST_DIR}/${TEST_NAME}.cc")
target_link_libraries("${TEST_NAME}" "${CONNECTORLIB}")
add_test("${TEST_NAME}" "${TEST_NAME}")
