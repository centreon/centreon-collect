#!/bin/bash

cd resources
python3 -m grpc_tools.protoc -I../../engine/enginerpc --python_out=. --grpc_python_out=. --experimental_allow_proto3_optional engine.proto
python3 -m grpc_tools.protoc -I../../broker/core/src --python_out=. --grpc_python_out=. --experimental_allow_proto3_optional broker.proto
cd ..
