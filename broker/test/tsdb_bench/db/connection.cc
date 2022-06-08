#include "connection.hh"

connection::connection(const io_context_ptr& io_context,
                       const boost::json::object& conf,
                       const logger_ptr& logger)
    : _io_context(io_context),
      _logger(logger),
      _port(0),
      _conf(conf),
      _state(e_state::not_connected) {
  const boost::json::value* val = conf.if_contains("name");
  if (val) {
    _name = val->as_string().c_str();
  }

  val = conf.if_contains("host");
  if (!val) {
    SPDLOG_LOGGER_ERROR(_logger, "config: host not found for {}", _name);
    throw std::invalid_argument("config: host not found for " + _name);
  }
  _host = val->as_string().c_str();

  val = conf.if_contains("port");
  if (val) {
    try {
      _port = val->as_int64();
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(_logger, "fail to parse port for {}: {}", _name,
                          e.what());
      throw;
    }
  }

  val = conf.if_contains("user");
  if (!val) {
    SPDLOG_LOGGER_ERROR(_logger, "config: user not found for {}", _name);
    throw std::invalid_argument("config: user not found for " + _name);
  }
  _user = val->as_string().c_str();

  val = conf.if_contains("password");
  if (!val) {
    SPDLOG_LOGGER_ERROR(_logger, "config: password not found for {}", _name);
    throw std::invalid_argument("config: password not found for " + _name);
  }
  _password = val->as_string().c_str();

  SPDLOG_LOGGER_INFO(_logger, "create connection {}", *this);
}

connection::~connection() {
  SPDLOG_LOGGER_INFO(_logger, "delete connection {}", *this);
}

void connection::dump(std::ostream& s) const {
  s << "this:" << this << " name:" << _name << " host:" << _host
    << " port:" << _port << " user:" << _user;
}

void connection::start_request(const request_base::pointer& req) {
  {
    lock l(_protect);
    if (_state != e_state::idle && _state != e_state::busy) {
      SPDLOG_LOGGER_ERROR(_logger, "{} not idle or busy: {}", *this, _state);
      throw std::invalid_argument("connection not idle or busy");
    }
    _request.push(req);
  }
  execute();
}

void connection::on_error(const std::error_code& err,
                          const std::string& err_detail) {
  request_queue to_call;
  {
    lock l(_protect);
    _state = e_state::error;
    to_call = std::move(_request);
  }

  while (!to_call.empty()) {
    request_base::pointer req = to_call.front();
    to_call.pop();
    req->call_callback(err, err_detail);
  }
}

std::ostream& operator<<(std::ostream& s, const connection& to_dump) {
  (&to_dump)->dump(s);
  return s;
}

#define CASE_STATE_STR(val)      \
  case connection::e_state::val: \
    s << #val;                   \
    break;

std::ostream& operator<<(std::ostream& s, const connection::e_state state) {
  switch (state) {
    CASE_STATE_STR(not_connected);
    CASE_STATE_STR(connecting);
    CASE_STATE_STR(idle);
    CASE_STATE_STR(busy);
    CASE_STATE_STR(error);
  }
  return s;
}
