#!/usr/bin/env python3
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

"""
BBDO v3 event injector for RRD retention benchmarking.

Connects to a running Centreon Broker instance (central broker input port) and
injects pb_metric / pb_status events with controlled timestamps to exercise the
retention buffer without going through the Engine.

Wire format (BBDO v3 packet):
  Bytes  0-1   uint16 BE  CRC (nibble-based, see _crc16())
  Bytes  2-3   uint16 BE  payload size  (0xFFFF = continuation packet)
  Bytes  4-7   uint32 BE  event type    ((category << 16) | element)
  Bytes  8-11  uint32 BE  source_id
  Bytes 12-15  uint32 BE  dest_id
  Bytes 16-…              serialised protobuf payload

The CRC covers bytes 2-15 only (the 14 header bytes that follow the CRC
field).  The algorithm is the nibble-based variant used by Centreon Broker
(see broker/core/src/misc/misc.cc :: crc16_ccitt).

Usage – Robot Framework keyword:
    ${result}=    Ctn Inject Metric Events
    ...    host=127.0.0.1    port=5669
    ...    metric_ids=${metric_ids}    rrd_len=${15552000}    step=${60}
    ...    n_old_points=${3600}

Usage – standalone CLI:
    python3 bbdo_injector.py \\
        --host 127.0.0.1 --port 5669 \\
        --metric-ids 42,43,44 \\
        --rrd-len 15552000 --step 60 \\
        --n-old-points 3600
"""

import argparse
import select
import socket
import struct
import threading
import time
from typing import List

import bbdo_pb2
import common_pb2
import storage_pb2

# ---------------------------------------------------------------------------
# BBDO constants
# ---------------------------------------------------------------------------

BBDO_HEADER_SIZE = 16

# Event type = (io_category << 16) | element
_IO_BBDO = 2
_IO_STORAGE = 3

TYPE_PB_WELCOME = (_IO_BBDO << 16) | 7     # bbdo::de_welcome
TYPE_PB_ACK = (_IO_BBDO << 16) | 8         # bbdo::de_pb_ack
TYPE_PB_STOP = (_IO_BBDO << 16) | 9        # bbdo::de_pb_stop
TYPE_PB_METRIC = (_IO_STORAGE << 16) | 9   # storage::de_pb_metric
TYPE_PB_STATUS = (_IO_STORAGE << 16) | 10  # storage::de_pb_status

# ---------------------------------------------------------------------------
# CRC
# ---------------------------------------------------------------------------

# Nibble-based CRC table from broker/core/src/misc/misc.cc
_CRC_TBL = [
    0x0000, 0x1081, 0x2102, 0x3183,
    0x4204, 0x5285, 0x6306, 0x7387,
    0x8408, 0x9489, 0xa50a, 0xb58b,
    0xc60c, 0xd68d, 0xe70e, 0xf78f,
]


def _crc16(data: bytes) -> int:
    """
    CRC-16 as implemented by Centreon Broker (misc::crc16_ccitt).

    This is *not* standard CRC-16/CCITT-FALSE; it uses a 16-entry nibble
    table and complements the result.
    """
    crc = 0xFFFF
    for byte in data:
        crc = ((crc >> 4) & 0x0FFF) ^ _CRC_TBL[(crc ^ byte) & 0xF]
        byte >>= 4
        crc = ((crc >> 4) & 0x0FFF) ^ _CRC_TBL[(crc ^ byte) & 0xF]
    return (~crc) & 0xFFFF


# ---------------------------------------------------------------------------
# Low-level packet helpers
# ---------------------------------------------------------------------------

def _recv_exactly(sock: socket.socket, n: int) -> bytes:
    """Read exactly *n* bytes from *sock*, blocking until available."""
    buf = b''
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("Socket closed before all bytes received")
        buf += chunk
    return buf


def _make_packet(event_type: int, payload: bytes,
                 source_id: int = 0, dest_id: int = 0) -> bytes:
    """
    Encode one BBDO packet.

    For payloads larger than 65 534 bytes, multi-packet splitting would be
    needed.  All events injected here are tiny (<< 65 535 B), so we raise
    on overflow.
    """
    if len(payload) > 0xFFFF:
        raise ValueError(
            f"Payload too large ({len(payload)} B); "
            "multi-packet splitting not implemented in the injector."
        )
    # Bytes 2-15 of the header (14 bytes): size | type | source | dest
    header_body = struct.pack('>HIII', len(payload), event_type,
                              source_id, dest_id)
    crc = _crc16(header_body)
    return struct.pack('>H', crc) + header_body + payload


