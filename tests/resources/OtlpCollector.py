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
# A minimal OTLP/gRPC metrics collector, used as the peer of the Broker 'otlp'
# output (70-otlp.so). It implements MetricsService/Export and accumulates
# counters the Robot tests can assert on: number of requests, number of
# datapoints, metric names seen and hosts seen.
#
# It is deliberately dumb: no batching, no persistence. Everything is kept in
# memory under a lock, so it stays usable for hours in a soak test.

import json
import os
import threading
import time
from concurrent import futures

import grpc
from google.protobuf.json_format import MessageToDict
from robot.api import logger

from Common import VAR_ROOT

import opentelemetry.proto.collector.metrics.v1.metrics_service_pb2 as ms_pb2
import opentelemetry.proto.collector.metrics.v1.metrics_service_pb2_grpc as ms_pb2_grpc


class _MetricsServicer(ms_pb2_grpc.MetricsServiceServicer):
    """The MetricsService implementation accumulating what Broker exports."""

    def __init__(self, dump_file: str = "", dump_limit: int = 5):
        self._lock = threading.Lock()
        self._dump_file = dump_file
        self._dump_limit = dump_limit
        self.reset()

    def reset(self):
        with self._lock:
            self.requests = 0
            self.datapoints = 0
            self.resources = 0
            self.metric_names = {}
            self.hosts = {}
            self.first_request_time = 0.0
            self.last_request_time = 0.0
            self.dumped = 0

    def Export(self, request, context):
        """Called by Broker's otlp_exporter. Never fails: an OK status is what
        lets the stream acknowledge its events to the muxer."""
        now = time.time()
        with self._lock:
            self.requests += 1
            if self.first_request_time == 0.0:
                self.first_request_time = now
            self.last_request_time = now

            for rm in request.resource_metrics:
                self.resources += 1
                host = ""
                for attr in rm.resource.attributes:
                    if attr.key == "host.name":
                        host = attr.value.string_value
                        break
                for sm in rm.scope_metrics:
                    for m in sm.metrics:
                        # gauge and sum share a protobuf oneof; reading the
                        # unset one yields an empty message, so this is safe.
                        nb = len(m.gauge.data_points) + len(m.sum.data_points)
                        self.datapoints += nb
                        self.metric_names[m.name] = self.metric_names.get(
                            m.name, 0) + nb
                        if host:
                            self.hosts[host] = self.hosts.get(host, 0) + nb

            if self._dump_file and self.dumped < self._dump_limit:
                self.dumped += 1
                with open(self._dump_file, "a") as f:
                    f.write(json.dumps(MessageToDict(request), indent=2))
                    f.write("\n")

        return ms_pb2.ExportMetricsServiceResponse()


_server = None
_servicer = None
_port = 0


def ctn_start_otlp_collector(port: int = 14317, dump_file: str = "", dump_limit: int = 5):
    """
    Start an OTLP/gRPC metrics collector listening on the given port.

    Args:
        port (int, optional): TCP port to listen on. Defaults to 14317 so it
            never collides with the 4317 an engine otel server may use.
        dump_file (str, optional): if set, the first dump_limit requests are
            appended to this file as indented JSON, for eyeballing what Broker
            actually sends. Empty disables the dump.
        dump_limit (int, optional): how many requests to dump. Defaults to 5.

    *Example:*

    | Ctn Start Otlp Collector | 14317 | /tmp/otlp-export.json |
    """
    global _server, _servicer, _port
    if _server is not None:
        ctn_stop_otlp_collector()

    _servicer = _MetricsServicer(dump_file, int(dump_limit))
    _server = grpc.server(futures.ThreadPoolExecutor(max_workers=4))
    ms_pb2_grpc.add_MetricsServiceServicer_to_server(_servicer, _server)
    bound = _server.add_insecure_port(f"0.0.0.0:{int(port)}")
    if bound == 0:
        _server = None
        _servicer = None
        raise Exception(f"OTLP collector cannot listen on port {port}")
    _port = bound
    _server.start()
    logger.console(f"\nOTLP collector listening on 0.0.0.0:{_port}")
    return _port


def ctn_stop_otlp_collector(grace: float = 1.0):
    """
    Stop the OTLP collector. Safe to call when it is not running.

    Args:
        grace (float, optional): seconds given to in-flight RPCs. Defaults to 1.
    """
    global _server, _servicer
    if _server is None:
        return
    _server.stop(float(grace)).wait(float(grace) + 5)
    _server = None
    logger.console("\nOTLP collector stopped")


def ctn_reset_otlp_collector():
    """Zero all the collector counters, keeping the server running."""
    if _servicer is None:
        raise Exception("OTLP collector is not started")
    _servicer.reset()


def ctn_otlp_collector_stats():
    """
    Return the collector counters as a dictionary.

    Returns:
        A dict with keys requests, datapoints, resources, metrics (number of
        distinct metric names), hosts (number of distinct hosts) and
        last_request_age (seconds since the last Export, -1 if none yet).
    """
    if _servicer is None:
        raise Exception("OTLP collector is not started")
    with _servicer._lock:
        age = -1.0
        if _servicer.last_request_time > 0.0:
            age = time.time() - _servicer.last_request_time
        return {
            "requests": _servicer.requests,
            "datapoints": _servicer.datapoints,
            "resources": _servicer.resources,
            "metrics": len(_servicer.metric_names),
            "hosts": len(_servicer.hosts),
            "last_request_age": round(age, 1),
        }


