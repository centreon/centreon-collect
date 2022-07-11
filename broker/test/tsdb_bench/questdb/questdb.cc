#include "questdb.hh"

#include "timescale/pg_request.hh"

#include "questdb/ilp/line_sender.hpp"

using namespace questdb;

questdb_client::questdb_client(const io_context_ptr& io_context,
                               const boost::json::object& conf,
                               const logger_ptr& logger)
    : _io_context(io_context), _logger(logger), _conf(conf, logger) {}

questdb_client::~questdb_client() {}

/**
 * @brief we use 2 modes to use questdb
 * ingest use of influxdb on ilp client
 * select use of postgres client
 * @param callback
 */
void questdb_client::connect_ingest() {
  _sender = std::make_unique<questdb::ilp::line_sender>(_conf.get_host(),
                                                        _conf.get_port());
  if (_sender->must_close()) {
    _logger->critical("fail to connect to {}:{}", _conf.get_host(),
                      _conf.get_port());
    throw std::runtime_error("fail to connect");
  }
}

/**
 * @brief we use 2 modes to use questdb
 * ingest use of influxdb on ilp client
 * select use of postgres client
 * @param callback
 */
void questdb_client::connect_select(connection_handler callback) {
  _pg_connection = pg::pg_connection::create(_io_context, _conf, _logger);
  _pg_connection->connect(system_clock::now() + std::chrono::seconds(10),
                          callback);
}

void questdb_client::send(metric::metric_cont_ptr& datas,
                          unsigned offset,
                          unsigned nb_to_send) {
  if (offset >= datas->size()) {
    return;
  }
  if (offset + nb_to_send > datas->size()) {
    nb_to_send = datas->size() - offset;
  }
  questdb::ilp::line_sender_buffer buff(nb_to_send, 15);
  static const questdb::ilp::table_name_view table_name("metrics");
  static const questdb::ilp::column_name_view metric_column("metric");
  static const questdb::ilp::column_name_view host_column("host");
  static const questdb::ilp::column_name_view service_column("service");
  static const questdb::ilp::column_name_view value_column("value");

  metric::metric_cont::const_iterator iter = datas->begin() + offset;
  metric::metric_cont::const_iterator end = iter + nb_to_send;

  absl::node_hash_set<std::string> metric_names;

  const std::string metric_prefix("metric_");
  for (; iter != end; ++iter) {
    const metric& to_ins(*iter);
    std::string metric_name(metric_prefix);
    metric_name += std::to_string(to_ins.get_metric_id());
    // absl::StrAppend(&metric_name, to_ins.get_metric_id());

    buff.table(table_name);
    buff.symbol(metric_column, *metric_names.insert(metric_name).first);
    buff.column(host_column, static_cast<int64_t>(to_ins.get_host_id()));
    buff.column(service_column, static_cast<int64_t>(to_ins.get_service_id()));
    buff.column(value_column, to_ins.cast_value<int64_t>());
    buff.at(questdb::ilp::timestamp_nanos(to_ins.get_time()));
  }

  _sender->flush(buff);
}

void questdb_client::select(const time_point& begin,
                            const time_point& end,
                            uint64_t metric_id,
                            unsigned nb_point,
                            select_handler callback) {
  std::ostringstream sz_request;
  unsigned interval =
      std::chrono::duration_cast<std::chrono::seconds>(end - begin).count() /
      nb_point;
  sz_request << "SELECT host, service, avg(value)  FROM metrics WHERE "
                "metric='metric_"
             << metric_id << "' AND timestamp BETWEEN '"
             << date::format("%F %T", begin) << "' AND '"
             << date::format("%F %T", end);
  if (interval > 0) {
    sz_request << "SAMPLE BY " << interval << 's';
  }
  /* not finished because too bad result on questdb ingestion
  pg::pg_no_result_request::pointer request =
      std::make_shared<pg::pg_no_result_request>(sz_request.str(), callback);

  _pg_connection->start_request(request);*/
}
