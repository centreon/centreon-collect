#!/usr/bin/env python3
"""A poller presenting a certificate of another CA to the central broker.

Used by verify.sh: sends a BBDO welcome offering TLS, then does the TLS
handshake with the certificate and key given, and waits. cbd checks the
certificate against the CA of the managed PKI and closes the connection.
Exits 0 when the connection is refused or closed, 1 when it stays open.

    rogue-poller.py <host> <port> <certificate> <key>
"""

import socket
import ssl
import struct
import sys

CRC_TABLE = [0x0000, 0x1081, 0x2102, 0x3183, 0x4204, 0x5285, 0x6306, 0x7387,
             0x8408, 0x9489, 0xA50A, 0xB58B, 0xC60C, 0xD68D, 0xE70E, 0xF78F]
WELCOME = (2 << 16) | 7


def crc16(data: bytes) -> int:
    """misc::crc16_ccitt of Broker (CRC-16/X-25)."""
    crc = 0xFFFF
    for byte in data:
        for nibble in (byte & 15, byte >> 4):
            crc = ((crc >> 4) & 0x0FFF) ^ CRC_TABLE[(crc ^ nibble) & 15]
    return ~crc & 0xFFFF


def varint(value: int) -> bytes:
    out = b""
    while True:
        byte = value & 0x7F
        value >>= 7
        if value:
            out += bytes([byte | 0x80])
        else:
            return out + bytes([byte])


def field(number: int, value: int | bytes) -> bytes:
    if isinstance(value, int):
        return varint(number << 3) + varint(value)
    return varint((number << 3) | 2) + varint(len(value)) + value


def frame(event_type: int, payload: bytes) -> bytes:
    header = struct.pack(">HII", len(payload), event_type, 0) + b"\0" * 4
    return struct.pack(">H", crc16(header)) + header + payload


def main() -> int:
    host, port, certificate, key = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4]
    version = field(1, 3) + field(2, 0) + field(3, 1)
    welcome = (field(1, version) + field(2, b"TLS") + field(3, 4040)
               + field(4, b"rogue-poller") + field(5, b"rogue-poller-module") + field(6, 2))
    raw = socket.create_connection((host, port), timeout=10)
    raw.sendall(frame(WELCOME, welcome))
    header = raw.recv(16, socket.MSG_WAITALL)
    size = struct.unpack(">H", header[2:4])[0]
    raw.recv(size, socket.MSG_WAITALL)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE
    context.load_cert_chain(certificate, key)
    try:
        tls = context.wrap_socket(raw, server_hostname=host)
        tls.settimeout(10)
        # TLS 1.3: cbd checks the client certificate once the handshake is
        # done, then closes the connection.
        if tls.recv(1) == b"":
            print("closed by the central broker")
            return 0
    except (ssl.SSLError, ConnectionError, OSError) as error:
        print(f"refused: {error}")
        return 0
    print("the central broker kept the connection")
    return 1


if __name__ == "__main__":
    sys.exit(main())
