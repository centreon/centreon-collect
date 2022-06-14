#ifndef __TSDB__BENCH__PG_REQUEST_HH
#define __TSDB__BENCH__PG_REQUEST_HH

#include "request.hh"

typedef struct pg_result PGresult;

namespace pg {

class pg_no_result_request : public no_result_request {
 public:
  template <class callback_type>
  pg_no_result_request(const std::string& req, callback_type&& callback);

  int send_query(connection&);
};

template <class callback_type>
pg_no_result_request::pg_no_result_request(const std::string& req,
                                           callback_type&& callback)
    : no_result_request(req, callback) {}

/**
 * @brief COPY request
 *
 */
class pg_load_request : public load_request {
 protected:
  char _delimiter;

  using value_type =
      boost::variant<time_point, double, float, int64_t, null_value>;

  size_t _nb_column, _nb_row;
  std::vector<value_type> _data;
  std::vector<bool> _txt_column;  // true if text format, false if binary

 public:
  template <typename column_iterator, class callback_type>
  pg_load_request(const std::string& req,
                  char delimiter,
                  size_t nb_row,
                  size_t nb_column,
                  column_iterator begin,
                  column_iterator end,
                  callback_type&& callback);

  std::pair<bool, std::string> start_send_data(const PGresult*);

  void in(const time_point&) override;
  void in(double) override;
  void in(float) override;
  void in(int64_t) override;
  void in(const null_value&) override;
};

template <typename column_iterator, class callback_type>
pg_load_request::pg_load_request(const std::string& req,
                                 char delimiter,
                                 size_t nb_row,
                                 size_t nb_column,
                                 column_iterator begin,
                                 column_iterator end,
                                 callback_type&& callback)
    : load_request(req, begin, end, callback),
      _delimiter(delimiter),
      _nb_column(nb_column),
      _nb_row(nb_row) {
  _data.reserve(nb_row * nb_column);
  _txt_column.resize(nb_column);
}

class pg_no_result_statement_request : public no_result_statement_request {
 protected:
  std::vector<unsigned int> _param_oid;

  std::vector<int> _param_length;
  std::vector<int64_t> _int64_values;
  std::vector<float> _float_values;
  std::vector<double> _double_values;
  std::vector<char*> _values;
  std::vector<int> _param_formats;

  std::vector<int64_t>::iterator _int64_value_iter;
  std::vector<float>::iterator _float_value_iter;
  std::vector<double>::iterator _double_value_iter;

  void init();

 public:
  struct unkown_column_type_exception : public virtual boost::exception,
                                        public virtual std::exception {};

  template <typename column_iterator, class callback_type>
  pg_no_result_statement_request(const std::string& name,
                                 const std::string& req,
                                 column_iterator begin,
                                 column_iterator end,
                                 callback_type&& callback)
      : no_result_statement_request(name, req, begin, end, callback) {
    init();
  }

  int send_query(connection&) override;

  void in(const time_point&) override;
  void in(double) override;
  void in(float) override;
  void in(int64_t) override;
  void in(const null_value&) override;
};

}  // namespace pg

#endif
