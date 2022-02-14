#include "grpc_stream.grpc.pb.h"

#include "com/centreon/broker/grpc/acceptor.hh"
#include "com/centreon/broker/grpc/server.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::grpc;

acceptor::acceptor(uint16_t port)
    : io::endpoint(true),
      _grpc_instance(server::create("0.0.0.0:" + std::to_string(port))) {}

acceptor::~acceptor() {}

std::unique_ptr<io::stream> acceptor::open() {
  return _grpc_instance->open(system_clock::now() + std::chrono::seconds(3));
}

std::unique_ptr<io::stream> acceptor::open(
    const system_clock::time_point& dead_line) {
  return _grpc_instance->open(dead_line);
}

bool acceptor::is_ready() const {
  return _grpc_instance->is_ready();
}
