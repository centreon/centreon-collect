#include <boost/json/src.hpp>
#include "precomp_inc.hh"

#include "db/connection.hh"
#include "influxdb/inf_http_client.hh"
#include "m3/m3_http_client.hh"
#include "metric.hh"
#include "mimir/mimir_http_client.hh"
#include "pg_metric.hh"
#include "prometheus/pr_http_server.hh"
#include "questdb/questdb.hh"
#include "timescale/pg_connection.hh"
#include "timescale/pg_request.hh"
#include "warp10/warp10_http_client.hh"

namespace po = boost::program_options;

using connection_cont = std::vector<connection::pointer>;

/******************************************************************************************
 *
 *          timescale
 *
 ******************************************************************************************/
void copy_to_timescale(const metric::metric_cont_ptr& to_insert,
                       const connection::pointer& conn,
                       unsigned bulk_size,
                       unsigned offset,
                       unsigned end_offset) {
  constexpr std::array<request_base::e_column_type, 6> cols = {
      request_base::e_column_type::timestamp_c,
      request_base::e_column_type::int64_c,
      request_base::e_column_type::int64_c,
      request_base::e_column_type::int64_c,
      request_base::e_column_type::int64_c,
      request_base::e_column_type::double_c,
  };

  unsigned end = offset + bulk_size;
  if (end > end_offset) {
    end = end_offset;
  }
  if (end == offset) {
    return;
  }
  auto req = std::make_shared<pg::pg_load_request>(
      "COPY metrics FROM STDIN WITH BINARY", ' ', end - offset, cols.begin(),
      cols.end(),
      [to_insert, conn, bulk_size, end, end_offset](
          const std::error_code& err, const std::string& detail,
          const request_base::pointer& req) {
        if (err) {
          SPDLOG_LOGGER_ERROR(conn->get_logger(),
                              "copy_to_timescale fail to copy data {}, {}",
                              err.message(), detail);
          return;
        }
        copy_to_timescale(to_insert, conn, bulk_size, end, end_offset);
      });

  for (; offset < end; ++offset) {
    *req << (*to_insert)[offset];
  }
  conn->start_request(req);
}

void bench_timescale(const io_context_ptr& io_context,
                     logger_ptr logger,
                     const metric::metric_cont_ptr& to_insert,
                     const boost::json::object& db_conf,
                     unsigned bulk_size,
                     unsigned nb_conn) {
  unsigned nb_metric_by_conn = to_insert->size() / nb_conn;
  unsigned offset = 0;
  for (; nb_conn; --nb_conn, offset += nb_metric_by_conn) {
    connection::pointer conn =
        pg::pg_connection::create(io_context, db_conf, logger);
    conn->connect(
        system_clock::now() + std::chrono::seconds(30),
        [conn, to_insert, bulk_size, offset, nb_metric_by_conn, logger](
            const std::error_code& err, const std::string& err_detail) {
          if (!err) {
            copy_to_timescale(to_insert, conn, bulk_size, offset,
                              offset + nb_metric_by_conn);
            // conn->start_request(std::make_shared<pg::pg_no_result_request>(
            //     "insert into rides (vendor_id, pickup_datetime, "
            //     "dropoff_datetime) values('55','2022-06-13 "
            //     "07:56:21','2022-06-13 "
            //     "07:59:21')",
            //     [](const std::error_code&, const std::string&,
            //        const request_base::pointer&) {}));
            // auto req =
            // std::make_shared<pg::pg_no_result_statement_request>(
            //     "",
            //     "insert into rides (vendor_id, pickup_datetime, "
            //     "dropoff_datetime) values($1,$2,$3)",
            //     cols.begin(), cols.end(),
            //     [](const std::error_code&, const std::string&,
            //        const request_base::pointer&) {});
          } else {
            SPDLOG_LOGGER_ERROR(logger, " fail to connect to database :{}:{}",
                                err, err_detail);
            return;
          }
        });
  }
}

