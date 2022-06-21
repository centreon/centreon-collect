#include "libpq-fe.h"

#include "pg_connection.hh"
#include "pg_request.hh"

namespace pg {
namespace detail {
/**
 * @brief when you send several inserts or when you retreive several rows from a
 * select, you will receive several data packets  if you have activate one
 * packet by raw for example
 *
 */
class result_collector {
 public:
  using pg_result_vect = std::vector<PGresult*>;

 protected:
  PGconn* _conn;
  pg_result_vect _res;
  bool _error;
  logger_ptr _logger;

 public:
  using pointer = std::shared_ptr<result_collector>;

  result_collector(PGconn* conn, const logger_ptr logger)
      : _conn(conn), _error(false), _logger(logger) {}
  ~result_collector();

  using ret_type = std::pair<bool, PGresult*>;
  /**
   * @brief to call after the socket had received a packet
   *
   * @return true all data is received, request is finished
   * @return false some data is not received yet
   * @return PGResult * last result received
   */
  ret_type on_data_available();

  const pg_result_vect& get_result() const { return _res; }

  const PGresult* get_last_result() const;
  bool is_error() const { return _error; }
};

result_collector::~result_collector() {
  for (PGresult* toclear : _res) {
    PQclear(toclear);
  }
}

/**
 * @brief consume input on socket for the current request
 *  we may receive several status for one request
 * @return std::pair<bool, PGresult*> first = true if all is received, second =
 * received status
 */
std::pair<bool, PGresult*> result_collector::on_data_available() {
  int ok = PQconsumeInput(_conn);
  if (!ok) {
    _error = true;
  }
  while (!PQisBusy(_conn)) {
    PGresult* res = PQgetResult(_conn);
    if (res) {
      ExecStatusType st = PQresultStatus(res);
      SPDLOG_LOGGER_TRACE(_logger, "receive status {}", PQresStatus(st));
      if (st == PGRES_NONFATAL_ERROR || st == PGRES_FATAL_ERROR ||
          st == PGRES_PIPELINE_ABORTED) {
        _error = true;
      }
      _res.push_back(res);
      if (st == PGRES_COPY_IN) {  // special case of copy, first send request
                                  // and then send datas
        return std::make_pair(true, res);
      }
    } else {
      return std::make_pair(true, res);
    }
  }
  return std::make_pair(false, nullptr);
}

const PGresult* result_collector::get_last_result() const {
  if (!_res.empty()) {
    return *_res.rbegin();
  }
  return nullptr;
}

}  // namespace detail
}  // namespace pg

using namespace pg;

pg_connection::pg_connection(const io_context_ptr& io_context,
                             const boost::json::object& conf,
                             const logger_ptr& logger)
    : connection(io_context, conf, logger),
      _socket(*io_context),
      _conn(nullptr) {
  SPDLOG_LOGGER_INFO(_logger, "new pg_connection {}", *this);
}

pg_connection::pointer pg_connection::create(const io_context_ptr& io_context,
                                             const boost::json::object& conf,
                                             const logger_ptr& logger) {
  return pg_connection::pointer(new pg_connection(io_context, conf, logger));
}

pg_connection::~pg_connection() {
  SPDLOG_LOGGER_INFO(_logger, "delete pg_connection {}", *this);
  if (_conn) {
    PQfinish(_conn);
    _conn = nullptr;
  }
}

/**********************************************************************************************
 *       connection
 **********************************************************************************************/

static const std::unordered_set<std::string> _authorized_keys = {
    "port", "dbname", "user", "password"};

void pg_connection::connect(const time_point& until,
                            const connection_handler& handler) {
  {
    lock l(_protect);
    if (_state != e_state::not_connected) {
      SPDLOG_LOGGER_ERROR(_logger, "{} not bad_state {}", *this, _state);
      throw std::invalid_argument("pg_connection::connect bad state");
    }
    _state = e_state::connecting;
  }

  std::vector<const char*> keys, values;
  // has to look-up name?
  boost::system::error_code err;
  boost::asio::ip::make_address(_host, err);
  if (err) {  // name to look up
    keys.push_back("host");
    values.push_back(_host.c_str());
  } else {
    keys.push_back("hostaddr");
    values.push_back(_host.c_str());
  }
  // time out
  std::string time_out;
  time_point now = system_clock::now();
  if (now < until) {
    time_out = std::to_string(
        std::chrono::duration_cast<std::chrono::seconds>(until - now).count());
    keys.push_back("connect_timeout");
    values.push_back(time_out.c_str());
  }

  std::list<std::string> key_value_data;
  // pass other args to postgres
  for (const boost::json::key_value_pair args : _conf) {
    if (_authorized_keys.find(args.key()) != _authorized_keys.end()) {
      const boost::json::string* val = args.value().if_string();
      if (val) {
        key_value_data.emplace_back(args.key().data());
        keys.push_back(key_value_data.rbegin()->c_str());
        key_value_data.emplace_back(val->c_str());
        values.push_back(key_value_data.rbegin()->c_str());
      } else {
        const int64_t* ival = args.value().if_int64();
        if (ival) {
          key_value_data.emplace_back(args.key().data());
          keys.push_back(key_value_data.rbegin()->c_str());
          key_value_data.emplace_back(std::to_string(*ival));
          values.push_back(key_value_data.rbegin()->c_str());
        } else {
          SPDLOG_LOGGER_ERROR(_logger, "{} is not a string", args.key());
        }
      }
    }
  }

  keys.push_back(nullptr);
  values.push_back(nullptr);

  _conn = PQconnectStartParams(keys.data(), values.data(), 0);

  if (!_conn) {
    SPDLOG_LOGGER_ERROR(
        _logger, "PQconnectStartParams fails to create conn obj for {}", *this);
    handler(std::make_error_code(std::errc::not_enough_memory),
            "PQconnectStartParams fail");
    return;
  }

  ConnStatusType status = PQstatus(_conn);
  if (status == CONNECTION_BAD) {
    SPDLOG_LOGGER_ERROR(
        _logger, "PQconnectStartParams fails to parse args for {}: {}, err:{}",
        *this, _conf, PQerrorMessage(_conn));
    handler(std::make_error_code(std::errc::invalid_argument),
            "PQconnectStartParams fail to parse args " +
                std::string(PQerrorMessage(_conn)));
    return;
  }
  connect_poll(handler);
}

