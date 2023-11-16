#!/bin/bash

cd resources
python3 -m grpc_tools.protoc -I../../engine/enginerpc --python_out=. --grpc_python_out=. engine.proto --experimental_allow_proto3_optional
python3 -m grpc_tools.protoc -I../../broker/core/src --python_out=. --grpc_python_out=. broker.proto --experimental_allow_proto3_optional
cd ..