def ctn_otlp_collector_datapoints():
    """Return the total number of datapoints received so far."""
    return ctn_otlp_collector_stats()["datapoints"]


def ctn_otlp_collector_requests():
    """Return the total number of Export requests received so far."""
    return ctn_otlp_collector_stats()["requests"]


def ctn_otlp_collector_metric_names():
    """Return the sorted list of distinct metric names received so far."""
    if _servicer is None:
        raise Exception("OTLP collector is not started")
    with _servicer._lock:
        return sorted(_servicer.metric_names.keys())


def ctn_otlp_collector_hosts():
    """Return the sorted list of distinct host.name values received so far."""
    if _servicer is None:
        raise Exception("OTLP collector is not started")
    with _servicer._lock:
        return sorted(_servicer.hosts.keys())


def ctn_wait_for_otlp_datapoints(count: int, timeout: int = 60):
    """
    Wait until the collector has received at least count datapoints.

    Args:
        count (int): the number of datapoints to wait for.
        timeout (int, optional): Defaults to 60s.

    Returns:
        True if the count was reached before the timeout.
    """
    limit = time.time() + int(timeout)
    while time.time() < limit:
        if ctn_otlp_collector_datapoints() >= int(count):
            return True
        time.sleep(1)
    return False


def ctn_log_otlp_collector_summary():
    """Log a one line summary of the collector state. Tolerates a collector that
    was never started, so it is safe to call from a test teardown."""
    if _servicer is None:
        logger.console("\nOTLP collector: not started")
        return None
    stats = ctn_otlp_collector_stats()
    logger.console(
        f"\nOTLP collector: {stats['requests']} requests, {stats['datapoints']} datapoints, "
        f"{stats['hosts']} hosts, {stats['metrics']} metric names, "
        f"last export {stats['last_request_age']}s ago")
    return stats


#
# The counters below come from the Broker side, read from the stats file the
# 'stats' endpoint writes. They are what otlp::stream::statistics() publishes,
# so they work whether the peer is the collector above or an external one.
#

def _read_stats_file(filename: str, timeout: float = 5.0):
    """
    Read and parse a Broker stats file.

    That file is a FIFO: stats/worker.cc keeps it open O_WRONLY|O_NONBLOCK and
    only builds a snapshot once a reader shows up. So it must be opened
    O_NONBLOCK (a blocking open hangs forever when Broker is down), then polled
    for a short while, and the fd must stay open while we wait or the writer
    stops seeing us. A truncated read is normal and simply means the snapshot is
    still being written, so we keep appending until the JSON parses.

    Returns the parsed dict, or None when nothing readable arrived in time.
    """
    path = f"{VAR_ROOT}/lib/centreon-broker/{filename}"
    try:
        fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK)
    except OSError:
        return None
    try:
        buf = b""
        limit = time.time() + float(timeout)
        while time.time() < limit:
            try:
                chunk = os.read(fd, 65536)
            except BlockingIOError:
                chunk = b""
            if not chunk:
                time.sleep(0.1)
                continue
            buf += chunk
            try:
                return json.loads(buf)
            except ValueError:
                continue
        return None
    finally:
        os.close(fd)


def ctn_otlp_output_stats(output: str = "central-broker-otlp", name: str = "central"):
    """
    Return the statistics Broker publishes for its otlp output.

    Args:
        output (str, optional): the endpoint name. Defaults to central-broker-otlp.
        name (str, optional): the broker instance among central, rrd, module%d.

    Returns:
        A dict with batches_sent, datapoints_sent, export_errors,
        dropped_no_host_name, inflight_requests and pending_datapoints. All zero
        if the endpoint is not in the stats file yet.
    """
    if name == 'central':
        filename = "central-broker-master-stats.json"
    elif name == 'module':
        filename = "central-module-master-stats.json"
    else:
        filename = "central-rrd-master-stats.json"

    empty = {
        "batches_sent": 0,
        "datapoints_sent": 0,
        "export_errors": 0,
        "dropped_no_host_name": 0,
        "inflight_requests": 0,
        "pending_datapoints": 0,
    }
    stats = _read_stats_file(filename)
    if stats is None:
        return empty

    endpoint = stats.get(f"endpoint {output}")
    if endpoint is None:
        return empty
    return {key: endpoint.get(key, 0) for key in empty}


def ctn_wait_for_otlp_export(count: int, timeout: int = 120, output: str = "central-broker-otlp"):
    """
    Wait until Broker reports it has exported at least count datapoints.

    Args:
        count (int): the number of datapoints to wait for.
        timeout (int, optional): Defaults to 120s.
        output (str, optional): the endpoint name.

    Returns:
        True if the count was reached before the timeout.
    """
    limit = time.time() + int(timeout)
    while time.time() < limit:
        if ctn_otlp_output_stats(output)["datapoints_sent"] >= int(count):
            return True
        time.sleep(2)
    return False


def ctn_log_otlp_output_summary(output: str = "central-broker-otlp"):
    """Log a one line summary of what Broker says about its otlp output."""
    stats = ctn_otlp_output_stats(output)
    logger.console(
        f"otlp output: {stats['batches_sent']} batches, {stats['datapoints_sent']} datapoints sent, "
        f"{stats['export_errors']} export errors, {stats['dropped_no_host_name']} dropped without host name, "
        f"{stats['inflight_requests']} in flight, {stats['pending_datapoints']} pending")
    return stats
