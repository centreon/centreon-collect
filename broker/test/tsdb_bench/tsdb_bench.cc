#include "precomp_inc.hh"

#include <boost/json/src.hpp>

#include "db/connection.hh"
#include "metric.hh"
#include "timescale/pg_connection.hh"

namespace po = boost::program_options;

using connection_cont = std::vector<connection::pointer>;

void bench_timescale(const io_context_ptr& io_context,
                     const logger_ptr& logger,
                     metric::metric_cont_ptr& to_insert,
                     const boost::json::object& db_conf,
                     unsigned bulk_size,
                     unsigned nb_conn) {
  std::shared_ptr<std::mutex> to_insert_mut;
  for (; nb_conn; --nb_conn) {
    connection::pointer conn =
        pg::pg_connection::create(io_context, db_conf, logger);
    conn->connect(
        system_clock::now() + std::chrono::seconds(30),
        [conn, to_insert, to_insert_mut](const std::error_code& err,
                                         const std::string& err_detail) {
          if (!err) {
            conn->start_request(std::make_shared<no_result_request>(
                "select * from toto",
                [](const std::error_code&, const std::string&,
                   const request_base::pointer&) {}));
          }
        });
  }
}

int main(int argc, char** argv) {
  auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  logger_ptr logger =
      std::make_shared<spdlog::logger>("tsdb_bench", stdout_sink);
  spdlog::register_logger(logger);

  io_context_ptr io_context(std::make_shared<asio::io_context>());

  po::options_description desc("Allowed options");
  desc.add_options()("help", "produce help message")(
      "metric-nb", po::value<uint>()->default_value(100000), "nb to insert")(
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

  SPDLOG_LOGGER_INFO(logger, "create metrics");
  metric_conf metric_cnf = {
      vm["metric-nb"].as<uint>(),     vm["metric-id-nb"].as<uint>(),
      vm["nb-host"].as<uint>(),       vm["nb-service-by-host"].as<uint>(),
      vm["float-percent"].as<uint>(), vm["double-percent"].as<uint>(),
      vm["int64-percent"].as<uint>()};

  metric::metric_cont_ptr to_insert = metric::create_metrics(metric_cnf);
  SPDLOG_LOGGER_INFO(logger, "bench begin on {}", database_type);

  try {
    if (database_type == "timescale" || database_type == "postgres") {
      bench_timescale(io_context, logger, to_insert, db_conf, bulk_size,
                      nb_conn);
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
