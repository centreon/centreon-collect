#include "precomp_inc.hh"

#include <boost/json/src.hpp>

#include "db/connection.hh"
#include "influxdb/inf_http_client.hh"
#include "metric.hh"
#include "pg_metric.hh"
#include "prometheus/pr_http_server.hh"
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
                       unsigned offset) {
  constexpr std::array<request_base::e_column_type, 6> cols = {
      request_base::e_column_type::timestamp_c,
      request_base::e_column_type::int64_c,
      request_base::e_column_type::int64_c,
      request_base::e_column_type::int64_c,
      request_base::e_column_type::int64_c,
      request_base::e_column_type::double_c,
  };

  unsigned end = offset + bulk_size;
  if (end > to_insert->size()) {
    end = to_insert->size();
  }
  if (end == offset) {
    return;
  }
  auto req = std::make_shared<pg::pg_load_request>(
      "COPY metrics FROM STDIN WITH BINARY", ' ', end - offset, cols.begin(),
      cols.end(),
      [to_insert, conn, bulk_size, end](const std::error_code& err,
                                        const std::string& detail,
                                        const request_base::pointer& req) {
        if (err) {
          SPDLOG_LOGGER_ERROR(conn->get_logger(),
                              "copy_to_timescale fail to copy data {}, {}",
                              err.message(), detail);
          return;
        }
        copy_to_timescale(to_insert, conn, bulk_size, end);
      });

  for (; offset < end; ++offset) {
    *req << (*to_insert)[offset];
  }
  conn->start_request(req);
}

void bench_timescale(const io_context_ptr& io_context,
                     const logger_ptr& logger,
                     const metric::metric_cont_ptr& to_insert,
                     const boost::json::object& db_conf,
                     unsigned bulk_size,
                     unsigned nb_conn) {
  for (; nb_conn; --nb_conn) {
    connection::pointer conn =
        pg::pg_connection::create(io_context, db_conf, logger);
    conn->connect(system_clock::now() + std::chrono::seconds(30),
                  [conn, to_insert, bulk_size](const std::error_code& err,
                                               const std::string& err_detail) {
                    if (!err) {
                      copy_to_timescale(to_insert, conn, bulk_size, 0);
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
                    }
                  });
  }
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

/******************************************************************************************
 *
 *          main
 *
 ******************************************************************************************/
int main(int argc, char** argv) {
  auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  logger_ptr logger =
      std::make_shared<spdlog::logger>("tsdb_bench", stdout_sink);
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
      "number of simultaneous connections to database")(
      "float-percent", po::value<unsigned>()->default_value(25),
      "percent of float metrics")("double-percent",
                                  po::value<unsigned>()->default_value(25),
                                  "percent of double metrics")(
      "int64-percent", po::value<unsigned>()->default_value(25),
      "percent of int64 metrics the rest will be uint64 values")(
      "conf-file", po::value<std::string>(),
      "file where to write cmd line arguments in the "
      "form:\nmetric-nb=1000\nmetric-id-nb=2000");
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

  metric_conf metric_cnf = {
      vm["metric-nb"].as<uint>(),     vm["metric-id-nb"].as<uint>(),
      vm["nb-host"].as<uint>(),       vm["nb-service-by-host"].as<uint>(),
      vm["float-percent"].as<uint>(), vm["double-percent"].as<uint>(),
      vm["int64-percent"].as<uint>()};

  SPDLOG_LOGGER_INFO(logger, "create {} metrics", metric_cnf.metric_nb);
  metric::metric_cont_ptr to_insert = metric::create_metrics(metric_cnf);
  SPDLOG_LOGGER_INFO(logger, "bench begin on {}", database_type);

  try {
    if (database_type == "timescale" || database_type == "postgres") {
      bench_timescale(io_context, logger, to_insert, db_conf, bulk_size,
                      nb_conn);
    } else if (database_type == "prometheus") {
      bench_prometheus(io_context, logger, to_insert, db_conf, bulk_size,
                       nb_conn);
    } else if (database_type == "influxdb") {
      bench_influxdb(io_context, logger, to_insert, db_conf, bulk_size,
                     nb_conn);
    } else if (database_type == "warp10") {
      bench_warp10(io_context, logger, to_insert, db_conf, bulk_size, nb_conn);
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