def _recv_packet(sock: socket.socket):
    """
    Receive one BBDO packet.

    Returns (event_type: int, payload: bytes).
    Raises ConnectionError on closed socket.
    """
    header = _recv_exactly(sock, BBDO_HEADER_SIZE)
    size = struct.unpack('>H', header[2:4])[0]
    event_type = struct.unpack('>I', header[4:8])[0]
    payload = _recv_exactly(sock, size) if size else b''
    # Handle continuation packets (size == 0xFFFF): keep appending.
    while size == 0xFFFF:
        header = _recv_exactly(sock, BBDO_HEADER_SIZE)
        size = struct.unpack('>H', header[2:4])[0]
        payload += _recv_exactly(sock, size) if size else b''
    return event_type, payload


# ---------------------------------------------------------------------------
# BBDO client
# ---------------------------------------------------------------------------

class BbdoClient:
    """
    Minimal BBDO v3 client that can inject pb_metric / pb_status events.

    The client connects directly to the RRD broker input port (typically 5670)
    and presents itself as a BROKER peer with no extensions so that the broker
    does not attempt TLS, compression, or extended negotiation.

    Connecting to the RRD broker (not the central broker on 5669) avoids the
    BBDO3 Engine configuration handshake: in BBDO3 mode the central broker
    waits for pb_diff_state_ack from every ENGINE peer before forwarding
    events, which would block the injector's events indefinitely.

    A background thread drains any incoming packets (acks, …) to prevent the
    TCP receive buffer from filling up and blocking the sender.
    """

    def __init__(self, host: str, port: int,
                 source_id: int = 99, poller_id: int = 99):
        self._host = host
        self._port = port
        self._source_id = source_id
        self._poller_id = poller_id
        self._sock: socket.socket | None = None
        self._drain_thread: threading.Thread | None = None
        self._stop_drain = threading.Event()

    # ------------------------------------------------------------------
    # Connection lifecycle
    # ------------------------------------------------------------------

    def connect(self, timeout: float = 10.0) -> None:
        """Open a TCP connection and perform the BBDO v3 handshake."""
        self._sock = socket.create_connection(
            (self._host, self._port), timeout=timeout)
        self._sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self._handshake()
        # Start background reader to drain acks and avoid buffer deadlock.
        self._stop_drain.clear()
        self._drain_thread = threading.Thread(
            target=self._drain_loop, daemon=True)
        self._drain_thread.start()

    def close(self, linger_s: float = 3.0) -> None:
        """Shut down the connection and stop the drain thread."""
        # Stop the background drain thread first to avoid concurrent reads.
        self._stop_drain.set()
        if self._drain_thread:
            self._drain_thread.join(timeout=1.0)
            self._drain_thread = None

        if self._sock:
            try:
                # Half-close write side: signal end-of-send to the broker so
                # it can finish reading all buffered events.  SHUT_RDWR would
                # send RST and cause the broker to discard unread data.
                self._sock.shutdown(socket.SHUT_WR)
            except OSError:
                pass
            # Linger: drain incoming data until the broker closes or timeout.
            deadline = time.monotonic() + linger_s
            while time.monotonic() < deadline:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    break
                try:
                    r, _, _ = select.select([self._sock], [], [], remaining)
                    if not r:
                        break
                    data = self._sock.recv(65536)
                    if not data:
                        break
                except OSError:
                    break
            try:
                self._sock.close()
            except OSError:
                pass
            self._sock = None

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, *_):
        self.close()

    # ------------------------------------------------------------------
    # Handshake
    # ------------------------------------------------------------------

    def _handshake(self) -> None:
        """
        BBDO v3 negotiation.

        We connect as a BROKER peer (not ENGINE) to avoid the BBDO3 Engine
        configuration handshake (pb_diff_state / ack) that the central broker
        requires from ENGINE peers before it forwards events.  Connecting
        directly to the RRD broker (port 5670) as BROKER lets us inject
        events straight into the RRD stream without any config exchange.
        """
        welcome = bbdo_pb2.Welcome()
        welcome.version.major = 3
        welcome.version.minor = 0
        welcome.version.patch = 1
        welcome.poller_id = self._poller_id
        welcome.poller_name = "bbdo_injector"
        welcome.broker_name = "bbdo_injector"
        welcome.peer_type = common_pb2.BROKER
        welcome.extended_negotiation = False
        # extensions left empty → no TLS, no compression

        self._sock.sendall(
            _make_packet(TYPE_PB_WELCOME,
                         welcome.SerializeToString(),
                         self._source_id))

        # Read packets until we get the broker's Welcome.
        while True:
            ev_type, payload = _recv_packet(self._sock)
            if ev_type == TYPE_PB_WELCOME:
                break
            # Any other packet (unlikely at this stage) is silently ignored.

    # ------------------------------------------------------------------
    # Background drain
    # ------------------------------------------------------------------

    def _drain_loop(self) -> None:
        """
        Background thread: read and discard all incoming packets.

        Without this, the broker's ACK packets would fill the kernel receive
        buffer and eventually block our send() calls.
        """
        while not self._stop_drain.is_set():
            # Capture locally to avoid a race with close() setting self._sock
            # to None between the None-check and the actual recv call.
            sock = self._sock
            if sock is None:
                break
            try:
                r, _, _ = select.select([sock], [], [], 0.1)
                if r:
                    _recv_packet(sock)
            except (OSError, ConnectionError, AttributeError):
                break

    # ------------------------------------------------------------------
    # Event senders
    # ------------------------------------------------------------------

    def send_metric(self, metric_id: int, t: int, value: float,
                    step: int, rrd_len: int,
                    value_type: int = 0) -> None:
        """Send one pb_metric event."""
        m = storage_pb2.Metric()
        m.metric_id = metric_id
        m.time = t
        m.interval = step
        m.rrd_len = rrd_len
        m.value = value
        m.value_type = value_type  # 0 = GAUGE
        self._sock.sendall(
            _make_packet(TYPE_PB_METRIC, m.SerializeToString(),
                         self._source_id))

    def send_status(self, index_id: int, t: int, state: int,
                    step: int, rrd_len: int) -> None:
        """Send one pb_status event."""
        s = storage_pb2.Status()
        s.index_id = index_id
        s.time = t
        s.state = state
        s.interval = step
        s.rrd_len = rrd_len
        self._sock.sendall(
            _make_packet(TYPE_PB_STATUS, s.SerializeToString(),
                         self._source_id))


