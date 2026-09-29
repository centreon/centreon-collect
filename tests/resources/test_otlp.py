"""Collector contract tests: run with python3 -m unittest discover -s resources -p test_otlp.py."""

from pathlib import Path
import tempfile
import unittest

import grpc
from opentelemetry.proto.collector.metrics.v1 import metrics_service_pb2
from opentelemetry.proto.collector.metrics.v1 import metrics_service_pb2_grpc

from Otlp import Otlp, _bbdo_crc


class CollectorTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.collector = Otlp()
        self.addCleanup(self.collector.ctn_stop_otlp_collector)
        self.capture = Path(self.directory.name) / "capture.jsonl"
        endpoint = self.collector.ctn_start_otlp_collector(str(self.capture))
        self.channel = grpc.insecure_channel(endpoint)
        self.addCleanup(self.channel.close)
        grpc.channel_ready_future(self.channel).result(timeout=5)
        self.stub = metrics_service_pb2_grpc.MetricsServiceStub(self.channel)

    def export(self, value, host="host_1", metric="centreon.robot_probe", kind="gauge"):
        request = metrics_service_pb2.ExportMetricsServiceRequest()
        resource = request.resource_metrics.add()
        attr = resource.resource.attributes.add(key="host.name")
        attr.value.string_value = host
        attr = resource.resource.attributes.add(key="host.ip")
        attr.value.array_value.values.add(string_value="192.0.2.10")
        scope = resource.scope_metrics.add()
        scope.scope.name = "test"
        point = getattr(scope.metrics.add(
            name=metric, unit="1"), kind).data_points.add()
        point.as_double = value
        point.time_unix_nano = 123456789
        point.attributes.add(key="centreon.service.id").value.int_value = 42
        self.stub.Export(request, timeout=5)

    def test_real_grpc_export_preserves_typed_attributes(self):
        self.export(2)
        point = self.collector.ctn_wait_for_otlp_point(
            "host_1", "centreon.robot_probe", 2, 0)
        self.assertEqual(point["resource"]["host.ip"], ["192.0.2.10"])
        self.assertEqual(point["attributes"]["centreon.service.id"], 42)
        self.assertEqual(point["time_unix_nano"], 123456789)
        self.assertEqual(point["unit"], "1")
        self.assertIn('"value": 2.0', self.capture.read_text())

    def test_old_value_other_host_and_other_metric_cannot_match(self):
        self.export(1)
        self.export(2, host="host_2")
        self.export(2, metric="centreon.other")
        with self.assertRaisesRegex(AssertionError, "No OTLP point"):
            self.collector.ctn_wait_for_otlp_point(
                "host_1", "centreon.robot_probe", 2, 0.01)

    def test_sum_is_captured(self):
        self.export(3, kind="sum")
        point = self.collector.ctn_wait_for_otlp_point(
            "host_1", "centreon.robot_probe", 3, 0)
        self.assertEqual(point["kind"], "sum")

    def test_legacy_header_checksum(self):
        self.assertEqual(_bbdo_crc(b"123456789"), 0x906e)

    def test_restart_discards_previous_capture(self):
        self.export(4)
        self.collector.ctn_stop_otlp_collector()
        self.collector.ctn_stop_otlp_collector()
        self.collector.ctn_start_otlp_collector(str(self.capture))
        with self.assertRaisesRegex(AssertionError, "No OTLP point"):
            self.collector.ctn_wait_for_otlp_point(
                "host_1", "centreon.robot_probe", 4, 0)


if __name__ == "__main__":
    unittest.main()