void pg_connection::connect_poll(const connection_handler& handler) {
  PostgresPollingStatusType poll_status = PQconnectPoll(_conn);
  if (_socket.native_handle() != PQsocket(_conn)) {
    _socket.assign(boost::asio::ip::tcp::v4(), PQsocket(_conn));
  }
  switch (poll_status) {
    case PGRES_POLLING_WRITING:
      _socket.async_wait(socket_type::wait_write,
                         [me = shared_from_this(),
                          _handler = handler](const std::error_code& err) {
                           me->connect_handler(err, _handler);
                         });
      break;
    case PGRES_POLLING_READING:
      _socket.async_wait(socket_type::wait_read,
                         [me = shared_from_this(),
                          _handler = handler](const std::error_code& err) {
                           me->connect_handler(err, _handler);
                         });
      break;
    case PGRES_POLLING_OK:
      SPDLOG_LOGGER_INFO(_logger, "connected {}", *this);
      {
        lock l(_protect);
        _state = e_state::idle;
      }
      PQsetnonblocking(_conn, 1);
      handler({}, {});
      break;
    default:
      SPDLOG_LOGGER_ERROR(_logger, "protocol error for {}: {}", *this,
                          PQerrorMessage(_conn));
      {
        std::error_code err =
            std::make_error_code(std::errc::network_unreachable);
        std::string detail =
            "PQconnectPoll fail" + std::string(PQerrorMessage(_conn));
        on_error(err, detail);
        handler(err, detail);
      }
      break;
  }
}

void pg_connection::connect_handler(const std::error_code& err,
                                    const connection_handler& handler) {
  if (err) {
    SPDLOG_LOGGER_ERROR(_logger, "network error for {}: {}", *this,
                        err.message());
    handler(err, "fail to connect");
  } else {
    connect_poll(handler);
  }
}

/**********************************************************************************************
 *       send query
 **********************************************************************************************/

void pg_connection::execute() {
  request_base::pointer to_execute;
  {
    lock l(_protect);
    if (_state != e_state::idle || _request.empty()) {
      return;
    }
    to_execute = _request.front();
    _request.pop();
    _state = e_state::busy;
  }
  switch (to_execute->get_type()) {
    case request_base::e_request_type::simple_no_result_request:
    case request_base::e_request_type::statement_request:
      send_no_result_request(to_execute, false);
      break;
    case request_base::e_request_type::load_request:
      send_no_result_request(to_execute, true);
      break;
    default:
      SPDLOG_LOGGER_ERROR(_logger, "unsupported request {}", *to_execute);
      _io_context->post([to_execute]() {
        std::error_code err = std::make_error_code(std::errc::not_supported);
        std::ostringstream detail;
        detail << "unsupported request:" << *to_execute;
        to_execute->call_callback(err, detail.str());
      });
  }
}

void pg_connection::send_no_result_request(const request_base::pointer& req,
                                           bool have_to_send_copy_data) {
  SPDLOG_LOGGER_DEBUG(_logger, "send request: {}", *req);
  int res = std::static_pointer_cast<no_result_request>(req)->send_query(*this);

  if (!res) {
    std::string err_detail =
        fmt::format("{} send_no_result_request, fail to send query: {}", *this,
                    PQerrorMessage(_conn));
    SPDLOG_LOGGER_ERROR(_logger, "{}", err_detail);
    _io_context->post([req, err_detail]() {
      req->call_callback(std::make_error_code(std::errc::protocol_error),
                         err_detail);
    });
  }
  send_request_data(req, have_to_send_copy_data);
}

/**
 * @brief this async function send data over the socket
 * and then call start_read_result to receive response from the server
 *
 * @param req
 */