void select_timescale(const io_context_ptr& io_context,
                      logger_ptr logger,
                      const metric_conf& conf,
                      const boost::json::object& db_conf) {
  unsigned nb_second = std::chrono::duration_cast<std::chrono::seconds>(
                           conf.select_end - conf.select_begin)
                           .count();

  std::ostringstream request;
  if (nb_second / 3600 >= conf.nb_point) {  // less than one point by hour
    request << "SELECT bucket, avg FROM avg_hour_metric WHERE bucket BETWEEN '"
            << date::format("%F %T", conf.select_begin) << "' AND '"
            << date::format("%F %T", conf.select_end) << '\'';
  } else if (nb_second / 60 >=
             conf.nb_point) {  // less than one point by minute
    request
        << "SELECT bucket, avg FROM avg_minute_metric WHERE bucket BETWEEN '"
        << date::format("%F %T", conf.select_begin) << "' AND '"
        << date::format("%F %T", conf.select_end) << '\'';

  } else {  // use metrics table
    request << "SELECT t, int_val + double_val FROM metrics where t BETWEEN '"
            << date::format("%F %T", conf.select_begin) << "' AND '"
            << date::format("%F %T", conf.select_end) << '\'';
  }
  connection::pointer conn =
      pg::pg_connection::create(io_context, db_conf, logger);
  conn->connect(
      system_clock::now() + std::chrono::seconds(30),
      [conn, sz_request = request.str(), logger](
          const std::error_code& err, const std::string& err_detail) {
        if (!err) {
          constexpr std::array<request_base::e_column_type, 0> cols = {};
          pg::pg_no_result_statement_request::pointer req =
              std::make_shared<pg::pg_no_result_statement_request>(
                  "", sz_request, cols.begin(), cols.end(),
                  [logger](const std::error_code& err,
                           const std::string& detail,
                           const request_base::pointer& req) {
                    if (err) {
                      SPDLOG_LOGGER_ERROR(logger, " fail to execute {} : {}:{}",
                                          *req, err, detail);
                    } else {
                      SPDLOG_LOGGER_INFO(logger, " end of {}", *req);
                    }
                  });
          conn->start_request(req);
        } else {
          SPDLOG_LOGGER_ERROR(logger, " fail to connect to database :{}:{}",
                              err, err_detail);
        }
      });
}
/******************************************************************************************
 *
 *          prometheus
 *
 ******************************************************************************************/

void bench_prometheus(const io_context_ptr& io_context,
                      const logger_ptr& logger,
                      const metric::metric_cont_ptr& to_insert,
                      const boost::json::object& db_conf,
                      unsigned bulk_size,
                      unsigned nb_conn) {
  prometheus::http_server::pointer prom_server =
      std::make_shared<prometheus::http_server>(io_context, db_conf, logger,
                                                bulk_size);
  prom_server->push(to_insert->begin(), to_insert->end());
  prom_server->start();
}

/******************************************************************************************
 *
 *          influxdb
 *
 ******************************************************************************************/

void send_to_influx_db(const influxdb::inf_client::pointer& client,
                       metric::metric_cont_ptr to_insert,
                       unsigned bulk_size,
                       metric::metric_cont::const_iterator iter) {
  unsigned reminder = std::distance(iter, to_insert->cend());
  if (reminder <= bulk_size) {
    metric::metric_cont::const_iterator end = iter;
    std::advance(end, reminder);
    client->send(iter, end,
                 [client](const std::error_code&, const std::string&,
                          const http_client::connection::response_ptr&) {

                 });
  } else {
    metric::metric_cont::const_iterator end = iter;
    std::advance(end, bulk_size);
    client->send(iter, end,
                 [client, to_insert, bulk_size, end](
                     const std::error_code& err, const std::string&,
                     const http_client::connection::response_ptr& response) {
                   if (err) {
                     return;
                   }
                   if (response->keep_alive()) {
                     send_to_influx_db(client, to_insert, bulk_size, end);
                   } else {
                   }
                 });
  }
}

void bench_influxdb(const io_context_ptr& io_context,
                    const logger_ptr& logger,
                    metric::metric_cont_ptr to_insert,
                    const boost::json::object& db_conf,
                    unsigned bulk_size,
                    unsigned nb_conn,
                    metric::metric_cont::const_iterator iter) {
  influxdb::inf_client::pointer client =
      std::make_shared<influxdb::inf_client>(io_context, logger, db_conf);
  client->connect(
      [client, to_insert, bulk_size, iter](const boost::beast::error_code& err,
                                           const std::string& detail) {
        send_to_influx_db(client, to_insert, bulk_size, iter);
      });
}

