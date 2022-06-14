#include <byteswap.h>

#include <catalog/pg_type_d.h>
#include <libpq-fe.h>

#include "pg_connection.hh"
#include "pg_request.hh"

using namespace pg;

static constexpr time_point pg_epoch(std::chrono::hours(262968));

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

std::pair<bool, std::string> pg_load_request::start_send_data(
    const PGresult* res) {
  bool all_text = PQbinaryTuples(res) == 0;
  if (PQnfields(res) != _columns.size()) {
    return std::make_pair(false,
                          "request columns:" + std::to_string(PQnfields(res)) +
                              " expected:" + std::to_string(_columns.size()));
  }
  for (unsigned col_index = 0; col_index < _columns.size(); ++col_index) {
    _txt_column[col_index] = PQfformat(res, col_index) == 0;
  }
  // to do one day perhaps
  return std::make_pair(false, std::string());
}

void pg_load_request::in(const time_point& val) {
  _data.emplace_back(val);
}

void pg_load_request::in(double val) {
  _data.emplace_back(val);
}

void pg_load_request::in(float val) {
  _data.emplace_back(val);
}

void pg_load_request::in(int64_t val) {
  _data.emplace_back(val);
}

void pg_load_request::in(const null_value& val) {
  _data.emplace_back(val);
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
  *_int64_value_iter = bswap_64(
      std::chrono::duration_cast<std::chrono::microseconds>(val - pg_epoch)
          .count());
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

void pg_no_result_statement_request::in(const null_value& val) {
  throw std::invalid_argument("not implemented yet");
}
