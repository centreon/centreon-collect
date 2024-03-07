#!/bin/bash

cd resources
python3 -m grpc_tools.protoc -I../../engine/enginerpc -I../../common/src --python_out=. --grpc_python_out=. engine.proto
python3 -m grpc_tools.protoc -I../../broker/core/src -I../../common/src --python_out=. --grpc_python_out=. broker.proto
python3 -m grpc_tools.protoc -I../../common/src --python_out=. --grpc_python_out=. process_stat.proto

python3 -m grpc_tools.protoc -I../../opentelemetry-proto --python_out=. --grpc_python_out=. opentelemetry/proto/collector/metrics/v1/metrics_service.proto
python3 -m grpc_tools.protoc -I../../opentelemetry-proto --python_out=. --grpc_python_out=. opentelemetry/proto/metrics/v1/metrics.proto
python3 -m grpc_tools.protoc -I../../opentelemetry-proto --python_out=. --grpc_python_out=. opentelemetry/proto/common/v1/common.proto
python3 -m grpc_tools.protoc -I../../opentelemetry-proto --python_out=. --grpc_python_out=. opentelemetry/proto/resource/v1/resource.proto
cd ..
