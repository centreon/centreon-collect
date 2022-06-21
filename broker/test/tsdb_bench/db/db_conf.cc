#include "db_conf.hh"

db_conf::db_conf(const boost::json::object& conf, const logger_ptr& logger)
    : _port(0), _conf(conf) {
  const boost::json::value* val = conf.if_contains("name");
  if (val) {
    _name = val->as_string().c_str();
  }

  val = conf.if_contains("host");
  if (!val) {
    SPDLOG_LOGGER_ERROR(logger, "config: host not found for {}", _name);
    throw std::invalid_argument("config: host not found for " + _name);
  }
  _host = val->as_string().c_str();

  val = conf.if_contains("port");
  if (val) {
    try {
      _port = val->as_int64();
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(logger, "fail to parse port for {}: {}", _name,
                          e.what());
      throw;
    }
  }

  val = conf.if_contains("user");
  if (!val) {
    SPDLOG_LOGGER_ERROR(logger, "config: user not found for {}", _name);
    throw std::invalid_argument("config: user not found for " + _name);
  }
  _user = val->as_string().c_str();

  val = conf.if_contains("password");
  if (!val) {
    SPDLOG_LOGGER_ERROR(logger, "config: password not found for {}", _name);
    throw std::invalid_argument("config: password not found for " + _name);
  }
  _password = val->as_string().c_str();
}