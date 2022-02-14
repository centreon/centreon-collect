#include "grpc_stream.grpc.pb.h"

#include "com/centreon/broker/grpc/server.hh"
#include "com/centreon/broker/grpc/stream.hh"
#include "com/centreon/broker/misc/trash.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker::grpc;
using namespace com::centreon::broker;
using namespace com::centreon::exceptions;

/****************************************************************************
 * accepted_service
 ****************************************************************************/

static com::centreon::broker::misc::trash<accepted_service> _service_trash;

accepted_service::accepted_service()
    : channel(""), _error(false), _write_pending(false), _read_pending(false) {
  log_v2::grpc()->trace("{} this={:p}", __PRETTY_FUNCTION__,
                        static_cast<void*>(this));
}

accepted_service::~accepted_service() {
  log_v2::grpc()->trace("{} this={:p}", __PRETTY_FUNCTION__,
                        static_cast<void*>(this));
}

void accepted_service::to_trash() {
  desactivate();
  _thrown = true;
  _service_trash.to_trash(shared_from_this(), time(nullptr) + 60);
}

void accepted_service::desactivate() {
  _error = true;
}

void accepted_service::OnCancel() {
  desactivate();
}

/*******************************************************
 *     read section
 *******************************************************/

void accepted_service::start_read() {
  {
    unique_lock l(_protect);
    if (!is_alive()) {
      return;
    }
    if (_read_pending) {
      return;
    }
    _read_current = std::make_shared<grpc_event>();
    _read_pending = true;
    log_v2::grpc()->trace("{:p} {} start read", static_cast<void*>(this),
                          __PRETTY_FUNCTION__);
  }
  StartRead(_read_current.get());
}

void accepted_service::OnReadDone(bool ok) {
  {
    unique_lock l(_protect);
    _read_pending = false;
    _read_queue.push_back(_read_current);
    _read_cond.notify_all();
    if (ok) {
      if (log_v2::grpc()->level() == spdlog::level::trace) {
        log_v2::grpc()->trace("{:p} {} receive:{}", static_cast<void*>(this),
                              __PRETTY_FUNCTION__,
                              detail_centreon_event(*_read_current));
      } else {
        log_v2::grpc()->debug("{:p} {} receive:{}", static_cast<void*>(this),
                              __PRETTY_FUNCTION__, *_read_current);
      }
    } else {
      log_v2::grpc()->error("{:p} {} echec receive", static_cast<void*>(this),
                            __PRETTY_FUNCTION__);
      _error = true;
    }
  }
  if (ok) {
    start_read();
  }
}
std::pair<event_ptr, bool> accepted_service::read(
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

bool accepted_service::start_write() {
  event_ptr to_send;
  {
    unique_lock l(_protect);
    if (!is_alive()) {
      return false;
    }
    if (_write_pending || _write_queue.empty()) {
      return false;
    }
    to_send = _write_current = _write_queue.front();
    _write_pending = true;
    log_v2::grpc()->trace("{:p} {} start write", static_cast<void*>(this),
                          __PRETTY_FUNCTION__);
  }
  StartWrite(to_send.get());
  return true;
}

void accepted_service::OnWriteDone(bool ok) {
  bool another_to_write = false;
  {
    unique_lock l(_protect);
    _write_pending = false;
    if (ok) {
      _write_queue.pop_front();
      ++_nb_written;
      another_to_write = !_write_queue.empty();
      if (log_v2::grpc()->level() == spdlog::level::trace) {
        log_v2::grpc()->trace("{:p} {} written:{}", static_cast<void*>(this),
                              __PRETTY_FUNCTION__,
                              detail_centreon_event(*_write_current));
      } else {
        log_v2::grpc()->debug("{:p} {} written:{}", static_cast<void*>(this),
                              __PRETTY_FUNCTION__, *_write_current);
      }
    } else {
      log_v2::grpc()->error("{:p} {} echec write", static_cast<void*>(this),
                            __PRETTY_FUNCTION__, *_write_current);
    }
  }
  if (another_to_write) {
    start_write();
  }
}
int accepted_service::write(const event_ptr& to_send) {
  int nb_written = 0;
  if (is_down()) {
    throw(msg_fmt("{} connexion is down", __PRETTY_FUNCTION__));
  }
  {
    unique_lock l(_protect);
    _write_queue.push_back(to_send);
    nb_written = _nb_written;
    _nb_written = 0;
  }
  start_write();
  return nb_written;
}

int accepted_service::flush() {
  unique_lock l(_protect);
  int nb_written = _nb_written;
  _nb_written = 0;
  return nb_written;
}

int accepted_service::stop() {
  int ret = flush();
  to_trash();
  return ret;
}

/****************************************************************************
 * server
 ****************************************************************************/
server::server(const std::string& hostport) : _hostport(hostport) {}

void server::start() {
  ::grpc::Service::MarkMethodCallback(
      0, new ::grpc::internal::CallbackBidiHandler<
             ::com::centreon::broker::stream::centreon_event,
             ::com::centreon::broker::stream::centreon_event>(
             [me = shared_from_this()](::grpc::CallbackServerContext* context) {
               return me->exchange(context);
             }));

  ::grpc::ServerBuilder builder;
  builder.AddListeningPort(_hostport, ::grpc::InsecureServerCredentials());
  builder.RegisterService(this);
  builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);
  builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIME_MS, 30000);
  builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 10000);
  builder.AddChannelArgument(GRPC_ARG_HTTP2_MAX_PING_STRIKES, 0);
  builder.AddChannelArgument(GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA, 0);
  builder.AddChannelArgument(
      GRPC_ARG_HTTP2_MIN_RECV_PING_INTERVAL_WITHOUT_DATA_MS, 60000);
  _server = std::unique_ptr<::grpc::Server>(builder.BuildAndStart());
}

server::pointer server::create(const std::string& hostport) {
  server::pointer ret(new server(hostport));
  ret->start();
  return ret;
}

::grpc::ServerBidiReactor<::centreon_grpc::grpc_event,
                          ::centreon_grpc::grpc_event>*
server::exchange(::grpc::CallbackServerContext*) {
  accepted_service::pointer serv;
  {
    unique_lock l(_protect);
    serv = std::make_shared<accepted_service>();
    _accepted.push(serv);
    _accept_cond.notify_one();
  }

  serv->start_read();
  return serv.get();
}

std::unique_ptr<io::stream> server::open() {
  std::unique_ptr<io::stream> ret;
  {
    unique_lock l(_protect);
    if (!_accepted.empty()) {
      ret = std::make_unique<stream>(_accepted.front());
      _accepted.pop();
    }
  }
  return ret;
}

std::unique_ptr<io::stream> server::open(
    const system_clock::time_point& dead_line) {
  std::unique_ptr<io::stream> ret;
  {
    unique_lock l(_protect);
    if (!_accepted.empty()) {
      ret = std::make_unique<stream>(_accepted.front());
      _accepted.pop();
    } else {
      _accept_cond.wait_until(l, dead_line,
                              [this]() { return !_accepted.empty(); });
      if (!_accepted.empty()) {
        ret = std::make_unique<stream>(_accepted.front());
        _accepted.pop();
      }
    }
  }
  return ret;
}

bool server::is_ready() const {
  unique_lock l(_protect);
  return !_accepted.empty();
}
