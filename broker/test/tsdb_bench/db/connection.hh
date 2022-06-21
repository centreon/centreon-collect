#ifndef __TSDB__BENCH__CONNECTION_HH
#define __TSDB__BENCH__CONNECTION_HH

#include "db_conf.hh"
#include "request.hh"

class connection : public std::enable_shared_from_this<connection>,
                   public db_conf {
 public:
  enum class e_state { not_connected, connecting, idle, busy, error };

 protected:
  io_context_ptr _io_context;
  logger_ptr _logger;

  volatile e_state _state;
  using request_queue = std::queue<request_base::pointer>;
  request_queue _request;
  mutable std::mutex _protect;

  using lock = std::lock_guard<std::mutex>;

  /**
   * @brief used to inform the connection object of a new request
   * call is done with _protect released
   */
  virtual void execute() = 0;

  /**
   * @brief call all enqueued request handler
   * _protect must be released before call
   *
   * @param err
   * @param err_detail
   */
  virtual void on_error(const std::error_code& err,
                        const std::string& err_detail);

 public:
  using pointer = std::shared_ptr<connection>;
  using connection_handler =
      std::function<void(const std::error_code&,
                         const std::string& /*detail of error*/)>;

  connection(const connection&) = delete;
  connection& operator=(const connection&) = delete;

  connection(const io_context_ptr& io_context,
             const boost::json::object& conf,
             const logger_ptr& logger);

  virtual ~connection();

  logger_ptr get_logger() const { return _logger; }

  e_state get_state() const { return _state; }

  virtual void dump(std::ostream&) const;

  virtual void connect(const time_point& until,
                       const connection_handler& handler) = 0;

  virtual void start_request(const request_base::pointer& req);
};

std::ostream& operator<<(std::ostream& s, const connection& to_dump);
std::ostream& operator<<(std::ostream&, const connection::e_state state);
#endif