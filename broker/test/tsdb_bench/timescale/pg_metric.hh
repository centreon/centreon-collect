#ifndef __TSDB__BENCH__PG_METRIC_HH
#define __TSDB__BENCH__PG_METRIC_HH

#include "../metric.hh"
#include "pg_connection.hh"
#include "pg_request.hh"

template <>
pg::pg_load_request& operator<<<pg::pg_load_request>(pg::pg_load_request& req,
                                                     const metric& data) {
  class value_visitor : public boost::static_visitor<> {
    pg::pg_load_request& _req;

   public:
    value_visitor(pg::pg_load_request& req) : _req(req) {}

    void operator()(uint64_t val) const { _req << (int64_t)val << 0.0; }
    void operator()(int64_t val) const { _req << val << 0.0; }
    void operator()(float val) const { _req << 0l << (double)val; }
    void operator()(double val) const { _req << 0l << val; }
  };

  req << data.get_time() << data.get_host_id() << data.get_service_id()
      << data.get_metric_id();

  boost::apply_visitor(value_visitor(req), data.get_value());
  return req;
}

#endif
