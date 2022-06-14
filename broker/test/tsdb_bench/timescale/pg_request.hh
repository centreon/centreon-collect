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
 public:
  using vector_int64 = std::vector<int64_t>;
  using vector_float = std::vector<float>;
  using vector_double = std::vector<double>;

 protected:
  char _delimiter;

  using column_value =
      boost::variant<vector_int64, vector_float, vector_double>;
  using data_matrix = std::vector<column_value>;

  data_matrix _data;
  unsigned _col_index;

  expandable_buffer _pg_buffer;

  void init();
  void create_binary_buffer(const PGresult* res);

 public:
  template <typename column_iterator, class callback_type>
  pg_load_request(const std::string& req,
                  char delimiter,
                  size_t nb_row,
                  column_iterator begin,
                  column_iterator end,
                  callback_type&& callback);

  std::pair<bool, std::string> start_send_data(const PGresult*, PGconn* conn);
  int send_query(connection&) override;

  void in(const time_point&) override;
  void in(double) override;
  void in(float) override;
  void in(int64_t) override;
  void in(uint64_t) override;
  void in(const null_value&) override;
};

template <typename column_iterator, class callback_type>
pg_load_request::pg_load_request(const std::string& req,
                                 char delimiter,
                                 size_t nb_row,
                                 column_iterator begin,
                                 column_iterator end,
                                 callback_type&& callback)
    : load_request(req, nb_row, begin, end, callback), _delimiter(delimiter) {
  init();
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
  void in(uint64_t) override;
  void in(const null_value&) override;
};

}  // namespace pg

#endif
