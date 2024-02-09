#!/bin/bash

cd resources
python3 -m grpc_tools.protoc -I../../engine/enginerpc -I../../common/src --python_out=. --grpc_python_out=. engine.proto
python3 -m grpc_tools.protoc -I../../broker/core/src -I../../common/src --python_out=. --grpc_python_out=. broker.proto
python3 -m grpc_tools.protoc -I../../common/src --python_out=. --grpc_python_out=. process_stat.proto

OTL_GRPC_PATH=`grep CONAN_RES_DIRS_OPENTELEMETRY-PROTO ../../build*/conanbuildinfo.cmake | awk '{print $2}' | cut -f2 -d\" | head -1`
python3 -m grpc_tools.protoc -I${OTL_GRPC_PATH} --python_out=. --grpc_python_out=. opentelemetry/proto/collector/metrics/v1/metrics_service.proto
python3 -m grpc_tools.protoc -I${OTL_GRPC_PATH} --python_out=. --grpc_python_out=. opentelemetry/proto/metrics/v1/metrics.proto
python3 -m grpc_tools.protoc -I${OTL_GRPC_PATH} --python_out=. --grpc_python_out=. opentelemetry/proto/common/v1/common.proto
python3 -m grpc_tools.protoc -I${OTL_GRPC_PATH} --python_out=. --grpc_python_out=. opentelemetry/proto/resource/v1/resource.proto
cd ..
