#!/usr/bin/python3
# Copyright 2026 Centreon
# Licensed under the Apache License, Version 2.0.

"""Local OTLP MetricsService used by the Broker Robot tests.

Broker connects to this server as an OTLP client. Assertions select a unique
probe value, not just a resource name, so exports from earlier steps cannot
satisfy a later assertion. The JSONL capture is kept for failure diagnosis.
"""

from collections import deque
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import queue
import struct
import sys
import threading
import time

# Robot's Process library forks to start cbd and centengine while the gRPC
# threads of this collector run, and gRPC then logs "Other threads are
# currently calling into gRPC, skipping fork() handlers" at each fork. These
# handlers only matter to a child that keeps using gRPC; ours exec at once.
# Read when the process creates its first gRPC channel or server.
os.environ.setdefault("GRPC_ENABLE_FORK_SUPPORT", "false")

import grpc
from google.protobuf.json_format import Parse
import grpc_stream_pb2
import grpc_stream_pb2_grpc
from robot.api import logger
from robot.api.deco import keyword, library
from opentelemetry.proto.collector.metrics.v1 import metrics_service_pb2
from opentelemetry.proto.collector.metrics.v1 import metrics_service_pb2_grpc


def _step(kind, message):
    """Show a test step in log.html and, aligned, on the console.

    On a terminal, Robot writes a marker after each keyword on the current
    console line, and writes the test name again after a few markers, so the
    step starts by clearing that line.
    """
    logger.info(f"{kind}: {message}")
    clear = "\r\x1b[2K" if sys.__stdout__.isatty() else ""
    logger.console(f"{clear}    {kind:<7} {message}")


def _value(value):
    kind = value.WhichOneof("value")
    if kind == "array_value":
        return [_value(item) for item in value.array_value.values]
    if kind == "kvlist_value":
        return _attributes(value.kvlist_value.values)
    return getattr(value, kind) if kind else None


def _attributes(attributes):
    return {item.key: _value(item.value) for item in attributes}


def _bbdo_crc(header):
    # BBDO's CRC-16/X-25, over the 14 header bytes following the CRC.
    crc = 0xffff
    for byte in header:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0x8408 if crc & 1 else 0)
    return crc ^ 0xffff


class _BbdoPeer:
    """A test poller with a real BBDO/gRPC handshake and ordered event stream."""

    def __init__(self, endpoint, poller_id):
        self.queue = queue.Queue()
        self.channel = grpc.insecure_channel(endpoint)
        self.call = None
        self.error = None
        self.ready = threading.Event()
        self.thread = None
        try:
            grpc.channel_ready_future(self.channel).result(timeout=15)
            welcome = grpc_stream_pb2.CentreonEvent()
            welcome.Welcome_.version.major = 3
            welcome.Welcome_.version.minor = 1
            welcome.Welcome_.poller_id = poller_id
            welcome.Welcome_.poller_name = f"robot-{poller_id}"
            welcome.Welcome_.broker_name = f"robot-module-{poller_id}"
            self.queue.put(welcome)
            self.call = grpc_stream_pb2_grpc.centreon_bbdoStub(
                self.channel).exchange(self._requests())
            self.thread = threading.Thread(target=self._receive, daemon=True)
            self.thread.start()
            if not self.ready.wait(15) or self.error:
                raise AssertionError(f"BBDO handshake failed: {self.error}")
        except Exception:
            self.close()
            raise

    def _requests(self):
        while True:
            event = self.queue.get()
            if event is None:
                return
            yield event

    def _receive(self):
        try:
            for event in self.call:
                if event.HasField("Welcome_"):
                    self.ready.set()
        except grpc.RpcError as error:
            self.error = str(error)
        finally:
            if not self.ready.is_set() and self.error is None:
                self.error = "Stream closed before Broker's Welcome"
            self.ready.set()

    def close(self):
        self.queue.put(None)
        if self.call is not None:
            self.call.cancel()
        self.channel.close()
        if self.thread is not None:
            self.thread.join(timeout=5)