void bench_influxdb(const io_context_ptr& io_context,
                    const logger_ptr& logger,
                    metric::metric_cont_ptr to_insert,
                    const boost::json::object& db_conf,
                    unsigned bulk_size,
                    unsigned nb_conn) {
  bench_influxdb(io_context, logger, to_insert, db_conf, bulk_size, nb_conn,
                 to_insert->begin());
}

void select_influxdb(const io_context_ptr& io_context,
                     logger_ptr logger,
                     const metric_conf& conf,
                     const boost::json::object& db_conf) {
  influxdb::inf_client::pointer client =
      std::make_shared<influxdb::inf_client>(io_context, logger, db_conf);
  client->connect(
      [client, copy_conf = conf](const boost::beast::error_code& err,
                                 const std::string& detail) {
        client->select(
            copy_conf.select_begin, copy_conf.select_end, copy_conf.metric_id,
            copy_conf.nb_point,
            [](const boost::beast::error_code& err, const std::string& detail,
               const http_client::connection::response_ptr& response) {});
      });
}

/******************************************************************************************
 *
 *          warp10
 *
 ******************************************************************************************/

void send_to_warp10_db(const warp10::warp10_client::pointer& client,
                       metric::metric_cont_ptr to_insert,
                       unsigned bulk_size,
                       metric::metric_cont::const_iterator iter) {
  unsigned reminder = std::distance(iter, to_insert->cend());
  if (reminder <= bulk_size) {
    metric::metric_cont::const_iterator end = iter;
    std::advance(end, reminder);
    client->send(iter, end,
                 [client](const std::error_code&, const std::string&,
                          const http_client::connection::response_ptr&) {

                 });
  } else {
    metric::metric_cont::const_iterator end = iter;
    std::advance(end, bulk_size);
    client->send(iter, end,
                 [client, to_insert, bulk_size, end](
                     const std::error_code& err, const std::string&,
                     const http_client::connection::response_ptr& response) {
                   if (err) {
                     return;
                   }
                   if (response->keep_alive()) {
                     send_to_warp10_db(client, to_insert, bulk_size, end);
                   } else {
                   }
                 });
  }
}

void bench_warp10(const io_context_ptr& io_context,
                  const logger_ptr& logger,
                  metric::metric_cont_ptr to_insert,
                  const boost::json::object& db_conf,
                  unsigned bulk_size,
                  unsigned nb_conn,
                  metric::metric_cont::const_iterator iter) {
  warp10::warp10_client::pointer client =
      std::make_shared<warp10::warp10_client>(io_context, logger, db_conf);
  client->connect(
      [client, to_insert, bulk_size, iter](const boost::beast::error_code& err,
                                           const std::string& detail) {
        send_to_warp10_db(client, to_insert, bulk_size, iter);
      });
}

void bench_warp10(const io_context_ptr& io_context,
                  const logger_ptr& logger,
                  metric::metric_cont_ptr to_insert,
                  const boost::json::object& db_conf,
                  unsigned bulk_size,
                  unsigned nb_conn) {
  bench_warp10(io_context, logger, to_insert, db_conf, bulk_size, nb_conn,
               to_insert->begin());
}

void select_warp10(const io_context_ptr& io_context,
                   logger_ptr logger,
                   const metric_conf& conf,
                   const boost::json::object& db_conf) {
  warp10::warp10_client::pointer client =
      std::make_shared<warp10::warp10_client>(io_context, logger, db_conf);
  client->connect(
      [client, copy_conf = conf](const boost::beast::error_code& err,
                                 const std::string& detail) {
        client->select(
            copy_conf.select_begin, copy_conf.select_end, copy_conf.metric_id,
            copy_conf.nb_point,
            [](const boost::beast::error_code& err, const std::string& detail,
               const http_client::connection::response_ptr& response) {});
      });
}

/******************************************************************************************
 *
 *          m3db
 *
 ******************************************************************************************/

void send_to_m3db_db(const m3db::m3_client::pointer& client,
                     metric::metric_cont_ptr to_insert,
                     unsigned bulk_size,
                     metric::metric_cont::const_iterator iter) {
  unsigned reminder = std::distance(iter, to_insert->cend());
  if (reminder <= bulk_size) {
    metric::metric_cont::const_iterator end = iter;
    std::advance(end, reminder);
    client->send(iter, end,
                 [client](const std::error_code&, const std::string&,
                          const http_client::connection::response_ptr&) {

                 });
  } else {
    metric::metric_cont::const_iterator end = iter;
    std::advance(end, bulk_size);
    client->send(iter, end,
                 [client, to_insert, bulk_size, end](
                     const std::error_code& err, const std::string&,
                     const http_client::connection::response_ptr& response) {
                   if (err) {
                     return;
                   }
                   if (response->keep_alive()) {
                     send_to_m3db_db(client, to_insert, bulk_size, end);
                   } else {
                   }
                 });
  }
}

