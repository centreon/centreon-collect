#include <byteswap.h>

#include <catalog/pg_type_d.h>
#include <libpq-fe.h>

#include "pg_connection.hh"
#include "pg_request.hh"

using namespace pg;

static constexpr time_point pg_epoch(std::chrono::hours(262968));

constexpr int64_t time_point_to_pg_timestamp(const time_point& t) {
  return std::chrono::duration_cast<std::chrono::microseconds>(t - pg_epoch)
      .count();
}

/********************************************************************************
 *
 *     pg_no_result_request
 *
 ********************************************************************************/
int pg_no_result_request::send_query(connection& conn) {
  return PQsendQuery(static_cast<pg_connection&>(conn).get_conn(),
                     _request.c_str());
}

/********************************************************************************
 *
 *     pg_load_request
 *
 ********************************************************************************/

namespace pg {
namespace detail {

/**
 * @brief as we deal with variant of vector, we need visitor to access them
 *
 */
class vector_reserve : public boost::static_visitor<> {
  size_t _size_to_reserve;

 public:
  vector_reserve(const size_t size_to_reserve)
      : _size_to_reserve(size_to_reserve) {}

  template <class T>
  void operator()(T& vect_to_reserve) const {
    vect_to_reserve.reserve(_size_to_reserve);
  }
};

class vector_size : public boost::static_visitor<size_t> {
 public:
  template <class T>
  size_t operator()(const T& vect) const {
    return vect.size();
  }
};

template <class vector_type, typename data_type>
struct vector_filler {
  void operator()(vector_type& vect, data_type data) {
    throw std::invalid_argument(
        fmt::format("can't fill {} with {} value",
                    boost::typeindex::type_id<vector_type>().pretty_name(),
                    boost::typeindex::type_id<data_type>().pretty_name()));
  }
};

template <class vector_type>
struct vector_filler<vector_type, typename vector_type::value_type> {
  void operator()(vector_type& vect, typename vector_type::value_type data) {
    vect.push_back(data);
  }
};

template <typename data_type>
class filler : public boost::static_visitor<> {
  const data_type& _data;

 public:
  explicit filler(const data_type& data) : _data(data) {}

  template <typename T>
  void operator()(T& vect) const {
    vector_filler<T, data_type>()(vect, _data);
  }
};

constexpr uint32_t _length_four = 4;
constexpr uint32_t _length_eight = 8;

class to_buffer_conv : public boost::static_visitor<> {
  expandable_buffer& _buff;
  size_t _row;

 public:
  to_buffer_conv(expandable_buffer& buff, size_t row)
      : _buff(buff), _row(row) {}

  void operator()(pg_load_request::vector_int64& vect) const {
    _buff.hton_append32(_length_eight);
    _buff.hton_append64(vect[_row]);
  }
  void operator()(pg_load_request::vector_double& vect) const {
    _buff.hton_append32(_length_eight);
    _buff.hton_append64(*(reinterpret_cast<const uint64_t*>(&vect[_row])));
  }
  void operator()(pg_load_request::vector_float& vect) const {
    _buff.hton_append32(_length_four);
    _buff.hton_append64(*(reinterpret_cast<const uint32_t*>(&vect[_row])));
  }
};

}  // namespace detail
}  // namespace pg

void pg_load_request::init() {
  _data.reserve(_nb_row * _columns.size());
  for (e_column_type col_type : _columns) {
    switch (col_type) {
      case e_column_type::double_c:
        _data.push_back(vector_double());
        break;
      case e_column_type::float_c:
        _data.push_back(vector_float());
        break;
      case e_column_type::int64_c:
      case e_column_type::timestamp_c:
        _data.push_back(vector_int64());
        break;
    }
  }

  detail::vector_reserve reserver(_nb_row);
  for (column_value& to_reserve : _data) {
    boost::apply_visitor(reserver, to_reserve);
  }

  _col_index = 0;
}

