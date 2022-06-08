#ifndef __TSDB__BENCH__METRIC_HH
#define __TSDB__BENCH__METRIC_HH

struct metric_conf {
  uint metric_nb;
  uint metric_id_nb;
  uint nb_host;
  uint nb_service_by_host;
  uint float_percent;
  uint double_percent;
  uint int64_percent;
};

class metric {
 public:
  using metric_type = boost::variant<uint64_t, int64_t, float, double>;

  using metric_cont = std::vector<metric>;
  using metric_cont_ptr = std::shared_ptr<metric_cont>;

 protected:
  uint64_t _service_id;
  uint64_t _host_id;
  uint64_t _metric_id;
  time_point _time;

  metric_type _value;

 public:
  explicit metric(uint64_t service_id,
                  uint64_t host_id,
                  uint metric_id,
                  const time_point& now,
                  u_int64_t value)
      : _service_id(service_id),
        _host_id(host_id),
        _metric_id(metric_id),
        _time(now),
        _value(value) {}

  explicit metric(uint64_t service_id,
                  uint64_t host_id,
                  uint metric_id,
                  const time_point& now,
                  int64_t value)
      : _service_id(service_id),
        _host_id(host_id),
        _metric_id(metric_id),
        _time(now),
        _value(value) {}

  explicit metric(uint64_t service_id,
                  uint64_t host_id,
                  uint metric_id,
                  const time_point& now,
                  float value)
      : _service_id(service_id),
        _host_id(host_id),
        _metric_id(metric_id),
        _time(now),
        _value(value) {}

  explicit metric(uint64_t service_id,
                  uint64_t host_id,
                  uint metric_id,
                  const time_point& now,
                  double value)
      : _service_id(service_id),
        _host_id(host_id),
        _metric_id(metric_id),
        _time(now),
        _value(value) {}

  const uint64_t& get_service_id() const { return _service_id; }
  const uint64_t& get_host_id() const { return _host_id; }
  const uint64_t& get_metric_id() const { return _metric_id; }
  const time_point& get_time() const { return _time; }
  const metric_type& get_value() const { return _value; }

  static metric_cont_ptr create_metrics(const metric_conf& conf);
};

#endif
