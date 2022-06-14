#ifndef __TSDB__BENCH__PG_CONNECTION_HH
#define __TSDB__BENCH__PG_CONNECTION_HH

#include "connection.hh"

typedef struct pg_conn PGconn;

namespace pg {
namespace detail {
class result_collector;
};
class pg_connection : public ::connection {
  using socket_type = asio::ip::tcp::socket;
  socket_type _socket;
  PGconn* _conn;

  void connect_handler(const std::error_code& err,
                       const connection_handler& handler);

  void connect_poll(const connection_handler& handler);

 protected:
  pg_connection(const io_context_ptr& io_context,
                const boost::json::object& conf,
                const logger_ptr& logger);

  PGconn* get_conn() { return _conn; }

  void execute() override;

  void send_no_result_request(const request_base::pointer& req,
                              bool have_to_send_copy_data);

  void send_request_data(const request_base::pointer& req,
                         bool have_to_send_copy_data);

  void start_read_result(const request_base::pointer& req,
                         bool have_to_send_copy_data);

  void start_read_result(const request_base::pointer& req,
                         const std::shared_ptr<detail::result_collector>& coll);

  void start_first_copy_in_read_result(
      const request_base::pointer& req,
      const std::shared_ptr<detail::result_collector>& coll);

  void completion_handler(
      const request_base::pointer& req,
      const std::shared_ptr<detail::result_collector>& coll);

  friend class pg_no_result_request;
  friend class pg_load_request;
  friend class pg_no_result_statement_request;

 public:
  using pointer = std::shared_ptr<pg_connection>;

  static pointer create(const io_context_ptr& io_context,
                        const boost::json::object& conf,
                        const logger_ptr& logger);
  ~pg_connection();

  pointer shared_from_this() {
    return std::static_pointer_cast<pg_connection>(
        ::connection::shared_from_this());
  }

  void connect(const time_point& until,
               const connection_handler& handler) override;
};

}  // namespace pg

#endif
