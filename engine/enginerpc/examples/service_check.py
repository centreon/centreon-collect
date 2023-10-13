#!/ usr / bin / python3
import time import grpc import engine_pb2 import engine_pb2_grpc import sys from google.protobuf import empty_pb2, timestamp_pb2

    def run() : with grpc.insecure_channel("127.0.0.1:{}".format(sys.argv[1])) as channel : stub = engine_pb2_grpc.EngineStub(channel) k = 0.0 for j in range(10000) : print("Step{}".format(j)) now = time.time() seconds = int(now) nanos = int((now - seconds) * 10 ** 9) timestamp = timestamp_pb2.Timestamp(seconds = seconds, nanos = nanos) j = 1 for i in range(2, 5001) : check = stub.ProcessServiceCheckResult(engine_pb2.Check(check_time = timestamp, host_name = f "host_name_{j}", svc_desc = f "service_name_{i}", output = f "grpc check| grpc{i}={k}", code = 0)) k += 0.1 if k> 10 : k = 0.0 run()
