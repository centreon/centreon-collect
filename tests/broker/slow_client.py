#!/usr/bin/python3
#
# Copyright 2026 Centreon
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# For more information : contact@centreon.com
#
# TCP client that connects to broker's map output on port 5671, completes
# the BBDO handshake, then stops reading.  The tiny SO_RCVBUF causes the
# kernel receive buffer to fill up quickly, preventing broker from draining
# its internal write queue (_exposed_write_queue in tcp_connection.cc).
# Once the queue exceeds event_queue_max_size, broker logs:
#   "write queue full => remove oldest event"

import socket
import sys
import time


def welcome(bbdo_version: str) -> bytearray:
    """Return a BBDO version-hello packet for the given BBDO version."""
    if bbdo_version >= "3.0.0":
        raw = (
            "\x65\xfe\x00\x0e\x00\x02\x00\x01\x00\x00\x00\x01\x00\x00\x00\x01"
            "\x00\x03\x00\x00\x00\x00\x54\x4c\x53\x00\x00\x00\x00\x00"
        )
    else:
        raw = (
            "\x65\xfe\x00\x0e\x00\x02\x00\x01\x00\x00\x00\x01\x00\x00\x00\x01"
            "\x00\x02\x00\x00\x00\x00\x54\x4c\x53\x00\x00\x00\x00\x00"
        )
    buf = bytearray()
    buf.extend(map(ord, raw))
    return buf


host_addr = "localhost"
host_port = 5671
bbdo_version = sys.argv[1] if len(sys.argv) > 1 else "2.0.0"

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
# Tiny receive buffer: fills up quickly so the kernel TCP window drops to 0
# and broker cannot drain its write queue.
sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)

while True:
    try:
        sock.connect((host_addr, host_port))
        break
    except ConnectionRefusedError:
        time.sleep(1)

# Send our BBDO hello so broker completes version negotiation and starts
# streaming events toward this connection.
sock.sendall(welcome(bbdo_version))

# Read very slowly (100 bytes every 10 s) so broker's write queue fills up
# faster than it can drain, triggering "write queue full => remove oldest event".
while True:
    time.sleep(1)
    sock.recv(10)
