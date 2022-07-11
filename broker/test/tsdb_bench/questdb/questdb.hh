#ifndef __TSDB__BENCH__QUESTDB_HH
#define __TSDB__BENCH__QUESTDB_HH

#include "db/connection.hh"
#include "db/db_conf.hh"
#include "http_client/http_connection.hh"
#include "metric.hh"
#include "timescale/pg_connection.hh"

namespace questdb::ilp {
class line_sender;
};

namespace questdb {
class questdb_client : public std::enable_shared_from_this<questdb_client> {
  io_context_ptr _io_context;
  logger_ptr _logger;
  db_conf _conf;

  std::unique_ptr<questdb::ilp::line_sender> _sender;
  pg::pg_connection::pointer _pg_connection;

 public:
  using pointer = std::shared_ptr<questdb_client>;

  using connection_handler = connection::connection_handler;
  using select_handler = connection::connection_handler;

  questdb_client(const io_context_ptr& io_context,
                 const boost::json::object& conf,
                 const logger_ptr& logger);

  ~questdb_client();

  void connect_ingest();
  void connect_select(connection_handler callback);

  void send(metric::metric_cont_ptr& datas,
            unsigned offset,
            unsigned nb_to_send);

  void select(const time_point& begin,
              const time_point& end,
              uint64_t metric_id,
              unsigned nb_point,
              select_handler callback);
};

};  // namespace questdb

#endif