std::pair<bool, std::string> pg_load_request::start_send_data(
    const PGresult* res,
    PGconn* conn) {
  bool all_text = PQbinaryTuples(res) == 0;
  if (PQnfields(res) != _columns.size()) {
    return std::make_pair(false,
                          "request columns:" + std::to_string(PQnfields(res)) +
                              " expected:" + std::to_string(_columns.size()));
  }

  if (all_text) {
    throw std::invalid_argument("unimplemented yet");
  } else {
    create_binary_buffer(res);
  }

  // the postgres documentation says that function can return 0 in asynchronous
  // mode, in fact (14 version), it returns 0 in case of allocation failure so
  // we don't care about 0 return
  if (PQputCopyData(conn,
                    reinterpret_cast<const char*>(_pg_buffer.get_buffer()),
                    _pg_buffer.size()) > 0) {
    if (PQputCopyEnd(conn, nullptr) > 0) {
      return std::make_pair(true, std::string());
    } else {
      return std::make_pair(
          false, fmt::format("PQputCopyEnd fail to push {} bytes: {}",
                             _pg_buffer.size(), PQerrorMessage(conn)));
    }
  } else {
    return std::make_pair(false,
                          fmt::format("PQputCopyData fail to push {} bytes: {}",
                                      _pg_buffer.size(), PQerrorMessage(conn)));
  }
}

constexpr char _binary_header[] = "PGCOPY\n\377\r\n\0\0\0\0\0\0\0\0";
constexpr char _binary_trailer[] = "\377\377";

void pg_load_request::create_binary_buffer(const PGresult* res) {
  std::vector<bool> txt_column;  // true if text format, false if binary
  txt_column.reserve(_columns.size());
  for (unsigned col_index = 0; col_index < _columns.size(); ++col_index) {
    txt_column.push_back(PQfformat(res, col_index) == 0);
  }

  uint16_t nb_col = _columns.size();

  size_t nb_filled_row =
      boost::apply_visitor(detail::vector_size(), *_data.rbegin());

  _pg_buffer.append(_binary_header, 19);

  for (size_t row_index = 0; row_index < nb_filled_row; ++row_index) {
    std::vector<bool>::const_iterator txt_column_iter = txt_column.begin();
    data_matrix::iterator data_iter = _data.begin();
    _pg_buffer.hton_append16(nb_col);
    for (; data_iter != _data.end(); ++data_iter, ++txt_column_iter) {
      if (*txt_column_iter) {  // convert to string
        throw std::invalid_argument(
            "create_binary_buffer txt not implemented yet ");
      } else {
        boost::apply_visitor(detail::to_buffer_conv(_pg_buffer, row_index),
                             *data_iter);
      }
    }
  }

  _pg_buffer.append(_binary_trailer, 2);
  std::cerr << fmt::format("create_binary_buffer create a {} bytes buffer",
                           _pg_buffer.size())
            << std::endl;
}

int pg_load_request::send_query(connection& conn) {
  return PQsendQuery(static_cast<pg_connection&>(conn).get_conn(),
                     _request.c_str());
}

void pg_load_request::in(const time_point& val) {
  int64_t timestamp = time_point_to_pg_timestamp(val);
  boost::apply_visitor(detail::filler<int64_t>(timestamp), _data[_col_index++]);
  if (_col_index >= _data.size()) {
    _col_index = 0;
  }
}

void pg_load_request::in(double val) {
  boost::apply_visitor(detail::filler<double>(val), _data[_col_index++]);
  if (_col_index >= _data.size()) {
    _col_index = 0;
  }
}

void pg_load_request::in(float val) {
  boost::apply_visitor(detail::filler<float>(val), _data[_col_index++]);
  if (_col_index >= _data.size()) {
    _col_index = 0;
  }
}

void pg_load_request::in(int64_t val) {
  boost::apply_visitor(detail::filler<int64_t>(val), _data[_col_index++]);
  if (_col_index >= _data.size()) {
    _col_index = 0;
  }
}

void pg_load_request::in(uint64_t val) {
  boost::apply_visitor(detail::filler<int64_t>((int64_t)val),
                       _data[_col_index++]);
  if (_col_index >= _data.size()) {
    _col_index = 0;
  }
}

void pg_load_request::in(const null_value& val) {
  throw std::invalid_argument("not implemented yet");
}

/********************************************************************************
 *
 *     pg_no_result_statement_request
 *
 ********************************************************************************/

