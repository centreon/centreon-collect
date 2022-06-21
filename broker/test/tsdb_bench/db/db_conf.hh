#ifndef __TSDB__BENCH__DB_CONF_HH
#define __TSDB__BENCH__DB_CONF_HH

class db_conf {
 protected:
  std::string _name;
  std::string _host;
  u_int16_t _port;
  std::string _user;
  std::string _password;
  boost::json::object _conf;

 public:
  db_conf(const boost::json::object& conf, const logger_ptr& logger);

  constexpr const std::string& get_name() const { return _name; }
  constexpr const std::string& get_host() const { return _host; }
  constexpr const u_int16_t& get_port() const { return _port; }
  constexpr const std::string& get_user() const { return _user; }
  constexpr const std::string& get_password() const { return _password; }
};

#endif