# ---------------------------------------------------------------------------
# High-level Robot Framework keywords
# ---------------------------------------------------------------------------

def ctn_inject_metric_events(
        host: str,
        port,
        metric_ids,
        rrd_len,
        step,
        n_old_points,
        value: float = 1.0,
        add_current_point: bool = True,
) -> dict:
    """
    Inject pb_metric events into a Centreon Broker and measure throughput.

    For each metric ID in *metric_ids*, sends *n_old_points* events whose
    timestamps run from ``now − n_old_points − step`` to ``now − step − 1``
    (all "old", i.e. older than one step).  If *add_current_point* is True
    a final event is sent at ``now`` for each metric to trigger the junction
    merge condition.

    Events are sent in round-robin across all metrics so that the broker's
    merge thread sees interleaved work.

    Args:
        host:              Broker host (string).
        port:              Broker input port (int or string, e.g. 5669).
        metric_ids:        List of metric IDs (or a single int).
        rrd_len:           RRD file length in seconds (int, e.g. 15552000).
        step:              Metric step / check_interval in seconds (int).
        n_old_points:      Number of old-timestamped events per metric (int).
        value:             Metric value used for all old points (float).
        add_current_point: If True, append one current-time event per metric
                           after all old points to trigger the junction merge.

    Returns:
        dict with keys:
          n_injected        – total number of BBDO packets sent
          inject_s          – wall-clock duration of the injection phase
          events_per_second – n_injected / inject_s
          t_first_old       – first (oldest) timestamp injected
          t_last_old        – last old timestamp injected
          t_current         – current-time timestamp (0 if not sent)

    *Example (Robot Framework):*

    | ${ids}=    Create List    ${42}    ${43}
    | ${r}=      Ctn Inject Metric Events
    | ...        host=127.0.0.1    port=5669
    | ...        metric_ids=${ids}    rrd_len=${15552000}    step=${60}
    | ...        n_old_points=${3600}
    | Log    Throughput: ${r}[events_per_second] ev/s
    """
    port = int(port)
    rrd_len = int(rrd_len)
    step = int(step)
    n_old_points = int(n_old_points)

    if isinstance(metric_ids, (int, str)):
        metric_ids = [int(metric_ids)]
    else:
        metric_ids = [int(m) for m in metric_ids]

    now = int(time.time())
    # Old points are spaced *step* seconds apart so they spread across real
    # history (e.g. 720 × 60 s = 12 h).
    #
    # Timestamps: t_first_old, t_first_old + step, …, t_first_old + (n-1)*step
    # where t_first_old is chosen so the last point lands at now - step - 1
    # (strictly old: < now - step).
    #
    # Junction condition (stream.cc): last_retention_time + step >= ect
    #   last_retention_time = now - step - 1
    #   ect (current-point timestamp) = now - 1     (>= now - step → current)
    #   check: (now - step - 1) + step >= now - 1
    #          now - 1 >= now - 1  →  True ✓
    #
    # Using ect = now would give now - 1 >= now → False (junction never fires).
    t_last_old = now - step - 1
    t_first_old = t_last_old - (n_old_points - 1) * step
    t_current = (now - 1) if add_current_point else 0

    n_injected = 0

    with BbdoClient(host, port) as client:
        t0 = time.monotonic()

        # Send old points in round-robin across all metrics.
        for i in range(n_old_points):
            t = t_first_old + i * step
            for mid in metric_ids:
                client.send_metric(mid, t, value + i * 0.001, step, rrd_len)
                n_injected += 1

        # Send one current-time point per metric to trigger junction merge.
        if add_current_point:
            for mid in metric_ids:
                client.send_metric(mid, t_current, value, step, rrd_len)
                n_injected += 1

        inject_s = time.monotonic() - t0

    return {
        'n_injected': n_injected,
        'inject_s': inject_s,
        'events_per_second': n_injected / inject_s if inject_s > 0 else 0.0,
        't_first_old': t_first_old,
        't_last_old': t_last_old,
        't_current': t_current,
    }


