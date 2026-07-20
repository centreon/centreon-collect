#!/bin/bash

set -e

cd resources

my_python=python3

if command -v python3.8 >/dev/null 2>&1; then
    my_python=python3.8
    echo "Alias python → python3.8 activated"
fi

$my_python -m grpc_tools.protoc -I../../engine/enginerpc -I../../common/src --python_out=. --grpc_python_out=. engine.proto
$my_python -m grpc_tools.protoc -I../../bbdo -I../../broker/core/brokerrpc -I../../common/src --python_out=. --grpc_python_out=. broker.proto
$my_python -m grpc_tools.protoc -I../../common/src --python_out=. --grpc_python_out=. process_stat.proto
for file in `ls ../../bbdo/*.proto`
do
    $my_python -m grpc_tools.protoc -I../../bbdo --python_out=. --grpc_python_out=. `basename $file`
done
$my_python ../../broker/grpc/generate_proto.py -f grpc_stream.proto -c /tmp/not_used.cc -d ../../bbdo
$my_python -m grpc_tools.protoc -I. -I../../bbdo -I../../opentelemetry-proto --python_out=. --grpc_python_out=. grpc_stream.proto

$my_python -m grpc_tools.protoc -I../../opentelemetry-proto --python_out=. --grpc_python_out=. opentelemetry/proto/collector/metrics/v1/metrics_service.proto
$my_python -m grpc_tools.protoc -I../../opentelemetry-proto --python_out=. --grpc_python_out=. opentelemetry/proto/metrics/v1/metrics.proto
$my_python -m grpc_tools.protoc -I../../opentelemetry-proto --python_out=. --grpc_python_out=. opentelemetry/proto/common/v1/common.proto
$my_python -m grpc_tools.protoc -I../../opentelemetry-proto --python_out=. --grpc_python_out=. opentelemetry/proto/resource/v1/resource.proto
$my_python -m grpc_tools.protoc -I../../common/engine_conf -I../../common/engine_conf --python_out=. state.proto
$my_python -m grpc_tools.protoc -I../../broker/rrd/proto --python_out=. rrd_retention.proto
cd ..
