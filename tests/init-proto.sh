#!/bin/bash

cd resources
python3 -m grpc_tools.protoc -I../../engine/enginerpc -I../../common/src --python_out=. --grpc_python_out=. engine.proto
python3 -m grpc_tools.protoc -I../../broker/core/src -I../../common/src --python_out=. --grpc_python_out=. broker.proto
python3 -m grpc_tools.protoc -I../../common/src --python_out=. --grpc_python_out=. process_stat.proto
for file in `ls ../../bbdo/*.proto`
do
    python3 -m grpc_tools.protoc -I../../bbdo --python_out=. --grpc_python_out=. `basename $file`
done
python3 -m grpc_tools.protoc -I../../broker/grpc -I../../bbdo --python_out=. --grpc_python_out=. grpc_stream.proto
cd ..
