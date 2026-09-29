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
from pathlib import Path
import threading
import time

import grpc
from robot.api.deco import keyword, library
from opentelemetry.proto.collector.metrics.v1 import metrics_service_pb2
from opentelemetry.proto.collector.metrics.v1 import metrics_service_pb2_grpc


def _value(value):
    kind = value.WhichOneof("value")
    if kind == "array_value":
        return [_value(item) for item in value.array_value.values]
    if kind == "kvlist_value":
        return _attributes(value.kvlist_value.values)
    return getattr(value, kind) if kind else None


def _attributes(attributes):
    return {item.key: _value(item.value) for item in attributes}


@library(scope="SUITE", auto_keywords=False)
class Otlp(metrics_service_pb2_grpc.MetricsServiceServicer):
    def __init__(self):
        self._server = None
        self._executor = None
        self._capture = None
        self._condition = threading.Condition()
        self._points = deque(maxlen=4096)
        self._probe = 0

    @keyword
    def ctn_otlp_next_probe(self):
        """Return a value no earlier export of this suite can carry."""
        self._probe += 1
        return self._probe

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
                        return point
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    recent = list(self._points)[-8:]
                    raise AssertionError(
                        f"No OTLP point {host}/{metric}={value}; latest: {recent}")
                self._condition.wait(remaining)
