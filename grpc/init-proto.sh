python3 -m grpc_tools.protoc -I../engine/enginerpc --python_out=. --grpc_python_out=. engine.proto
python3 -m grpc_tools.protoc -I../broker/core/src --python_out=. --grpc_python_out=. broker.proto
#python3 -m grpc_tools.protoc --proto_path=../centreon-broker/core/src/ --python_out=. --grpc_python_out=. ../centreon-broker/core/src/broker.proto

