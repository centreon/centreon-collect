#include "request.hh"

void request_base::call_callback(const std::error_code& err,
                                 const std::string& err_detail) {
  bool desired = false;
  if (_callback_called.compare_exchange_strong(desired, true)) {
    _callback(err, err_detail, shared_from_this());
  }
}

void request_base::dump(std::ostream& s) const {
  s << "this:" << this;
}

std::ostream& operator<<(std::ostream& s, const request_base& req) {
  (&req)->dump(s);
  return s;
}

#define CASE_REQUEST_STR(val)             \
  case request_base::e_request_type::val: \
    s << #val;                            \
    break;
std::ostream& operator<<(std::ostream& s,
                         const request_base::e_request_type& req_type) {
  switch (req_type) {
    CASE_REQUEST_STR(simple_no_result_request);
    default:
      s << "unknown value:" << (int)req_type;
  }
  return s;
}

void no_result_request::dump(std::ostream& s) const {
  request_base::dump(s);
  s << ' ' << _request;
}
