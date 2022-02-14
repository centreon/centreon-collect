#include "com/centreon/broker/grpc/client.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;

using namespace com::centreon::broker;
using namespace com::centreon::broker::grpc;

client::client(const std::string& hostport)
    : channel(hostport),
      _read_pending(false),
      _read_started(false),
      _write_pending(false),
      _error(false) {
  log_v2::grpc()->trace("{} this={:p}", __PRETTY_FUNCTION__,
                        static_cast<void*>(this));
  ::grpc::ChannelArguments args;
  args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);
  args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, 30000);
  args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 10000);
  args.SetInt(GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA, 0);
  _channel = ::grpc::CreateCustomChannel(
      hostport, ::grpc::InsecureChannelCredentials(), args);
  _stub = std::unique_ptr<com::centreon::broker::stream::centreon_bbdo::Stub>(
      com::centreon::broker::stream::centreon_bbdo::NewStub(_channel));
  _context = std::make_unique<::grpc::ClientContext>();
  _stub->async()->exchange(_context.get(), this);
}

client::pointer client::create(const std::string& hostport) {
  client::pointer newClient(new client(hostport));
  newClient->start_read();
  return newClient;
}

client::~client() {
  log_v2::grpc()->trace("{} this={:p}", __PRETTY_FUNCTION__,
                        static_cast<void*>(this));
  _stub.reset();
  _context.reset();
  _channel.reset();
}

/*******************************************************
 *     read section
 *******************************************************/

void client::start_read() {
  event_ptr to_read;
  bool has_to_call = false;
  {
    unique_lock l(_protect);
    if (_read_pending) {
      return;
    }
    to_read = _read_current = std::make_shared<grpc_event>();

    _read_pending = true;
    if (!_read_started) {
      log_v2::grpc()->debug("{} start call and read", __PRETTY_FUNCTION__);
      _read_started = true;
      has_to_call = true;
    } else {
      log_v2::grpc()->trace("{} start read", __PRETTY_FUNCTION__);
    }
  }
  if (to_read) {
    StartRead(to_read.get());
    if (has_to_call) {
      StartCall();
    }
  }
}

void client::OnReadDone(bool ok) {
  if (ok) {
    {
      unique_lock l(_protect);
      if (log_v2::grpc()->level() == spdlog::level::trace) {
        log_v2::grpc()->trace("{} receive:{}", __PRETTY_FUNCTION__,
                              detail_centreon_event(*_read_current));
      } else {
        log_v2::grpc()->debug("{} receive:{}", __PRETTY_FUNCTION__,
                              *_read_current);
      }
      _read_queue.push_back(_read_current);
      _read_cond.notify_one();
      _read_pending = false;
    }
    start_read();
  } else {
    _error = true;
  }
}

std::pair<event_ptr, bool> client::read(time_t deadline) {
  return read(system_clock::from_time_t(deadline));
}

std::pair<event_ptr, bool> client::read(const system_clock::duration& timeout) {
  return read(system_clock::now() + timeout);
}

std::pair<event_ptr, bool> client::read(
    const system_clock::time_point& deadline) {
  event_ptr read;
  {
    unique_lock l(_protect);
    if (!_read_queue.empty()) {
      read = _read_queue.front();
      _read_queue.pop_front();
      return std::make_pair(read, true);
    }
    if (is_down()) {
      throw(msg_fmt("{} connexion is down", __PRETTY_FUNCTION__));
    }
    _read_cond.wait_until(l, deadline,
                          [this]() { return !_read_queue.empty(); });
    if (!_read_queue.empty()) {
      read = _read_queue.front();
      _read_queue.pop_front();
      return std::make_pair(read, true);
    }
  }
  return std::make_pair(read, false);
}

/*******************************************************
 *     write section
 *******************************************************/

void client::start_write() {
  event_ptr write_current;
  {
    unique_lock l(_protect);
    if (_write_pending) {
      return;
    }
    if (_write_queue.empty()) {
      return;
    }
    _write_pending = true;
    write_current = _write_current = _write_queue.front();
  }
  if (log_v2::grpc()->level() == spdlog::level::trace) {
    log_v2::grpc()->trace("{} write:{}", __PRETTY_FUNCTION__,
                          detail_centreon_event(*write_current));
  } else {
    log_v2::grpc()->debug("{} write:{}", __PRETTY_FUNCTION__, *write_current);
  }
  StartWrite(write_current.get());
}

int client::write(const event_ptr& to_send) {
  int ret = 0;
  if (is_down()) {
    throw(msg_fmt("{} connexion is down", __PRETTY_FUNCTION__));
  }
  {
    unique_lock l(_protect);
    _write_queue.push_back(to_send);
    ret = _nb_written;
    _nb_written = 0;
  }
  start_write();
  return ret;
}

int client::flush() {
  unique_lock l(_protect);
  int ret = _nb_written;
  _nb_written = 0;
  return ret;
}

int client::stop() {
  to_trash();
  return flush();
}

void client::OnWriteDone(bool ok) {
  if (ok) {
    bool data_to_write = false;
    {
      unique_lock l(_protect);
      _write_pending = false;
      if (log_v2::grpc()->level() == spdlog::level::trace) {
        log_v2::grpc()->trace("{} write done :{}", __PRETTY_FUNCTION__,
                              detail_centreon_event(*_write_current));
      } else {
        log_v2::grpc()->debug("{} write done :{}", __PRETTY_FUNCTION__,
                              *_write_current);
      }

      ++_nb_written;
      _write_queue.pop_front();
      data_to_write = !_write_queue.empty();
    }
    if (data_to_write) {
      start_write();
    }
  } else {
    unique_lock l(_protect);
    log_v2::grpc()->error("{} write failed :{}", __PRETTY_FUNCTION__,
                          *_write_current);
    _error = true;
  }
}