void pg_connection::send_request_data(const request_base::pointer& req,
                                      bool have_to_send_copy_data) {
  int data_to_send = PQflush(_conn);
  if (data_to_send < 0) {  // failure
    std::error_code err = std::make_error_code(std::errc::protocol_error);
    std::string err_detail = fmt::format("{} fail to send {}: {}", *this, *req,
                                         PQerrorMessage(_conn));
    on_error(err, err_detail);
    req->call_callback(err, err_detail);
  } else if (data_to_send > 0) {
    _socket.async_wait(socket_type::wait_write,
                       [me = shared_from_this(), req,
                        have_to_send_copy_data](const std::error_code& err) {
                         if (err) {
                           me->on_error(err, "fail to write socket");
                           req->call_callback(err, "fail to write socket");
                         } else {
                           me->send_request_data(req, have_to_send_copy_data);
                         }
                       });
  } else {
    start_read_result(req, have_to_send_copy_data);
  }
}

void pg_connection::start_read_result(const request_base::pointer& req,
                                      bool have_to_send_copy_data) {
  detail::result_collector::pointer result =
      std::make_shared<detail::result_collector>(_conn, _logger);
  if (have_to_send_copy_data) {
    start_first_copy_in_read_result(req, result);
  } else {
    start_read_result(req, result);
  }
}

/**
 * @brief after intitiated a async request we must receive until PQgetResult
 * return null
 *
 * @param req current request
 * @param coll PGresult collector
 */
void pg_connection::start_read_result(
    const request_base::pointer& req,
    const std::shared_ptr<detail::result_collector>& coll) {
  _socket.async_wait(
      socket_type::wait_read,
      [me = shared_from_this(), req, coll](const std::error_code& err) {
        if (err) {
          SPDLOG_LOGGER_ERROR(me->_logger, "{} read error: {}", *me,
                              err.message());
          me->on_error(err, "fail to read socket");
          req->call_callback(err, "fail to read socket");
          return;
        }
        detail::result_collector::ret_type ended = coll->on_data_available();
        if (ended.first) {
          me->completion_handler(req, coll);
        } else {  // not yet all received => we continue
          me->start_read_result(req, coll);
        }
      });
}

/**
 * @brief after intitiated a async request we must receive an  PQgetResult
 * that will give us informations about copy and that will let us send data
 *
 * @param req current request
 * @param coll PGresult collector
 */
void pg_connection::start_first_copy_in_read_result(
    const request_base::pointer& req,
    const std::shared_ptr<detail::result_collector>& coll) {
  _socket.async_wait(
      socket_type::wait_read,
      [me = shared_from_this(), req, coll, this](const std::error_code& err) {
        if (err) {
          SPDLOG_LOGGER_ERROR(_logger, "{} read error: {}", *me, err.message());
          on_error(err, "fail to read socket");
          req->call_callback(err, "fail to read socket");
          return;
        }
        detail::result_collector::ret_type ended = coll->on_data_available();
        if (ended.first) {
          const PGresult* lastres = coll->get_last_result();
          if (lastres && PQresultStatus(lastres) == PGRES_COPY_IN) {
            SPDLOG_LOGGER_DEBUG(_logger, "copy in data received: {}", *req);
            std::pair<bool, std::string> res =
                std::static_pointer_cast<pg_load_request>(req)->start_send_data(
                    lastres, me->_conn);
            if (!res.first) {
              SPDLOG_LOGGER_ERROR(_logger, "pb with copy first datas {} for {}",
                                  res.second, req);
              std::error_code err =
                  std::make_error_code(std::errc::invalid_argument);
              on_error(err, res.second);
              req->call_callback(err, res.second);
            } else {  // send datas on wire and wait for request result
              send_request_data(req, false);
            }
          } else {
            std::string detail =
                lastres
                    ? fmt::format("unexpected status received: {}:{} for {}",
                                  PQresStatus(PQresultStatus(lastres)),
                                  PQresultErrorMessage(lastres), *req)
                    : fmt::format("no status received for {}", *req);
            SPDLOG_LOGGER_ERROR(me->_logger, detail);
            std::error_code err =
                std::make_error_code(std::errc::operation_not_supported);
            on_error(err, detail);
            req->call_callback(err, detail);
          }
        } else {
          start_first_copy_in_read_result(req, coll);
        }
      });
}

/**
 * @brief called when request is finished
 *
 * @param req request that contains the callback to call
 * @param coll result of the request
 */
void pg_connection::completion_handler(
    const request_base::pointer& req,
    const std::shared_ptr<detail::result_collector>& coll) {
  {
    lock l(_protect);
    if (_state == e_state::busy) {
      _state = e_state::idle;
    }
  }
  if (coll->get_result().empty()) {
    req->call_callback({}, {});
  } else {
    if (!coll->is_error()) {
      SPDLOG_LOGGER_DEBUG(_logger, "request completed: {}", *req);
      req->call_callback({}, {});
    } else {
      const PGresult* res = coll->get_last_result();
      ExecStatusType st = PQresultStatus(res);
      SPDLOG_LOGGER_ERROR(_logger, "{} fail to execute {}: {} {}", *this, *req,
                          PQresStatus(st), PQresultErrorMessage(res));
      req->call_callback(std::make_error_code(std::errc::invalid_argument),
                         PQresultErrorMessage(res));
    }
  }
}
