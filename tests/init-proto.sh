#!/bin/bash

set -e

cd resources
python3 -m grpc_tools.protoc -I../../engine/enginerpc -I../../common/src --python_out=. --grpc_python_out=. engine.proto
python3 -m grpc_tools.protoc -I../../bbdo -I../../broker/core/src -I../../common/src --python_out=. --grpc_python_out=. broker.proto
python3 -m grpc_tools.protoc -I../../common/src --python_out=. --grpc_python_out=. process_stat.proto
for file in `ls ../../bbdo/*.proto` ; do
    python3 -m grpc_tools.protoc -I../../bbdo --python_out=. --grpc_python_out=. `basename $file`
done
python3 ../../broker/grpc/generate_proto.py -f grpc_stream.proto -c /tmp/not_used.cc -d ../../bbdo
python3 -m grpc_tools.protoc -I. -I../../bbdo -I../../opentelemetry-proto --python_out=. --grpc_python_out=. grpc_stream.proto

python3 -m grpc_tools.protoc -I../../opentelemetry-proto --python_out=. --grpc_python_out=. opentelemetry/proto/collector/metrics/v1/metrics_service.proto
python3 -m grpc_tools.protoc -I../../opentelemetry-proto --python_out=. --grpc_python_out=. opentelemetry/proto/metrics/v1/metrics.proto
python3 -m grpc_tools.protoc -I../../opentelemetry-proto --python_out=. --grpc_python_out=. opentelemetry/proto/common/v1/common.proto
python3 -m grpc_tools.protoc -I../../opentelemetry-proto --python_out=. --grpc_python_out=. opentelemetry/proto/resource/v1/resource.proto
cd ..