void bench_m3db(const io_context_ptr& io_context,
                const logger_ptr& logger,
                metric::metric_cont_ptr to_insert,
                const boost::json::object& db_conf,
                unsigned bulk_size,
                unsigned nb_conn,
                metric::metric_cont::const_iterator iter) {
  m3db::m3_client::pointer client =
      std::make_shared<m3db::m3_client>(io_context, logger, db_conf);
  client->connect(
      [client, to_insert, bulk_size, iter](const boost::beast::error_code& err,
                                           const std::string& detail) {
        send_to_m3db_db(client, to_insert, bulk_size, iter);
      });
}

void bench_m3db(const io_context_ptr& io_context,
                const logger_ptr& logger,
                metric::metric_cont_ptr to_insert,
                const boost::json::object& db_conf,
                unsigned bulk_size,
                unsigned nb_conn) {
  bench_m3db(io_context, logger, to_insert, db_conf, bulk_size, nb_conn,
             to_insert->begin());
}

/******************************************************************************************
 *
 *          mimir
 *
 ******************************************************************************************/

void send_to_mimir_db(const mimir::mimir_client::pointer& client,
                      metric::metric_cont_ptr to_insert,
                      unsigned bulk_size,
                      metric::metric_cont::const_iterator iter) {
  unsigned reminder = std::distance(iter, to_insert->cend());
  if (reminder <= bulk_size) {
    metric::metric_cont::const_iterator end = iter;
    std::advance(end, reminder);
    client->send(iter, end,
                 [client](const std::error_code&, const std::string&,
                          const http_client::connection::response_ptr&) {

                 });
  } else {
    metric::metric_cont::const_iterator end = iter;
    std::advance(end, bulk_size);
    client->send(iter, end,
                 [client, to_insert, bulk_size, end](
                     const std::error_code& err, const std::string&,
                     const http_client::connection::response_ptr& response) {
                   if (err) {
                     return;
                   }
                   if (response->keep_alive()) {
                     send_to_mimir_db(client, to_insert, bulk_size, end);
                   } else {
                   }
                 });
  }
}

void bench_mimir(const io_context_ptr& io_context,
                 const logger_ptr& logger,
                 metric::metric_cont_ptr to_insert,
                 const boost::json::object& db_conf,
                 unsigned bulk_size,
                 unsigned nb_conn,
                 metric::metric_cont::const_iterator iter) {
  mimir::mimir_client::pointer client =
      std::make_shared<mimir::mimir_client>(io_context, logger, db_conf);
  client->connect(
      [client, to_insert, bulk_size, iter](const boost::beast::error_code& err,
                                           const std::string& detail) {
        send_to_mimir_db(client, to_insert, bulk_size, iter);
      });
}

void bench_mimir(const io_context_ptr& io_context,
                 const logger_ptr& logger,
                 metric::metric_cont_ptr to_insert,
                 const boost::json::object& db_conf,
                 unsigned bulk_size,
                 unsigned nb_conn) {
  bench_mimir(io_context, logger, to_insert, db_conf, bulk_size, nb_conn,
              to_insert->begin());
}

/******************************************************************************************
 *
 *          questdb
 *
 ******************************************************************************************/
void bench_questdb(const io_context_ptr& io_context,
                   const logger_ptr& logger,
                   metric::metric_cont_ptr to_insert,
                   const boost::json::object& db_conf,
                   unsigned bulk_size,
                   unsigned nb_conn) {
  questdb::questdb_client client(io_context, db_conf, logger);
  client.connect_ingest();
  for (unsigned offset = 0; offset < to_insert->size(); offset += bulk_size) {
    client.send(to_insert, offset, bulk_size);
  }
}

