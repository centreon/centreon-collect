#include "com/centreon/broker/grpc/stream.hh"
#include "grpc_stream.grpc.pb.h"

#include "com/centreon/broker/grpc/client.hh"
#include "com/centreon/broker/grpc/server.hh"

using namespace com::centreon::broker::grpc;
using namespace com::centreon::broker;

com::centreon::broker::grpc::stream::stream(const std::string& hostport)
    : io::stream("GRPC"), _hostport(hostport), _accept(false) {
  _channel = client::create(_hostport);
}

com::centreon::broker::grpc::stream::stream(
    const std::shared_ptr<accepted_service>& accepted)
    : io::stream("GRPC"), _hostport(""), _accept(true), _channel(accepted) {}

com::centreon::broker::grpc::stream::~stream() noexcept {
  if (_channel) {
    _channel->to_trash();
  }
}

#define READ_IMPL                                                             \
  std::pair<event_ptr, bool> read_res = _channel->read(duration_or_deadline); \
  if (read_res.second) {                                                      \
    const grpc_event& to_convert = *read_res.first;                           \
    if (to_convert.has_raw_data()) {                                          \
      d = std::make_shared<io::raw>(to_convert.raw_data().type(),             \
                                    to_convert.raw_data().source(),           \
                                    to_convert.raw_data().destination(),      \
                                    to_convert.raw_data().buffer());          \
    } else {                                                                  \
      return false;                                                           \
    }                                                                         \
  }                                                                           \
  return read_res.second;

bool com::centreon::broker::grpc::stream::read(std::shared_ptr<io::data>& d,
                                               time_t duration_or_deadline) {
  READ_IMPL
}

bool com::centreon::broker::grpc::stream::read(
    std::shared_ptr<io::data>& d,
    const system_clock::time_point& duration_or_deadline) {
  READ_IMPL
}

bool com::centreon::broker::grpc::stream::read(
    std::shared_ptr<io::data>& d,
    const system_clock::duration& duration_or_deadline){READ_IMPL}

int32_t com::centreon::broker::grpc::stream::write(
    std::shared_ptr<io::data> const& d) {
  event_ptr to_send(std::make_shared<grpc_event>());

  grpc_raw_data* raw = to_send->mutable_raw_data();
  raw->set_type(d->type());
  raw->set_source(d->source_id);
  raw->set_destination(d->destination_id);
  std::shared_ptr<io::raw> raw_src = std::dynamic_pointer_cast<io::raw>(d);
  if (raw_src) {
    raw->mutable_buffer()->assign(raw_src->_buffer.begin(),
                                  raw_src->_buffer.end());
  }

  return _channel->write(to_send);
}

int32_t com::centreon::broker::grpc::stream::flush() {
  return _channel->flush();
}

int32_t com::centreon::broker::grpc::stream::stop() {
  return _channel->stop();
}

bool com::centreon::broker::grpc::stream::is_down() const {
  return _channel->is_down();
}