@library(scope="SUITE", auto_keywords=False)
class Otlp(metrics_service_pb2_grpc.MetricsServiceServicer):
    def __init__(self):
        self._server = None
        self._executor = None
        self._capture = None
        self._condition = threading.Condition()
        self._points = deque(maxlen=4096)
        self._probe = 0
        self._peer = None

    @keyword
    def ctn_log_otlp_step(self, kind: str, message: str):
        """Show what a test does (kind: config, action, runtime, event or
        check) in log.html and on the console."""
        _step(kind, message)

    @keyword
    def ctn_connect_otlp_bbdo_peer(self, endpoint: str, poller_id: int = 10):
        """Inject BBDO events for delayed/deleted hosts and pre-upgrade senders."""
        self.ctn_disconnect_otlp_bbdo_peer()
        self._peer = _BbdoPeer(endpoint, poller_id)
        _step("action", f"BBDO test peer connected to {endpoint}")

    @keyword
    def ctn_disconnect_otlp_bbdo_peer(self):
        if self._peer is not None:
            self._peer.close()
            self._peer = None

    @keyword
    def ctn_send_otlp_bbdo_event(self, event_name: str, content: str):
        """Queue a protobuf event; content uses the protobuf JSON field names."""
        self._send_bbdo_event(event_name, content)
        _step("event", f"{event_name} {content}")

    def _send_bbdo_event(self, event_name, content):
        if self._peer is None or self._peer.error:
            raise AssertionError("BBDO test peer is not connected")
        event = grpc_stream_pb2.CentreonEvent(source_id=999)
        Parse(content, getattr(event, event_name + "_"))
        self._peer.queue.put(event)

    @keyword
    def ctn_send_otlp_legacy_macro(self, name: str, value: str,
                                   status: bool = False, enabled: bool = True):
        """Send a BBDO2-layout event inside gRPC's raw buffer envelope.

        Field order follows neb/custom_variable{,_status}.cc. Neither layout
        contains instance_id. This exercises the real BBDO2-to-protobuf adapter.
        """
        if self._peer is None or self._peer.error:
            raise AssertionError("BBDO test peer is not connected")
        if "\0" in name or "\0" in value:
            raise ValueError("BBDO strings cannot contain NUL")
        payload = struct.pack("!IB", 101, 1) + name.encode() + b"\0"
        payload += struct.pack("!IQ", 0, int(time.time()))
        if not status:
            payload = bytes([enabled]) + payload + struct.pack("!H", 0)
        payload += value.encode() + b"\0"
        if not status:
            payload += b"\0"  # default_value
        # neb category = 1; custom_variable = 3; custom_variable_status = 4.
        event_type = 0x10004 if status else 0x10003
        header = struct.pack("!HIII", len(payload), event_type, 999, 0)
        packet = struct.pack("!H", _bbdo_crc(header)) + header + payload
        self._peer.queue.put(grpc_stream_pb2.CentreonEvent(buffer=packet))
        kind = "custom_variable_status" if status else "custom_variable"
        state = "" if status else f" enabled={enabled}"
        _step("event", f"BBDO2 {kind} {name}='{value}'{state}")

    @keyword
    def ctn_otlp_next_probe(self):
        """Return a value no earlier export of this suite can carry."""
        self._probe += 1
        return self._probe

    @keyword
    def ctn_otlp_bbdo_probe(self, host_id: int = 101, host: str = "robot-host"):
        probe = self.ctn_otlp_next_probe()
        self._send_bbdo_event("ServiceStatus", json.dumps({
            "host_id": host_id, "service_id": 1,
            "last_check": int(time.time()),
            "perfdata": f"robot_probe={probe}",
        }))
        return self.ctn_wait_for_otlp_point(host, "centreon.robot_probe", probe)

    @keyword
    def ctn_start_otlp_collector(self, capture_path: str):
        """Listen on an OS-assigned loopback port and return host:port."""
        self.ctn_stop_otlp_collector()
        self._points.clear()
        path = Path(capture_path)
        path.parent.mkdir(parents=True, exist_ok=True)
        self._capture = path.open("w", encoding="utf-8")
        self._executor = ThreadPoolExecutor(max_workers=2)
        self._server = grpc.server(self._executor)
        metrics_service_pb2_grpc.add_MetricsServiceServicer_to_server(
            self, self._server)
        try:
            port = self._server.add_insecure_port("127.0.0.1:0")
            if not port:
                raise RuntimeError("Cannot bind the OTLP test collector")
            self._server.start()
        except Exception:
            self.ctn_stop_otlp_collector()
            raise
        return f"127.0.0.1:{port}"

    @keyword
    def ctn_stop_otlp_collector(self):
        """Stop RPC workers before closing their capture file; safe to repeat."""
        if self._server is not None:
            self._server.stop(0).wait(timeout=5)
            self._server = None
        if self._executor is not None:
            self._executor.shutdown(wait=True)
            self._executor = None
        if self._capture is not None:
            self._capture.close()
            self._capture = None

    def Export(self, request, context):
        with self._condition:
            for resource_metrics in request.resource_metrics:
                resource = _attributes(resource_metrics.resource.attributes)
                for scope_metrics in resource_metrics.scope_metrics:
                    for metric in scope_metrics.metrics:
                        kind = metric.WhichOneof("data")
                        if kind not in ("gauge", "sum"):
                            continue
                        for point in getattr(metric, kind).data_points:
                            number = point.WhichOneof("value")
                            record = {
                                "resource": resource,
                                "scope": scope_metrics.scope.name,
                                "metric": metric.name,
                                "unit": metric.unit,
                                "kind": kind,
                                "value": getattr(point, number) if number else None,
                                "attributes": _attributes(point.attributes),
                                "time_unix_nano": point.time_unix_nano,
                            }
                            self._points.append(record)
                            self._capture.write(json.dumps(record) + "\n")
            self._capture.flush()
            self._condition.notify_all()
        return metrics_service_pb2.ExportMetricsServiceResponse()

    @keyword
    def ctn_wait_for_otlp_point(self, host: str, metric: str, value: float,
                                timeout: float = 30):
        """Return the actual point, or fail with the most recent observations."""
        deadline = time.monotonic() + timeout
        with self._condition:
            while True:
                for point in self._points:
                    if (point["resource"].get("host.name") == host
                            and point["metric"] == metric
                            and point["value"] == value):
                        logger.info(f"OTLP point received: {point}")
                        return point
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    recent = list(self._points)[-8:]
                    raise AssertionError(
                        f"No OTLP point {host}/{metric}={value}; latest: {recent}")
                self._condition.wait(remaining)