void pg_no_result_statement_request::init() {
  _param_oid.reserve(_in_types.size());
  _param_length.reserve(_in_types.size());
  _values.reserve(_in_types.size());
  _param_formats = std::vector<int>(_in_types.size(), 1);
  size_t int64_nb = 0, float_nb = 0, double_nb = 0;
  for (e_column_type t : _in_types) {
    switch (t) {
      case e_column_type::double_c:
        _param_oid.push_back(FLOAT8OID);
        _param_length.push_back(8);
        ++double_nb;
        break;
      case e_column_type::float_c:
        _param_oid.push_back(FLOAT4OID);
        _param_length.push_back(4);
        ++float_nb;
        break;
      case e_column_type::int64_c:
        _param_oid.push_back(INT8OID);
        _param_length.push_back(8);
        ++int64_nb;
        break;
      case e_column_type::timestamp_c:
        _param_oid.push_back(TIMESTAMPOID);
        _param_length.push_back(8);
        ++int64_nb;
        break;
      default:
        throw std::invalid_argument(
            "pg_no_result_statement_request::init() invalid "
            "type: " +
            std::to_string((unsigned)t));
    }
  }

  if (!_name.empty() &&
      !_request.empty()) {  // send stmt without execute => no value to add
    return;
  }

  _int64_values.resize(int64_nb);
  _float_values.resize(float_nb);
  _double_values.resize(double_nb);

  _int64_value_iter = _int64_values.begin();
  _float_value_iter = _float_values.begin();
  _double_value_iter = _double_values.begin();

  int64_nb = 0;
  float_nb = 0;
  double_nb = 0;
  for (e_column_type t : _in_types) {
    switch (t) {
      case e_column_type::double_c:
        _values.push_back(
            reinterpret_cast<char*>(_double_values.data() + double_nb++));
        break;
      case e_column_type::float_c:
        _values.push_back(
            reinterpret_cast<char*>(_float_values.data() + float_nb++));
        break;
      case e_column_type::int64_c:
        _values.push_back(
            reinterpret_cast<char*>(_int64_values.data() + int64_nb++));
        break;
      case e_column_type::timestamp_c:
        _values.push_back(
            reinterpret_cast<char*>(_int64_values.data() + int64_nb++));
        break;
    }
  }
}

int pg_no_result_statement_request::send_query(connection& conn) {
  if (_name.empty()) {  // execute stmt from _request
    return PQsendQueryParams(static_cast<pg_connection&>(conn).get_conn(),
                             _request.c_str(), _param_oid.size(),
                             _param_oid.data(), _values.data(),
                             _param_length.data(), _param_formats.data(), 1);
  } else {
    if (_request.empty()) {  // execute stmt prepared before
      return PQsendQueryPrepared(static_cast<pg_connection&>(conn).get_conn(),
                                 _name.c_str(), _values.size(), _values.data(),
                                 _param_length.data(), nullptr, 1);
    } else {  // prepare stmt
      return PQsendPrepare(static_cast<pg_connection&>(conn).get_conn(),
                           _name.c_str(), _request.c_str(), _param_oid.size(),
                           _param_oid.data());
    }
  }
}

void pg_no_result_statement_request::in(const time_point& val) {
  if (_int64_value_iter == _int64_values.end()) {
    throw(std::out_of_range(
        "pg_no_result_statement_request::in(const time_point)"));
  }
  *_int64_value_iter = bswap_64(time_point_to_pg_timestamp(val));
  ++_int64_value_iter;
}

void pg_no_result_statement_request::in(double val) {
  if (_double_value_iter == _double_values.end()) {
    throw(std::out_of_range("pg_no_result_statement_request::in(double)"));
  }
  *_double_value_iter = val;
  ++_double_value_iter;
}

void pg_no_result_statement_request::in(float val) {
  if (_float_value_iter == _float_values.end()) {
    throw(std::out_of_range("pg_no_result_statement_request::in(float)"));
  }
  *_float_value_iter = val;
  ++_float_value_iter;
}

void pg_no_result_statement_request::in(int64_t val) {
  if (_int64_value_iter == _int64_values.end()) {
    throw(std::out_of_range("pg_no_result_statement_request::in(int64_t)"));
  }
  *_int64_value_iter = bswap_64(val);
  ++_int64_value_iter;
}

void pg_no_result_statement_request::in(uint64_t val) {
  if (_int64_value_iter == _int64_values.end()) {
    throw(std::out_of_range("pg_no_result_statement_request::in(uint64_t)"));
  }
  *_int64_value_iter = bswap_64(val);
  ++_int64_value_iter;
}

void pg_no_result_statement_request::in(const null_value& val) {
  throw std::invalid_argument("not implemented yet");
}