def ctn_inject_status_events(
        host: str,
        port,
        index_ids,
        rrd_len,
        step,
        n_old_points,
        add_current_point: bool = True,
) -> dict:
    """
    Same as ctn_inject_metric_events but for pb_status events.

    States cycle through 0 (OK), 1 (WARNING), 2 (CRITICAL) to produce a
    realistic mix.

    *Example (Robot Framework):*

    | ${ids}=    Create List    ${7}    ${8}
    | ${r}=      Ctn Inject Status Events
    | ...        host=127.0.0.1    port=5669
    | ...        index_ids=${ids}    rrd_len=${15552000}    step=${60}
    | ...        n_old_points=${3600}
    """
    port = int(port)
    rrd_len = int(rrd_len)
    step = int(step)
    n_old_points = int(n_old_points)

    if isinstance(index_ids, (int, str)):
        index_ids = [int(index_ids)]
    else:
        index_ids = [int(i) for i in index_ids]

    now = int(time.time())
    t_last_old = now - step - 1
    t_first_old = t_last_old - (n_old_points - 1) * step
    t_current = (now - 1) if add_current_point else 0

    n_injected = 0

    with BbdoClient(host, port) as client:
        t0 = time.monotonic()

        for i in range(n_old_points):
            t = t_first_old + i * step
            state = i % 3
            for iid in index_ids:
                client.send_status(iid, t, state, step, rrd_len)
                n_injected += 1

        if add_current_point:
            for iid in index_ids:
                client.send_status(iid, t_current, 0, step, rrd_len)
                n_injected += 1

        inject_s = time.monotonic() - t0

    return {
        'n_injected': n_injected,
        'inject_s': inject_s,
        'events_per_second': n_injected / inject_s if inject_s > 0 else 0.0,
        't_first_old': t_first_old,
        't_last_old': t_last_old,
        't_current': t_current,
    }


# ---------------------------------------------------------------------------
# Command-line interface
# ---------------------------------------------------------------------------

def _parse_args():
    p = argparse.ArgumentParser(
        description="BBDO metric injector for RRD retention benchmarking")
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=5669,
                   help="Broker input port (default: 5669)")
    p.add_argument("--metric-ids", required=True,
                   help="Comma-separated list of metric IDs")
    p.add_argument("--rrd-len", type=int, default=15552000,
                   help="RRD file length in seconds (default: 15552000 = 6 months)")
    p.add_argument("--step", type=int, default=60,
                   help="Metric step in seconds (default: 60)")
    p.add_argument("--n-old-points", type=int, default=3600,
                   help="Number of old-timestamped events per metric (default: 3600)")
    p.add_argument("--no-current", action="store_true",
                   help="Do not send a current-time point at the end")
    return p.parse_args()


def main():
    args = _parse_args()
    metric_ids = [int(x) for x in args.metric_ids.split(',')]

    print(f"Connecting to {args.host}:{args.port} …")
    result = ctn_inject_metric_events(
        host=args.host,
        port=args.port,
        metric_ids=metric_ids,
        rrd_len=args.rrd_len,
        step=args.step,
        n_old_points=args.n_old_points,
        add_current_point=not args.no_current,
    )

    print(f"Injected      : {result['n_injected']} events")
    print(f"Duration      : {result['inject_s']:.3f} s")
    print(f"Throughput    : {result['events_per_second']:.0f} events/s")
    print(f"First old ts  : {result['t_first_old']}")
    print(f"Last old ts   : {result['t_last_old']}")
    if result['t_current']:
        print(f"Current ts    : {result['t_current']}")


if __name__ == "__main__":
    main()