void select_questdb(const io_context_ptr& io_context,
                    logger_ptr logger,
                    const metric_conf& conf,
                    const boost::json::object& db_conf) {
  questdb::questdb_client::pointer conn =
      std::make_shared<questdb::questdb_client>(io_context, db_conf, logger);
  conn->connect_select([conn, logger, conf](const std::error_code& err,
                                            const std::string& detail) {
    if (err) {
      SPDLOG_LOGGER_ERROR(logger, "fail to connect to questd database: {}:{}",
                          err.message(), detail);
      return;
    }
    conn->select(
        conf.select_begin, conf.select_end, conf.metric_id, conf.nb_point,
        [conn, logger](const std::error_code& err, const std::string& detail) {
          if (err) {
            SPDLOG_LOGGER_ERROR(logger,
                                "fail to select from questd database: {}:{}",
                                err.message(), detail);
          } else {
            SPDLOG_LOGGER_INFO(logger, "select ok from questd database");
          }
        });
  });
}

/******************************************************************************************
 *
 *          main
 *
 ******************************************************************************************/
int main(int argc, char** argv) {
  auto stderr_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
  logger_ptr logger =
      std::make_shared<spdlog::logger>("tsdb_bench", stderr_sink);
  spdlog::register_logger(logger);

  io_context_ptr io_context(std::make_shared<boost::asio::io_context>());

  po::options_description desc("Allowed options");
  desc.add_options()("help", "produce help message")(
      "metric-nb", po::value<uint>()->default_value(100), "nb to insert")(
      "metric-id-nb", po::value<uint>()->default_value(20), "nb metric id")(
      "nb-host", po::value<uint>()->default_value(100), "nb host")(
      "nb-service-by-host", po::value<uint>()->default_value(100),
      "nb service by host")("bulk-size", po::value<uint>()->default_value(100),
                            "bulk size")(
      "log-level", po::value<std::string>()->default_value("trace"),
      "logger level as trace, debug, info, warning, error, critical, off")(
      "db-conf", po::value<std::string>(),
      "path to the file that content informations used to connect to db")(
      "nb-conn", po::value<unsigned>()->default_value(1),
      "number of simultaneous connections to database (only valid for "
      "timescale insert)")("float-percent",
                           po::value<unsigned>()->default_value(25),
                           "percent of float metrics")(
      "double-percent", po::value<unsigned>()->default_value(25),
      "percent of double metrics")(
      "int64-percent", po::value<unsigned>()->default_value(25),
      "percent of int64 metrics the rest will be uint64 values")(
      "conf-file", po::value<std::string>(),
      "file where to write cmd line arguments in the "
      "form:\nmetric-nb=1000\nmetric-id-nb=2000")(
      "time-frame", po::value<unsigned>()->default_value(1),
      "time frame of metrics in days")(
      "select-begin", po::value<std::string>()->default_value(""),
      "AAAA-MM-DD HH:MM:SS select time frame begin\n if this option is "
      "present, the program do a select instead of insert")(
      "select-end", po::value<std::string>()->default_value(""),
      "AAAA-MM-DD HH:MM:SS select time frame end")(
      "metric-id", po::value<uint>()->default_value(100),
      "metric to search in database")(
      "nb-point", po::value<uint>()->default_value(100), "select row number");
  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("conf-file")) {
      po::store(po::parse_config_file(vm["conf-file"].as<std::string>().c_str(),
                                      desc),
                vm);
    }
    po::notify(vm);
  } catch (const boost::exception& e) {
    SPDLOG_LOGGER_ERROR(logger, " invalid arguments {}", desc);
    return 1;
  }

  if (vm.count("help") || !vm.count("db-conf")) {
    SPDLOG_LOGGER_ERROR(logger, "invalid arguments{}", desc);
    return 1;
  }

  boost::json::value connect_info;
  try {
    std::ifstream conf_file(vm["db-conf"].as<std::string>());
    std::stringstream ss;
    ss << conf_file.rdbuf();
    conf_file.close();
    std::error_code parse_err;
    connect_info = boost::json::parse(ss.str(), parse_err);
    if (parse_err) {
      SPDLOG_LOGGER_ERROR(logger, "fail to parse {}:{}",
                          vm["db-conf"].as<std::string>(), parse_err.message());
      return 1;
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(logger, "fail to load {}:{}",
                        vm["db-conf"].as<std::string>(), e.what());
    return 1;
  }

  switch (vm["log-level"].as<std::string>()[0]) {
    case 't':
      logger->set_level(spdlog::level::trace);
      break;
    case 'd':
      logger->set_level(spdlog::level::debug);
      break;
    case 'i':
      logger->set_level(spdlog::level::info);
      break;
    case 'w':
      logger->set_level(spdlog::level::warn);
      break;
    case 'e':
      logger->set_level(spdlog::level::err);
      break;
    case 'c':
      logger->set_level(spdlog::level::critical);
      break;
    case 'o':
      logger->set_level(spdlog::level::off);
      break;
    default:
      break;
  }

  unsigned bulk_size = vm["bulk-size"].as<unsigned>();
  unsigned nb_conn = vm["nb-conn"].as<unsigned>();
  boost::json::object db_conf = connect_info.as_object();
  boost::json::string database_type = db_conf["database_type"].as_string();

  metric_conf metric_cnf = {vm["metric-nb"].as<uint>(),
                            vm["metric-id-nb"].as<uint>(),
                            vm["nb-host"].as<uint>(),
                            vm["nb-service-by-host"].as<uint>(),
                            vm["float-percent"].as<uint>(),
                            vm["double-percent"].as<uint>(),
                            vm["int64-percent"].as<uint>(),
                            vm["time-frame"].as<uint>(),
                            time_point::min(),
                            time_point::min(),
                            vm["metric-id"].as<uint>(),
                            vm["nb-point"].as<uint>()};

  if (!vm["select-begin"].as<std::string>().empty() &&
      !vm["select-end"].as<std::string>().empty()) {
    std::istringstream start(vm["select-begin"].as<std::string>());
    start >> date::parse("%F %T", metric_cnf.select_begin);
    if (start.fail()) {
      SPDLOG_LOGGER_ERROR(logger, "fail to parse select time frame {}",
                          vm["select-begin"].as<std::string>());
      return 1;
    }
    std::istringstream end(vm["select-end"].as<std::string>());
    end >> date::parse("%F %T", metric_cnf.select_end);
    if (start.fail()) {
      SPDLOG_LOGGER_ERROR(logger, "fail to parse select time frame {}",
                          vm["select-end"].as<std::string>());
      return 1;
    }
  }

  metric::metric_cont_ptr to_insert;
  if (metric_cnf.select_begin == time_point::min()) {
    SPDLOG_LOGGER_INFO(logger, "create {} metrics", metric_cnf.metric_nb);
    to_insert = metric::create_metrics(metric_cnf);
  }
  SPDLOG_LOGGER_INFO(logger, "bench begin on {}", database_type);

  try {
    if (metric_cnf.select_begin > time_point::min() &&
        metric_cnf.select_begin < metric_cnf.select_end) {  // select bench
      if (database_type == "timescale" || database_type == "postgres") {
        select_timescale(io_context, logger, metric_cnf, db_conf);
      } else if (database_type == "prometheus") {
      } else if (database_type == "influxdb" || database_type == "victoria") {
        select_influxdb(io_context, logger, metric_cnf, db_conf);
      } else if (database_type == "warp10") {
        select_warp10(io_context, logger, metric_cnf, db_conf);
      } else if (database_type == "m3db") {
      } else if (database_type == "questdb") {
        select_questdb(io_context, logger, metric_cnf, db_conf);
      }
    } else {  // insert bench
      if (database_type == "timescale" || database_type == "postgres") {
        bench_timescale(io_context, logger, to_insert, db_conf, bulk_size,
                        nb_conn);
      } else if (database_type == "prometheus") {
        bench_prometheus(io_context, logger, to_insert, db_conf, bulk_size,
                         nb_conn);
      } else if (database_type == "influxdb" || database_type == "victoria") {
        bench_influxdb(io_context, logger, to_insert, db_conf, bulk_size,
                       nb_conn);
      } else if (database_type == "warp10") {
        bench_warp10(io_context, logger, to_insert, db_conf, bulk_size,
                     nb_conn);
      } else if (database_type == "m3db") {
        bench_m3db(io_context, logger, to_insert, db_conf, bulk_size, nb_conn);
      } else if (database_type == "mimir") {
        bench_mimir(io_context, logger, to_insert, db_conf, bulk_size, nb_conn);
      } else if (database_type == "questdb") {
        bench_questdb(io_context, logger, to_insert, db_conf, bulk_size,
                      nb_conn);
      }
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(logger, "fail to connect {}", e.what());
    return 1;
  }

  try {
    io_context->run();
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(logger, "run throw!!!!! {}", e.what());
  }
  SPDLOG_LOGGER_INFO(logger, "bench end");

  return 0;
}
