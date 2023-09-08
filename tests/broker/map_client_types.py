#!/usr/bin/python3
#
# Copyright 2023 Centreon
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
# This script is a little tcp server working on port 5669. It can simulate
# a cbd instance. It is useful to test the validity of BBDO packets sent by
# centengine.

import socket
import sys
import time
from datetime import datetime
import sys


def welcome():
    s = "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x08\x00\x45\x00\x00\x55\xd8\xe3\x40\x00\x40\x06\x63\xbd\x7f\x00\x00\x01\x7f\x00\x00\x01\xe5\x9e\x16\x26\x30\xee\xd5\xd3\xf5\xd0\xf2\x9c\x80\x18\x02\x00\xfe\x49\x00\x00\x01\x01\x08\x0a\xa3\x9a\x23\xe4\xa3\x9a\x23\xe3\xbf\x93\x00\x11\x00\x02\x00\x07\x00\x00\x00\x01\x00\x00\x00\x00\x0a\x04\x08\x03\x18\x01\x18\x01\x22\x07" + "JavaMap"
    retval = bytearray()
    retval.extend(map(ord, s))
    return retval


def crc16_ccitt(data, data_len):
    crc_tbl = [
        0x0000, 0x1081, 0x2102, 0x3183, 0x4204, 0x5285, 0x6306, 0x7387,
        0x8408, 0x9489, 0xa50a, 0xb58b, 0xc60c, 0xd68d, 0xe70e, 0xf78f]

    crc = 0xffff
    for i in range(data_len):
        c = int.from_bytes(data[i:i+1], byteorder='big')
        crc = ((crc >> 4) & 0x0fff) ^ crc_tbl[((crc ^ c) & 15)]
        c >>= 4
        crc = ((crc >> 4) & 0x0fff) ^ crc_tbl[((crc ^ c) & 15)]
    return ~crc & 0xffff


def get_header(header):
    h = int.from_bytes(header[0:2], byteorder='big')
    s = int.from_bytes(header[2:4], byteorder='big')
    t = int.from_bytes(header[4:8], byteorder='big')
    src = int.from_bytes(header[8:12], byteorder='big')
    dst = int.from_bytes(header[12:16], byteorder='big')
    verif_chksum = crc16_ccitt(header[2:16], 14)
    with open('/tmp/map-output.log', 'a') as f:
        f.write(f"date: {datetime.today()}\n")
        f.write(f"chksum: {hex(h)}\n")
        f.write(f"verif_chksum: {hex(verif_chksum)}\n")
        f.write(f"size: {s}\n")
        f.write(f"type: {t}\n")
        f.flush()
    assert (0 <= dst <= 3)
    assert (0 <= src <= 3)
    return h, s, t, src, dst


host_addr = 'localhost'
host_port = 5671

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
connected = False
while not connected:
    try:
        print("connection")
        s.connect((host_addr, host_port))
    except ConnectionRefusedError:
        time.sleep(1)
        continue

    connected = True
    print("welcome")
    w = welcome()
    print("send")
    s.send(w)

    with open('/tmp/map-output.log', 'w') as f:
        f.write("")

    buffer = bytearray()
    current = False
    while True:
        tmp = s.recv(4096)
        if tmp == b'':
            print("connection end")
            break
        buffer.extend(tmp)

        while len(buffer) >= 16:
            if not current:
                header = buffer[:16]
                chksum, size, typ, src, dst = get_header(header)

            if size + 16 < len(buffer):
                buffer = buffer[size + 16:]
                current = False
            else:
                current = True
                break


# while True:
#    header = s.recv(16)
#    chksum, size, typ, src, dst = get_header(header)
#    content = s.recv(size)
#    while len(content) < size:
#        size -= len(content)
#        content = s.recv(size)
